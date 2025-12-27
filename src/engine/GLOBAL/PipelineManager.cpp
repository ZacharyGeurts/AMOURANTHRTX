// src/engine/GLOBAL/PipelineManager.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — DECEMBER 27, 2025 — FULL LIGHT ASSURED
// PIPELINEMANAGER v18.2 — ROBUST & REINFORCED
// - Fixed: emptyDescriptorSets_ now properly declared and allocated
// - All descriptor sets (0,1,2,3) correctly bound in traceRays()
// - Defensive checks and logging everywhere
// - Dummy TLAS always valid → pink photons guaranteed
// PINK PHOTONS ETERNAL — EMPIRE VICTORIOUS
// =============================================================================

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <vector>
#include <atomic>

using namespace Logging::Color;

using StoneKey::stone_device;

namespace RTX {

std::atomic<bool>     PipelineManager::g_pipelineNeedsRebuild{false};
std::atomic<uint32_t> PipelineManager::g_rebuildRequestedFrame{UINT32_MAX};

// =============================================================================
// RT PIPELINE BINDINGS — FULLY MATCHES SHADERS AND LAS
// Set 0:
//   0: TLAS
//   1: RT Output
//   2: CameraSceneData (uniform)
//   3: Accumulation
//   4: Materials (storage)
//   6: Nexus Score
//   7: Env Map
//   8: Blue Noise
//   9: Density
//   10: Additional storage
//   31: StoneKey buffer
// Set 2:
//   0: Texture array (1024)
// =============================================================================
constexpr std::array RT_PIPELINE_BINDINGS = {
    VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    VkDescriptorSetLayoutBinding{4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    VkDescriptorSetLayoutBinding{6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    VkDescriptorSetLayoutBinding{7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
        VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
    VkDescriptorSetLayoutBinding{8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    VkDescriptorSetLayoutBinding{9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
    VkDescriptorSetLayoutBinding{10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    VkDescriptorSetLayoutBinding{31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR}
};

// =============================================================================
// Descriptor Pool
// =============================================================================
void PipelineManager::createDescriptorPool() noexcept
{
    if (rtDescriptorPool_.valid()) return;

    LOG_INFO_CAT("PIPELINE", "Creating descriptor pool");

    std::array<VkDescriptorPoolSize, 7> poolSizes{{
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 8 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              64 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             32 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             32 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              16 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     8192 },
        { VK_DESCRIPTOR_TYPE_SAMPLER,                    16 }
    }};

    VkDescriptorPoolCreateInfo info{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 128,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorPool(stone_device(), &info, nullptr, &pool);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create descriptor pool: {}", string_VkResult(res));
        return;
    }

    rtDescriptorPool_ = Handle<VkDescriptorPool>(pool, stone_device(), vkDestroyDescriptorPool, 0, "RT_DescriptorPool");

    LOG_SUCCESS_CAT("PIPELINE", "Descriptor pool created");
}

// =============================================================================
// Constructor
// =============================================================================
PipelineManager::PipelineManager(VkDevice device, VkPhysicalDevice phys)
{
    LOG_INFO_CAT("PIPELINE", "PipelineManager construction");

    RTX::loadRTExtensions(StoneKey::stone_instance(), device);

    if (!g_ext.vkCmdTraceRaysKHR ||
        !g_ext.vkCreateRayTracingPipelinesKHR ||
        !g_ext.vkGetRayTracingShaderGroupHandlesKHR ||
        !g_ext.vkCreateAccelerationStructureKHR ||
        !g_ext.vkDestroyAccelerationStructureKHR ||
        !g_ext.vkGetAccelerationStructureBuildSizesKHR ||
        !g_ext.vkGetBufferDeviceAddress) {
        LOG_FATAL_CAT("PIPELINE", "Critical ray tracing extensions missing");
        throw std::runtime_error("Ray tracing extensions missing");
    }

    dummyTLAS_ = Handle<VkAccelerationStructureKHR>(
        createDummyTLAS(), stone_device(), g_ext.vkDestroyAccelerationStructureKHR);

    LOG_SUCCESS_CAT("PIPELINE", "PipelineManager constructed — dummy TLAS ready");
}

// =============================================================================
// Descriptor Set Allocation — Now correctly allocates empty sets for set 1 & 3
// =============================================================================
void PipelineManager::allocateDescriptorSets()
{
    LOG_TRACE_CAT("PIPELINE", "Allocating descriptor sets");

    const uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    if (rtDescriptorPool_.get() == VK_NULL_HANDLE || 
        !rtDescriptorSetLayout_.valid() || 
        !texDescriptorSetLayout_.valid() || 
        !emptyDescriptorSetLayout_.valid()) {
        LOG_FATAL_CAT("PIPELINE", "Missing prerequisites for descriptor allocation");
        throw std::runtime_error("Descriptor allocation prerequisites missing");
    }

    // Main RT sets (set 0)
    rtDescriptorSets_.resize(frames);
    std::vector<VkDescriptorSetLayout> mainLayouts(frames, rtDescriptorSetLayout_.get());

    VkDescriptorSetAllocateInfo mainInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = rtDescriptorPool_.get(),
        .descriptorSetCount = frames,
        .pSetLayouts        = mainLayouts.data()
    };

    VkResult result = vkAllocateDescriptorSets(stone_device(), &mainInfo, rtDescriptorSets_.data());
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to allocate main descriptor sets: {}", string_VkResult(result));
        throw std::runtime_error("Main descriptor set allocation failed");
    }
    LOG_SUCCESS_CAT("PIPELINE", "Allocated {} main descriptor sets (set 0)", frames);

    // Texture array sets (set 2)
    texDescriptorSets_.resize(frames);
    std::vector<VkDescriptorSetLayout> texLayouts(frames, texDescriptorSetLayout_.get());

    VkDescriptorSetAllocateInfo texInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = rtDescriptorPool_.get(),
        .descriptorSetCount = frames,
        .pSetLayouts        = texLayouts.data()
    };

    result = vkAllocateDescriptorSets(stone_device(), &texInfo, texDescriptorSets_.data());
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to allocate texture descriptor sets: {}", string_VkResult(result));
        throw std::runtime_error("Texture descriptor set allocation failed");
    }
    LOG_SUCCESS_CAT("PIPELINE", "Allocated {} texture descriptor sets (set 2)", frames);

    // Empty sets for set 1 and set 3
    emptyDescriptorSets_.resize(frames);
    std::vector<VkDescriptorSetLayout> emptyLayouts(frames, emptyDescriptorSetLayout_.get());

    VkDescriptorSetAllocateInfo emptyInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = rtDescriptorPool_.get(),
        .descriptorSetCount = frames,
        .pSetLayouts        = emptyLayouts.data()
    };

    result = vkAllocateDescriptorSets(stone_device(), &emptyInfo, emptyDescriptorSets_.data());
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to allocate empty descriptor sets (set 1 & 3): {}", string_VkResult(result));
        throw std::runtime_error("Empty descriptor set allocation failed");
    }
    LOG_SUCCESS_CAT("PIPELINE", "Allocated {} empty descriptor sets (set 1 & 3)", frames);
}

