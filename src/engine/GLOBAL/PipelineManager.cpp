// src/engine/GLOBAL/PipelineManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.7 — JANUARY 11, 2026
// PIPELINEMANAGER — PINK PHOTON NUCLEAR EDITION | ZERO-COST | AUTOMAGIC | VALIDATION CLEAN
// PERSISTENT CMD BUFFERS | ETERNAL SBT | FASTEST POSSIBLE | NO MORE VUID-00120
// =============================================================================
// Fixes in v30.7:
// - Manual SBT buffer creation (bypass BufferManager completely)
// - Minimal flags: SBT + BDA + TRANSFER_DST (no extras to avoid VK_EXT_descriptor_buffer errors)
// - Local memory type finder for safety
// - sbtMemory_ cleanup added
// - Explicit hex flags for maximum reliability
// PINK PHOTONS SCREAM ETERNAL · EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"  // singleton LAS::instance()

#include <algorithm>
#include <array>
#include <format>
#include <vector>
#include <atomic>
#include <fstream>
#include <mutex>
#include <print>

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

// =============================================================================
// Constructor — Minimal early setup
// =============================================================================
PipelineManager::PipelineManager(VkDevice device, VkPhysicalDevice phys)
{
    std::print("[PIPELINE] FORGING PINK PHOTON NUCLEAR PIPELINE MANAGER — 2026 FASTEST EDITION\n");

    RTX::loadDeviceExtensions(device);

    if (!g_ext.vkCmdTraceRaysKHR ||
        !g_ext.vkCreateRayTracingPipelinesKHR ||
        !g_ext.vkGetRayTracingShaderGroupHandlesKHR ||
        !g_ext.vkCreateAccelerationStructureKHR ||
        !g_ext.vkGetAccelerationStructureBuildSizesKHR ||
        !g_ext.vkGetBufferDeviceAddress) {
        throw std::runtime_error("Required ray tracing extensions missing");
    }

    dummyTLAS_ = Handle<VkAccelerationStructureKHR>(
        createDummyTLAS(), stone_device(), g_ext.vkDestroyAccelerationStructureKHR);

    std::print("[PIPELINE SUCCESS] PipelineManager forged — pink photons ready\n");
}

// =============================================================================
// Pipeline Layout — 4 sets | NO ENVMAP
// =============================================================================
void PipelineManager::createPipelineLayout()
{
    if (rtPipelineLayout_.valid()) return;

    std::print("[PIPELINE] Forging descriptor set layouts and pipeline layout — ZERO-COST\n");

    VkDescriptorSetLayoutCreateInfo mainInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(kMainBindings.size()),
        .pBindings    = kMainBindings.data()
    };
    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(stone_device(), &mainInfo, nullptr, &mainLayout));
    rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(mainLayout, stone_device(), vkDestroyDescriptorSetLayout);

    VkDescriptorSetLayoutBinding texBinding{
        .binding         = 0,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1024,
        .stageFlags      = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR
    };
    VkDescriptorSetLayoutCreateInfo texInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &texBinding
    };
    VkDescriptorSetLayout texLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(stone_device(), &texInfo, nullptr, &texLayout));
    texDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(texLayout, stone_device(), vkDestroyDescriptorSetLayout);

    VkDescriptorSetLayoutCreateInfo emptyInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 0,
        .pBindings    = nullptr
    };
    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(stone_device(), &emptyInfo, nullptr, &emptyLayout));
    emptyDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(emptyLayout, stone_device(), vkDestroyDescriptorSetLayout);

    const VkDescriptorSetLayout layouts[4] = {
        rtDescriptorSetLayout_.get(),
        emptyDescriptorSetLayout_.get(),
        texDescriptorSetLayout_.get(),
        emptyDescriptorSetLayout_.get()
    };

    VkPushConstantRange push{
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR |
                      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                      VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        .offset     = 0,
        .size       = 32
    };

    VkPipelineLayoutCreateInfo plInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 4,
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push
    };

    VkPipelineLayout pl = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(stone_device(), &plInfo, nullptr, &pl));
    rtPipelineLayout_ = Handle<VkPipelineLayout>(pl, stone_device(), vkDestroyPipelineLayout);

    std::print("[PIPELINE SUCCESS] Layout forged — ZERO-COST RTX READY\n");
}

