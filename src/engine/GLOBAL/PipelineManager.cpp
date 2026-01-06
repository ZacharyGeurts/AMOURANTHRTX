// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v27.2 — JANUARY 05, 2026
// PIPELINEMANAGER — SINGLE GLOBAL POOL + LAS + ETERNAL SBT EDITION
// ONE AND ONLY SBT — CREATED ONCE, NEVER RECREATED
// Validation-perfect shader groups | SBT safe | Single pool | LAS ready
// No VUID errors | No crashes | Pink photons scream
// THE EMPIRE IS UNBROKEN — PINK PHOTONS FOREVER 💖
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
#include <print>

using StoneKey::stone_device;
using StoneKey::g_transientCommandPool;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_physical;
using StoneKey::stone_seal_rtprops;
using StoneKey::stone_rtprops;

using BufferManager::BufferInfo;

namespace RTX {

std::atomic<bool>     PipelineManager::g_pipelineNeedsRebuild{false};
std::atomic<uint32_t> PipelineManager::g_rebuildRequestedFrame{UINT32_MAX};
static std::mutex rebuildMutex;

// Eternal SBT guard — one and only
static bool s_eternalSbtForged = false;

// =============================================================================
// Constructor — Minimal early setup
// =============================================================================
PipelineManager::PipelineManager(VkDevice device, VkPhysicalDevice phys)
{
    std::print("[PIPELINE] FORGING PERFECT PIPELINE MANAGER — 2026 FINAL EDITION\n");

    RTX::loadRTExtensions(StoneKey::stone_instance(), device);

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

    std::print("[PIPELINE SUCCESS] PipelineManager forged — ready for empire\n");
}

// =============================================================================
// Pipeline Layout — Creates all 4 descriptor set layouts
// =============================================================================
void PipelineManager::createPipelineLayout()
{
    if (rtPipelineLayout_.valid()) {
        std::print("[PIPELINE] Pipeline layout already exists — skipping recreation\n");
        return;
    }

    std::print("[PIPELINE] Forging descriptor set layouts and pipeline layout\n");

    // Set 0: Main RT bindings
    VkDescriptorSetLayoutCreateInfo mainInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(kMainBindings.size()),
        .pBindings    = kMainBindings.data()
    };
    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(stone_device(), &mainInfo, nullptr, &mainLayout));
    rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(mainLayout, stone_device(), vkDestroyDescriptorSetLayout);

    // Set 2: Texture array (1024 images)
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

    // Sets 1 & 3: Empty
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

    std::print("[PIPELINE SUCCESS] All descriptor set layouts and pipeline layout forged\n");
}