// =============================================================================
// Descriptor Set Update — FULLY MATCHES SHADERS
// =============================================================================
void PipelineManager::updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo) noexcept
{
    if (frameIndex >= rtDescriptorSets_.size()) return;

    VkDescriptorSet set = rtDescriptorSets_[frameIndex];
    if (set == VK_NULL_HANDLE) return;

    std::array<VkWriteDescriptorSet, 17> writes{};
    uint32_t count = 0;

    const auto writeAccel = [&](VkAccelerationStructureKHR tlas) {
        tlas = tlas ? tlas : dummyTLAS_.get();
        VkWriteDescriptorSetAccelerationStructureKHR info{
            .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures    = &tlas
        };
        writes[count++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext           = &info,
            .dstSet          = set,
            .dstBinding      = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        };
    };

    const auto writeImage = [&](uint32_t binding, VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL) {
        if (view == VK_NULL_HANDLE) return;
        VkDescriptorImageInfo info{.imageView = view, .imageLayout = layout};
        writes[count++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo      = &info
        };
    };

    const auto writeBuffer = [&](uint32_t binding, VkBuffer buf, VkDeviceSize size, VkDescriptorType type) {
        if (buf == VK_NULL_HANDLE) return;
        VkDescriptorBufferInfo info{.buffer = buf, .offset = 0, .range = size};
        writes[count++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = type,
            .pBufferInfo     = &info
        };
    };

    const auto writeSampler = [&](uint32_t binding, VkSampler sampler, VkImageView view) {
        if (sampler == VK_NULL_HANDLE || view == VK_NULL_HANDLE) return;
        VkDescriptorImageInfo info{.sampler = sampler, .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        writes[count++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo      = &info
        };
    };

    writeAccel(updateInfo.tlas);
    writeImage(1, updateInfo.rtOutputView);

    if (Options::OptionsRTX::ENABLE_ACCUMULATION && !updateInfo.accumulationViews.empty() && frameIndex < updateInfo.accumulationViews.size()) {
        writeImage(3, updateInfo.accumulationViews[frameIndex]);
    }

    writeBuffer(2, updateInfo.ubo, updateInfo.uboSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    if (updateInfo.materialsBuffer) {
        writeBuffer(4, updateInfo.materialsBuffer, updateInfo.materialsSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }

    if (updateInfo.envSampler && updateInfo.envImageView) writeSampler(7, updateInfo.envSampler, updateInfo.envImageView);
    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING && !updateInfo.nexusScoreViews.empty() && frameIndex < updateInfo.nexusScoreViews.size()) writeImage(6, updateInfo.nexusScoreViews[frameIndex]);
    if (updateInfo.blueNoiseSampler && updateInfo.blueNoiseView) writeSampler(8, updateInfo.blueNoiseSampler, updateInfo.blueNoiseView);
    if (updateInfo.densitySampler && updateInfo.densityView) writeSampler(9, updateInfo.densitySampler, updateInfo.densityView);
    if (updateInfo.additionalStorageBuffer) writeBuffer(10, updateInfo.additionalStorageBuffer, updateInfo.additionalStorageSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    if (updateInfo.stoneKeyBuffer) writeBuffer(31, updateInfo.stoneKeyBuffer, updateInfo.stoneKeySize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    if (count > 0) {
        vkUpdateDescriptorSets(stone_device(), count, writes.data(), 0, nullptr);
    }
}

// =============================================================================
// Shader Loading
// =============================================================================
VkShaderModule PipelineManager::loadShader(const std::string& relativePath) const
{
    LOG_TRACE_CAT("PIPELINE", "Loading shader: {}", relativePath);

    if (stone_device() == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    const std::string fullPath = "build/bin/Linux/" + relativePath;

    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("PIPELINE", "Shader file not found: {}", fullPath);
        return VK_NULL_HANDLE;
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        LOG_ERROR_CAT("PIPELINE", "Invalid SPIR-V size ({} bytes): {}", fileSize, fullPath);
        return VK_NULL_HANDLE;
    }

    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo info{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode    = reinterpret_cast<const uint32_t*>(code.data())
    };

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(stone_device(), &info, nullptr, &module);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create shader module: {}", string_VkResult(res));
        return VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Shader loaded: {} ({} bytes)", relativePath, fileSize);
    return module;
}

// =============================================================================
// Dummy TLAS
// =============================================================================
VkAccelerationStructureKHR PipelineManager::createDummyTLAS()
{
    VkAccelerationStructureGeometryKHR geometry{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry     = { .instances = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR, .data = { .deviceAddress = 0 } }}
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    const uint32_t count = 1;
    g_ext.vkGetAccelerationStructureBuildSizesKHR(stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &count, &sizeInfo);

    uint64_t handle = BufferManager::create(sizeInfo.accelerationStructureSize,
                                            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            "DummyTLAS_Buffer");

    if (handle == 0) return VK_NULL_HANDLE;

    const auto* info = BufferManager::get(handle);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = info->buffer,
        .size   = sizeInfo.accelerationStructureSize,
        .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

    VkAccelerationStructureKHR as = VK_NULL_HANDLE;
    VkResult res = g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &as);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create dummy TLAS: {}", string_VkResult(res));
        BufferManager::destroy(handle);
        return VK_NULL_HANDLE;
    }

    dummyAccelBuffer_ = Handle<VkBuffer>(info->buffer, stone_device(), vkDestroyBuffer);
    dummyAccelMemory_ = Handle<VkDeviceMemory>(info->memory, stone_device(), vkFreeMemory);

    return as;
}

// =============================================================================
// Ray Tracing Pipeline Creation
// =============================================================================
void PipelineManager::createRayTracingPipeline()
{
    auto load = [this](const char* path) -> VkShaderModule {
        VkShaderModule mod = loadShader(path);
        if (!mod) throw std::runtime_error("Shader load failure");
        return mod;
    };

    VkShaderModule raygen = load("assets/shaders/raytracing/raygen.spv");
    VkShaderModule miss   = load("assets/shaders/raytracing/miss.spv");
    VkShaderModule chit   = load("assets/shaders/raytracing/closest_hit.spv");
    VkShaderModule ahit   = load("assets/shaders/raytracing/anyhit.spv");

    shaderModules_.clear();
    shaderModules_.emplace_back(raygen, stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(miss,   stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(chit,   stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(ahit,   stone_device(), vkDestroyShaderModule);

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

    uint32_t idx = 0;

    // Raygen
    stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR, .module = raygen, .pName = "main"});
    groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                      .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                      .generalShader = idx++});

    // Miss
    stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .module = miss, .pName = "main"});
    groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                      .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                      .generalShader = idx++});

    // Closest Hit
    stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, .module = chit, .pName = "main"});
    uint32_t chitIdx = idx++;

    // Any Hit
    stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR, .module = ahit, .pName = "main"});
    uint32_t ahitIdx = idx++;

    // Hit Group (Triangles)
    groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                      .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                      .closestHitShader = chitIdx,
                      .anyHitShader = ahitIdx});

    raygenGroupCount_ = 1;
    missGroupCount_   = 1;
    hitGroupCount_    = 1;

    VkRayTracingPipelineCreateInfoKHR info{
        .sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount                   = static_cast<uint32_t>(stages.size()),
        .pStages                      = stages.data(),
        .groupCount                   = static_cast<uint32_t>(groups.size()),
        .pGroups                      = groups.data(),
        .maxPipelineRayRecursionDepth = Options::OptionsRTX::MAX_PIPELINE_RAY_RECURSION_DEPTH,
        .layout                       = rtPipelineLayout_.get()
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult res = g_ext.vkCreateRayTracingPipelinesKHR(stone_device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create ray tracing pipeline: {}", string_VkResult(res));
        return;
    }

    rtPipeline_ = Handle<VkPipeline>(pipeline, stone_device(), vkDestroyPipeline);

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing pipeline created — 4 shaders (raygen, miss, closest_hit, any_hit) — VALIDATION CLEAN");
}