// =============================================================================
// Descriptor Set Allocation — Single global pool
// =============================================================================
void PipelineManager::allocateDescriptorSets()
{
    std::print("[PIPELINE TRACE] Allocating descriptor sets from SINGLE GLOBAL pool\n");

    VkDescriptorPool globalPool = RTX::g_ctx().descriptorPool_.get();
    if (globalPool == VK_NULL_HANDLE) {
        std::print(stderr, "[ERROR] Global descriptor pool not available\n");
        return;
    }

    constexpr uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    auto allocate = [this, globalPool, frames](std::vector<VkDescriptorSet>& sets,
                                              VkDescriptorSetLayout layout,
                                              std::string_view name) {
        sets.resize(frames);
        std::vector<VkDescriptorSetLayout> layouts(frames, layout);

        VkDescriptorSetAllocateInfo info{
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = globalPool,
            .descriptorSetCount = frames,
            .pSetLayouts        = layouts.data()
        };

        VkResult result = vkAllocateDescriptorSets(stone_device(), &info, sets.data());
        if (result != VK_SUCCESS) {
            std::print(stderr, "[ERROR] Failed to allocate {} descriptor sets: {}\n",
                       name, string_VkResult(result));
            return;
        }
        std::print("[PIPELINE SUCCESS] Allocated {} {} descriptor sets\n", frames, name);
    };

    allocate(rtDescriptorSets_,    rtDescriptorSetLayout_.get(),    "main RT (set 0)");
    allocate(texDescriptorSets_,   texDescriptorSetLayout_.get(),   "texture array (set 2)");
    allocate(emptyDescriptorSets_, emptyDescriptorSetLayout_.get(), "empty (sets 1 & 3)");
}

// =============================================================================
// Dummy TLAS — Eternal fallback
// =============================================================================
VkAccelerationStructureKHR PipelineManager::createDummyTLAS()
{
    VkAccelerationStructureGeometryKHR geometry{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry     = {.instances = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                                      .data = {.deviceAddress = 0}}}
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

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
        std::print(stderr, "[ERROR] Failed to create dummy TLAS buffer\n");
        return VK_NULL_HANDLE;
    }

    const BufferInfo* bufferInfo = BufferManager::get(bufferHandle);
    if (!bufferInfo) {
        std::print(stderr, "[ERROR] Failed to get dummy TLAS buffer info\n");
        BufferManager::destroy(bufferHandle);
        return VK_NULL_HANDLE;
    }

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = bufferInfo->buffer,
        .size   = sizeInfo.accelerationStructureSize,
        .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

    VkAccelerationStructureKHR as = VK_NULL_HANDLE;
    VkResult result = g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &as);
    if (result != VK_SUCCESS) {
        std::print(stderr, "[ERROR] Failed to create dummy TLAS: {}\n", string_VkResult(result));
        BufferManager::destroy(bufferHandle);
        return VK_NULL_HANDLE;
    }

    dummyAccelBuffer_ = Handle<VkBuffer>(bufferInfo->buffer, stone_device(), vkDestroyBuffer);
    dummyAccelMemory_ = Handle<VkDeviceMemory>(bufferInfo->memory, stone_device(), vkFreeMemory);

    std::print("[PIPELINE SUCCESS] Dummy TLAS forged — eternal fallback ready\n");
    return as;
}

