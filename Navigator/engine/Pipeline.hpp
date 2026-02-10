#pragma once

// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"

#include <algorithm>
#include <array>
#include <vector>
#include <fstream>

// Main descriptor bindings — fixed 9 slots (no auto, explicit)
// Note: binding 5 was missing in original → added placeholder if needed
static constexpr VkDescriptorSetLayoutBinding kMainBindings[9] = {
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
    {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr},
    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr},
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr},
    {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr}, // placeholder / unused
    {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
    {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
    {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr}
};

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
// Helpers
// =============================================================================

inline VkShaderModule load_shader(const std::string& relativePath) noexcept {
    LOG_ATTEMPT_CAT("PIPELINE", "Loading shader: {}", relativePath);

    std::array<std::string, 3> paths = {
        std::format("build/bin/Linux/{}", relativePath),
        std::format("build/bin/Windows/{}", relativePath),
        std::format("assets/shaders/{}", relativePath)
    };

    std::ifstream file;
    std::string usedPath;

    for (size_t i = 0; i < paths.size(); ++i) {
        file.open(paths[i], std::ios::ate | std::ios::binary);
        if (file.is_open()) {
            usedPath = paths[i];
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
    file.read(code.data(), static_cast<std::streamsize>(size));
    file.close();

    VkShaderModuleCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size;
    info.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    vkh.checker(
        vkCreateShaderModule(rtx().device, &info, nullptr, &module),
        "vkCreateShaderModule",
        std::format("{} ({} bytes)", relativePath, size).c_str()
    );

    LOG_SUCCESS_CAT("PIPELINE", "Shader loaded — {} ({} bytes) from {}", relativePath, size, usedPath);
    return module;
}

inline VkAccelerationStructureKHR create_dummy_tlas() noexcept {
    LOG_ATTEMPT_CAT("PIPELINE", "Creating dummy TLAS");

    vkh.checker(rtx().transient_pool != VK_NULL_HANDLE, "Transient pool", "Missing");

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType                                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType                          = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.flags                                 = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.instances.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers    = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress = 0; // dummy → no real instances

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    uint32_t primitiveCount = 1; // dummy count
    ext().vkGetAccelerationStructureBuildSizesKHR(
        rtx().device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primitiveCount, &sizeInfo);

    uint64_t buffer_handle = Memory::create(sizeInfo.accelerationStructureSize,
                                            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                            "Dummy_TLAS_Buffer");

    vkh.checker(buffer_handle != 0, "Memory::create (Dummy TLAS buffer)", "Failed");

    VkBuffer buffer = Memory::getBuffer(buffer_handle);
    vkh.checker(buffer != VK_NULL_HANDLE, "Memory::getBuffer (Dummy TLAS)", "Invalid");

    rtx().dummy_accel_buffer = buffer;

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = buffer;
    createInfo.size   = sizeInfo.accelerationStructureSize;
    createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkAccelerationStructureKHR as = VK_NULL_HANDLE;
    vkh.checker(
        ext().vkCreateAccelerationStructureKHR(rtx().device, &createInfo, nullptr, &as),
        "vkCreateAccelerationStructureKHR (dummy TLAS)",
        "Failed"
    );

    rtx().dummy_tlas = as;

    LOG_SUCCESS_CAT("PIPELINE", "Dummy TLAS created — {:016x}", (uintptr_t)as);
    return as;
}

inline void cache_descriptor_properties() noexcept {
    if (rtx().descriptor_props_cached) return;

    VkPhysicalDeviceDescriptorBufferPropertiesEXT props{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;

    VkPhysicalDeviceProperties2 pdProps2{};
    pdProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    pdProps2.pNext = &props;

    vkGetPhysicalDeviceProperties2(rtx().physical, &pdProps2);

    rtx().descriptor_props = props;
    rtx().descriptor_props_cached = true;
}

inline void cache_ray_tracing_properties() noexcept {
    if (rtx().rt_props_cached) return;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR props{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 pdProps2{};
    pdProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    pdProps2.pNext = &props;

    vkGetPhysicalDeviceProperties2(rtx().physical, &pdProps2);

    rtx().rt_props = props;
    rtx().rt_props_cached = true;
}

// =============================================================================
// Main pipeline functions
// =============================================================================

inline void pipeline_initialize() noexcept {
    cache_descriptor_properties();
    cache_ray_tracing_properties();

    rtx().dummy_tlas = create_dummy_tlas();

    uint64_t lw_handle = Memory::create(64,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                        "LivingWorldBuffer");

    rtx().living_world_buffer_handle = lw_handle;
}

inline void pipeline_create_pipeline_layout() noexcept {
    if (rtx().pipeline_layout != VK_NULL_HANDLE) return;

    VkDescriptorSetLayoutCreateInfo mainInfo{};
    mainInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    mainInfo.bindingCount = 9;
    mainInfo.pBindings    = kMainBindings;
    mainInfo.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    vkh.checker(
        vkCreateDescriptorSetLayout(rtx().device, &mainInfo, nullptr, &mainLayout),
        "vkCreateDescriptorSetLayout (main)",
        "Failed"
    );
    rtx().main_descriptor_layout = mainLayout;

    for (uint32_t i = 0; i < 9; ++i) {
        ext().vkGetDescriptorSetLayoutBindingOffsetEXT(rtx().device, mainLayout, i, &rtx().binding_offsets[i]);
    }

    VkDescriptorSetLayoutBinding texBinding{};
    texBinding.binding         = 0;
    texBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    texBinding.descriptorCount = 1024; // adjust to your actual max textures
    texBinding.stageFlags      = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo texInfo{};
    texInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    texInfo.bindingCount = 1;
    texInfo.pBindings    = &texBinding;
    texInfo.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout texLayout = VK_NULL_HANDLE;
    vkh.checker(
        vkCreateDescriptorSetLayout(rtx().device, &texInfo, nullptr, &texLayout),
        "vkCreateDescriptorSetLayout (texture)",
        "Failed"
    );
    rtx().tex_descriptor_layout = texLayout;

    VkDescriptorSetLayoutCreateInfo emptyInfo{};
    emptyInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    emptyInfo.bindingCount = 0;
    emptyInfo.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    vkh.checker(
        vkCreateDescriptorSetLayout(rtx().device, &emptyInfo, nullptr, &emptyLayout),
        "vkCreateDescriptorSetLayout (empty)",
        "Failed"
    );
    rtx().empty_descriptor_layout = emptyLayout;

    const VkDescriptorSetLayout layouts[4] = {
        mainLayout, emptyLayout, texLayout, emptyLayout
    };

    VkPushConstantRange push{};
    push.stageFlags = FULL_PUSH_MASK;
    push.offset     = 0;
    push.size       = sizeof(float);

    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount         = 4;
    plInfo.pSetLayouts            = layouts;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges    = &push;

    VkPipelineLayout pl = VK_NULL_HANDLE;
    vkh.checker(
        vkCreatePipelineLayout(rtx().device, &plInfo, nullptr, &pl),
        "vkCreatePipelineLayout",
        "Failed"
    );

    rtx().pipeline_layout = pl;
}

inline void pipeline_create_compute_pipeline() noexcept {
    if (rtx().compute_pipeline != VK_NULL_HANDLE) return;

    VkShaderModule compModule = load_shader("assets/shaders/compute/living_world.spv");
    vkh.checker(compModule != VK_NULL_HANDLE, "load_shader (living_world.spv)", "Failed");

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = compModule;
    stage.pName  = "main";

    VkComputePipelineCreateInfo compInfo{};
    compInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compInfo.stage  = stage;
    compInfo.layout = rtx().pipeline_layout;
    compInfo.flags  = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkPipeline compPipe = VK_NULL_HANDLE;
    vkh.checker(
        vkCreateComputePipelines(rtx().device, VK_NULL_HANDLE, 1, &compInfo, nullptr, &compPipe),
        "vkCreateComputePipelines",
        "Failed"
    );

    rtx().compute_pipeline = compPipe;
    vkDestroyShaderModule(rtx().device, compModule, nullptr);
}

inline void pipeline_create_ray_tracing_pipeline() noexcept {
    if (rtx().rt_pipeline != VK_NULL_HANDLE) return;

    auto load = [](const char* path) -> VkShaderModule {
        return load_shader(std::string(path));
    };

    VkShaderModule raygen = load("assets/shaders/raytracing/raygen.spv");
    VkShaderModule miss   = load("assets/shaders/raytracing/miss.spv");
    VkShaderModule chit   = load("assets/shaders/raytracing/closest_hit.spv");
    VkShaderModule ahit   = load("assets/shaders/raytracing/anyhit.spv");

    vkh.checker(raygen && miss && chit && ahit, "Shader loading", "One or more ray tracing shaders failed to load");

    VkPipelineShaderStageCreateInfo stages[4]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[0].module = raygen;
    stages[0].pName  = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[1].module = miss;
    stages[1].pName  = "main";

    stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stages[2].module = chit;
    stages[2].pName  = "main";

    stages[3].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[3].stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    stages[3].module = ahit;
    stages[3].pName  = "main";

    // Corrected shader group setup
    VkRayTracingShaderGroupCreateInfoKHR groups[3]{};

    // Group 0: Raygen (GENERAL)
    groups[0].sType                         = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[0].type                          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].generalShader                 = 0; // raygen
    groups[0].closestHitShader              = VK_SHADER_UNUSED_KHR;
    groups[0].anyHitShader                  = VK_SHADER_UNUSED_KHR;
    groups[0].intersectionShader            = VK_SHADER_UNUSED_KHR;

    // Group 1: Miss (GENERAL)
    groups[1].sType                         = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[1].type                          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[1].generalShader                 = 1; // miss
    groups[1].closestHitShader              = VK_SHADER_UNUSED_KHR;
    groups[1].anyHitShader                  = VK_SHADER_UNUSED_KHR;
    groups[1].intersectionShader            = VK_SHADER_UNUSED_KHR;

    // Group 2: Hit group (TRIANGLES_HIT_GROUP)
    groups[2].sType                         = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[2].type                          = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[2].generalShader                 = VK_SHADER_UNUSED_KHR;
    groups[2].closestHitShader              = 2; // closest hit
    groups[2].anyHitShader                  = 3;  // any hit
    groups[2].intersectionShader            = VK_SHADER_UNUSED_KHR;

    rtx().raygen_group_count = 1;
    rtx().miss_group_count   = 1;
    rtx().hit_group_count    = 1;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType                      = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount                 = 4;
    pipelineInfo.pStages                    = stages;
    pipelineInfo.groupCount                 = 3;
    pipelineInfo.pGroups                    = groups;
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout                     = rtx().pipeline_layout;
    pipelineInfo.flags                      = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkPipeline pipeline = VK_NULL_HANDLE;
    vkh.checker(
        ext().vkCreateRayTracingPipelinesKHR(
            rtx().device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline),
        "vkCreateRayTracingPipelinesKHR",
        "Failed"
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
                                                 VkCommandBuffer providedCmd = VK_NULL_HANDLE) noexcept {
    if (rtx().eternal_sbt_forged) return;

    VkPipeline rtPipe = rtx().rt_pipeline;
    vkh.checker(rtPipe != VK_NULL_HANDLE, "Ray tracing pipeline", "Missing");

    VkCommandPool cmdPool = pool ? pool : rtx().transient_pool;
    vkh.checker(cmdPool != VK_NULL_HANDLE, "Command pool for SBT", "Missing");

    cache_ray_tracing_properties();
    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtProps = rtx().rt_props;

    const VkDeviceSize handleSize  = rtProps.shaderGroupHandleSize;
    const VkDeviceSize handleAlign = rtProps.shaderGroupHandleAlignment;
    const VkDeviceSize baseAlign   = rtProps.shaderGroupBaseAlignment;

    const VkDeviceSize recordStride = Memory::align_up(handleSize, std::max(handleAlign, baseAlign));

    const uint32_t totalGroups = rtx().raygen_group_count + rtx().miss_group_count + rtx().hit_group_count;

    const VkDeviceSize raygenSize = rtx().raygen_group_count * recordStride;
    const VkDeviceSize missSize   = rtx().miss_group_count   * recordStride;
    const VkDeviceSize hitSize    = rtx().hit_group_count    * recordStride;

    VkDeviceSize sbtSize = raygenSize + missSize + hitSize;
    sbtSize = Memory::align_up(sbtSize, baseAlign);

    uint64_t sbt_handle = Memory::create(sbtSize,
                                         VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         "EternalSBT");

    vkh.checker(sbt_handle != 0, "Memory::create (EternalSBT)", "Failed");

    VkDeviceAddress sbtAddr = Memory::getDeviceAddress(sbt_handle);

    std::vector<uint8_t> handles(totalGroups * handleSize);
    vkh.checker(
        ext().vkGetRayTracingShaderGroupHandlesKHR(
            rtx().device, rtPipe, 0, totalGroups, handles.size(), handles.data()),
        "vkGetRayTracingShaderGroupHandlesKHR",
        "Failed"
    );

    VkCommandBuffer cmd = providedCmd;
    bool ownCmd = (providedCmd == VK_NULL_HANDLE);

    if (ownCmd) {
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool        = cmdPool;
        alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        vkh.checker(
            vkAllocateCommandBuffers(rtx().device, &alloc, &cmd),
            "vkAllocateCommandBuffers (SBT)",
            "Failed"
        );

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkh.checker(
            vkBeginCommandBuffer(cmd, &begin),
            "vkBeginCommandBuffer (SBT)",
            "Failed"
        );
    }

    auto [stgBuf, stgMem] = Memory::uploadToBuffer(sbt_handle, handles.data(), handles.size(), cmd);

    VkMemoryBarrier barrier{};
    barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);

    if (ownCmd) {
        vkh.checker(
            vkEndCommandBuffer(cmd),
            "vkEndCommandBuffer (SBT)",
            "Failed"
        );

        VkSubmitInfo submit{};
        submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &cmd;

        vkh.checker(
            vkQueueSubmit(queue ? queue : rtx().graphics_queue, 1, &submit, VK_NULL_HANDLE),
            "vkQueueSubmit (SBT)",
            "Failed"
        );

        vkQueueWaitIdle(queue ? queue : rtx().graphics_queue);

        if (stgBuf != VK_NULL_HANDLE) {
            vkDestroyBuffer(rtx().device, stgBuf, nullptr);
            vkFreeMemory(rtx().device, stgMem, nullptr);
        }

        vkFreeCommandBuffers(rtx().device, cmdPool, 1, &cmd);
    } else {
        if (stgBuf != VK_NULL_HANDLE) {
            LOG_WARNING_CAT("PIPELINE", "External SBT upload — caller must destroy staging after wait: buf={:016x}, mem={:016x}", 
                            (uintptr_t)stgBuf, (uintptr_t)stgMem);
        }
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

    LOG_SUCCESS_CAT("PIPELINE", "Shader binding table created — size={} bytes", sbtSize);
}

inline void pipeline_dispatch_living_world(VkCommandBuffer cmd, float totalTime) noexcept {
    if (rtx().compute_pipeline == VK_NULL_HANDLE) {
        pipeline_create_compute_pipeline();
    }

    vkh.checker(rtx().compute_pipeline != VK_NULL_HANDLE, "Compute pipeline", "Missing");

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rtx().compute_pipeline);

    VkDescriptorBufferBindingInfoEXT bind{};
    bind.sType  = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    bind.address = rtx().descriptor_buffer_address;
    bind.usage  = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    ext().vkCmdBindDescriptorBuffersEXT(cmd, 1, &bind);

    uint32_t idx = 0;
    VkDeviceSize off = 0;
    ext().vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                             rtx().pipeline_layout, 0, 1, &idx, &off);

    vkCmdPushConstants(cmd, rtx().pipeline_layout, FULL_PUSH_MASK, 0, sizeof(float), &totalTime);

    vkCmdDispatch(cmd, 1, 1, 1);
}

inline void pipeline_trace_rays(VkCommandBuffer cmd, uint32_t width, uint32_t height) noexcept {
    vkh.checker(rtx().eternal_sbt_forged && rtx().sbt_address != 0,
                "SBT readiness", "SBT not forged");

    vkh.checker(rtx().raygen_sbt_region.size != 0 &&
                rtx().miss_sbt_region.size != 0 &&
                rtx().hit_sbt_region.size != 0,
                "SBT regions", "Invalid");

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtx().rt_pipeline);

    VkStridedDeviceAddressRegionKHR callableRegion{};

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
}

inline void pipeline_write_rt_descriptors(const RTDescriptorUpdate& update) noexcept {
    uint64_t desc_handle = rtx().descriptor_buffer_handle;
    if (desc_handle == 0) {
        constexpr VkDeviceSize INITIAL = 4096ULL * 4;
        desc_handle = Memory::create(INITIAL, VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT, "EternalDescriptorBuffer");

        rtx().descriptor_buffer_handle = desc_handle;
        rtx().descriptor_buffer_address = Memory::getDeviceAddress(desc_handle);
    }

    void* mapped = rtx().descriptor_mapped;
    if (mapped == nullptr) {
        std::unordered_map<uint64_t, BufferInfo>::iterator it = rtx().buffers.find(desc_handle);
        if (it == rtx().buffers.end()) return;

        BufferInfo& info = it->second;

        if (info.mapped == nullptr) {
            vkMapMemory(rtx().device, info.memory, 0, info.size, 0, &info.mapped);
        }
        mapped = info.mapped;
        rtx().descriptor_mapped = mapped;
    }

    uint8_t* base = static_cast<uint8_t*>(mapped);
    const VkPhysicalDeviceDescriptorBufferPropertiesEXT& props = rtx().descriptor_props;

    // TLAS (binding 0)
    if (update.tlas != VK_NULL_HANDLE) {
        VkAccelerationStructureDeviceAddressInfoKHR info{};
        info.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        info.accelerationStructure = update.tlas;
        VkDeviceAddress addr = ext().vkGetAccelerationStructureDeviceAddressKHR(rtx().device, &info);

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        get.data.accelerationStructure = addr;

        ext().vkGetDescriptorEXT(rtx().device, &get, props.accelerationStructureDescriptorSize,
                                 base + rtx().binding_offsets[0]);
    }

    // Output image (binding 1)
    if (update.rtOutputView != VK_NULL_HANDLE) {
        VkDescriptorImageInfo img{};
        img.imageView   = update.rtOutputView;
        img.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        get.data.pStorageImage = &img;

        ext().vkGetDescriptorEXT(rtx().device, &get, props.storageImageDescriptorSize,
                                 base + rtx().binding_offsets[1]);
    }

    // UBO (binding 2)
    if (update.ubo != VK_NULL_HANDLE && update.uboSize != 0) {
        VkBufferDeviceAddressInfo info{};
        info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = update.ubo;
        VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(rtx().device, &info);

        VkDescriptorAddressInfoEXT descAddr{};
        descAddr.sType  = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        descAddr.address = addr;
        descAddr.range  = update.uboSize;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        get.data.pUniformBuffer = &descAddr;

        ext().vkGetDescriptorEXT(rtx().device, &get, props.uniformBufferDescriptorSize,
                                 base + rtx().binding_offsets[2]);
    }

    // Materials (binding 3)
    if (update.materialsBuffer != VK_NULL_HANDLE && update.materialsSize != 0) {
        VkBufferDeviceAddressInfo info{};
        info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = update.materialsBuffer;
        VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(rtx().device, &info);

        VkDescriptorAddressInfoEXT descAddr{};
        descAddr.sType  = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        descAddr.address = addr;
        descAddr.range  = update.materialsSize;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        get.data.pStorageBuffer = &descAddr;

        ext().vkGetDescriptorEXT(rtx().device, &get, props.storageBufferDescriptorSize,
                                 base + rtx().binding_offsets[3]);
    }

    // Living world buffer (binding 7)
    if (rtx().living_world_buffer_handle != 0) {
        VkBuffer buf = Memory::getBuffer(rtx().living_world_buffer_handle);

        VkBufferDeviceAddressInfo info{};
        info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = buf;
        VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(rtx().device, &info);

        VkDescriptorAddressInfoEXT descAddr{};
        descAddr.sType  = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        descAddr.address = addr;
        descAddr.range  = 64;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        get.data.pStorageBuffer = &descAddr;

        ext().vkGetDescriptorEXT(rtx().device, &get, props.storageBufferDescriptorSize,
                                 base + rtx().binding_offsets[7]);
    }
}

inline void pipeline_shutdown() noexcept {
    void* mapped = rtx().descriptor_mapped;
    if (mapped != nullptr) {
        std::unordered_map<uint64_t, BufferInfo>::iterator it = rtx().buffers.find(rtx().descriptor_buffer_handle);
        if (it != rtx().buffers.end()) {
            vkUnmapMemory(rtx().device, it->second.memory);
        }
        rtx().descriptor_mapped = nullptr;
    }

    Memory::destroy(rtx().descriptor_buffer_handle);
    Memory::destroy(rtx().living_world_buffer_handle);

    if (rtx().main_descriptor_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtx().device, rtx().main_descriptor_layout, nullptr);
        rtx().main_descriptor_layout = VK_NULL_HANDLE;
    }
    if (rtx().tex_descriptor_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtx().device, rtx().tex_descriptor_layout, nullptr);
        rtx().tex_descriptor_layout = VK_NULL_HANDLE;
    }
    if (rtx().empty_descriptor_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtx().device, rtx().empty_descriptor_layout, nullptr);
        rtx().empty_descriptor_layout = VK_NULL_HANDLE;
    }

    if (rtx().rt_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtx().device, rtx().rt_pipeline, nullptr);
        rtx().rt_pipeline = VK_NULL_HANDLE;
    }
    if (rtx().compute_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtx().device, rtx().compute_pipeline, nullptr);
        rtx().compute_pipeline = VK_NULL_HANDLE;
    }
    if (rtx().pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rtx().device, rtx().pipeline_layout, nullptr);
        rtx().pipeline_layout = VK_NULL_HANDLE;
    }

    if (rtx().dummy_tlas != VK_NULL_HANDLE) {
        ext().vkDestroyAccelerationStructureKHR(rtx().device, rtx().dummy_tlas, nullptr);
        rtx().dummy_tlas = VK_NULL_HANDLE;
    }
    if (rtx().dummy_accel_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(rtx().device, rtx().dummy_accel_buffer, nullptr);
        rtx().dummy_accel_buffer = VK_NULL_HANDLE;
    }
    // Note: dummy_accel_memory should be managed by Memory::destroy if tracked
}