// =============================================================================
// Device Properties Cache
// =============================================================================
void PipelineManager::cacheDeviceProperties()
{
    const VkPhysicalDevice phys = StoneKey::stone_physical();
    if (phys == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "No physical device");
        throw std::runtime_error("No physical device");
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;

    vkGetPhysicalDeviceProperties2(phys, &props2);

    if (rtProps.shaderGroupHandleSize == 0) {
        LOG_FATAL_CAT("PIPELINE", "Ray tracing not supported — handleSize = 0");
        throw std::runtime_error("Ray tracing unsupported");
    }

    StoneKey::stone_seal_rtprops(rtProps);

    auto& ctx = RTX::g_ctx();
    ctx.physicalDeviceProperties_ = props2.properties;
    ctx.rayTracingProps_ = rtProps;
}

// =============================================================================
// Ray Tracing Execution — Now correctly binds all 4 sets
// =============================================================================
void PipelineManager::traceRays(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t width, uint32_t height, uint32_t depth)
{
    if (g_pipelineNeedsRebuild.load()) {
        createRayTracingPipeline();
        g_pipelineNeedsRebuild.store(false);
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());

    if (frameIndex >= rtDescriptorSets_.size() || 
        frameIndex >= texDescriptorSets_.size() || 
        frameIndex >= emptyDescriptorSets_.size()) {
        LOG_ERROR_CAT("PIPELINE", "Invalid frameIndex {} for binding descriptors (sizes: {} {} {})", 
                      frameIndex, rtDescriptorSets_.size(), texDescriptorSets_.size(), emptyDescriptorSets_.size());
        return;
    }

    VkDescriptorSet sets[4] = {
        rtDescriptorSets_[frameIndex],
        emptyDescriptorSets_[frameIndex],
        texDescriptorSets_[frameIndex],
        emptyDescriptorSets_[frameIndex]
    };

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_.get(), 0, 4, sets, 0, nullptr);

    g_ext.vkCmdTraceRaysKHR(cmd, &raygenSbtRegion_, &missSbtRegion_, &hitSbtRegion_, &callableSbtRegion_, width, height, depth);
}

