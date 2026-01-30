// =============================================================================
// AMOURANTH RTX Engine — Pipeline Manager
// Ray tracing + compute pipeline, SBT, descriptor management
// Version 30.8 — January 27, 2026
// - Fixed push constant stageFlags to match layout range (fixes VUID-01796)
// - On-demand compute pipeline creation in dispatchLivingWorld
// - Timing driven by totalTime push only — no semaphores/fences
// - Eternal descriptor buffer + BufferManager macros
// - No sets, no vkUpdateDescriptorSets — vkGetDescriptorEXT + memcpy
// - Bindings: 0=TLAS, 1=output, 2=camera UBO, 3=materials, 7=living world
// - Living world buffer (64 bytes) bound at startup
// - Materials bound to 3 in descriptor update
// - SBT forged at startup via BufferManager
// - No blue noise — high-SPP convergence
// - Empire stable — pink photons breathe free
// =============================================================================

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <algorithm>
#include <array>
#include <vector>
#include <atomic>
#include <fstream>
#include <mutex>

using StoneKey::stone_device;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_physical;
using StoneKey::stone_rtprops;
using StoneKey::stone_transient_pool;
using StoneKey::stone_compute_pipeline;
using StoneKey::stone_rt_pipeline;
using StoneKey::stone_pipeline_layout;
using StoneKey::stone_seal_pipeline_layout;
using StoneKey::stone_seal_compute_pipeline;
using StoneKey::stone_seal_rt_pipeline;

using BufferManager::BufferInfo;
using BufferManager::align_up;

// Static atomic members
std::atomic<bool> RTX::PipelineManager::g_pipelineNeedsRebuild{false};

// Bindings — matches your kMainBindings
static constexpr std::array<VkDescriptorSetLayoutBinding, 9> kMainBindings = {{
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR}, // Materials
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}, // LivingWorldBuffer
    {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}, // MaterialOverrides
    {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR} // Reserved
}};

namespace RTX {

inline static std::mutex rebuildMutex;

// Living world buffer handle (64 bytes, persistent)
uint64_t livingWorldBufferHandle_ = 0;

// Descriptor buffer handle and mapping
uint64_t descriptorBufferHandle_ = 0;
void* descriptorMapped_ = nullptr;
VkDeviceAddress descriptorBufferAddress_ = 0;

// Binding offsets and descriptor properties
std::array<VkDeviceSize, kMainBindings.size()> bindingOffsets_{};
VkPhysicalDeviceDescriptorBufferPropertiesEXT descProps_{};
static bool descPropsCached = false;

// Eternal SBT forged flag
bool s_eternalSbtForged = false;

// Full push constant stage (must match layout range)
static constexpr VkShaderStageFlags FULL_PUSH_MASK =
    VK_SHADER_STAGE_COMPUTE_BIT |
    VK_SHADER_STAGE_RAYGEN_BIT_KHR |
    VK_SHADER_STAGE_MISS_BIT_KHR |
    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
    VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

// =============================================================================
// Constructor
// =============================================================================
PipelineManager::PipelineManager()
{
    LOG_INFO_CAT("PIPELINE", "Initializing PipelineManager — frame-free mode");

    if (!g_ext.vkCmdTraceRaysKHR ||
        !g_ext.vkCreateRayTracingPipelinesKHR ||
        !g_ext.vkGetRayTracingShaderGroupHandlesKHR ||
        !g_ext.vkCreateAccelerationStructureKHR ||
        !g_ext.vkGetAccelerationStructureBuildSizesKHR ||
        !g_ext.vkGetBufferDeviceAddress ||
        !g_ext.vkGetDescriptorEXT ||
        !g_ext.vkCmdBindDescriptorBuffersEXT ||
        !g_ext.vkCmdSetDescriptorBufferOffsetsEXT) {
        LOG_FATAL_CAT("PIPELINE", "Required extensions missing");
        throw std::runtime_error("Required extensions missing");
    }

    dummyTLAS_ = Handle<VkAccelerationStructureKHR>(
        createDummyTLAS(), stone_device(), g_ext.vkDestroyAccelerationStructureKHR);

    BM_CREATE(livingWorldBufferHandle_, 64,
              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
              VK_BUFFER_USAGE_TRANSFER_DST_BIT |
              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
              "LivingWorldBuffer");

    if (livingWorldBufferHandle_ == 0) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create living world buffer");
        throw std::runtime_error("Living world buffer creation failed");
    }

