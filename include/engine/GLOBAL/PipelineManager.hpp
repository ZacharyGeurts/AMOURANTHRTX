// =============================================================================
// AMOURANTH RTX Engine — Pipeline System (Fully Flattened)
// Ray tracing + compute pipeline, SBT, descriptor management
// Version 30.13 — January 31, 2026 — Header-only, fixed definition order
// - All inline functions defined in correct dependency order (no forward stubs)
// - Helpers (load_shader, create_dummy_tlas, caches) defined first
// - Main functions follow
// - No classes — pure namespace Pipeline
// - Empire-first consistency
// - PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <algorithm>
#include <array>
#include <vector>
#include <fstream>

using RTX::g_ext;

using StoneKey::stone_device;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_physical;
using StoneKey::stone_rtprops;
using StoneKey::stone_transient_pool;
using StoneKey::stone_compute_pipeline;
using StoneKey::stone_rt_pipeline;
using StoneKey::stone_pipeline_layout;
using StoneKey::stone_seal_rtprops;
using StoneKey::stone_living_world_buffer_handle;
using StoneKey::stone_descriptor_buffer_handle;
using StoneKey::stone_descriptor_mapped;
using StoneKey::stone_descriptor_buffer_address;
using StoneKey::stone_binding_offsets;
using StoneKey::stone_descriptor_props;
using StoneKey::stone_descriptor_props_cached;
using StoneKey::stone_eternal_sbt_forged;
using StoneKey::stone_sbt_address;
using StoneKey::stone_sbt_size;
using StoneKey::stone_raygen_sbt_region;
using StoneKey::stone_miss_sbt_region;
using StoneKey::stone_hit_sbt_region;
using StoneKey::stone_dummy_tlas;
using StoneKey::stone_dummy_accel_buffer;
using StoneKey::stone_dummy_accel_memory;
using StoneKey::stone_main_descriptor_layout;
using StoneKey::stone_tex_descriptor_layout;
using StoneKey::stone_empty_descriptor_layout;
using StoneKey::stone_raygen_group_count;
using StoneKey::stone_miss_group_count;
using StoneKey::stone_hit_group_count;

using BufferManager::BufferInfo;
using BufferManager::align_up;

static constexpr std::array<VkDescriptorSetLayoutBinding, 9> kMainBindings = {{
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
    {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
    {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR}
}};

static constexpr VkShaderStageFlags FULL_PUSH_MASK =
    VK_SHADER_STAGE_COMPUTE_BIT |
    VK_SHADER_STAGE_RAYGEN_BIT_KHR |
    VK_SHADER_STAGE_MISS_BIT_KHR |
    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
    VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

struct RTDescriptorUpdate {
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    VkImageView                rtOutputView = VK_NULL_HANDLE;
    VkBuffer                   ubo = VK_NULL_HANDLE;
    VkDeviceSize               uboSize = 0;
    VkBuffer                   materialsBuffer = VK_NULL_HANDLE;
    VkDeviceSize               materialsSize = 0;
};

namespace Pipeline {

// =============================================================================
// Helpers — defined first (dependency order critical in header-only)
// =============================================================================

// Shader loading
inline VkShaderModule load_shader(const std::string& relativePath)
{
    LOG_INFO_CAT("PIPELINE", "Loading shader: {}", relativePath);

    std::array<std::string, 3> paths = {
        std::format("build/bin/Linux/{}", relativePath),
        std::format("build-windows/bin/Windows/{}", relativePath),
        std::format("assets/shaders/{}", relativePath)
    };

    std::ifstream file;
    std::string usedPath;

    for (const auto& p : paths) {
        file.open(p, std::ios::ate | std::ios::binary);
        if (file.is_open()) {
            usedPath = p;
            break;
        }
    }

    if (!file.is_open()) {
        LOG_FATAL_CAT("PIPELINE", "Shader not found — tried {} / {} / {}", paths[0], paths[1], paths[2]);
        return VK_NULL_HANDLE;
    }

    size_t size = static_cast<size_t>(file.tellg());
    if (size == 0 || size % 4 != 0) {
        LOG_FATAL_CAT("PIPELINE", "Invalid SPIR-V size ({} bytes): {}", size, usedPath);
        return VK_NULL_HANDLE;
    }

    std::vector<char> code(size);
    file.seekg(0);
    file.read(code.data(), size);

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size;
    info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(stone_device(), &info, nullptr, &module);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkCreateShaderModule failed: {} ({} bytes) — {}", string_VkResult(res), size, usedPath);
        return VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Shader loaded: {} ({} bytes) from {}", relativePath, size, usedPath);
    return module;
}

// Dummy TLAS creation
inline VkAccelerationStructureKHR create_dummy_tlas()
{
    LOG_INFO_CAT("PIPELINE", "Creating dummy TLAS");

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.data.deviceAddress = 0;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    constexpr uint32_t primitiveCount = 1;
    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primitiveCount, &sizeInfo);

    uint64_t buffer_handle = 0;
    BM_CREATE(buffer_handle, sizeInfo.accelerationStructureSize,
              VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              "Dummy_TLAS_Buffer");

    if (buffer_handle == 0) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create dummy TLAS buffer");
        return VK_NULL_HANDLE;
    }

    const BufferInfo* info = BM_GET(buffer_handle);
    if (!info) {
        BM_DESTROY(buffer_handle);
        return VK_NULL_HANDLE;
    }

    StoneKey::detail::empire().dummy_accel_buffer = info->buffer;
    StoneKey::detail::empire().dummy_accel_memory = info->memory;

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = info->buffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkAccelerationStructureKHR as = VK_NULL_HANDLE;
    VkResult res = g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &as);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create dummy TLAS: {}", string_VkResult(res));
        BM_DESTROY(buffer_handle);
        return VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Dummy TLAS created");
    return as;
}

