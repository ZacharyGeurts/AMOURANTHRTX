// =============================================================================
// AMOURANTH RTX Engine - Pipeline Manager
// Ray tracing pipeline, SBT, and descriptor management
// Version 30.12 — January 21, 2026
// - Preconfigured exactly 8 safe bindings for set 0 (guaranteed by Vulkan spec)
//   → TLAS (0), Output Image (1), Camera UBO (2), Materials (3), Instance Data (4),
//     Blue Noise (5), Prev Frame (6), Debug/Constants (7)
// - Removed high-risk binding 31 — no StoneKey uniform in RT shaders
// - Unified logging with LOG_*_CAT("PIPELINE", ...)
// - Hardware-adaptive SBT alignment preserved
// =============================================================================

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <vector>
#include <atomic>
#include <fstream>
#include <mutex>

using StoneKey::stone_device;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_physical;
using StoneKey::stone_rtprops;
using StoneKey::stone_transient_pool;

using BufferManager::BufferInfo;
using BufferManager::align_up;

// Static atomic members
std::atomic<bool>     RTX::PipelineManager::g_pipelineNeedsRebuild{false};
std::atomic<uint32_t> RTX::PipelineManager::g_rebuildRequestedFrame{UINT32_MAX};

namespace RTX {

inline static std::mutex rebuildMutex;

// Fixed 8 bindings for set 0 — safe & guaranteed
static constexpr std::array<VkDescriptorSetLayoutBinding, 8> kMainBindings = {{
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR}, // instance data/custom index
    {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR},
    {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR}, // prev frame accumulation
    {7, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR} // debug/constants
}};

// =============================================================================
// Constructor — Minimal early setup
// =============================================================================
PipelineManager::PipelineManager(VkDevice device, VkPhysicalDevice phys)
{
    LOG_INFO_CAT("PIPELINE", "Initializing PipelineManager");

    RTX::loadDeviceExtensions(device);

    if (!g_ext.vkCmdTraceRaysKHR ||
        !g_ext.vkCreateRayTracingPipelinesKHR ||
        !g_ext.vkGetRayTracingShaderGroupHandlesKHR ||
        !g_ext.vkCreateAccelerationStructureKHR ||
        !g_ext.vkGetAccelerationStructureBuildSizesKHR ||
        !g_ext.vkGetBufferDeviceAddress) {
        LOG_FATAL_CAT("PIPELINE", "Required ray tracing extensions missing — empire cannot proceed");
        throw std::runtime_error("Required ray tracing extensions missing");
    }

    dummyTLAS_ = Handle<VkAccelerationStructureKHR>(
        createDummyTLAS(), stone_device(), g_ext.vkDestroyAccelerationStructureKHR);

    LOG_SUCCESS_CAT("PIPELINE", "PipelineManager initialized");
}

// =============================================================================
// Pipeline Layout — 4 sets with fixed 8 bindings on set 0
// =============================================================================
void PipelineManager::createPipelineLayout()
{
    if (rtPipelineLayout_.valid()) return;

    LOG_INFO_CAT("PIPELINE", "Creating descriptor set layouts and pipeline layout");

    // Main RT set — exactly 8 fixed bindings
    VkDescriptorSetLayoutCreateInfo mainInfo{};
    mainInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    mainInfo.bindingCount = static_cast<uint32_t>(kMainBindings.size());
    mainInfo.pBindings    = kMainBindings.data();

    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(stone_device(), &mainInfo, nullptr, &mainLayout) != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create main descriptor set layout (8 fixed bindings)");
        return;
    }
    rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(mainLayout, stone_device(), vkDestroyDescriptorSetLayout);

    // Texture array set (set 2)
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

    VkDescriptorSetLayout texLayout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(stone_device(), &texInfo, nullptr, &texLayout) != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create texture descriptor set layout");
        return;
    }
    texDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(texLayout, stone_device(), vkDestroyDescriptorSetLayout);

    // Empty sets 1 & 3
    VkDescriptorSetLayoutCreateInfo emptyInfo{};
    emptyInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    emptyInfo.bindingCount = 0;

    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(stone_device(), &emptyInfo, nullptr, &emptyLayout) != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create empty descriptor set layout");
        return;
    }
    emptyDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(emptyLayout, stone_device(), vkDestroyDescriptorSetLayout);

    const VkDescriptorSetLayout layouts[4] = {
        rtDescriptorSetLayout_.get(),     // set 0: main RT (8 bindings)
        emptyDescriptorSetLayout_.get(),  // set 1: empty
        texDescriptorSetLayout_.get(),    // set 2: textures
        emptyDescriptorSetLayout_.get()   // set 3: empty
    };

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR |
                      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                      VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    push.offset     = 0;
    push.size       = 32;  // enough for time/frame + optional StoneKey key

    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount         = 4;
    plInfo.pSetLayouts            = layouts;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges    = &push;

    VkPipelineLayout pl = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(stone_device(), &plInfo, nullptr, &pl) != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create ray tracing pipeline layout");
        return;
    }
    rtPipelineLayout_ = Handle<VkPipelineLayout>(pl, stone_device(), vkDestroyPipelineLayout);

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created — 4 sets (set 0 has 8 fixed safe bindings) + push constants");
}