    cacheDescriptorProperties();

    LOG_SUCCESS_CAT("PIPELINE", "PipelineManager initialized");
}

// =============================================================================
// Pipeline Layout
// =============================================================================
void PipelineManager::createPipelineLayout()
{
    if (rtPipelineLayout_.valid()) return;

    LOG_INFO_CAT("PIPELINE", "Creating pipeline layout");

    VkDescriptorSetLayoutCreateInfo mainInfo{};
    mainInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    mainInfo.bindingCount = static_cast<uint32_t>(kMainBindings.size());
    mainInfo.pBindings    = kMainBindings.data();
    mainInfo.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(stone_device(), &mainInfo, nullptr, &mainLayout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create main layout: {}", string_VkResult(res));
        return;
    }
    rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(mainLayout, stone_device(), vkDestroyDescriptorSetLayout);

    for (uint32_t i = 0; i < kMainBindings.size(); ++i) {
        g_ext.vkGetDescriptorSetLayoutBindingOffsetEXT(stone_device(), mainLayout, i, &bindingOffsets_[i]);
    }

    VkDescriptorSetLayoutBinding texBinding{
        .binding         = 0,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1024,
        .stageFlags      = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR
    };

    VkDescriptorSetLayoutCreateInfo texInfo{};
    texInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    texInfo.bindingCount = 1;
    texInfo.pBindings    = &texBinding;
    texInfo.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout texLayout = VK_NULL_HANDLE;
    res = vkCreateDescriptorSetLayout(stone_device(), &texInfo, nullptr, &texLayout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create texture layout: {}", string_VkResult(res));
        return;
    }
    texDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(texLayout, stone_device(), vkDestroyDescriptorSetLayout);

    VkDescriptorSetLayoutCreateInfo emptyInfo{};
    emptyInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    emptyInfo.bindingCount = 0;
    emptyInfo.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    res = vkCreateDescriptorSetLayout(stone_device(), &emptyInfo, nullptr, &emptyLayout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create empty layout: {}", string_VkResult(res));
        return;
    }
    emptyDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(emptyLayout, stone_device(), vkDestroyDescriptorSetLayout);

    const VkDescriptorSetLayout layouts[4] = {
        rtDescriptorSetLayout_.get(),
        emptyDescriptorSetLayout_.get(),
        texDescriptorSetLayout_.get(),
        emptyDescriptorSetLayout_.get()
    };

    VkPushConstantRange push{};
    push.stageFlags = FULL_PUSH_MASK;  // Matches the full mask we now use everywhere
    push.offset     = 0;
    push.size       = sizeof(float);  // totalTime only

    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount         = 4;
    plInfo.pSetLayouts            = layouts;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges    = &push;

    VkPipelineLayout pl = VK_NULL_HANDLE;
    res = vkCreatePipelineLayout(stone_device(), &plInfo, nullptr, &pl);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create pipeline layout: {}", string_VkResult(res));
        return;
    }

    rtPipelineLayout_ = Handle<VkPipelineLayout>(pl, stone_device(), vkDestroyPipelineLayout);
    stone_seal_pipeline_layout(pl);

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created");
}

// =============================================================================
// Cache descriptor buffer properties
// =============================================================================
void PipelineManager::cacheDescriptorProperties()
{
    if (descPropsCached) return;

    LOG_INFO_CAT("PIPELINE", "Caching descriptor buffer properties");

    VkPhysicalDeviceDescriptorBufferPropertiesEXT dbProps{};
    dbProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &dbProps;

    vkGetPhysicalDeviceProperties2(stone_physical(), &props2);

    descProps_ = dbProps;
    descPropsCached = true;

    LOG_SUCCESS_CAT("PIPELINE", "Descriptor buffer properties cached");
}

