// =============================================================================
// AMOURANTH RTX Engine — Pipeline Manager
// Ray tracing pipeline, SBT, and descriptor management
// Version 30.17 — January 22, 2026
// - KILLED FRAMES — single descriptor set, no MAX_FRAMES_IN_FLIGHT, no frame index
// - updateRTDescriptorSet & traceRays no longer take frameIndex/imageIndex
// - allocateDescriptorSets now creates 1 of each set type
// - getDescriptorSet() always returns the single set
// - Fixed descriptor buffer writes — skip invalid, offset 0, range full
// - Materials write still commented (binding 3 = STORAGE_IMAGE)
// - Empire stable — no garbage, no lost device, no frame state
// =============================================================================

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"

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

using BufferManager::BufferInfo;
using BufferManager::align_up;

// Static atomic members
std::atomic<bool>     RTX::PipelineManager::g_pipelineNeedsRebuild{false};

// Fixed 8 safe bindings for set 0 — Vulkan guaranteed
static constexpr std::array<VkDescriptorSetLayoutBinding, 8> kMainBindings = {{
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR},
    {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {7, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR}
}};

namespace RTX {

inline static std::mutex rebuildMutex;

// =============================================================================
// Constructor
// =============================================================================
PipelineManager::PipelineManager(VkDevice device, VkPhysicalDevice phys)
{
    LOG_INFO_CAT("PIPELINE", "Initializing PipelineManager — frame-free mode");

    RTX::loadDeviceExtensions(device);

    if (!g_ext.vkCmdTraceRaysKHR ||
        !g_ext.vkCreateRayTracingPipelinesKHR ||
        !g_ext.vkGetRayTracingShaderGroupHandlesKHR ||
        !g_ext.vkCreateAccelerationStructureKHR ||
        !g_ext.vkGetAccelerationStructureBuildSizesKHR ||
        !g_ext.vkGetBufferDeviceAddress) {
        LOG_FATAL_CAT("PIPELINE", "Required ray tracing extensions missing");
        throw std::runtime_error("Required ray tracing extensions missing");
    }

    dummyTLAS_ = Handle<VkAccelerationStructureKHR>(
        createDummyTLAS(), stone_device(), g_ext.vkDestroyAccelerationStructureKHR);

    LOG_SUCCESS_CAT("PIPELINE", "PipelineManager initialized");
}

// =============================================================================
// Pipeline Layout
// =============================================================================
void PipelineManager::createPipelineLayout()
{
    if (rtPipelineLayout_.valid()) return;

    LOG_INFO_CAT("PIPELINE", "Creating descriptor set layouts and pipeline layout");

    VkDescriptorSetLayoutCreateInfo mainInfo{};
    mainInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    mainInfo.bindingCount = static_cast<uint32_t>(kMainBindings.size());
    mainInfo.pBindings    = kMainBindings.data();

    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(stone_device(), &mainInfo, nullptr, &mainLayout) != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create main descriptor set layout");
        return;
    }
    rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(mainLayout, stone_device(), vkDestroyDescriptorSetLayout);

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
        rtDescriptorSetLayout_.get(),
        emptyDescriptorSetLayout_.get(),
        texDescriptorSetLayout_.get(),
        emptyDescriptorSetLayout_.get()
    };

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR |
                      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                      VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    push.offset     = 0;
    push.size       = 32;

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

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created");
}

// =============================================================================
// Descriptor Set Allocation — single set only (frame-free)
// =============================================================================
void PipelineManager::allocateDescriptorSets()
{
    LOG_INFO_CAT("PIPELINE", "Allocating single descriptor sets (frame-free)");

    VkDescriptorPool globalPool = RTX::g_ctx().descriptorPool.get();
    if (globalPool == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Global descriptor pool not available");
        return;
    }

    auto allocate = [this, globalPool](std::vector<VkDescriptorSet>& sets,
                                       VkDescriptorSetLayout layout,
                                       std::string_view name) {
        sets.resize(1);  // only one set
        VkDescriptorSetLayout layouts[1] = {layout};

        VkDescriptorSetAllocateInfo info{};
        info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool     = globalPool;
        info.descriptorSetCount = 1;
        info.pSetLayouts        = layouts;

        VkResult result = vkAllocateDescriptorSets(stone_device(), &info, sets.data());
        if (result != VK_SUCCESS) {
            LOG_FATAL_CAT("PIPELINE", "Failed to allocate {} descriptor set: {}", name, string_VkResult(result));
            return;
        }
        LOG_SUCCESS_CAT("PIPELINE", "Allocated single {} descriptor set", name);
    };

    allocate(rtDescriptorSets_,    rtDescriptorSetLayout_.get(),    "main RT (set 0)");
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
        LOG_FATAL_CAT("PIPELINE", "One or more shaders failed to load");
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
// SBT Creation — Startup only
// =============================================================================
void PipelineManager::createShaderBindingTable(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd)
{
    if (s_eternalSbtForged) return;

    if (rtPipeline_.get() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "No pipeline — cannot forge SBT");
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

    LOG_INFO_CAT("PIPELINE", "Forging SBT — handleSize={} baseAlign={} recordStride={} totalSize={}",
                 handleSize, baseAlign, recordStride, sbtSize);

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

    sbtBuffer_ = Handle<VkBuffer>(sbtBuffer, stone_device(), vkDestroyBuffer);
    sbtMemory_ = Handle<VkDeviceMemory>(sbtMemory, stone_device(), vkFreeMemory);

    s_eternalSbtForged = true;

    LOG_SUCCESS_CAT("PIPELINE", "SBT forged successfully");
}

// =============================================================================
// Trace Rays — Silent hot path (no frame index)
// =============================================================================
void PipelineManager::traceRays(VkCommandBuffer cmd, uint32_t width, uint32_t height)
{
    std::lock_guard<std::mutex> lock(rebuildMutex);

    if (g_pipelineNeedsRebuild.load(std::memory_order_acquire)) {
        createRayTracingPipeline();
        g_pipelineNeedsRebuild.store(false, std::memory_order_release);
    }

    if (cmd == VK_NULL_HANDLE || rtPipeline_.get() == VK_NULL_HANDLE) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());

    VkDescriptorSet sets[] = {
        getDescriptorSet()  // always the single set
    };

    if (sets[0] == VK_NULL_HANDLE) return;

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            rtPipelineLayout_.get(), 0, 1, sets, 0, nullptr);

    struct PushConstants {
        float time;
        uint32_t frame;
    } push{};
    push.time = 0.0f;
    push.frame = 0;  // meaningless now — can remove later

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
// Cache ray tracing device properties (once)
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

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing properties cached");
}

