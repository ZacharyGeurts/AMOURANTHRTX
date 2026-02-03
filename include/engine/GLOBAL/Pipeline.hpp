// =============================================================================
// AMOURANTH RTX Engine (C) 2026
// engine/GLOBAL/Pipeline.hpp
// Ray tracing + compute pipeline, SBT, descriptor management
// Version 0.81 —  February 02, 2026 — Header-only
// - No namespaces — pure global scope
// - AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include "engine/GLOBAL/AMOURANTHRTX.hpp"
#include "engine/GLOBAL/ELLIE.hpp"

#include <algorithm>
#include <array>
#include <vector>
#include <fstream>

// Main descriptor bindings
static constexpr std::array<VkDescriptorSetLayoutBinding, 9> kMainBindings = {{
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
    {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR},
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

// =============================================================================
// Helpers — defined first (dependency order critical in header-only)
// =============================================================================

inline VkShaderModule load_shader(const std::string& relativePath) {
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
    VkResult res = vkCreateShaderModule(rtx().device, &info, nullptr, &module);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkCreateShaderModule failed: {} ({} bytes) — {}", string_VkResult(res), size, usedPath);
        return VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Shader loaded: {} ({} bytes) from {}", relativePath, size, usedPath);
    return module;
}

inline VkAccelerationStructureKHR create_dummy_tlas() {
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
    ext().vkGetAccelerationStructureBuildSizesKHR(
        rtx().device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
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

    rtx().dummy_accel_buffer = info->buffer;
    rtx().dummy_accel_memory = info->memory;

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = info->buffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkAccelerationStructureKHR as = VK_NULL_HANDLE;
    VkResult res = ext().vkCreateAccelerationStructureKHR(rtx().device, &createInfo, nullptr, &as);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create dummy TLAS: {}", string_VkResult(res));
        BM_DESTROY(buffer_handle);
        return VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Dummy TLAS created");
    return as;
}

inline void cache_descriptor_properties() {
    if (rtx().descriptor_props_cached) return;

    LOG_INFO_CAT("PIPELINE", "Caching descriptor buffer properties");

    VkPhysicalDeviceDescriptorBufferPropertiesEXT props{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;

    VkPhysicalDeviceProperties2 pdProps2{};
    pdProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    pdProps2.pNext = &props;

    vkGetPhysicalDeviceProperties2(rtx().physical, &pdProps2);

    rtx().descriptor_props = props;
    rtx().descriptor_props_cached = true;

    LOG_SUCCESS_CAT("PIPELINE", "Descriptor buffer properties cached");
}

inline void cache_ray_tracing_properties() {
    static bool cached = false;
    if (cached) return;

    LOG_INFO_CAT("PIPELINE", "Caching ray tracing properties");

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR props{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 pdProps2{};
    pdProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    pdProps2.pNext = &props;

    vkGetPhysicalDeviceProperties2(rtx().physical, &pdProps2);

    if (props.shaderGroupHandleSize == 0) {
        LOG_FATAL_CAT("PIPELINE", "Ray tracing not supported");
        return;
    }

    rtx().rt_props = props;
    cached = true;

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing properties cached");
}

// =============================================================================
// Main functions — now safe to use helpers
// =============================================================================

inline void pipeline_initialize() noexcept {
    LOG_INFO_CAT("PIPELINE", "Initializing Pipeline — frame-free mode");

    if (!ext().vkCmdTraceRaysKHR || !ext().vkCreateRayTracingPipelinesKHR ||
        !ext().vkGetRayTracingShaderGroupHandlesKHR || !ext().vkCreateAccelerationStructureKHR ||
        !ext().vkGetAccelerationStructureBuildSizesKHR || !ext().vkGetBufferDeviceAddress ||
        !ext().vkGetDescriptorEXT || !ext().vkCmdBindDescriptorBuffersEXT ||
        !ext().vkCmdSetDescriptorBufferOffsetsEXT) {
        LOG_FATAL_CAT("PIPELINE", "Required extensions missing");
        return;
    }

    rtx().dummy_tlas = create_dummy_tlas();

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
    rtx().living_world_buffer_handle = lw_handle;

    cache_descriptor_properties();

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline initialized");
}

inline void pipeline_create_pipeline_layout() {
    if (rtx().pipeline_layout != VK_NULL_HANDLE) return;

    LOG_INFO_CAT("PIPELINE", "Creating pipeline layout");

    VkDescriptorSetLayoutCreateInfo mainInfo{};
    mainInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    mainInfo.bindingCount = static_cast<uint32_t>(kMainBindings.size());
    mainInfo.pBindings = kMainBindings.data();
    mainInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(rtx().device, &mainInfo, nullptr, &mainLayout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create main layout: {}", string_VkResult(res));
        return;
    }
    rtx().main_descriptor_layout = mainLayout;

    for (uint32_t i = 0; i < kMainBindings.size(); ++i) {
        ext().vkGetDescriptorSetLayoutBindingOffsetEXT(rtx().device, mainLayout, i, &rtx().binding_offsets[i]);
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
    res = vkCreateDescriptorSetLayout(rtx().device, &texInfo, nullptr, &texLayout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create texture layout: {}", string_VkResult(res));
        return;
    }
    rtx().tex_descriptor_layout = texLayout;

    VkDescriptorSetLayoutCreateInfo emptyInfo{};
    emptyInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    emptyInfo.bindingCount = 0;
    emptyInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    res = vkCreateDescriptorSetLayout(rtx().device, &emptyInfo, nullptr, &emptyLayout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create empty layout: {}", string_VkResult(res));
        return;
    }
    rtx().empty_descriptor_layout = emptyLayout;

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
    res = vkCreatePipelineLayout(rtx().device, &plInfo, nullptr, &pl);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create pipeline layout: {}", string_VkResult(res));
        return;
    }

    rtx().pipeline_layout = pl;

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created");
}

inline void pipeline_create_compute_pipeline() {
    if (rtx().compute_pipeline != VK_NULL_HANDLE) return;

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
    compInfo.layout = rtx().pipeline_layout;
    compInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkPipeline compPipe = VK_NULL_HANDLE;
    VkResult res = vkCreateComputePipelines(rtx().device, VK_NULL_HANDLE, 1, &compInfo, nullptr, &compPipe);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkCreateComputePipelines failed: {}", string_VkResult(res));
        vkDestroyShaderModule(rtx().device, compModule, nullptr);
        return;
    }

    rtx().compute_pipeline = compPipe;
    vkDestroyShaderModule(rtx().device, compModule, nullptr);

    LOG_SUCCESS_CAT("PIPELINE", "Compute pipeline created");
}

inline void pipeline_create_ray_tracing_pipeline() {
    if (rtx().rt_pipeline != VK_NULL_HANDLE) return;

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

    rtx().raygen_group_count = 1;
    rtx().miss_group_count = 1;
    rtx().hit_group_count = 1;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    pipelineInfo.pGroups = groups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout = rtx().pipeline_layout;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult res = ext().vkCreateRayTracingPipelinesKHR(
        rtx().device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    vkDestroyShaderModule(rtx().device, raygen, nullptr);
    vkDestroyShaderModule(rtx().device, miss, nullptr);
    vkDestroyShaderModule(rtx().device, chit, nullptr);
    vkDestroyShaderModule(rtx().device, ahit, nullptr);

    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkCreateRayTracingPipelinesKHR failed: {}", string_VkResult(res));
        return;
    }

    rtx().rt_pipeline = pipeline;

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing pipeline created");
}

inline void pipeline_create_shader_binding_table(VkCommandPool pool = VK_NULL_HANDLE,
                                                 VkQueue queue = VK_NULL_HANDLE,
                                                 VkCommandBuffer providedCmd = VK_NULL_HANDLE) {
    if (rtx().eternal_sbt_forged) return;

    VkPipeline rtPipe = rtx().rt_pipeline;
    if (rtPipe == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "No ray-tracing pipeline — cannot forge SBT");
        return;
    }

    cache_ray_tracing_properties();
    const auto& rtProps = rtx().rt_props;

    const VkDeviceSize handleSize  = rtProps.shaderGroupHandleSize;
    const VkDeviceSize handleAlign = rtProps.shaderGroupHandleAlignment;
    const VkDeviceSize baseAlign   = rtProps.shaderGroupBaseAlignment;

    const VkDeviceSize recordStride = align_up(handleSize, std::max(handleAlign, baseAlign));

    const uint32_t totalGroups = rtx().raygen_group_count + rtx().miss_group_count + rtx().hit_group_count;

    const VkDeviceSize raygenSize = recordStride;
    const VkDeviceSize missSize   = rtx().miss_group_count * recordStride;
    const VkDeviceSize hitSize    = rtx().hit_group_count * recordStride;

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
    VkResult res = ext().vkGetRayTracingShaderGroupHandlesKHR(
        rtx().device, rtPipe, 0, totalGroups, handles.size(), handles.data());

    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkGetRayTracingShaderGroupHandlesKHR failed: {}", string_VkResult(res));
        BM_DESTROY(sbt_handle);
        return;
    }

    VkCommandBuffer cmd = providedCmd;
    bool ownCmd = (providedCmd == VK_NULL_HANDLE);
    VkCommandPool cmdPool = pool ? pool : rtx().transient_pool;

    if (ownCmd) {
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool = cmdPool;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        res = vkAllocateCommandBuffers(rtx().device, &alloc, &cmd);
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

        vkQueueSubmit(queue ? queue : rtx().graphics_queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue ? queue : rtx().graphics_queue);

        vkFreeCommandBuffers(rtx().device, cmdPool, 1, &cmd);
    }

    VkDeviceAddress raygenAddr = align_up(sbtAddr, rtProps.shaderGroupBaseAlignment);
    VkDeviceAddress missAddr   = align_up(raygenAddr + raygenSize, rtProps.shaderGroupBaseAlignment);
    VkDeviceAddress hitAddr    = align_up(missAddr + missSize, rtProps.shaderGroupBaseAlignment);

    rtx().sbt_address = sbtAddr;
    rtx().sbt_size = sbtSize;

    rtx().raygen_sbt_region = {raygenAddr, recordStride, raygenSize};
    rtx().miss_sbt_region   = {missAddr,   recordStride, missSize};
    rtx().hit_sbt_region    = {hitAddr,    recordStride, hitSize};

    rtx().eternal_sbt_forged = true;

    LOG_SUCCESS_CAT("PIPELINE", "SBT forged");
}

inline void pipeline_dispatch_living_world(VkCommandBuffer cmd, float totalTime) noexcept {
    if (rtx().compute_pipeline == VK_NULL_HANDLE) {
        pipeline_create_compute_pipeline();
    }

    if (rtx().compute_pipeline == VK_NULL_HANDLE) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rtx().compute_pipeline);

    VkDescriptorBufferBindingInfoEXT bind{};
    bind.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    bind.address = rtx().descriptor_buffer_address;
    bind.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    ext().vkCmdBindDescriptorBuffersEXT(cmd, 1, &bind);

    uint32_t idx = 0;
    VkDeviceSize off = 0;
    ext().vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                             rtx().pipeline_layout, 0, 1, &idx, &off);

    vkCmdPushConstants(cmd, rtx().pipeline_layout, FULL_PUSH_MASK, 0, sizeof(float), &totalTime);

    vkCmdDispatch(cmd, 1, 1, 1);
}