// =============================================================================
// Compute Pipeline — created on-demand in dispatchLivingWorld
// =============================================================================
void PipelineManager::createComputePipeline()
{
    if (stone_compute_pipeline() != VK_NULL_HANDLE) return;

    LOG_INFO_CAT("PIPELINE", "Creating compute pipeline for living world");

    VkShaderModule compModule = loadShader("assets/shaders/compute/living_world.spv");
    if (compModule == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Failed to load living_world.spv");
        return;
    }

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = compModule;
    stage.pName  = "main";

    VkComputePipelineCreateInfo compInfo{};
    compInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compInfo.stage  = stage;
    compInfo.layout = rtPipelineLayout_.get();
    compInfo.flags  = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkPipeline compPipe = VK_NULL_HANDLE;
    VkResult res = vkCreateComputePipelines(stone_device(), VK_NULL_HANDLE, 1, &compInfo, nullptr, &compPipe);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkCreateComputePipelines failed: {}", string_VkResult(res));
        vkDestroyShaderModule(stone_device(), compModule, nullptr);
        return;
    }

    computePipeline_ = Handle<VkPipeline>(compPipe, stone_device(), vkDestroyPipeline);
    vkDestroyShaderModule(stone_device(), compModule, nullptr);

    stone_seal_compute_pipeline(compPipe);

    LOG_SUCCESS_CAT("PIPELINE", "Compute pipeline created");
}

// =============================================================================
// Dispatch living world — creates pipeline if missing
// =============================================================================
void PipelineManager::dispatchLivingWorld(VkCommandBuffer cmd, float totalTime) noexcept
{
    // One-time creation if not sealed yet
    if (stone_compute_pipeline() == VK_NULL_HANDLE) {
        createComputePipeline();
    }

    if (stone_compute_pipeline() == VK_NULL_HANDLE) {
        return;  // Failed to create — silent skip
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, stone_compute_pipeline());

    VkDescriptorBufferBindingInfoEXT bindInfo{};
    bindInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    bindInfo.address = descriptorBufferAddress_;
    bindInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    g_ext.vkCmdBindDescriptorBuffersEXT(cmd, 1, &bindInfo);

    uint32_t bufferIndex = 0;
    VkDeviceSize offset = 0;
    g_ext.vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                             stone_pipeline_layout(), 0, 1, &bufferIndex, &offset);

    // FIXED: Use full stage mask to match the layout's push constant range
    vkCmdPushConstants(cmd, stone_pipeline_layout(),
                       FULL_PUSH_MASK,
                       0, sizeof(float), &totalTime);

    vkCmdDispatch(cmd, 1, 1, 1);
}

// =============================================================================
// Dummy TLAS
// =============================================================================
VkAccelerationStructureKHR PipelineManager::createDummyTLAS()
{
    LOG_INFO_CAT("PIPELINE", "Creating dummy TLAS");

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.data.deviceAddress = 0;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    constexpr uint32_t primitiveCount = 1;
    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo);

    uint64_t bufferHandle = 0;
    BM_CREATE(bufferHandle, sizeInfo.accelerationStructureSize,
              VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              "Dummy_TLAS_Buffer");

    if (bufferHandle == 0) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create dummy TLAS buffer");
        return VK_NULL_HANDLE;
    }

    const BufferInfo* bufferInfo = BM_GET(bufferHandle);
    if (!bufferInfo) {
        BM_DESTROY(bufferHandle);
        return VK_NULL_HANDLE;
    }

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = bufferInfo->buffer;
    createInfo.size   = sizeInfo.accelerationStructureSize;
    createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkAccelerationStructureKHR as = VK_NULL_HANDLE;
    VkResult result = g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &as);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create dummy TLAS: {}", string_VkResult(result));
        BM_DESTROY(bufferHandle);
        return VK_NULL_HANDLE;
    }

    dummyAccelBuffer_ = Handle<VkBuffer>(bufferInfo->buffer, stone_device(), vkDestroyBuffer);
    dummyAccelMemory_ = Handle<VkDeviceMemory>(bufferInfo->memory, stone_device(), vkFreeMemory);

    LOG_SUCCESS_CAT("PIPELINE", "Dummy TLAS created");
    return as;
}