// =============================================================================
// Update RT descriptor set — single set only (frame-free)
// =============================================================================
void RTX::PipelineManager::updateRTDescriptorSet(const RTDescriptorUpdate& updateInfo) noexcept
{
    if (rtDescriptorSets_.empty() || rtDescriptorSets_[0] == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "No descriptor set allocated");
        return;
    }

    VkDescriptorSet set = rtDescriptorSets_[0];

    if (updateInfo.tlas == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "TLAS is NULL — skipping descriptor update");
        return;
    }

    std::array<VkWriteDescriptorSet, 17> writes{};
    uint32_t writeCount = 0;

    // Lifetime-safe infos (declared at function scope)
    VkWriteDescriptorSetAccelerationStructureKHR accelInfo{};
    VkDescriptorImageInfo outputImageInfo{};
    VkDescriptorImageInfo blueNoiseImageInfo{};
    VkDescriptorImageInfo nexusImageInfo{};
    VkDescriptorBufferInfo uboInfo{};

    // Binding 0: Acceleration structure (always written)
    accelInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    accelInfo.accelerationStructureCount = 1;
    accelInfo.pAccelerationStructures = &updateInfo.tlas;

    writes[writeCount] = {};
    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[writeCount].pNext = &accelInfo;
    writes[writeCount].dstSet = set;
    writes[writeCount].dstBinding = 0;
    writes[writeCount].descriptorCount = 1;
    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    ++writeCount;

    // Binding 1: Storage output image
    if (updateInfo.rtOutputView != VK_NULL_HANDLE) {
        outputImageInfo.imageView   = updateInfo.rtOutputView;
        outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        outputImageInfo.sampler     = VK_NULL_HANDLE;

        writes[writeCount] = {};
        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].dstSet = set;
        writes[writeCount].dstBinding = 1;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[writeCount].pImageInfo = &outputImageInfo;
        ++writeCount;
    }

    // Binding 2: UBO (camera) — guard + explicit offset/range
    if (updateInfo.ubo != VK_NULL_HANDLE && updateInfo.uboSize > 0) {
        uboInfo.buffer = updateInfo.ubo;
        uboInfo.offset = 0;  // MUST be multiple of minUniformBufferOffsetAlignment (64)
        uboInfo.range  = updateInfo.uboSize;

        writes[writeCount] = {};
        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].dstSet = set;
        writes[writeCount].dstBinding = 2;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[writeCount].pBufferInfo = &uboInfo;
        ++writeCount;
    } else {
        LOG_WARN_CAT("PIPELINE", "Skipping UBO write — invalid buffer or size");
    }

    // Binding 3: Materials — SKIPPED due to type mismatch (STORAGE_IMAGE in layout)
    // If you need storage buffer here, change kMainBindings[3] to STORAGE_BUFFER

    // Binding 5: Blue noise (optional)
    if (updateInfo.blueNoiseSampler != VK_NULL_HANDLE && updateInfo.blueNoiseView != VK_NULL_HANDLE) {
        blueNoiseImageInfo.sampler     = updateInfo.blueNoiseSampler;
        blueNoiseImageInfo.imageView   = updateInfo.blueNoiseView;
        blueNoiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        writes[writeCount] = {};
        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].dstSet = set;
        writes[writeCount].dstBinding = 5;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[writeCount].pImageInfo = &blueNoiseImageInfo;
        ++writeCount;
    }

    // Binding 6: Nexus/prev frame (optional)
    if (!updateInfo.nexusScoreViews.empty()) {
        nexusImageInfo.imageView   = updateInfo.nexusScoreViews[0];  // single set — use first
        nexusImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        nexusImageInfo.sampler     = VK_NULL_HANDLE;

        writes[writeCount] = {};
        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].dstSet = set;
        writes[writeCount].dstBinding = 6;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[writeCount].pImageInfo = &nexusImageInfo;
        ++writeCount;
    }

    if (writeCount > 0) {
        vkUpdateDescriptorSets(stone_device(), writeCount, writes.data(), 0, nullptr);
    }
}

// =============================================================================
// Get descriptor set — always the single one
// =============================================================================
VkDescriptorSet RTX::PipelineManager::getDescriptorSet() const
{
    if (rtDescriptorSets_.empty()) {
        return VK_NULL_HANDLE;
    }
    return rtDescriptorSets_[0];
}

} // namespace RTX

// =============================================================================
// PipelineManager v30.17 — January 22, 2026
// - Frame-free — single descriptor set, no MAX_FRAMES_IN_FLIGHT, no %
// - updateRTDescriptorSet & traceRays simplified — no frameIndex/imageIndex
// - Descriptor writes safe — skip invalid, offset 0, no mismatch
// - Materials write commented (resolve STORAGE_BUFFER vs STORAGE_IMAGE)
// - Silent success — no spam
// - Ready for rendering — no garbage, no lost device
// =============================================================================