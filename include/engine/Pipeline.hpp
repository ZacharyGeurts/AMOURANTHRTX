#pragma once

// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/AMOURANTHRTX.hpp"
#include "engine/ELLIE.hpp"

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
// Helpers — no pool creation here
// =============================================================================

inline VkShaderModule load_shader(const std::string& relativePath) {
    LOG_ATTEMPT_CAT("PIPELINE", "Loading shader: {}", relativePath);

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

    vkh.checker(file.is_open(), "Shader file open", 
                std::format("Shader not found — tried {} / {} / {}", paths[0], paths[1], paths[2]).c_str());

    size_t size = static_cast<size_t>(file.tellg());
    vkh.checker(size > 0 && size % 4 == 0, "SPIR-V file size validation",
                std::format("Invalid SPIR-V size ({} bytes): {}", size, usedPath).c_str());

    std::vector<char> code(size);
    file.seekg(0);
    file.read(code.data(), size);
    file.close();

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size;
    info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    vkh.checker(
        vkCreateShaderModule(rtx().device, &info, nullptr, &module),
        "vkCreateShaderModule",
        std::format("{} ({} bytes)", relativePath, size).c_str()
    );

    LOG_SUCCESS_CAT("PIPELINE", "Shader loaded successfully — {} ({} bytes) from {}", relativePath, size, usedPath);
    return module;
}

inline VkAccelerationStructureKHR create_dummy_tlas() {
    LOG_ATTEMPT_CAT("PIPELINE", "Creating dummy TLAS for pipeline safety");

    vkh.checker(rtx().transient_pool != VK_NULL_HANDLE, "Transient pool existence",
                "Transient pool missing — cannot create dummy TLAS (init order issue)");

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

    uint64_t buffer_handle = Memory::create(sizeInfo.accelerationStructureSize,
                                            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            "Dummy_TLAS_Buffer");

    vkh.checker(buffer_handle != 0, "Memory::create (Dummy TLAS buffer)",
                "Failed to allocate dummy TLAS storage buffer");

    VkBuffer buffer = Memory::getBuffer(buffer_handle);
    vkh.checker(buffer != VK_NULL_HANDLE, "Memory::getBuffer (Dummy TLAS)",
                "Dummy TLAS buffer handle invalid after creation");

    rtx().dummy_accel_buffer = buffer;

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = buffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkAccelerationStructureKHR as = VK_NULL_HANDLE;
    vkh.checker(
        ext().vkCreateAccelerationStructureKHR(rtx().device, &createInfo, nullptr, &as),
        "vkCreateAccelerationStructureKHR (dummy TLAS)",
        "Failed to create dummy acceleration structure"
    );

    rtx().dummy_tlas = as;

    LOG_SUCCESS_CAT("PIPELINE", "Dummy TLAS created successfully — handle {}, size {} bytes",
                    (uintptr_t)as, sizeInfo.accelerationStructureSize);
    return as;
}