// =============================================================================
// Shader Loading — improved logging
// =============================================================================
VkShaderModule PipelineManager::loadShader(const std::string& relativePath) const
{
    LOG_INFO_CAT("PIPELINE", "Loading shader: {}", relativePath);

    // Try Linux path first (most common dev environment)
    std::string fullPath = std::format("build/bin/Linux/{}", relativePath);
    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);

    // If Linux path fails, try Windows path
    if (!file.is_open()) {
        fullPath = std::format("build-windows/bin/Windows/{}", relativePath);
        file.open(fullPath, std::ios::ate | std::ios::binary);
    }

    // Final fallback: try plain "assets/shaders/" if both above fail (useful for source tree)
    if (!file.is_open()) {
        fullPath = std::format("assets/shaders/{}", relativePath);
        file.open(fullPath, std::ios::ate | std::ios::binary);
    }

    if (!file.is_open()) {
        LOG_FATAL_CAT("PIPELINE", "Shader not found — tried:\n  {}\n  {}\n  {}", 
                      std::format("build/bin/Linux/{}", relativePath),
                      std::format("build-windows/bin/Windows/{}", relativePath),
                      std::format("assets/shaders/{}", relativePath));
        return VK_NULL_HANDLE;
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        LOG_FATAL_CAT("PIPELINE", "Invalid SPIR-V size ({} bytes): {}", fileSize, fullPath);
        return VK_NULL_HANDLE;
    }

    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), fileSize);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(stone_device(), &createInfo, nullptr, &module);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create shader module: {} (size {} bytes) — path: {}", 
                      string_VkResult(result), fileSize, fullPath);
        return VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Shader loaded: {} ({} bytes) from {}", relativePath, fileSize, fullPath);
    return module;
}

// =============================================================================
// Ray Tracing Pipeline Creation
// =============================================================================
void PipelineManager::createRayTracingPipeline()
{
    LOG_INFO_CAT("PIPELINE", "Creating ray tracing pipeline");

    shaderModules_.clear();

    auto load = [this](std::string_view path) -> VkShaderModule {
        return loadShader(std::string(path));
    };

    VkShaderModule raygen = load("assets/shaders/raytracing/raygen.spv");
    VkShaderModule miss   = load("assets/shaders/raytracing/miss.spv");
    VkShaderModule chit   = load("assets/shaders/raytracing/closest_hit.spv");
    VkShaderModule ahit   = load("assets/shaders/raytracing/anyhit.spv");

    if (!raygen || !miss || !chit || !ahit) {
        LOG_FATAL_CAT("PIPELINE", "Shader load failed");
        return;
    }

    shaderModules_.emplace_back(raygen, stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(miss,   stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(chit,   stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(ahit,   stone_device(), vkDestroyShaderModule);

    std::vector<VkPipelineShaderStageCreateInfo> stages(4);

    stages[0] = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                 .stage  = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                 .module = raygen,
                 .pName  = "main"};

    stages[1] = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                 .stage  = VK_SHADER_STAGE_MISS_BIT_KHR,
                 .module = miss,
                 .pName  = "main"};

    stages[2] = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                 .stage  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                 .module = chit,
                 .pName  = "main"};

    stages[3] = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                 .stage  = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                 .module = ahit,
                 .pName  = "main"};

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups(3);

    groups[0] = {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                 .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                 .generalShader = 0,
                 .closestHitShader = VK_SHADER_UNUSED_KHR,
                 .anyHitShader = VK_SHADER_UNUSED_KHR,
                 .intersectionShader = VK_SHADER_UNUSED_KHR};

    groups[1] = {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                 .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                 .generalShader = 1,
                 .closestHitShader = VK_SHADER_UNUSED_KHR,
                 .anyHitShader = VK_SHADER_UNUSED_KHR,
                 .intersectionShader = VK_SHADER_UNUSED_KHR};

    groups[2] = {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                 .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                 .generalShader = VK_SHADER_UNUSED_KHR,				 
                 .closestHitShader = 2,
                 .anyHitShader = 3,                 
                 .intersectionShader = VK_SHADER_UNUSED_KHR};

    raygenGroupCount_ = 1;
    missGroupCount_   = 1;
    hitGroupCount_    = 1;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount                   = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages                      = stages.data();
    pipelineInfo.groupCount                   = static_cast<uint32_t>(groups.size());
    pipelineInfo.pGroups                      = groups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout                       = rtPipelineLayout_.get();
    pipelineInfo.flags                        = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result = g_ext.vkCreateRayTracingPipelinesKHR(
        stone_device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkCreateRayTracingPipelinesKHR failed: {}", string_VkResult(result));
        return;
    }

    rtPipeline_ = Handle<VkPipeline>(pipeline, stone_device(), vkDestroyPipeline);
    stone_seal_rt_pipeline(pipeline);

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing pipeline created");
}