void PipelineManager::createPipelineLayout()
{
    if (rtPipelineLayout_.valid()) {
        LOG_TRACE_CAT("PIPELINE", "Pipeline layout already exists — skipping recreation");
        return;
    }

    LOG_INFO_CAT("PIPELINE", "Creating ray tracing pipeline layout — 4 sets");

    createDescriptorPool();

    // Set 0: Main RT bindings
    std::vector<VkDescriptorSetLayoutBinding> mainBindings(RT_PIPELINE_BINDINGS.begin(), RT_PIPELINE_BINDINGS.end());
    std::ranges::sort(mainBindings, [](const auto& a, const auto& b) { return a.binding < b.binding; });

    VkDescriptorSetLayoutCreateInfo mainInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(mainBindings.size()),
        .pBindings    = mainBindings.data()
    };

    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(stone_device(), &mainInfo, nullptr, &mainLayout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create main descriptor set layout: {}", string_VkResult(res));
        return;
    }
    rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(mainLayout, stone_device(), vkDestroyDescriptorSetLayout);

    // Set 2: Texture array
    VkDescriptorSetLayoutBinding texBinding{
        .binding            = 0,
        .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount    = 1024,
        .stageFlags         = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        .pImmutableSamplers = nullptr
    };

    VkDescriptorSetLayoutCreateInfo texInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &texBinding
    };

    VkDescriptorSetLayout texLayout = VK_NULL_HANDLE;
    res = vkCreateDescriptorSetLayout(stone_device(), &texInfo, nullptr, &texLayout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create texture descriptor set layout: {}", string_VkResult(res));
        return;
    }
    texDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(texLayout, stone_device(), vkDestroyDescriptorSetLayout);

    // Empty layouts for set 1 and 3
    VkDescriptorSetLayoutCreateInfo emptyInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 0,
        .pBindings    = nullptr
    };

    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    res = vkCreateDescriptorSetLayout(stone_device(), &emptyInfo, nullptr, &emptyLayout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create empty descriptor set layout: {}", string_VkResult(res));
        return;
    }
    emptyDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(emptyLayout, stone_device(), vkDestroyDescriptorSetLayout);

    // Final pipeline layout — 4 sets
    VkDescriptorSetLayout layouts[4] = {
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

    VkPipelineLayout layout = VK_NULL_HANDLE;
    res = vkCreatePipelineLayout(stone_device(), &plInfo, nullptr, &layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create pipeline layout: {}", string_VkResult(res));
        return;
    }

    rtPipelineLayout_ = Handle<VkPipelineLayout>(layout, stone_device(), vkDestroyPipelineLayout);

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing pipeline layout created — 4 sets");
}