inline void cache_descriptor_properties() {
    if (rtx().descriptor_props_cached) {
        LOG_INFO_CAT("PIPELINE", "Descriptor buffer properties already cached — skipping");
        return;
    }

    LOG_ATTEMPT_CAT("PIPELINE", "Caching physical device descriptor buffer properties");

    VkPhysicalDeviceDescriptorBufferPropertiesEXT props{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;

    VkPhysicalDeviceProperties2 pdProps2{};
    pdProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    pdProps2.pNext = &props;

    vkGetPhysicalDeviceProperties2(rtx().physical, &pdProps2);

    rtx().descriptor_props = props;
    rtx().descriptor_props_cached = true;

    LOG_SUCCESS_CAT("PIPELINE", "Descriptor buffer properties cached successfully");
}

inline void cache_ray_tracing_properties() {
    static bool cached = false;
    if (cached) {
        LOG_INFO_CAT("PIPELINE", "Ray tracing properties already cached — skipping");
        return;
    }

    LOG_ATTEMPT_CAT("PIPELINE", "Caching ray tracing pipeline properties");

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR props{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 pdProps2{};
    pdProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    pdProps2.pNext = &props;

    vkGetPhysicalDeviceProperties2(rtx().physical, &pdProps2);

    vkh.checker(props.shaderGroupHandleSize != 0, "Ray tracing support check",
                "Ray tracing not supported — shader group handle size is zero");

    rtx().rt_props = props;
    cached = true;

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing properties cached — handle size {}, alignment {}", 
                    props.shaderGroupHandleSize, props.shaderGroupHandleAlignment);
}

// =============================================================================
// Main pipeline functions — no pool creation inside
// =============================================================================

inline void pipeline_initialize() noexcept {
    LOG_ATTEMPT_CAT("PIPELINE", "Initializing pipeline subsystem — frame-free mode");

    // Early extension check
    vkh.checker(ext().vkCmdTraceRaysKHR != nullptr, "Required extension: vkCmdTraceRaysKHR",
                "Critical ray-tracing extension missing");
    vkh.checker(ext().vkCreateRayTracingPipelinesKHR != nullptr, "Required extension: vkCreateRayTracingPipelinesKHR",
                "Critical ray-tracing extension missing");
    vkh.checker(ext().vkGetRayTracingShaderGroupHandlesKHR != nullptr, "Required extension: vkGetRayTracingShaderGroupHandlesKHR",
                "Critical ray-tracing extension missing");
    vkh.checker(ext().vkCreateAccelerationStructureKHR != nullptr, "Required extension: vkCreateAccelerationStructureKHR",
                "Critical acceleration structure extension missing");
    vkh.checker(ext().vkGetAccelerationStructureBuildSizesKHR != nullptr, "Required extension: vkGetAccelerationStructureBuildSizesKHR",
                "Critical build size extension missing");
    vkh.checker(ext().vkGetBufferDeviceAddress != nullptr, "Required extension: vkGetBufferDeviceAddress",
                "Critical buffer device address extension missing");
    vkh.checker(ext().vkGetDescriptorEXT != nullptr, "Required extension: vkGetDescriptorEXT",
                "Critical descriptor buffer extension missing");
    vkh.checker(ext().vkCmdBindDescriptorBuffersEXT != nullptr, "Required extension: vkCmdBindDescriptorBuffersEXT",
                "Critical descriptor buffer command extension missing");
    vkh.checker(ext().vkCmdSetDescriptorBufferOffsetsEXT != nullptr, "Required extension: vkCmdSetDescriptorBufferOffsetsEXT",
                "Critical descriptor buffer offset extension missing");

    vkh.checker(rtx().transient_pool != VK_NULL_HANDLE, "Transient pool existence",
                "Transient command pool missing — must be created before pipeline init");

    rtx().dummy_tlas = create_dummy_tlas();
    vkh.checker(rtx().dummy_tlas != VK_NULL_HANDLE, "Dummy TLAS creation",
                "Dummy TLAS creation failed — pipeline init aborted");

    uint64_t lw_handle = Memory::create(64,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                        "LivingWorldBuffer");

    vkh.checker(lw_handle != 0, "Memory::create (LivingWorldBuffer)",
                "Failed to create living world buffer — init aborted");

    rtx().living_world_buffer_handle = lw_handle;

    cache_descriptor_properties();

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline subsystem initialized — dummy TLAS ready, living world buffer allocated");
}

inline void pipeline_create_pipeline_layout() {
    if (rtx().pipeline_layout != VK_NULL_HANDLE) {
        LOG_INFO_CAT("PIPELINE", "Pipeline layout already exists — skipping creation");
        return;
    }

    LOG_ATTEMPT_CAT("PIPELINE", "Creating main pipeline layout");

    VkDescriptorSetLayoutCreateInfo mainInfo{};
    mainInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    mainInfo.bindingCount = static_cast<uint32_t>(kMainBindings.size());
    mainInfo.pBindings = kMainBindings.data();
    mainInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    vkh.checker(
        vkCreateDescriptorSetLayout(rtx().device, &mainInfo, nullptr, &mainLayout),
        "vkCreateDescriptorSetLayout (main)",
        "Failed to create main descriptor set layout"
    );
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
    vkh.checker(
        vkCreateDescriptorSetLayout(rtx().device, &texInfo, nullptr, &texLayout),
        "vkCreateDescriptorSetLayout (texture)",
        "Failed to create texture descriptor set layout"
    );
    rtx().tex_descriptor_layout = texLayout;

    VkDescriptorSetLayoutCreateInfo emptyInfo{};
    emptyInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    emptyInfo.bindingCount = 0;
    emptyInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    vkh.checker(
        vkCreateDescriptorSetLayout(rtx().device, &emptyInfo, nullptr, &emptyLayout),
        "vkCreateDescriptorSetLayout (empty)",
        "Failed to create empty descriptor set layout"
    );
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
    vkh.checker(
        vkCreatePipelineLayout(rtx().device, &plInfo, nullptr, &pl),
        "vkCreatePipelineLayout (main)",
        "Failed to create main pipeline layout"
    );

    rtx().pipeline_layout = pl;

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created successfully — main, texture, and empty sets bound");
}

inline void pipeline_create_compute_pipeline() {
    if (rtx().compute_pipeline != VK_NULL_HANDLE) {
        LOG_INFO_CAT("PIPELINE", "Compute pipeline already exists — skipping creation");
        return;
    }

    LOG_ATTEMPT_CAT("PIPELINE", "Creating compute pipeline for living world simulation");

    VkShaderModule compModule = load_shader("assets/shaders/compute/living_world.spv");
    vkh.checker(compModule != VK_NULL_HANDLE, "load_shader (living_world.spv)",
                "Failed to load living_world.spv — compute pipeline creation aborted");

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
    vkh.checker(
        vkCreateComputePipelines(rtx().device, VK_NULL_HANDLE, 1, &compInfo, nullptr, &compPipe),
        "vkCreateComputePipelines (living world)",
        "Failed to create living world compute pipeline"
    );

    rtx().compute_pipeline = compPipe;
    vkDestroyShaderModule(rtx().device, compModule, nullptr);

    LOG_SUCCESS_CAT("PIPELINE", "Compute pipeline created successfully for living world");
}

inline void pipeline_create_ray_tracing_pipeline() {
    if (rtx().rt_pipeline != VK_NULL_HANDLE) {
        LOG_INFO_CAT("PIPELINE", "Ray tracing pipeline already exists — skipping creation");
        return;
    }

    LOG_ATTEMPT_CAT("PIPELINE", "Creating ray tracing pipeline");

    auto load = [](std::string_view path) -> VkShaderModule {
        return load_shader(std::string(path));
    };

    VkShaderModule raygen = load("assets/shaders/raytracing/raygen.spv");
    VkShaderModule miss   = load("assets/shaders/raytracing/miss.spv");
    VkShaderModule chit   = load("assets/shaders/raytracing/closest_hit.spv");
    VkShaderModule ahit   = load("assets/shaders/raytracing/anyhit.spv");

    vkh.checker(raygen != VK_NULL_HANDLE && miss != VK_NULL_HANDLE && chit != VK_NULL_HANDLE && ahit != VK_NULL_HANDLE,
                "Ray tracing shader modules load check",
                "One or more ray tracing shaders failed to load — pipeline creation aborted");

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
    vkh.checker(
        ext().vkCreateRayTracingPipelinesKHR(
            rtx().device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline),
        "vkCreateRayTracingPipelinesKHR",
        "Failed to create ray tracing pipeline"
    );

    vkDestroyShaderModule(rtx().device, raygen, nullptr);
    vkDestroyShaderModule(rtx().device, miss, nullptr);
    vkDestroyShaderModule(rtx().device, chit, nullptr);
    vkDestroyShaderModule(rtx().device, ahit, nullptr);

    rtx().rt_pipeline = pipeline;

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing pipeline created successfully");
}

inline void pipeline_create_shader_binding_table(VkCommandPool pool = VK_NULL_HANDLE,
                                                 VkQueue queue = VK_NULL_HANDLE,
                                                 VkCommandBuffer providedCmd = VK_NULL_HANDLE) {
    if (rtx().eternal_sbt_forged) {
        LOG_INFO_CAT("PIPELINE", "SBT already forged — skipping recreation");
        return;
    }

    VkPipeline rtPipe = rtx().rt_pipeline;
    vkh.checker(rtPipe != VK_NULL_HANDLE, "Ray tracing pipeline existence",
                "No ray-tracing pipeline exists — cannot forge SBT");

    VkCommandPool cmdPool = pool ? pool : rtx().transient_pool;
    vkh.checker(cmdPool != VK_NULL_HANDLE, "Command pool for SBT upload",
                "No transient command pool available for SBT upload");

    cache_ray_tracing_properties();
    const auto& rtProps = rtx().rt_props;

    const VkDeviceSize handleSize  = rtProps.shaderGroupHandleSize;
    const VkDeviceSize handleAlign = rtProps.shaderGroupHandleAlignment;
    const VkDeviceSize baseAlign   = rtProps.shaderGroupBaseAlignment;

    const VkDeviceSize recordStride = Memory::align_up(handleSize, std::max(handleAlign, baseAlign));

    const uint32_t totalGroups = rtx().raygen_group_count + rtx().miss_group_count + rtx().hit_group_count;

    const VkDeviceSize raygenSize = recordStride;
    const VkDeviceSize missSize   = rtx().miss_group_count * recordStride;
    const VkDeviceSize hitSize    = rtx().hit_group_count * recordStride;

    VkDeviceSize sbtSize = raygenSize + missSize + hitSize;
    sbtSize = Memory::align_up(sbtSize, baseAlign);

    LOG_ATTEMPT_CAT("PIPELINE", "Forging eternal shader binding table — total size {} bytes ({} groups)", sbtSize, totalGroups);

    uint64_t sbt_handle = Memory::create(sbtSize,
                                         VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         "EternalSBT");

    vkh.checker(sbt_handle != 0, "Memory::create (EternalSBT)",
                "Failed to allocate eternal SBT buffer");

    VkDeviceAddress sbtAddr = Memory::getDeviceAddress(sbt_handle);

    std::vector<uint8_t> handles(totalGroups * handleSize);
    vkh.checker(
        ext().vkGetRayTracingShaderGroupHandlesKHR(
            rtx().device, rtPipe, 0, totalGroups, handles.size(), handles.data()),
        "vkGetRayTracingShaderGroupHandlesKHR",
        "Failed to retrieve shader group handles for SBT"
    );

    VkCommandBuffer cmd = providedCmd;
    bool ownCmd = (providedCmd == VK_NULL_HANDLE);

    if (ownCmd) {
        LOG_INFO_CAT("PIPELINE", "No provided cmd buffer — allocating one-time command buffer for SBT upload");

        VkCommandBufferAllocateInfo alloc{};
        alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool        = cmdPool;
        alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        vkh.checker(
            vkAllocateCommandBuffers(rtx().device, &alloc, &cmd),
            "vkAllocateCommandBuffers (temp SBT upload)",
            "Failed to allocate temp cmd buffer for SBT upload"
        );

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkh.checker(
            vkBeginCommandBuffer(cmd, &begin),
            "vkBeginCommandBuffer (SBT upload)",
            "Failed to begin temp cmd buffer for SBT"
        );
    }

    // Upload shader group handles to SBT buffer
    Memory::uploadToBuffer(sbt_handle, handles.data(), handles.size(), cmd);

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);

    if (ownCmd) {
        vkh.checker(
            vkEndCommandBuffer(cmd),
            "vkEndCommandBuffer (SBT upload)",
            "Failed to end temp cmd buffer for SBT"
        );

        VkSubmitInfo submit{};
        submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &cmd;

        vkh.checker(
            vkQueueSubmit(queue ? queue : rtx().graphics_queue, 1, &submit, VK_NULL_HANDLE),
            "vkQueueSubmit (SBT upload)",
            "vkQueueSubmit failed for SBT upload"
        );

        vkQueueWaitIdle(queue ? queue : rtx().graphics_queue);

        vkFreeCommandBuffers(rtx().device, cmdPool, 1, &cmd);
    }

    VkDeviceAddress raygenAddr = Memory::align_up(sbtAddr, rtProps.shaderGroupBaseAlignment);
    VkDeviceAddress missAddr   = Memory::align_up(raygenAddr + raygenSize, rtProps.shaderGroupBaseAlignment);
    VkDeviceAddress hitAddr    = Memory::align_up(missAddr + missSize, rtProps.shaderGroupBaseAlignment);

    rtx().sbt_address = sbtAddr;
    rtx().sbt_size = sbtSize;

    rtx().raygen_sbt_region = {raygenAddr, recordStride, raygenSize};
    rtx().miss_sbt_region   = {missAddr,   recordStride, missSize};
    rtx().hit_sbt_region    = {hitAddr,    recordStride, hitSize};

    rtx().eternal_sbt_forged = true;

    LOG_SUCCESS_CAT("PIPELINE", "Eternal shader binding table forged successfully — size {} bytes, address 0x{}",
                    sbtSize, sbtAddr);
}