// =============================================================================
// SBT Creation
// =============================================================================
void PipelineManager::createShaderBindingTable(VkCommandPool pool, VkQueue queue, VkCommandBuffer providedCmd)
{
    if (s_eternalSbtForged) return;

    VkPipeline rtPipe = stone_rt_pipeline();
    if (rtPipe == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "No ray-tracing pipeline — cannot forge SBT");
        return;
    }

    cacheDeviceProperties();
    const auto& rtProps = stone_rtprops();

    const VkDeviceSize handleSize      = rtProps.shaderGroupHandleSize;
    const VkDeviceSize handleAlign     = rtProps.shaderGroupHandleAlignment;
    const VkDeviceSize baseAlign       = rtProps.shaderGroupBaseAlignment;

    const VkDeviceSize recordStride    = align_up(handleSize, std::max(handleAlign, baseAlign));

    const uint32_t totalGroups = raygenGroupCount_ + missGroupCount_ + hitGroupCount_;

    const VkDeviceSize raygenSize = recordStride;
    const VkDeviceSize missSize   = missGroupCount_ * recordStride;
    const VkDeviceSize hitSize    = hitGroupCount_ * recordStride;

    VkDeviceSize sbtSize = raygenSize + missSize + hitSize;
    sbtSize = align_up(sbtSize, baseAlign);

    LOG_INFO_CAT("PIPELINE", "Forging SBT — size={}", sbtSize);

    uint64_t sbtHandle = 0;
    BM_CREATE(sbtHandle, sbtSize,
              VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              "EternalSBT");

    if (sbtHandle == 0) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create SBT buffer");
        return;
    }

    const BufferInfo* sbtInfo = BM_GET(sbtHandle);
    if (!sbtInfo) {
        BM_DESTROY(sbtHandle);
        return;
    }

    VkDeviceAddress sbtAddress = BM_GET_DEVICE_ADDRESS(sbtHandle);

    std::vector<uint8_t> handles(totalGroups * handleSize);
    VkResult res = g_ext.vkGetRayTracingShaderGroupHandlesKHR(
        stone_device(), rtPipe, 0, totalGroups,
        handles.size(), handles.data());

    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkGetRayTracingShaderGroupHandlesKHR failed: {}", string_VkResult(res));
        BM_DESTROY(sbtHandle);
        return;
    }

    VkCommandBuffer uploadCmd = providedCmd;
    bool ownCmd = (providedCmd == VK_NULL_HANDLE);
    VkCommandPool cmdPool = pool ? pool : stone_transient_pool();

    if (ownCmd) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = cmdPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        res = vkAllocateCommandBuffers(stone_device(), &allocInfo, &uploadCmd);
        if (res != VK_SUCCESS) {
            BM_DESTROY(sbtHandle);
            return;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(uploadCmd, &beginInfo);
    }

    // Clean macro upload — handles staging ring, memcpy, vkCmdCopyBuffer internally
    BM_UPLOAD_TO_BUFFER(sbtHandle, handles.data(), handles.size(), uploadCmd);

    VkMemoryBarrier memBarrier{};
    memBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(
        uploadCmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        1, &memBarrier,
        0, nullptr,
        0, nullptr
    );

    vkEndCommandBuffer(uploadCmd);

    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &uploadCmd;

    vkQueueSubmit(queue ? queue : stone_graphics_queue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue ? queue : stone_graphics_queue());

    if (ownCmd) {
        vkFreeCommandBuffers(stone_device(), cmdPool, 1, &uploadCmd);
    }

    VkDeviceAddress raygenAddr = align_up(sbtAddress, rtProps.shaderGroupBaseAlignment);
    VkDeviceAddress missAddr   = align_up(raygenAddr + raygenSize, rtProps.shaderGroupBaseAlignment);
    VkDeviceAddress hitAddr    = align_up(missAddr + missSize, rtProps.shaderGroupBaseAlignment);

    sbtAddress_ = sbtAddress;
    sbtSize_    = sbtSize;

    raygenSbtRegion_ = {raygenAddr, recordStride, raygenSize};
    missSbtRegion_   = {missAddr,   recordStride, missSize};
    hitSbtRegion_    = {hitAddr,    recordStride, hitSize};

    sbtBuffer_ = Handle<VkBuffer>(sbtInfo->buffer, stone_device(), vkDestroyBuffer);
    sbtMemory_ = Handle<VkDeviceMemory>(sbtInfo->memory, stone_device(), vkFreeMemory);

    s_eternalSbtForged = true;

    LOG_SUCCESS_CAT("PIPELINE", "SBT forged");
}