// =============================================================================
// Descriptor Set Allocation
// =============================================================================
void PipelineManager::allocateDescriptorSets()
{
    LOG_INFO_CAT("PIPELINE", "Allocating descriptor sets from global pool");

    VkDescriptorPool globalPool = RTX::g_ctx().descriptorPool_.get();
    if (globalPool == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Global descriptor pool not available");
        return;
    }

    constexpr uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    auto allocate = [this, globalPool, frames](std::vector<VkDescriptorSet>& sets,
                                              VkDescriptorSetLayout layout,
                                              std::string_view name) {
        sets.resize(frames);
        std::vector<VkDescriptorSetLayout> layouts(frames, layout);

        VkDescriptorSetAllocateInfo info{};
        info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool     = globalPool;
        info.descriptorSetCount = frames;
        info.pSetLayouts        = layouts.data();

        VkResult result = vkAllocateDescriptorSets(stone_device(), &info, sets.data());
        if (result != VK_SUCCESS) {
            LOG_FATAL_CAT("PIPELINE", "Failed to allocate {} descriptor sets: {}", name, string_VkResult(result));
            return;
        }
        LOG_SUCCESS_CAT("PIPELINE", "Allocated {} {} descriptor sets", frames, name);
    };

    allocate(rtDescriptorSets_,    rtDescriptorSetLayout_.get(),    "main RT (set 0 — 8 bindings)");
    allocate(texDescriptorSets_,   texDescriptorSetLayout_.get(),   "texture array (set 2)");
    allocate(emptyDescriptorSets_, emptyDescriptorSetLayout_.get(), "empty (sets 1 & 3)");
}

// =============================================================================
// Dummy TLAS — Fallback
// =============================================================================
VkAccelerationStructureKHR PipelineManager::createDummyTLAS()
{
    LOG_INFO_CAT("PIPELINE", "Creating dummy TLAS fallback");

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

    uint64_t bufferHandle = BufferManager::create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        "Dummy_TLAS_Buffer");

    if (bufferHandle == 0) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create dummy TLAS buffer");
        return VK_NULL_HANDLE;
    }

    const BufferInfo* bufferInfo = BufferManager::get(bufferHandle);
    if (!bufferInfo) {
        LOG_FATAL_CAT("PIPELINE", "Failed to get dummy TLAS buffer info");
        BufferManager::destroy(bufferHandle);
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
        BufferManager::destroy(bufferHandle);
        return VK_NULL_HANDLE;
    }

    dummyAccelBuffer_ = Handle<VkBuffer>(bufferInfo->buffer, stone_device(), vkDestroyBuffer);
    dummyAccelMemory_ = Handle<VkDeviceMemory>(bufferInfo->memory, stone_device(), vkFreeMemory);

    LOG_SUCCESS_CAT("PIPELINE", "Dummy TLAS created");
    return as;
}