// Cache descriptor properties
inline void cache_descriptor_properties()
{
    if (stone_descriptor_props_cached()) return;

    LOG_INFO_CAT("PIPELINE", "Caching descriptor buffer properties");

    VkPhysicalDeviceDescriptorBufferPropertiesEXT props{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;

    VkPhysicalDeviceProperties2 pdProps2{};
    pdProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    pdProps2.pNext = &props;

    vkGetPhysicalDeviceProperties2(stone_physical(), &pdProps2);

    stone_descriptor_props() = props;
    stone_descriptor_props_cached() = true;

    LOG_SUCCESS_CAT("PIPELINE", "Descriptor buffer properties cached");
}

// Cache ray tracing properties
inline void cache_ray_tracing_properties()
{
    static bool cached = false;
    if (cached) return;

    LOG_INFO_CAT("PIPELINE", "Caching ray tracing properties");

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR props{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 pdProps2{};
    pdProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    pdProps2.pNext = &props;

    vkGetPhysicalDeviceProperties2(stone_physical(), &pdProps2);

    if (props.shaderGroupHandleSize == 0) {
        LOG_FATAL_CAT("PIPELINE", "Ray tracing not supported");
        return;
    }

    stone_seal_rtprops(props);
    cached = true;

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing properties cached");
}

// =============================================================================
// Main functions — now safe to use helpers
// =============================================================================

inline void initialize() noexcept
{
    LOG_INFO_CAT("PIPELINE", "Initializing Pipeline — frame-free mode");

    if (!g_ext.vkCmdTraceRaysKHR || !g_ext.vkCreateRayTracingPipelinesKHR ||
        !g_ext.vkGetRayTracingShaderGroupHandlesKHR || !g_ext.vkCreateAccelerationStructureKHR ||
        !g_ext.vkGetAccelerationStructureBuildSizesKHR || !g_ext.vkGetBufferDeviceAddress ||
        !g_ext.vkGetDescriptorEXT || !g_ext.vkCmdBindDescriptorBuffersEXT ||
        !g_ext.vkCmdSetDescriptorBufferOffsetsEXT) {
        LOG_FATAL_CAT("PIPELINE", "Required extensions missing");
        return;
    }

    StoneKey::detail::empire().dummy_tlas = create_dummy_tlas();

    uint64_t lw_handle = 0;
    BM_CREATE(lw_handle, 64,
              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
              VK_BUFFER_USAGE_TRANSFER_DST_BIT |
              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
              "LivingWorldBuffer");

    if (lw_handle == 0) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create living world buffer");
        return;
    }
    StoneKey::detail::empire().living_world_buffer_handle = lw_handle;

    cache_descriptor_properties();

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline initialized");
}

inline void create_pipeline_layout()
{
    if (stone_pipeline_layout() != VK_NULL_HANDLE) return;

    LOG_INFO_CAT("PIPELINE", "Creating pipeline layout");

    VkDescriptorSetLayoutCreateInfo mainInfo{};
    mainInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    mainInfo.bindingCount = static_cast<uint32_t>(kMainBindings.size());
    mainInfo.pBindings = kMainBindings.data();
    mainInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(stone_device(), &mainInfo, nullptr, &mainLayout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create main layout: {}", string_VkResult(res));
        return;
    }
    StoneKey::detail::empire().main_descriptor_layout = mainLayout;

    for (uint32_t i = 0; i < kMainBindings.size(); ++i) {
        g_ext.vkGetDescriptorSetLayoutBindingOffsetEXT(stone_device(), mainLayout, i, &stone_binding_offsets()[i]);
    }

    VkDescriptorSetLayoutBinding texBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1024,
        .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR
    };

    VkDescriptorSetLayoutCreateInfo texInfo{};
    texInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    texInfo.bindingCount = 1;
    texInfo.pBindings = &texBinding;
    texInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout texLayout = VK_NULL_HANDLE;
    res = vkCreateDescriptorSetLayout(stone_device(), &texInfo, nullptr, &texLayout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create texture layout: {}", string_VkResult(res));
        return;
    }
    StoneKey::detail::empire().tex_descriptor_layout = texLayout;

    VkDescriptorSetLayoutCreateInfo emptyInfo{};
    emptyInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    emptyInfo.bindingCount = 0;
    emptyInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    res = vkCreateDescriptorSetLayout(stone_device(), &emptyInfo, nullptr, &emptyLayout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create empty layout: {}", string_VkResult(res));
        return;
    }
    StoneKey::detail::empire().empty_descriptor_layout = emptyLayout;

    const VkDescriptorSetLayout layouts[4] = {
        mainLayout, emptyLayout, texLayout, emptyLayout
    };

    VkPushConstantRange push{};
    push.stageFlags = FULL_PUSH_MASK;
    push.offset = 0;
    push.size = sizeof(float);

    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount = 4;
    plInfo.pSetLayouts = layouts;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &push;

    VkPipelineLayout pl = VK_NULL_HANDLE;
    res = vkCreatePipelineLayout(stone_device(), &plInfo, nullptr, &pl);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create pipeline layout: {}", string_VkResult(res));
        return;
    }

    StoneKey::detail::empire().pipeline_layout = pl;

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created");
}