inline void pipeline_dispatch_living_world(VkCommandBuffer cmd, float totalTime) noexcept {
    if (rtx().compute_pipeline == VK_NULL_HANDLE) {
        pipeline_create_compute_pipeline();
    }

    vkh.checker(rtx().compute_pipeline != VK_NULL_HANDLE, "Compute pipeline existence",
                "No compute pipeline available — skipping living world dispatch");

    LOG_INFO_CAT("PIPELINE", "Dispatching living world compute — time {}s", totalTime);

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

    LOG_SUCCESS_CAT("PIPELINE", "Living world dispatch recorded");
}

void pipeline_trace_rays(VkCommandBuffer cmd, uint32_t width, uint32_t height) noexcept {
    vkh.checker(rtx().eternal_sbt_forged && rtx().sbt_address != 0,
                "Eternal SBT readiness",
                "Cannot trace rays — eternal SBT not forged or invalid address");

    vkh.checker(rtx().raygen_sbt_region.size != 0 &&
                rtx().miss_sbt_region.size != 0 &&
                rtx().hit_sbt_region.size != 0,
                "SBT region validity",
                "Invalid SBT regions — raygen/miss/hit sizes are zero");

    LOG_ATTEMPT_CAT("PIPELINE", "Tracing rays — resolution {}x{}", width, height);

    VkStridedDeviceAddressRegionKHR callableRegion{};

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtx().rt_pipeline);

    ext().vkCmdTraceRaysKHR(
        cmd,
        &rtx().raygen_sbt_region,
        &rtx().miss_sbt_region,
        &rtx().hit_sbt_region,
        &callableRegion,
        width,
        height,
        1
    );

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing dispatch recorded");
}