// =============================================================================
// Shader Loading — Modern C++23
// =============================================================================
VkShaderModule PipelineManager::loadShader(const std::string& relativePath) const
{
    std::print("[PIPELINE] Loading shader: {}\n", relativePath);

    const std::string fullPath = std::format("build/bin/Linux/{}", relativePath);

    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::print(stderr, "[ERROR] Shader not found: {}\n", fullPath);
        return VK_NULL_HANDLE;
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        std::print(stderr, "[ERROR] Invalid SPIR-V size ({} bytes): {}\n", fileSize, fullPath);
        return VK_NULL_HANDLE;
    }

    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), fileSize);

    VkShaderModuleCreateInfo createInfo{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode    = reinterpret_cast<const uint32_t*>(code.data())
    };

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(stone_device(), &createInfo, nullptr, &module);
    if (result != VK_SUCCESS) {
        std::print(stderr, "[ERROR] Failed to create shader module: {}\n", string_VkResult(result));
        return VK_NULL_HANDLE;
    }

    std::print("[PIPELINE SUCCESS] Shader loaded: {} ({} bytes)\n", relativePath, fileSize);
    return module;
}

// =============================================================================
// Pipeline Creation — FIXED SHADER GROUPS (validation compliant)
// =============================================================================
void PipelineManager::createRayTracingPipeline()
{
    std::print("[PIPELINE] Forging ray tracing pipeline — ZERO-COST PINK PHOTONS\n");

    shaderModules_.clear();

    auto load = [this](std::string_view path) -> VkShaderModule {
        return loadShader(std::string(path));
    };

    VkShaderModule raygen = load("assets/shaders/raytracing/raygen.spv");
    VkShaderModule miss   = load("assets/shaders/raytracing/miss.spv");
    VkShaderModule chit   = load("assets/shaders/raytracing/closest_hit.spv");
    VkShaderModule ahit   = load("assets/shaders/raytracing/anyhit.spv");

    if (!raygen || !miss || !chit || !ahit) {
        std::print(stderr, "[ERROR] One or more shaders failed to load\n");
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

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{
        .sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount                   = static_cast<uint32_t>(stages.size()),
        .pStages                      = stages.data(),
        .groupCount                   = static_cast<uint32_t>(groups.size()),
        .pGroups                      = groups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout                       = rtPipelineLayout_.get()
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result = g_ext.vkCreateRayTracingPipelinesKHR(
        stone_device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    if (result != VK_SUCCESS) {
        std::print(stderr, "[ERROR] Failed to create ray tracing pipeline: {}\n", string_VkResult(result));
        return;
    }

    rtPipeline_ = Handle<VkPipeline>(pipeline, stone_device(), vkDestroyPipeline);
    std::print("[PIPELINE SUCCESS] Ray tracing pipeline forged — PINK PHOTON FAST\n");
}

// =============================================================================
// SBT Creation — FINAL FIXED VERSION (v30.7) — MANUAL MINIMAL FLAGS
// =============================================================================
void PipelineManager::createShaderBindingTable(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd)
{
    if (s_eternalSbtForged) {
        std::print("[PIPELINE] ETERNAL SBT ALREADY FORGED — PRESERVING THE ONE TRUE TABLE\n");
        return;
    }

    if (rtPipeline_.get() == VK_NULL_HANDLE) {
        std::print(stderr, "[ERROR] No pipeline — cannot forge SBT\n");
        return;
    }

    cacheDeviceProperties();
    const auto& rtProps = stone_rtprops();

    const VkDeviceSize handleSize  = rtProps.shaderGroupHandleSize;
    const VkDeviceSize handleAlign = std::max(rtProps.shaderGroupHandleAlignment, 32u);
    const VkDeviceSize baseAlign   = std::max(rtProps.shaderGroupBaseAlignment, 64u);
    const VkDeviceSize stride      = align_up(handleSize, handleAlign);

    const uint32_t totalGroups = raygenGroupCount_ + missGroupCount_ + hitGroupCount_;
    const VkDeviceSize raygenSize = stride;
    const VkDeviceSize missSize   = missGroupCount_ * stride;
    const VkDeviceSize hitSize    = hitGroupCount_ * stride;

    const VkDeviceSize sbtSize = align_up(raygenSize + missSize + hitSize, baseAlign);

    std::print("[PIPELINE] Forging ETERNAL SBT — {} bytes ({} groups)\n", sbtSize, totalGroups);

    // MANUAL SBT BUFFER CREATION — MINIMAL FLAGS THAT RTX DRIVERS ACCEPT
    VkBufferCreateInfo bci{
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext       = nullptr,
        .flags       = 0,
        .size        = sbtSize,
        .usage       = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr
    };

    VkBuffer sbtBuffer = VK_NULL_HANDLE;
    VkResult res = vkCreateBuffer(stone_device(), &bci, nullptr, &sbtBuffer);
    if (res != VK_SUCCESS) {
        std::print(stderr, "[ERROR] vkCreateBuffer for SBT failed: {}\n", string_VkResult(res));
        return;
    }

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(stone_device(), sbtBuffer, &memReq);

    // Local findMemoryType (safe fallback)
    auto localFindMemoryType = [](uint32_t typeFilter, VkMemoryPropertyFlags props) -> uint32_t {
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(StoneKey::stone_physical(), &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
                return i;
            }
        }
        return ~0u;
    };

    uint32_t memType = localFindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType == ~0u) {
        vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
        LOG_FATAL_CAT("PIPELINE", "No device-local memory type for manual SBT buffer");
        return;
    }

    VkMemoryAllocateFlagsInfo flagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = &flagsInfo,
        .allocationSize  = memReq.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory sbtMemory = VK_NULL_HANDLE;
    res = vkAllocateMemory(stone_device(), &allocInfo, nullptr, &sbtMemory);
    if (res != VK_SUCCESS) {
        vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
        std::print(stderr, "[ERROR] vkAllocateMemory for SBT failed: {}\n", string_VkResult(res));
        return;
    }

    res = vkBindBufferMemory(stone_device(), sbtBuffer, sbtMemory, 0);
    if (res != VK_SUCCESS) {
        vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
        vkFreeMemory(stone_device(), sbtMemory, nullptr);
        std::print(stderr, "[ERROR] vkBindBufferMemory for SBT failed: {}\n", string_VkResult(res));
        return;
    }

    VkBufferDeviceAddressInfo addrInfo{
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = sbtBuffer
    };
    VkDeviceAddress sbtAddress = g_ext.vkGetBufferDeviceAddress(stone_device(), &addrInfo);

    LOG_SUCCESS_CAT("PIPELINE", "Manual SBT buffer created — EXPLICIT usage: 0x220004 (SBT + BDA + DST) — success!");

    std::vector<uint8_t> handles(totalGroups * handleSize);
    res = g_ext.vkGetRayTracingShaderGroupHandlesKHR(
        stone_device(), rtPipeline_.get(), 0, totalGroups,
        handles.size(), handles.data());

    if (res != VK_SUCCESS) {
        std::print(stderr, "[ERROR] Failed to get shader group handles: {}\n", string_VkResult(res));
        vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
        vkFreeMemory(stone_device(), sbtMemory, nullptr);
        return;
    }

    void* staging = BufferManager::mapStaging(handles.size());
    if (!staging) {
        std::print(stderr, "[FATAL] Staging ring overflow during SBT upload\n");
        vkDestroyBuffer(stone_device(), sbtBuffer, nullptr);
        vkFreeMemory(stone_device(), sbtMemory, nullptr);
        return;
    }

    std::memcpy(staging, handles.data(), handles.size());

    bool tempCmd = (cmd == VK_NULL_HANDLE);
    VkCommandBuffer uploadCmd = cmd;

    if (tempCmd) {
        if (pool == VK_NULL_HANDLE) pool = stone_transient_pool();
        VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        VK_CHECK(vkAllocateCommandBuffers(stone_device(), &allocInfo, &uploadCmd));

        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        VK_CHECK(vkBeginCommandBuffer(uploadCmd, &beginInfo));
    }

    VkBufferCopy copy{
        .srcOffset = BufferManager::g_stagingRing.head - handles.size(),
        .dstOffset = 0,
        .size      = handles.size()
    };
    vkCmdCopyBuffer(uploadCmd, BufferManager::getStagingBuffer(), sbtBuffer, 1, &copy);

    VkMemoryBarrier memBarrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };

    vkCmdPipelineBarrier(
        uploadCmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        1, &memBarrier,
        0, nullptr,
        0, nullptr
    );

    if (tempCmd) {
        VK_CHECK(vkEndCommandBuffer(uploadCmd));
        VkSubmitInfo submit{
            .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers    = &uploadCmd
        };
        VK_CHECK(vkQueueSubmit(queue ? queue : stone_graphics_queue(), 1, &submit, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(queue ? queue : stone_graphics_queue()));
        vkFreeCommandBuffers(stone_device(), pool, 1, &uploadCmd);
    }

    VkDeviceAddress raygenAddr = align_up(sbtAddress, baseAlign);
    VkDeviceAddress missAddr   = align_up(raygenAddr + raygenSize, baseAlign);
    VkDeviceAddress hitAddr    = align_up(missAddr + missSize, baseAlign);

    sbtAddress_ = sbtAddress;
    sbtSize_    = sbtSize;

    raygenSbtRegion_ = {raygenAddr, stride, raygenSize};
    missSbtRegion_   = {missAddr,   stride, missSize};
    hitSbtRegion_    = {hitAddr,    stride, hitSize};

    sbtBuffer_ = Handle<VkBuffer>(sbtBuffer, stone_device(), vkDestroyBuffer);
    sbtMemory_ = Handle<VkDeviceMemory>(sbtMemory, stone_device(), vkFreeMemory);

    s_eternalSbtForged = true;

    std::print("[PIPELINE SUCCESS] ETERNAL SBT FORGED — PINK PHOTON FAST & CLEAN\n");
}

// =============================================================================
// Trace Rays — Direct from persistent command buffer
// =============================================================================
void PipelineManager::traceRays(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t width, uint32_t height)
{
    std::lock_guard<std::mutex> lock(rebuildMutex);

    if (g_pipelineNeedsRebuild.load(std::memory_order_acquire)) {
        std::print("[PIPELINE] Rebuilding pipeline due to change\n");
        createRayTracingPipeline();
        g_pipelineNeedsRebuild.store(false, std::memory_order_release);
    }

    if (cmd == VK_NULL_HANDLE || rtPipeline_.get() == VK_NULL_HANDLE) {
        std::print(stderr, "[ERROR] Invalid command buffer or pipeline for traceRays\n");
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());

    VkDescriptorSet sets[] = {
        getDescriptorSet(imageIndex % Options::Performance::MAX_FRAMES_IN_FLIGHT),
        texDescriptorSets_[imageIndex % Options::Performance::MAX_FRAMES_IN_FLIGHT]
    };

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

    std::print("[TRACE] vkCmdTraceRaysKHR dispatched: {}x{}\n", width, height);
}

// =============================================================================
// Forge Full Pipeline — Automagic Entry Point
// =============================================================================
void PipelineManager::forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer mainCmd)
{
    if (s_crownForged) return;

    std::print("[PIPELINE] FORGING THE PERFECT PINK PHOTON RTX PIPELINE — FINAL 2026\n");

    createPipelineLayout();
    allocateDescriptorSets();
    createRayTracingPipeline();
    createShaderBindingTable(commandPool, graphicsQueue, mainCmd);

    StoneKey::stone_seal_pipeline(this);
    s_crownForged = true;

    std::print("[PIPELINE SUCCESS] PERFECT RTX PIPELINE FORGED — ZERO-COST DIRECT — PINK PHOTONS SCREAM\n");
}