// =============================================================================
// Shader Loading
// =============================================================================
VkShaderModule PipelineManager::loadShader(const std::string& relativePath) const
{
    LOG_INFO_CAT("PIPELINE", "Loading shader: {}", relativePath);

    const std::string fullPath = std::format("build/bin/Linux/{}", relativePath);

    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_FATAL_CAT("PIPELINE", "Shader not found: {}", fullPath);
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
        LOG_FATAL_CAT("PIPELINE", "Failed to create shader module: {}", string_VkResult(result));
        return VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Shader loaded: {} ({} bytes)", relativePath, fileSize);
    return module;
}

// =============================================================================
// Pipeline Creation
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
        LOG_FATAL_CAT("PIPELINE", "One or more shaders failed to load — pipeline creation aborted");
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

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result = g_ext.vkCreateRayTracingPipelinesKHR(
        stone_device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkCreateRayTracingPipelinesKHR failed: {}", string_VkResult(result));
        return;
    }

    rtPipeline_ = Handle<VkPipeline>(pipeline, stone_device(), vkDestroyPipeline);
    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing pipeline created");
}

// =============================================================================
// SBT Creation — Hardware-adaptive alignment & stride
// =============================================================================
void PipelineManager::createShaderBindingTable(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd)
{
    if (s_eternalSbtForged) {
        LOG_INFO_CAT("PIPELINE", "SBT already forged — skipping");
        return;
    }

    if (rtPipeline_.get() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "No pipeline exists — cannot forge SBT");
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

    LOG_INFO_CAT("PIPELINE", "Forging SBT — handleSize={}, handleAlign={}, baseAlign={}, recordStride={}, totalSize={} ({} groups)",
                 handleSize, handleAlign, baseAlign, recordStride, sbtSize, totalGroups);

    VkBufferCreateInfo bci{};
    bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size        = sbtSize;
    bci.usage       = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer sbtBuffer = VK_NULL_HANDLE;
    VkResult res = vkCreateBuffer(stone_device(), &bci, nullptr, &sbtBuffer);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkCreateBuffer for SBT failed: {}", string_VkResult(res));
        return;
    }

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(stone_device(), sbtBuffer, &memReq);

    uint32_t memType = BufferManager::findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType == ~0u) {
        vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
        LOG_FATAL_CAT("PIPELINE", "No device-local memory type for SBT buffer");
        return;
    }

    VkMemoryAllocateFlagsInfo flagsInfo{};
    flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext           = &flagsInfo;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = memType;

    VkDeviceMemory sbtMemory = VK_NULL_HANDLE;
    res = vkAllocateMemory(stone_device(), &allocInfo, nullptr, &sbtMemory);
    if (res != VK_SUCCESS) {
        vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
        LOG_FATAL_CAT("PIPELINE", "vkAllocateMemory for SBT failed: {}", string_VkResult(res));
        return;
    }

    res = vkBindBufferMemory(stone_device(), sbtBuffer, sbtMemory, 0);
    if (res != VK_SUCCESS) {
        vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
        vkFreeMemory(stone_device(), sbtMemory, nullptr);
        LOG_FATAL_CAT("PIPELINE", "vkBindBufferMemory for SBT failed: {}", string_VkResult(res));
        return;
    }

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = sbtBuffer;
    VkDeviceAddress sbtAddress = g_ext.vkGetBufferDeviceAddress(stone_device(), &addrInfo);

    std::vector<uint8_t> handles(totalGroups * handleSize);
    res = g_ext.vkGetRayTracingShaderGroupHandlesKHR(
        stone_device(), rtPipeline_.get(), 0, totalGroups,
        handles.size(), handles.data());

    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkGetRayTracingShaderGroupHandlesKHR failed: {}", string_VkResult(res));
        vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
        vkFreeMemory(stone_device(), sbtMemory, nullptr);
        return;
    }

    // Upload via staging
    VkCommandBuffer uploadCmd = VK_NULL_HANDLE;
    bool ownCmd = (cmd == VK_NULL_HANDLE);

    if (ownCmd) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = pool ? pool : stone_transient_pool();
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(stone_device(), &allocInfo, &uploadCmd) != VK_SUCCESS) {
            LOG_FATAL_CAT("PIPELINE", "Failed to allocate upload command buffer");
            vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
            vkFreeMemory(stone_device(), sbtMemory, nullptr);
            return;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(uploadCmd, &beginInfo) != VK_SUCCESS) {
            LOG_FATAL_CAT("PIPELINE", "Failed to begin upload command buffer");
            vkFreeCommandBuffers(stone_device(), pool ? pool : stone_transient_pool(), 1, &uploadCmd);
            vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
            vkFreeMemory(stone_device(), sbtMemory, nullptr);
            return;
        }
    } else {
        uploadCmd = cmd;
    }

    void* staging = BufferManager::mapStaging(handles.size());
    if (!staging) {
        LOG_FATAL_CAT("PIPELINE", "Staging ring overflow during SBT upload");
        if (ownCmd) vkFreeCommandBuffers(stone_device(), pool ? pool : stone_transient_pool(), 1, &uploadCmd);
        vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
        vkFreeMemory(stone_device(), sbtMemory, nullptr);
        return;
    }

    std::memcpy(staging, handles.data(), handles.size());

    VkBufferCopy copy{};
    copy.srcOffset = BufferManager::g_stagingRing.head - handles.size();
    copy.dstOffset = 0;
    copy.size      = handles.size();
    vkCmdCopyBuffer(uploadCmd, BufferManager::getStagingBuffer(), sbtBuffer, 1, &copy);

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

    if (ownCmd) {
        if (vkEndCommandBuffer(uploadCmd) != VK_SUCCESS) {
            LOG_FATAL_CAT("PIPELINE", "vkEndCommandBuffer failed during SBT upload");
            vkFreeCommandBuffers(stone_device(), pool ? pool : stone_transient_pool(), 1, &uploadCmd);
            vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
            vkFreeMemory(stone_device(), sbtMemory, nullptr);
            return;
        }

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (vkCreateFence(stone_device(), &fci, nullptr, &fence) != VK_SUCCESS) {
            LOG_FATAL_CAT("PIPELINE", "Failed to create fence for SBT upload");
            vkFreeCommandBuffers(stone_device(), pool ? pool : stone_transient_pool(), 1, &uploadCmd);
            vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
            vkFreeMemory(stone_device(), sbtMemory, nullptr);
            return;
        }

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &uploadCmd;
        if (vkQueueSubmit(queue ? queue : stone_graphics_queue(), 1, &submit, fence) != VK_SUCCESS) {
            LOG_FATAL_CAT("PIPELINE", "vkQueueSubmit failed during SBT upload");
            vkDestroyFence(stone_device(), fence, nullptr);
            vkFreeCommandBuffers(stone_device(), pool ? pool : stone_transient_pool(), 1, &uploadCmd);
            vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
            vkFreeMemory(stone_device(), sbtMemory, nullptr);
            return;
        }

        if (vkWaitForFences(stone_device(), 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            LOG_FATAL_CAT("PIPELINE", "vkWaitForFences failed during SBT upload");
        }

        vkDestroyFence(stone_device(), fence, nullptr);
        vkFreeCommandBuffers(stone_device(), pool ? pool : stone_transient_pool(), 1, &uploadCmd);
    }

    VkDeviceAddress raygenAddr = align_up(sbtAddress, baseAlign);
    VkDeviceAddress missAddr   = align_up(raygenAddr + raygenSize, baseAlign);
    VkDeviceAddress hitAddr    = align_up(missAddr + missSize, baseAlign);

    sbtAddress_ = sbtAddress;
    sbtSize_    = sbtSize;

    raygenSbtRegion_ = {raygenAddr, recordStride, raygenSize};
    missSbtRegion_   = {missAddr,   recordStride, missSize};
    hitSbtRegion_    = {hitAddr,    recordStride, hitSize};

    LOG_TRACE_CAT("PIPELINE", "SBT regions forged:");
    LOG_TRACE_CAT("PIPELINE", "  raygen: addr=0x{:x} stride={} size={}", raygenAddr, recordStride, raygenSize);
    LOG_TRACE_CAT("PIPELINE", "  miss:   addr=0x{:x} stride={} size={}", missAddr, recordStride, missSize);
    LOG_TRACE_CAT("PIPELINE", "  hit:    addr=0x{:x} stride={} size={}", hitAddr, recordStride, hitSize);

    sbtBuffer_ = Handle<VkBuffer>(sbtBuffer, stone_device(), vkDestroyBuffer);
    sbtMemory_ = Handle<VkDeviceMemory>(sbtMemory, stone_device(), vkFreeMemory);

    s_eternalSbtForged = true;

    LOG_SUCCESS_CAT("PIPELINE", "SBT forged successfully — hardware-adaptive alignment applied");
}