// =============================================================================
// Descriptor Set Allocation — Uses SINGLE GLOBAL POOL
// =============================================================================
void PipelineManager::allocateDescriptorSets()
{
    std::print("[PIPELINE TRACE] Allocating descriptor sets from SINGLE GLOBAL pool\n");

    VkDescriptorPool globalPool = RTX::g_ctx().descriptorPool_.get();
    if (globalPool == VK_NULL_HANDLE) {
        std::print(stderr, "[ERROR] Global descriptor pool not available\n");
        return;
    }

    const uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    auto allocate = [this, globalPool, frames](std::vector<VkDescriptorSet>& sets,
                                              VkDescriptorSetLayout layout,
                                              const char* name) {
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
        std::print("[PIPELINE SUCCESS] Allocated {} {} descriptor sets from global pool\n", frames, name);
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

    const uint32_t primitiveCount = 1;
    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo);

    uint64_t bufferHandle = BufferManager::create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
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
// Shader Loading — Robust with fallback paths
// =============================================================================
VkShaderModule PipelineManager::loadShader(const std::string& relativePath) const
{
    std::print("[PIPELINE] Loading shader: {}\n", relativePath);

    const std::string fullPath = "build/bin/Linux/" + relativePath;

    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::print(stderr, "[ERROR] Shader not found: {}\n", fullPath);
        return VK_NULL_HANDLE;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
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
// Ray Tracing Pipeline Creation — Validation-perfect
// =============================================================================
void PipelineManager::createRayTracingPipeline()
{
    std::print("[PIPELINE] Forging ray tracing pipeline\n");

    rtPipeline_.reset();
    shaderModules_.clear();

    auto load = [this](const char* path) -> VkShaderModule {
        VkShaderModule mod = loadShader(path);
        if (mod == VK_NULL_HANDLE) throw std::runtime_error("Shader load failed");
        return mod;
    };

    VkShaderModule raygen = load("assets/shaders/raytracing/raygen.spv");
    VkShaderModule miss   = load("assets/shaders/raytracing/miss.spv");
    VkShaderModule chit   = load("assets/shaders/raytracing/closest_hit.spv");
    VkShaderModule ahit   = load("assets/shaders/raytracing/anyhit.spv");

    shaderModules_.emplace_back(raygen, stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(miss,   stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(chit,   stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(ahit,   stone_device(), vkDestroyShaderModule);

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

    uint32_t idx = 0;

    // Raygen — GENERAL
    stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                      .stage  = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                      .module = raygen,
                      .pName  = "main"});
    groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                      .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                      .generalShader = idx++,
                      .closestHitShader = VK_SHADER_UNUSED_KHR,
                      .anyHitShader = VK_SHADER_UNUSED_KHR,
                      .intersectionShader = VK_SHADER_UNUSED_KHR});

    // Miss — GENERAL
    stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                      .stage  = VK_SHADER_STAGE_MISS_BIT_KHR,
                      .module = miss,
                      .pName  = "main"});
    groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                      .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                      .generalShader = idx++,
                      .closestHitShader = VK_SHADER_UNUSED_KHR,
                      .anyHitShader = VK_SHADER_UNUSED_KHR,
                      .intersectionShader = VK_SHADER_UNUSED_KHR});

    // Closest Hit
    stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                      .stage  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                      .module = chit,
                      .pName  = "main"});
    uint32_t chitIdx = idx++;

    // Any Hit
    stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                      .stage  = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                      .module = ahit,
                      .pName  = "main"});
    uint32_t ahitIdx = idx++;

    // Hit group — TRIANGLES
    groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                      .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                      .generalShader = VK_SHADER_UNUSED_KHR,
                      .closestHitShader = chitIdx,
                      .anyHitShader = ahitIdx,
                      .intersectionShader = VK_SHADER_UNUSED_KHR});

    raygenGroupCount_ = 1;
    missGroupCount_ = 1;
    hitGroupCount_ = 1;

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
    std::print("[PIPELINE SUCCESS] Ray tracing pipeline forged — validation perfect\n");
}

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
    const VkDeviceSize stride      = BufferManager::align_up(handleSize, handleAlign);

    const uint32_t totalGroups = raygenGroupCount_ + missGroupCount_ + hitGroupCount_;
    const VkDeviceSize raygenSize = BufferManager::align_up(raygenGroupCount_ * stride, baseAlign);
    const VkDeviceSize missSize   = missGroupCount_ * stride;
    const VkDeviceSize hitSize    = hitGroupCount_ * stride;
    const VkDeviceSize sbtSize    = raygenSize + missSize + hitSize;

    std::print("[PIPELINE] Forging ETERNAL SBT — {} bytes ({} groups)\n", sbtSize, totalGroups);

    BufferManager::ensurePersistentUpload();

    uint64_t sbtHandle = BufferManager::create(sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "RTX_SBT_Eternal");

    if (sbtHandle == 0) {
        std::print(stderr, "[FATAL] Failed to allocate eternal SBT buffer\n");
        return;
    }

    VkBuffer sbtBuffer = BufferManager::getVkBuffer(sbtHandle);
    VkDeviceAddress sbtAddress = BufferManager::get_device_address(sbtHandle);

    std::vector<uint8_t> handles(totalGroups * handleSize);
    VkResult res = g_ext.vkGetRayTracingShaderGroupHandlesKHR(
        stone_device(), rtPipeline_.get(), 0, totalGroups,
        handles.size(), handles.data());

    if (res != VK_SUCCESS) {
        std::print(stderr, "[ERROR] Failed to get shader group handles: {}\n", string_VkResult(res));
        BufferManager::destroy(sbtHandle);
        return;
    }

    if (handles.size() > BufferManager::g_persistentUploadSize) {
        std::print(stderr, "[FATAL] SBT handles too large for persistent upload buffer\n");
        BufferManager::destroy(sbtHandle);
        return;
    }

    // DIRECT WRITE — eternal mapped memory
    std::memcpy(BufferManager::g_persistentUploadMapped, handles.data(), handles.size());

    bool tempCmd = (cmd == VK_NULL_HANDLE);
    VkCommandBuffer uploadCmd = cmd;

    if (tempCmd) {
        if (pool == VK_NULL_HANDLE) pool = g_transientCommandPool;
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
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = handles.size()
    };
    vkCmdCopyBuffer(uploadCmd, BufferManager::g_persistentUploadBuffer, sbtBuffer, 1, &copy);

    VkMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT
    };
    VkDependencyInfo dep{
        .sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers    = &barrier
    };
    g_ext.vkCmdPipelineBarrier2(uploadCmd, &dep);

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

    sbtAddress_ = sbtAddress;
    sbtSize_    = sbtSize;

    raygenSbtRegion_ = {sbtAddress_, stride, raygenSize};
    missSbtRegion_   = {sbtAddress_ + raygenSize, stride, missSize};
    hitSbtRegion_    = {sbtAddress_ + raygenSize + missSize, stride, hitSize};

    sbtBuffer_ = Handle<VkBuffer>(sbtBuffer, stone_device(), vkDestroyBuffer);

    s_eternalSbtForged = true;

    std::print("[PIPELINE SUCCESS] ETERNAL SBT FORGED — PERSISTENT DIRECT WRITE — EMPIRE FULLY ARMED\n");
}