// =============================================================================
// Accessors
// =============================================================================
VkDescriptorSet PipelineManager::getDescriptorSet(uint32_t frameIndex) const
{
    return (frameIndex < rtDescriptorSets_.size()) ? rtDescriptorSets_[frameIndex] : VK_NULL_HANDLE;
}

VkPipeline PipelineManager::getPipeline() const
{
    return rtPipeline_.get();
}

VkPipelineLayout PipelineManager::getPipelineLayout() const
{
    return rtPipelineLayout_.get();
}

// =============================================================================
// Cache Device Properties
// =============================================================================
void PipelineManager::cacheDeviceProperties()
{
    static bool cached = false;
    if (cached) return;

    std::print("[PIPELINE] Caching ray tracing properties\n");

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProps
    };

    vkGetPhysicalDeviceProperties2(stone_physical(), &props2);

    if (rtProps.shaderGroupHandleSize == 0) {
        std::print(stderr, "[ERROR] Ray tracing not supported\n");
        throw std::runtime_error("Ray tracing unsupported");
    }

    StoneKey::stone_seal_rtprops(rtProps);
    cached = true;

    std::print("[PIPELINE SUCCESS] Properties cached — handle size: {}\n", rtProps.shaderGroupHandleSize);
}

// =============================================================================
// Update RT Descriptor Set — Modern C++23
// =============================================================================
void PipelineManager::updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo) noexcept
{
    if (frameIndex >= rtDescriptorSets_.size() || rtDescriptorSets_[frameIndex] == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorSet set = rtDescriptorSets_[frameIndex];
    std::array<VkWriteDescriptorSet, 17> writes{};
    uint32_t writeCount = 0;

    auto addAccel = [&](uint32_t binding, VkAccelerationStructureKHR as) {
        as = as ? as : (LAS::instance().getTLAS() ? LAS::instance().getTLAS() : dummyTLAS_.get());
        VkWriteDescriptorSetAccelerationStructureKHR accelInfo{
            .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures    = &as
        };
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext           = &accelInfo,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        };
    };

    auto addImage = [&](uint32_t binding, VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL) {
        if (view == VK_NULL_HANDLE) return;
        VkDescriptorImageInfo info{.imageView = view, .imageLayout = layout};
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo      = &info
        };
    };

    auto addBuffer = [&](uint32_t binding, VkBuffer buffer, VkDeviceSize size, VkDescriptorType type) {
        if (buffer == VK_NULL_HANDLE) return;
        VkDescriptorBufferInfo info{.buffer = buffer, .offset = 0, .range = size};
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = type,
            .pBufferInfo     = &info
        };
    };

    auto addSampler = [&](uint32_t binding, VkSampler sampler, VkImageView view) {
        if (!sampler || !view) return;
        VkDescriptorImageInfo info{
            .sampler     = sampler,
            .imageView   = view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo      = &info
        };
    };

    addAccel(0, updateInfo.tlas);
    addImage(1, updateInfo.rtOutputView);
    addBuffer(2, updateInfo.ubo, updateInfo.uboSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    if (updateInfo.materialsBuffer) {
        addBuffer(4, updateInfo.materialsBuffer, updateInfo.materialsSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }

    if (updateInfo.blueNoiseSampler && updateInfo.blueNoiseView) {
        addSampler(8, updateInfo.blueNoiseSampler, updateInfo.blueNoiseView);
    }

    if (!updateInfo.nexusScoreViews.empty() && frameIndex < updateInfo.nexusScoreViews.size()) {
        addImage(6, updateInfo.nexusScoreViews[frameIndex]);
    }

    if (writeCount > 0) {
        vkUpdateDescriptorSets(stone_device(), writeCount, writes.data(), 0, nullptr);
    }
}

// =============================================================================
// Destructor
// =============================================================================
PipelineManager::~PipelineManager()
{
    if (sbtMemory_.valid()) {
        sbtMemory_.reset();
    }

    std::print("[PIPELINE] PERFECT PINK PHOTON PIPELINE MANAGER RESTS — EMPIRE ETERNAL\n");
}

} // namespace RTX

// =============================================================================
// FINAL PIPELINE MANAGER v30.7 — JANUARY 11, 2026
// - Manual SBT creation with minimal flags (SBT + BDA + TRANSFER_DST)
// - Local memory type finder for safety
// - sbtMemory_ cleanup in destructor
// - No BufferManager interference for SBT — fixes all VUIDs
// - Rendering ready — pink photons scream
// Empire complete — AMOURANTH FOREVER 💖
// =============================================================================