// =============================================================================
// Destructor
// =============================================================================
PipelineManager::~PipelineManager()
{
    if (descriptorMapped_) {
        const BufferInfo* info = BM_GET(descriptorBufferHandle_);
        if (info) vkUnmapMemory(stone_device(), info->memory);
    }

    BM_DESTROY(descriptorBufferHandle_);
    BM_DESTROY(livingWorldBufferHandle_);
    sbtBuffer_.reset();
    sbtMemory_.reset();

    LOG_INFO_CAT("PIPELINE", "PipelineManager destroyed");
}

// =============================================================================
// Cache ray tracing device properties
// =============================================================================
void RTX::PipelineManager::cacheDeviceProperties()
{
    static bool cached = false;
    if (cached) return;

    LOG_INFO_CAT("PIPELINE", "Caching ray tracing properties");

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;

    vkGetPhysicalDeviceProperties2(stone_physical(), &props2);

    if (rtProps.shaderGroupHandleSize == 0) {
        LOG_FATAL_CAT("PIPELINE", "Ray tracing not supported");
        throw std::runtime_error("Ray tracing unsupported");
    }

    StoneKey::stone_seal_rtprops(rtProps);

    cached = true;

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing properties cached");
}

void RTX::PipelineManager::traceRays(VkCommandBuffer cmd, uint32_t width, uint32_t height) {
    if (cmd == VK_NULL_HANDLE) return;

    VkPipeline rtPipe = stone_rt_pipeline();
    if (rtPipe == VK_NULL_HANDLE) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipe);

    VkDescriptorBufferBindingInfoEXT bindInfo{};
    bindInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    bindInfo.address = descriptorBufferAddress_;
    bindInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    g_ext.vkCmdBindDescriptorBuffersEXT(cmd, 1, &bindInfo);

    uint32_t bufferIndex = 0;
    VkDeviceSize offset = 0;
    g_ext.vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                             stone_pipeline_layout(), 0, 1, &bufferIndex, &offset);

    float totalTime = static_cast<float>(RTX::TotalTime::get().seconds());

    vkCmdPushConstants(cmd, stone_pipeline_layout(),
                       FULL_PUSH_MASK,
                       0, sizeof(float), &totalTime);

    VkStridedDeviceAddressRegionKHR raygenRegion   = raygenSbtRegion_;
    VkStridedDeviceAddressRegionKHR missRegion     = missSbtRegion_;
    VkStridedDeviceAddressRegionKHR hitRegion      = hitSbtRegion_;
    VkStridedDeviceAddressRegionKHR callableRegion = {};

    g_ext.vkCmdTraceRaysKHR(cmd,
                            &raygenRegion,
                            &missRegion,
                            &hitRegion,
                            &callableRegion,
                            width, height, 1);
}