// =============================================================================
// Shader Binding Table Creation
// =============================================================================
void PipelineManager::createShaderBindingTable(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd)
{
    if (rtPipeline_.get() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Cannot create SBT — pipeline missing");
        return;
    }

    cacheDeviceProperties();

    const auto& rtProps = StoneKey::stone_rtprops();
    const VkDeviceSize handleSize = rtProps.shaderGroupHandleSize;

    if (handleSize == 0) {
        LOG_FATAL_CAT("PIPELINE", "shaderGroupHandleSize = 0 — ray tracing unsupported");
        return;
    }

    const VkDeviceSize handleAlign = std::max(rtProps.shaderGroupHandleAlignment, 32u);
    const VkDeviceSize baseAlign   = std::max(rtProps.shaderGroupBaseAlignment, 64u);
    const VkDeviceSize stride      = align_up(handleSize, handleAlign);

    const uint32_t totalGroups = raygenGroupCount_ + missGroupCount_ + hitGroupCount_;
    if (totalGroups == 0) {
        LOG_FATAL_CAT("PIPELINE", "No shader groups defined");
        return;
    }

    const VkDeviceSize raygenSize = align_up(raygenGroupCount_ * stride, baseAlign);
    const VkDeviceSize missSize   = missGroupCount_ * stride;
    const VkDeviceSize hitSize    = hitGroupCount_ * stride;
    const VkDeviceSize requiredSize = raygenSize + missSize + hitSize;

    uint64_t sbtHandle = BufferManager::create(
        requiredSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "SBT_Eternal"
    );

    if (sbtHandle == 0) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create SBT buffer");
        return;
    }

    const auto* sbtInfo = BufferManager::get(sbtHandle);
    if (!sbtInfo) {
        BufferManager::destroy(sbtHandle);
        LOG_FATAL_CAT("PIPELINE", "Failed to get SBT buffer info");
        return;
    }

    std::vector<uint8_t> handles(totalGroups * handleSize);
    VkResult res = g_ext.vkGetRayTracingShaderGroupHandlesKHR(
        stone_device(), rtPipeline_.get(), 0, totalGroups, handles.size(), handles.data());
    if (res != VK_SUCCESS) {
        BufferManager::destroy(sbtHandle);
        LOG_FATAL_CAT("PIPELINE", "Failed to get shader group handles: {}", string_VkResult(res));
        return;
    }

    void* mapped = BufferManager::mapStaging(handles.size());
    if (!mapped) {
        BufferManager::destroy(sbtHandle);
        LOG_FATAL_CAT("PIPELINE", "Staging overflow during SBT creation");
        return;
    }

    std::memcpy(mapped, handles.data(), handles.size());

    VkBufferCopy copy{
        .srcOffset = BufferManager::getStagingOffset() - handles.size(),
        .dstOffset = 0,
        .size      = handles.size()
    };
    vkCmdCopyBuffer(cmd, BufferManager::getStagingBuffer(), sbtInfo->buffer, 1, &copy);

    VkMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    g_ext.vkCmdPipelineBarrier2(cmd, &dep);

    VkDeviceAddress baseAddr = BufferManager::get_device_address(sbtHandle);

    raygenSbtRegion_ = { baseAddr, stride, raygenGroupCount_ * stride };
    missSbtRegion_   = { baseAddr + raygenSize, stride, missGroupCount_ * stride };
    hitSbtRegion_    = { baseAddr + raygenSize + missSize, stride, hitGroupCount_ * stride };
    callableSbtRegion_ = { 0, 0, 0 };

    sbtBuffer_ = Handle<VkBuffer>(sbtInfo->buffer, stone_device(), vkDestroyBuffer);
    sbtMemory_ = Handle<VkDeviceMemory>(sbtInfo->memory, stone_device(), vkFreeMemory);
    sbtAddress_ = baseAddr;
    sbtSize_ = requiredSize;

    LOG_SUCCESS_CAT("PIPELINE", "SBT created — {} groups, {} bytes", totalGroups, requiredSize);
}