inline void create_compute_pipeline()
{
    if (stone_compute_pipeline() != VK_NULL_HANDLE) return;

    LOG_INFO_CAT("PIPELINE", "Creating compute pipeline for living world");

    VkShaderModule compModule = load_shader("assets/shaders/compute/living_world.spv");
    if (compModule == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Failed to load living_world.spv");
        return;
    }

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = compModule;
    stage.pName = "main";

    VkComputePipelineCreateInfo compInfo{};
    compInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compInfo.stage = stage;
    compInfo.layout = stone_pipeline_layout();
    compInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkPipeline compPipe = VK_NULL_HANDLE;
    VkResult res = vkCreateComputePipelines(stone_device(), VK_NULL_HANDLE, 1, &compInfo, nullptr, &compPipe);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkCreateComputePipelines failed: {}", string_VkResult(res));
        vkDestroyShaderModule(stone_device(), compModule, nullptr);
        return;
    }

    StoneKey::detail::empire().compute_pipeline = compPipe;
    vkDestroyShaderModule(stone_device(), compModule, nullptr);

    LOG_SUCCESS_CAT("PIPELINE", "Compute pipeline created");
}

inline void create_ray_tracing_pipeline()
{
    if (stone_rt_pipeline() != VK_NULL_HANDLE) return;

    LOG_INFO_CAT("PIPELINE", "Creating ray tracing pipeline");

    auto load = [](std::string_view path) -> VkShaderModule {
        return load_shader(std::string(path));
    };

    VkShaderModule raygen = load("assets/shaders/raytracing/raygen.spv");
    VkShaderModule miss   = load("assets/shaders/raytracing/miss.spv");
    VkShaderModule chit   = load("assets/shaders/raytracing/closest_hit.spv");
    VkShaderModule ahit   = load("assets/shaders/raytracing/anyhit.spv");

    if (!raygen || !miss || !chit || !ahit) {
        LOG_FATAL_CAT("PIPELINE", "Shader load failed");
        return;
    }

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    stages.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygen, "main", nullptr});
    stages.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR, miss, "main", nullptr});
    stages.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, chit, "main", nullptr});
    stages.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, ahit, "main", nullptr});

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    groups.push_back({VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});
    groups.push_back({VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});
    groups.push_back({VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, 3, VK_SHADER_UNUSED_KHR});

    stone_raygen_group_count() = 1;
    stone_miss_group_count() = 1;
    stone_hit_group_count() = 1;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    pipelineInfo.pGroups = groups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout = stone_pipeline_layout();
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult res = g_ext.vkCreateRayTracingPipelinesKHR(
        stone_device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    vkDestroyShaderModule(stone_device(), raygen, nullptr);
    vkDestroyShaderModule(stone_device(), miss, nullptr);
    vkDestroyShaderModule(stone_device(), chit, nullptr);
    vkDestroyShaderModule(stone_device(), ahit, nullptr);

    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkCreateRayTracingPipelinesKHR failed: {}", string_VkResult(res));
        return;
    }

    StoneKey::detail::empire().rt_pipeline = pipeline;

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing pipeline created");
}