// =============================================================================
// Trace Rays — Guarded dispatch
// =============================================================================
void PipelineManager::traceRays(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t width, uint32_t height)
{
    std::lock_guard<std::mutex> lock(rebuildMutex);

    if (g_pipelineNeedsRebuild.load(std::memory_order_acquire)) {
        LOG_WARNING_CAT("PIPELINE", "Pipeline rebuild requested — rebuilding now");
        createRayTracingPipeline();
        g_pipelineNeedsRebuild.store(false, std::memory_order_release);
    }

    if (cmd == VK_NULL_HANDLE || rtPipeline_.get() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Invalid command buffer or pipeline for traceRays");
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());

    VkDescriptorSet sets[] = {
        getDescriptorSet(imageIndex % Options::Performance::MAX_FRAMES_IN_FLIGHT),
        texDescriptorSets_[imageIndex % Options::Performance::MAX_FRAMES_IN_FLIGHT]
    };

    if (sets[0] == VK_NULL_HANDLE || sets[1] == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Invalid descriptor sets for traceRays");
        return;
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            rtPipelineLayout_.get(), 0, 2, sets, 0, nullptr);

    struct PushConstants {
        float time;
        uint32_t frame;
    } push{};
    push.time = 0.0f;
    push.frame = imageIndex;

    vkCmdPushConstants(cmd, rtPipelineLayout_.get(),
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                       VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                       0, sizeof(push), &push);

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

    LOG_TRACE_CAT("PIPELINE", "Ray tracing dispatched: {}x{}", width, height);
}