inline void pipeline_trace_rays(VkCommandBuffer cmd, uint32_t width, uint32_t height) noexcept {
    if (!cmd) return;

    VkPipeline rtPipe = rtx().rt_pipeline;
    if (!rtPipe) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipe);

    VkDescriptorBufferBindingInfoEXT bind{};
    bind.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    bind.address = rtx().descriptor_buffer_address;
    bind.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    ext().vkCmdBindDescriptorBuffersEXT(cmd, 1, &bind);

    uint32_t idx = 0;
    VkDeviceSize off = 0;
    ext().vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                             rtx().pipeline_layout, 0, 1, &idx, &off);

    float totalTime = static_cast<float>(TotalTime::get().seconds());

    vkCmdPushConstants(cmd, rtx().pipeline_layout, FULL_PUSH_MASK, 0, sizeof(float), &totalTime);

    ext().vkCmdTraceRaysKHR(cmd,
                            &rtx().raygen_sbt_region,
                            &rtx().miss_sbt_region,
                            &rtx().hit_sbt_region,
                            nullptr,
                            width, height, 1);
}

inline void pipeline_write_rt_descriptors(const RTDescriptorUpdate& update) noexcept {
    uint64_t desc_handle = rtx().descriptor_buffer_handle;
    if (desc_handle == 0) {
        constexpr VkDeviceSize INITIAL = 4096ULL;
        BM_CREATE(desc_handle, INITIAL, VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT, "EternalDescriptorBuffer");
        if (desc_handle == 0) return;
        rtx().descriptor_buffer_handle = desc_handle;
        rtx().descriptor_buffer_address = BM_GET_DEVICE_ADDRESS(desc_handle);
    }

    void* mapped = rtx().descriptor_mapped;
    if (!mapped) {
        mapped = BM_LAZY_MAP_DESCRIPTOR(desc_handle);
        if (!mapped) return;
        rtx().descriptor_mapped = mapped;
    }

    if (!update.tlas) return;

    uint8_t* base = static_cast<uint8_t*>(mapped);
    const auto& props = rtx().descriptor_props;

    // Binding 0: TLAS
    {
        VkAccelerationStructureDeviceAddressInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        info.accelerationStructure = update.tlas;
        VkDeviceAddress addr = ext().vkGetAccelerationStructureDeviceAddressKHR(rtx().device, &info);

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        get.data.accelerationStructure = addr;

        ext().vkGetDescriptorEXT(rtx().device, &get, props.accelerationStructureDescriptorSize,
                                 base + rtx().binding_offsets[0]);
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

        ext().vkGetDescriptorEXT(rtx().device, &get, props.storageImageDescriptorSize,
                                 base + rtx().binding_offsets[1]);
    }

    // Binding 2: Camera UBO
    if (update.ubo && update.uboSize) {
        VkBufferDeviceAddressInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = update.ubo;
        VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(rtx().device, &info);

        VkDescriptorAddressInfoEXT descAddr{};
        descAddr.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        descAddr.address = addr;
        descAddr.range = update.uboSize;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        get.data.pUniformBuffer = &descAddr;

        ext().vkGetDescriptorEXT(rtx().device, &get, props.uniformBufferDescriptorSize,
                                 base + rtx().binding_offsets[2]);
    }

    // Binding 3: Materials
    if (update.materialsBuffer && update.materialsSize) {
        VkBufferDeviceAddressInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = update.materialsBuffer;
        VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(rtx().device, &info);

        VkDescriptorAddressInfoEXT descAddr{};
        descAddr.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        descAddr.address = addr;
        descAddr.range = update.materialsSize;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        get.data.pStorageBuffer = &descAddr;

        ext().vkGetDescriptorEXT(rtx().device, &get, props.storageBufferDescriptorSize,
                                 base + rtx().binding_offsets[3]);
    }

    // Binding 7: Living world
    if (rtx().living_world_buffer_handle) {
        VkBuffer buf = BM_GET_BUFFER(rtx().living_world_buffer_handle);

        VkBufferDeviceAddressInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = buf;
        VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(rtx().device, &info);

        VkDescriptorAddressInfoEXT descAddr{};
        descAddr.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        descAddr.address = addr;
        descAddr.range = 64;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        get.data.pStorageBuffer = &descAddr;

        ext().vkGetDescriptorEXT(rtx().device, &get, props.storageBufferDescriptorSize,
                                 base + rtx().binding_offsets[7]);
    }
}

inline void pipeline_shutdown() noexcept {
    void* mapped = rtx().descriptor_mapped;
    if (mapped != nullptr) {
        const BufferInfo* info = BM_GET(rtx().descriptor_buffer_handle);
        if (info) vkUnmapMemory(rtx().device, info->memory);
    }

    BM_DESTROY(rtx().descriptor_buffer_handle);
    BM_DESTROY(rtx().living_world_buffer_handle);

    if (auto l = rtx().main_descriptor_layout) vkDestroyDescriptorSetLayout(rtx().device, l, nullptr);
    if (auto l = rtx().tex_descriptor_layout) vkDestroyDescriptorSetLayout(rtx().device, l, nullptr);
    if (auto l = rtx().empty_descriptor_layout) vkDestroyDescriptorSetLayout(rtx().device, l, nullptr);

    if (auto t = rtx().dummy_tlas) ext().vkDestroyAccelerationStructureKHR(rtx().device, t, nullptr);
    if (auto b = rtx().dummy_accel_buffer) vkDestroyBuffer(rtx().device, b, nullptr);
    if (auto m = rtx().dummy_accel_memory) vkFreeMemory(rtx().device, m, nullptr);

    LOG_INFO_CAT("PIPELINE", "Pipeline shutdown complete");
}