inline void create_shader_binding_table(VkCommandPool pool = VK_NULL_HANDLE,
                                        VkQueue queue = VK_NULL_HANDLE,
                                        VkCommandBuffer providedCmd = VK_NULL_HANDLE)
{
    if (stone_eternal_sbt_forged()) return;

    VkPipeline rtPipe = stone_rt_pipeline();
    if (rtPipe == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "No ray-tracing pipeline — cannot forge SBT");
        return;
    }

    cache_ray_tracing_properties();
    const auto& rtProps = stone_rtprops();

    const VkDeviceSize handleSize  = rtProps.shaderGroupHandleSize;
    const VkDeviceSize handleAlign = rtProps.shaderGroupHandleAlignment;
    const VkDeviceSize baseAlign   = rtProps.shaderGroupBaseAlignment;

    const VkDeviceSize recordStride = align_up(handleSize, std::max(handleAlign, baseAlign));

    const uint32_t totalGroups = stone_raygen_group_count() + stone_miss_group_count() + stone_hit_group_count();

    const VkDeviceSize raygenSize = recordStride;
    const VkDeviceSize missSize   = stone_miss_group_count() * recordStride;
    const VkDeviceSize hitSize    = stone_hit_group_count() * recordStride;

    VkDeviceSize sbtSize = raygenSize + missSize + hitSize;
    sbtSize = align_up(sbtSize, baseAlign);

    LOG_INFO_CAT("PIPELINE", "Forging SBT — size={}", sbtSize);

    uint64_t sbt_handle = 0;
    BM_CREATE(sbt_handle, sbtSize,
              VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              "EternalSBT");

    if (sbt_handle == 0) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create SBT buffer");
        return;
    }

    VkDeviceAddress sbtAddr = BM_GET_DEVICE_ADDRESS(sbt_handle);

    std::vector<uint8_t> handles(totalGroups * handleSize);
    VkResult res = g_ext.vkGetRayTracingShaderGroupHandlesKHR(
        stone_device(), rtPipe, 0, totalGroups, handles.size(), handles.data());

    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkGetRayTracingShaderGroupHandlesKHR failed: {}", string_VkResult(res));
        BM_DESTROY(sbt_handle);
        return;
    }

    VkCommandBuffer cmd = providedCmd;
    bool ownCmd = (providedCmd == VK_NULL_HANDLE);
    VkCommandPool cmdPool = pool ? pool : stone_transient_pool();

    if (ownCmd) {
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool = cmdPool;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        res = vkAllocateCommandBuffers(stone_device(), &alloc, &cmd);
        if (res != VK_SUCCESS) {
            BM_DESTROY(sbt_handle);
            return;
        }

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);
    }

    BM_UPLOAD_TO_BUFFER(sbt_handle, handles.data(), handles.size(), cmd);

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);

    if (ownCmd) {
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        vkQueueSubmit(queue ? queue : stone_graphics_queue(), 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue ? queue : stone_graphics_queue());

        vkFreeCommandBuffers(stone_device(), cmdPool, 1, &cmd);
    }

    VkDeviceAddress raygenAddr = align_up(sbtAddr, rtProps.shaderGroupBaseAlignment);
    VkDeviceAddress missAddr   = align_up(raygenAddr + raygenSize, rtProps.shaderGroupBaseAlignment);
    VkDeviceAddress hitAddr    = align_up(missAddr + missSize, rtProps.shaderGroupBaseAlignment);

    stone_sbt_address() = sbtAddr;
    stone_sbt_size() = sbtSize;

    stone_raygen_sbt_region() = {raygenAddr, recordStride, raygenSize};
    stone_miss_sbt_region()   = {missAddr,   recordStride, missSize};
    stone_hit_sbt_region()    = {hitAddr,    recordStride, hitSize};

    stone_eternal_sbt_forged() = true;

    LOG_SUCCESS_CAT("PIPELINE", "SBT forged");
}