// =============================================================================
// Destructor
// =============================================================================
PipelineManager::~PipelineManager()
{
    sbtBuffer_.reset();
    sbtMemory_.reset();

    BufferManager::purge_all();

    LOG_INFO_CAT("PIPELINE", "PipelineManager destroyed");
}

// =============================================================================
// Cache ray tracing device properties (called once)
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

    vkGetPhysicalDeviceProperties2(StoneKey::stone_physical(), &props2);

    if (rtProps.shaderGroupHandleSize == 0) {
        LOG_FATAL_CAT("PIPELINE", "Ray tracing not supported on this device");
        throw std::runtime_error("Ray tracing unsupported");
    }

    StoneKey::stone_seal_rtprops(rtProps);

    cached = true;

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing properties cached — handle size: {}, base align: {}",
                    rtProps.shaderGroupHandleSize, rtProps.shaderGroupBaseAlignment);
}

// =============================================================================
// Update RT descriptor set for a given frame — matches 8 fixed bindings
// =============================================================================
void RTX::PipelineManager::updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo) noexcept
{
    if (frameIndex >= rtDescriptorSets_.size() || rtDescriptorSets_[frameIndex] == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Invalid frame index or descriptor set for update");
        return;
    }

    VkDescriptorSet set = rtDescriptorSets_[frameIndex];

    std::array<VkWriteDescriptorSet, 17> writes{};
    uint32_t writeCount = 0;

    auto addAccel = [&](uint32_t binding, VkAccelerationStructureKHR as) {
        as = as ? as : (LAS::instance().getTLAS() ? LAS::instance().getTLAS() : dummyTLAS_.get());
        VkWriteDescriptorSetAccelerationStructureKHR accelInfo{};
        accelInfo.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        accelInfo.accelerationStructureCount = 1;
        accelInfo.pAccelerationStructures    = &as;

        writes[writeCount] = VkWriteDescriptorSet{};
        writes[writeCount].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].pNext            = &accelInfo;
        writes[writeCount].dstSet           = set;
        writes[writeCount].dstBinding       = binding;
        writes[writeCount].descriptorCount  = 1;
        writes[writeCount].descriptorType   = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        ++writeCount;
    };

    auto addImage = [&](uint32_t binding, VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL) {
        if (view == VK_NULL_HANDLE) return;
        VkDescriptorImageInfo info{};
        info.imageView   = view;
        info.imageLayout = layout;

        writes[writeCount] = VkWriteDescriptorSet{};
        writes[writeCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].dstSet          = set;
        writes[writeCount].dstBinding      = binding;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[writeCount].pImageInfo      = &info;
        ++writeCount;
    };

    auto addBuffer = [&](uint32_t binding, VkBuffer buffer, VkDeviceSize size, VkDescriptorType type) {
        if (buffer == VK_NULL_HANDLE || size == 0) return;
        VkDescriptorBufferInfo info{};
        info.buffer = buffer;
        info.offset = 0;
        info.range  = size;

        writes[writeCount] = VkWriteDescriptorSet{};
        writes[writeCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].dstSet          = set;
        writes[writeCount].dstBinding      = binding;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType  = type;
        writes[writeCount].pBufferInfo     = &info;
        ++writeCount;
    };

    // Fixed bindings — match kMainBindings
    addAccel(0, updateInfo.tlas);                    // 0: TLAS
    addImage(1, updateInfo.rtOutputView);            // 1: Output image
    addBuffer(2, updateInfo.ubo, updateInfo.uboSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); // 2: Camera UBO

    if (updateInfo.materialsBuffer && updateInfo.materialsSize > 0) {
        addBuffer(3, updateInfo.materialsBuffer, updateInfo.materialsSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // 3: Materials
    }

    if (updateInfo.blueNoiseSampler && updateInfo.blueNoiseView) {
        VkDescriptorImageInfo info{};
        info.sampler     = updateInfo.blueNoiseSampler;
        info.imageView   = updateInfo.blueNoiseView;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        writes[writeCount] = VkWriteDescriptorSet{};
        writes[writeCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].dstSet          = set;
        writes[writeCount].dstBinding      = 5;  // 5: Blue noise
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[writeCount].pImageInfo      = &info;
        ++writeCount;
    }

    if (!updateInfo.nexusScoreViews.empty() && frameIndex < updateInfo.nexusScoreViews.size()) {
        addImage(6, updateInfo.nexusScoreViews[frameIndex]); // 6: Prev frame / nexus
    }

    if (writeCount > 0) {
        vkUpdateDescriptorSets(stone_device(), writeCount, writes.data(), 0, nullptr);
        LOG_TRACE_CAT("PIPELINE", "Updated RT descriptor set for frame {} — {} writes (8 fixed bindings)", frameIndex, writeCount);
    }
}

// =============================================================================
// Get descriptor set for a given frame
// =============================================================================
VkDescriptorSet RTX::PipelineManager::getDescriptorSet(uint32_t frameIndex) const
{
    if (frameIndex >= rtDescriptorSets_.size()) {
        LOG_ERROR_CAT("PIPELINE", "Invalid frame index {} for descriptor set", frameIndex);
        return VK_NULL_HANDLE;
    }
    return rtDescriptorSets_[frameIndex];
}

} // namespace RTX

// =============================================================================
// PipelineManager v30.12 — January 21, 2026
// - Preconfigured 8 safe bindings for set 0 — no binding 31
// - Unified logging with LOG_*_CAT("PIPELINE", ...)
// - Empire style consistent across codebase
// - Ready for stable trace rays — pink photons incoming
// =============================================================================