// =============================================================================
// Write RT descriptors — eternal buffer memcpy
// =============================================================================
void RTX::PipelineManager::writeRTDescriptorsToBuffer(const RTDescriptorUpdate& updateInfo) noexcept
{
    if (descriptorBufferHandle_ == 0) {
        constexpr VkDeviceSize INITIAL_SIZE = 4096ULL;
        descriptorBufferHandle_ = BufferManager::createDescriptorBuffer(INITIAL_SIZE, "EternalDescriptorBuffer");
        if (descriptorBufferHandle_ == 0) return;
        descriptorBufferAddress_ = BM_GET_DEVICE_ADDRESS(descriptorBufferHandle_);
    }

    if (descriptorMapped_ == nullptr) {
        descriptorMapped_ = BM_LAZY_MAP_DESCRIPTOR(descriptorBufferHandle_);
        if (descriptorMapped_ == nullptr) return;
    }

    if (updateInfo.tlas == VK_NULL_HANDLE) return;

    // Binding 0: TLAS address
    {
        VkAccelerationStructureDeviceAddressInfoKHR asAddrInfo{};
        asAddrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        asAddrInfo.accelerationStructure = updateInfo.tlas;

        VkDeviceAddress asAddr = g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &asAddrInfo);

        VkDescriptorGetInfoEXT getInfo{};
        getInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        getInfo.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        getInfo.data.accelerationStructure = asAddr;

        g_ext.vkGetDescriptorEXT(stone_device(), &getInfo, descProps_.accelerationStructureDescriptorSize,
                                 static_cast<uint8_t*>(descriptorMapped_) + bindingOffsets_[0]);
    }

    // Binding 1: Output image
    if (updateInfo.rtOutputView != VK_NULL_HANDLE) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = updateInfo.rtOutputView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorGetInfoEXT getInfo{};
        getInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        getInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        getInfo.data.pStorageImage = &imageInfo;

        g_ext.vkGetDescriptorEXT(stone_device(), &getInfo, descProps_.storageImageDescriptorSize,
                                 static_cast<uint8_t*>(descriptorMapped_) + bindingOffsets_[1]);
    }

    // Binding 2: Camera UBO
    if (updateInfo.ubo != VK_NULL_HANDLE && updateInfo.uboSize > 0) {
        VkBufferDeviceAddressInfo uboAddrInfo{};
        uboAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        uboAddrInfo.buffer = updateInfo.ubo;

        VkDeviceAddress uboAddr = g_ext.vkGetBufferDeviceAddress(stone_device(), &uboAddrInfo);

        VkDescriptorAddressInfoEXT addrInfo{};
        addrInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        addrInfo.address = uboAddr;
        addrInfo.range = updateInfo.uboSize;

        VkDescriptorGetInfoEXT getInfo{};
        getInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        getInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        getInfo.data.pUniformBuffer = &addrInfo;

        g_ext.vkGetDescriptorEXT(stone_device(), &getInfo, descProps_.uniformBufferDescriptorSize,
                                 static_cast<uint8_t*>(descriptorMapped_) + bindingOffsets_[2]);
    }

    // Binding 3: Materials
    if (updateInfo.materialsBuffer != VK_NULL_HANDLE && updateInfo.materialsSize > 0) {
        VkBufferDeviceAddressInfo matAddrInfo{};
        matAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        matAddrInfo.buffer = updateInfo.materialsBuffer;

        VkDeviceAddress matAddr = g_ext.vkGetBufferDeviceAddress(stone_device(), &matAddrInfo);

        VkDescriptorAddressInfoEXT addrInfo{};
        addrInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        addrInfo.address = matAddr;
        addrInfo.range = updateInfo.materialsSize;

        VkDescriptorGetInfoEXT getInfo{};
        getInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        getInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        getInfo.data.pStorageBuffer = &addrInfo;

        g_ext.vkGetDescriptorEXT(stone_device(), &getInfo, descProps_.storageBufferDescriptorSize,
                                 static_cast<uint8_t*>(descriptorMapped_) + bindingOffsets_[3]);
    }

    // Binding 7: Living world
    if (livingWorldBufferHandle_ != 0) {
        VkBuffer lwBuffer = BM_GET_BUFFER(livingWorldBufferHandle_);

        VkBufferDeviceAddressInfo lwAddrInfo{};
        lwAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        lwAddrInfo.buffer = lwBuffer;

        VkDeviceAddress lwAddr = g_ext.vkGetBufferDeviceAddress(stone_device(), &lwAddrInfo);

        VkDescriptorAddressInfoEXT addrInfo{};
        addrInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        addrInfo.address = lwAddr;
        addrInfo.range = 64;

        VkDescriptorGetInfoEXT getInfo{};
        getInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        getInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        getInfo.data.pStorageBuffer = &addrInfo;

        g_ext.vkGetDescriptorEXT(stone_device(), &getInfo, descProps_.storageBufferDescriptorSize,
                                 static_cast<uint8_t*>(descriptorMapped_) + bindingOffsets_[7]);
    }

    // No flush needed (host-coherent)
}

} // namespace RTX nice