inline void dispatch_living_world(VkCommandBuffer cmd, float totalTime) noexcept
{
    if (stone_compute_pipeline() == VK_NULL_HANDLE) {
        create_compute_pipeline();
    }

    if (stone_compute_pipeline() == VK_NULL_HANDLE) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, stone_compute_pipeline());

    VkDescriptorBufferBindingInfoEXT bind{};
    bind.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    bind.address = stone_descriptor_buffer_address();
    bind.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    g_ext.vkCmdBindDescriptorBuffersEXT(cmd, 1, &bind);

    uint32_t idx = 0;
    VkDeviceSize off = 0;
    g_ext.vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                             stone_pipeline_layout(), 0, 1, &idx, &off);

    vkCmdPushConstants(cmd, stone_pipeline_layout(), FULL_PUSH_MASK, 0, sizeof(float), &totalTime);

    vkCmdDispatch(cmd, 1, 1, 1);
}

inline void trace_rays(VkCommandBuffer cmd, uint32_t width, uint32_t height) noexcept
{
    if (!cmd) return;

    VkPipeline rtPipe = stone_rt_pipeline();
    if (!rtPipe) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipe);

    VkDescriptorBufferBindingInfoEXT bind{};
    bind.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    bind.address = stone_descriptor_buffer_address();
    bind.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    g_ext.vkCmdBindDescriptorBuffersEXT(cmd, 1, &bind);

    uint32_t idx = 0;
    VkDeviceSize off = 0;
    g_ext.vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                             stone_pipeline_layout(), 0, 1, &idx, &off);

    float totalTime = static_cast<float>(RTX::TotalTime::get().seconds());

    vkCmdPushConstants(cmd, stone_pipeline_layout(), FULL_PUSH_MASK, 0, sizeof(float), &totalTime);

    g_ext.vkCmdTraceRaysKHR(cmd,
                            &stone_raygen_sbt_region(),
                            &stone_miss_sbt_region(),
                            &stone_hit_sbt_region(),
                            nullptr,
                            width, height, 1);
}