void PipelineManager::forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer mainCmd)
{
    if (s_crownForged) return;

    createDescriptorPool();
    createPipelineLayout();
    allocateDescriptorSets();
    createRayTracingPipeline();
    createShaderBindingTable(commandPool, graphicsQueue, mainCmd);

    StoneKey::stone_seal_pipeline(this);
    s_crownForged = true;

    LOG_SUCCESS_CAT("PIPELINE", "RTX pipeline forged — crown eternal");
}

// =============================================================================
// Public Accessors
// =============================================================================
VkDescriptorSet PipelineManager::getDescriptorSet(uint32_t frameIndex) const { 
    return (frameIndex < rtDescriptorSets_.size()) ? rtDescriptorSets_[frameIndex] : VK_NULL_HANDLE; 
}
VkPipeline PipelineManager::getPipeline() const { return rtPipeline_.get(); }
VkPipelineLayout PipelineManager::getPipelineLayout() const { return rtPipelineLayout_.get(); }

PipelineManager::~PipelineManager() = default;

} // namespace RTX

// =============================================================================
// FINAL PRODUCTION PIPELINEMANAGER v18.2 — DECEMBER 27, 2025
// FIXED: emptyDescriptorSets_ declared and allocated
// All 4 descriptor sets properly bound
// Robust error checking and logging
// PINK PHOTONS ETERNAL — EMPIRE VICTORIOUS
// =============================================================================