inline void pipeline_write_rt_descriptors(const RTDescriptorUpdate& update) noexcept {
    LOG_ATTEMPT_CAT("PIPELINE", "Updating global ray-tracing descriptor buffer");

    uint64_t desc_handle = rtx().descriptor_buffer_handle;
    if (desc_handle == 0) {
        constexpr VkDeviceSize INITIAL = 4096ULL * 4;
        desc_handle = Memory::create(INITIAL, VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT, "EternalDescriptorBuffer");

        vkh.checker(desc_handle != 0, "Memory::create (EternalDescriptorBuffer)",
                    "Failed to create eternal descriptor buffer");

        rtx().descriptor_buffer_handle = desc_handle;
        rtx().descriptor_buffer_address = Memory::getDeviceAddress(desc_handle);
    }

    void* mapped = rtx().descriptor_mapped;
    if (!mapped) {
        auto it = rtx().buffers.find(desc_handle);
        vkh.checker(it != rtx().buffers.end(), "Descriptor buffer lookup",
                    "Descriptor buffer handle invalid");

        BufferInfo& info = it->second;

        if (info.mapped == nullptr) {
            VkResult res = vkMapMemory(rtx().device, info.memory, 0, info.size, 0, &info.mapped);
            vkh.checker(res == VK_SUCCESS, "vkMapMemory (descriptor buffer)",
                        std::format("Failed to map descriptor buffer: {}", vkh.result(res)).c_str());
        }
        mapped = info.mapped;
        rtx().descriptor_mapped = mapped;
    }

    vkh.checker(update.tlas != VK_NULL_HANDLE, "TLAS validity (descriptor update)",
                "No TLAS provided — skipping descriptor update");

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
        VkBuffer buf = Memory::getBuffer(rtx().living_world_buffer_handle);

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

    LOG_SUCCESS_CAT("PIPELINE", "Ray-tracing descriptor buffer updated successfully");
}