inline void write_rt_descriptors(const RTDescriptorUpdate& update) noexcept
{
    uint64_t desc_handle = stone_descriptor_buffer_handle();
    if (desc_handle == 0) {
        constexpr VkDeviceSize INITIAL = 4096ULL;
        BM_CREATE(desc_handle, INITIAL, VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT, "EternalDescriptorBuffer");
        if (desc_handle == 0) return;
        StoneKey::detail::empire().descriptor_buffer_handle = desc_handle;
        stone_descriptor_buffer_address() = BM_GET_DEVICE_ADDRESS(desc_handle);
    }

    void* mapped = stone_descriptor_mapped();
    if (!mapped) {
        mapped = BM_LAZY_MAP_DESCRIPTOR(desc_handle);
        if (!mapped) return;
        StoneKey::detail::empire().descriptor_mapped = mapped;
    }

    if (!update.tlas) return;

    uint8_t* base = static_cast<uint8_t*>(mapped);
    const auto& props = stone_descriptor_props();

    // Binding 0: TLAS
    {
        VkAccelerationStructureDeviceAddressInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        info.accelerationStructure = update.tlas;
        VkDeviceAddress addr = g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &info);

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        get.data.accelerationStructure = addr;

        g_ext.vkGetDescriptorEXT(stone_device(), &get, props.accelerationStructureDescriptorSize,
                                 base + stone_binding_offsets()[0]);
    }

    // Binding 1: Output image
    if (update.rtOutputView) {
        VkDescriptorImageInfo img{};
        img.imageView = update.rtOutputView;
        img.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        get.data.pStorageImage = &img;

        g_ext.vkGetDescriptorEXT(stone_device(), &get, props.storageImageDescriptorSize,
                                 base + stone_binding_offsets()[1]);
    }

    // Binding 2: Camera UBO
    if (update.ubo && update.uboSize) {
        VkBufferDeviceAddressInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = update.ubo;
        VkDeviceAddress addr = g_ext.vkGetBufferDeviceAddress(stone_device(), &info);

        VkDescriptorAddressInfoEXT descAddr{};
        descAddr.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        descAddr.address = addr;
        descAddr.range = update.uboSize;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        get.data.pUniformBuffer = &descAddr;

        g_ext.vkGetDescriptorEXT(stone_device(), &get, props.uniformBufferDescriptorSize,
                                 base + stone_binding_offsets()[2]);
    }

    // Binding 3: Materials
    if (update.materialsBuffer && update.materialsSize) {
        VkBufferDeviceAddressInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = update.materialsBuffer;
        VkDeviceAddress addr = g_ext.vkGetBufferDeviceAddress(stone_device(), &info);

        VkDescriptorAddressInfoEXT descAddr{};
        descAddr.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        descAddr.address = addr;
        descAddr.range = update.materialsSize;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        get.data.pStorageBuffer = &descAddr;

        g_ext.vkGetDescriptorEXT(stone_device(), &get, props.storageBufferDescriptorSize,
                                 base + stone_binding_offsets()[3]);
    }

    // Binding 7: Living world
    if (stone_living_world_buffer_handle()) {
        VkBuffer buf = BM_GET_BUFFER(stone_living_world_buffer_handle());

        VkBufferDeviceAddressInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = buf;
        VkDeviceAddress addr = g_ext.vkGetBufferDeviceAddress(stone_device(), &info);

        VkDescriptorAddressInfoEXT descAddr{};
        descAddr.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        descAddr.address = addr;
        descAddr.range = 64;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        get.data.pStorageBuffer = &descAddr;

        g_ext.vkGetDescriptorEXT(stone_device(), &get, props.storageBufferDescriptorSize,
                                 base + stone_binding_offsets()[7]);
    }
}

inline void shutdown() noexcept
{
    void* mapped = stone_descriptor_mapped();
    if (mapped != nullptr) {
        const BufferInfo* info = BM_GET(stone_descriptor_buffer_handle());
        if (info) vkUnmapMemory(stone_device(), info->memory);
    }

    BM_DESTROY(stone_descriptor_buffer_handle());
    BM_DESTROY(stone_living_world_buffer_handle());

    if (auto l = stone_main_descriptor_layout()) vkDestroyDescriptorSetLayout(stone_device(), l, nullptr);
    if (auto l = stone_tex_descriptor_layout()) vkDestroyDescriptorSetLayout(stone_device(), l, nullptr);
    if (auto l = stone_empty_descriptor_layout()) vkDestroyDescriptorSetLayout(stone_device(), l, nullptr);

    if (auto t = stone_dummy_tlas()) g_ext.vkDestroyAccelerationStructureKHR(stone_device(), t, nullptr);
    if (auto b = stone_dummy_accel_buffer()) vkDestroyBuffer(stone_device(), b, nullptr);
    if (auto m = stone_dummy_accel_memory()) vkFreeMemory(stone_device(), m, nullptr);

    LOG_INFO_CAT("PIPELINE", "Pipeline shutdown complete");
}

} // namespace Pipeline