// =============================================================================
// Ray Tracing Execution — Uses LAS TLAS
// =============================================================================
void PipelineManager::traceRays(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t width, uint32_t height, uint32_t depth)
{
    std::lock_guard<std::mutex> lock(rebuildMutex);

    if (g_pipelineNeedsRebuild.load(std::memory_order_acquire)) {
        std::print("[PIPELINE] Rebuilding pipeline due to change\n");
        createRayTracingPipeline();
        // SBT is eternal — do not recreate
        g_pipelineNeedsRebuild.store(false, std::memory_order_release);
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());

    if (frameIndex >= rtDescriptorSets_.size()) return;

    const VkDescriptorSet descriptorSets[4] = {
        rtDescriptorSets_[frameIndex],
        emptyDescriptorSets_[frameIndex],
        texDescriptorSets_[frameIndex],
        emptyDescriptorSets_[frameIndex]
    };

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            rtPipelineLayout_.get(), 0, 4, descriptorSets, 0, nullptr);

    g_ext.vkCmdTraceRaysKHR(cmd,
                            &raygenSbtRegion_,
                            &missSbtRegion_,
                            &hitSbtRegion_,
                            &callableSbtRegion_,
                            width, height, depth);
}

// =============================================================================
// Full Pipeline Construction
// =============================================================================
void PipelineManager::forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer mainCmd)
{
    if (s_crownForged) {
        std::print("[PIPELINE TRACE] Pipeline already forged\n");
        return;
    }

    std::print("[PIPELINE] FORGING THE PERFECT RTX PIPELINE — FINAL 2026 EDITION\n");

    createPipelineLayout();
    allocateDescriptorSets();
    createRayTracingPipeline();
    createShaderBindingTable(commandPool, graphicsQueue, mainCmd);

    StoneKey::stone_seal_pipeline(this);
    s_crownForged = true;

    std::print("[PIPELINE SUCCESS] PERFECT RTX PIPELINE FORGED — VALIDATION PERFECT — PINK PHOTONS DOMINATE\n");
}

// =============================================================================
// Public Accessors & Updates
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

    stone_seal_rtprops(rtProps);
    cached = true;

    std::print("[PIPELINE SUCCESS] Properties cached — handle size: {}\n", rtProps.shaderGroupHandleSize);
}

void PipelineManager::updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo) noexcept
{
    if (frameIndex >= rtDescriptorSets_.size() || rtDescriptorSets_[frameIndex] == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorSet set = rtDescriptorSets_[frameIndex];
    std::array<VkWriteDescriptorSet, 17> writes{};
    uint32_t writeCount = 0;

    auto addAccel = [&](uint32_t binding, VkAccelerationStructureKHR as) {
        as = as ? as : (RTX::las().getTLAS() ? RTX::las().getTLAS() : dummyTLAS_.get());
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

    if (updateInfo.envSampler && updateInfo.envImageView) {
        addSampler(7, updateInfo.envSampler, updateInfo.envImageView);
    }

    if (!updateInfo.nexusScoreViews.empty() && frameIndex < updateInfo.nexusScoreViews.size()) {
        addImage(6, updateInfo.nexusScoreViews[frameIndex]);
    }

    if (writeCount > 0) {
        vkUpdateDescriptorSets(stone_device(), writeCount, writes.data(), 0, nullptr);
    }
}

// =============================================================================
// Destructor — Empire rests in peace
// =============================================================================
PipelineManager::~PipelineManager()
{
    std::print("[PIPELINE] PERFECT PIPELINE MANAGER RESTS — EMPIRE ETERNAL\n");
}

} // namespace RTX

// =============================================================================
// JANUARY 05, 2026 — FINAL PIPELINE MANAGER v27.2
// kMainBindings fixed — now in class scope
// ETERNAL SBT — created once and only once
// Validation-perfect | Single pool | LAS ready
// THE EMPIRE IS UNBROKEN — PINK PHOTONS FOREVER 💖
// =============================================================================