inline void pipeline_shutdown() noexcept {
    LOG_ATTEMPT_CAT("PIPELINE", "Shutting down pipeline subsystem");

    void* mapped = rtx().descriptor_mapped;
    if (mapped != nullptr) {
        auto it = rtx().buffers.find(rtx().descriptor_buffer_handle);
        if (it != rtx().buffers.end()) {
            BufferInfo& info = it->second;
            vkUnmapMemory(rtx().device, info.memory);
            LOG_INFO_CAT("PIPELINE", "Descriptor buffer unmapped");
        }
    }

    Memory::destroy(rtx().descriptor_buffer_handle);
    Memory::destroy(rtx().living_world_buffer_handle);

    if (auto l = rtx().main_descriptor_layout) {
        vkDestroyDescriptorSetLayout(rtx().device, l, nullptr);
        LOG_INFO_CAT("PIPELINE", "Main descriptor set layout destroyed");
    }
    if (auto l = rtx().tex_descriptor_layout) {
        vkDestroyDescriptorSetLayout(rtx().device, l, nullptr);
        LOG_INFO_CAT("PIPELINE", "Texture descriptor set layout destroyed");
    }
    if (auto l = rtx().empty_descriptor_layout) {
        vkDestroyDescriptorSetLayout(rtx().device, l, nullptr);
        LOG_INFO_CAT("PIPELINE", "Empty descriptor set layout destroyed");
    }

    if (auto t = rtx().dummy_tlas) {
        ext().vkDestroyAccelerationStructureKHR(rtx().device, t, nullptr);
        LOG_INFO_CAT("PIPELINE", "Dummy TLAS destroyed");
    }
    if (auto b = rtx().dummy_accel_buffer) {
        vkDestroyBuffer(rtx().device, b, nullptr);
        LOG_INFO_CAT("PIPELINE", "Dummy TLAS buffer destroyed");
    }
    if (auto m = rtx().dummy_accel_memory) {
        vkFreeMemory(rtx().device, m, nullptr);
        LOG_INFO_CAT("PIPELINE", "Dummy TLAS memory freed");
    }

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline subsystem shutdown complete");
}