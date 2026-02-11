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
#include <format>

// Fixed descriptor bindings (9 explicit slots for descriptor buffer)
// All stages that may read primitives/TLAS include INTERSECTION
static constexpr VkDescriptorSetLayoutBinding kMainBindings[9] = {
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR, nullptr},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
    {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR, nullptr},
    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR, nullptr},
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR, nullptr},
    {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR, nullptr}, // reserved
    {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
    {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR, nullptr},
    {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR, nullptr}
};

static constexpr VkShaderStageFlags FULL_PUSH_MASK =
    VK_SHADER_STAGE_COMPUTE_BIT |
    VK_SHADER_STAGE_RAYGEN_BIT_KHR |
    VK_SHADER_STAGE_MISS_BIT_KHR |
    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
    VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
    VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

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

inline VkShaderModule load_shader(const std::string& path) noexcept {
    LOG_ATTEMPT_CAT("PIPELINE", "Loading shader: {}", path);

    std::array<std::string, 3> candidates = {
        std::format("build/bin/Linux/{}", path),
        std::format("build/bin/Windows/{}", path),
        std::format("assets/shaders/{}", path)
    };

    std::ifstream file;
    std::string found;

    for (const auto& candidate : candidates) {
        file.open(candidate, std::ios::ate | std::ios::binary);
        if (file.is_open()) {
            found = candidate;
            break;
        }
    }

    vkh.checker(file.is_open(), "Shader file open",
                std::format("Shader not found — checked:\n  {}\n  {}\n  {}", candidates[0], candidates[1], candidates[2]).c_str());

    size_t size = static_cast<size_t>(file.tellg());
    vkh.checker(size > 0 && size % 4 == 0, "SPIR-V validation",
                std::format("Invalid SPIR-V size ({} bytes) in {}", size, found).c_str());

    std::vector<char> code(size);
    file.seekg(0);
    file.read(code.data(), static_cast<std::streamsize>(size));
    file.close();

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = size;
    ci.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    vkh.checker(
        vkCreateShaderModule(rtx().device, &ci, nullptr, &module),
        "vkCreateShaderModule",
        std::format("{} ({} bytes)", path, size).c_str()
    );

    LOG_SUCCESS_CAT("PIPELINE", "Loaded shader: {} ({} bytes) from {}", path, size, found);
    return module;
}

inline VkAccelerationStructureKHR create_dummy_tlas() noexcept {
    LOG_ATTEMPT_CAT("PIPELINE", "Creating dummy top-level acceleration structure (empty)");

    vkh.checker(rtx().transient_pool != VK_NULL_HANDLE, "Transient pool", "Missing");

    VkAccelerationStructureGeometryKHR geom{};
    geom.sType                                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType                          = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geom.flags                                 = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geom.geometry.instances.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geom.geometry.instances.arrayOfPointers    = VK_FALSE;
    geom.geometry.instances.data.deviceAddress = 0;

    VkAccelerationStructureBuildGeometryInfoKHR build{};
    build.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    build.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build.geometryCount = 1;
    build.pGeometries   = &geom;

    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    uint32_t dummyCount = 1;
    ext().vkGetAccelerationStructureBuildSizesKHR(
        rtx().device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &build, &dummyCount, &sizes);

    uint64_t bufHandle = Memory::createBuffer(
        sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "Dummy_TLAS_Storage"
    );

    vkh.checker(bufHandle != 0, "Memory::createBuffer", "Dummy TLAS storage allocation failed");

    VkBuffer buf = Memory::getBuffer(bufHandle);
    vkh.checker(buf != VK_NULL_HANDLE, "Memory::getBuffer", "Dummy TLAS buffer invalid");

    rtx().dummy_accel_buffer = buf;

    VkAccelerationStructureCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    ci.buffer = buf;
    ci.size   = sizes.accelerationStructureSize;
    ci.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    vkh.checker(
        ext().vkCreateAccelerationStructureKHR(rtx().device, &ci, nullptr, &tlas),
        "vkCreateAccelerationStructureKHR",
        "Failed to create dummy TLAS"
    );

    rtx().dummy_tlas = tlas;

    LOG_SUCCESS_CAT("PIPELINE", "Dummy TLAS created — handle {:016x}", (uintptr_t)tlas);
    return tlas;
}

inline void cache_descriptor_properties() noexcept {
    if (rtx().descriptor_props_cached) return;

    VkPhysicalDeviceDescriptorBufferPropertiesEXT props{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;

    VkPhysicalDeviceProperties2 pdProps{};
    pdProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    pdProps.pNext = &props;

    vkGetPhysicalDeviceProperties2(rtx().physical, &pdProps);

    rtx().descriptor_props = props;
    rtx().descriptor_props_cached = true;
}

inline void cache_ray_tracing_properties() noexcept {
    if (rtx().rt_props_cached) return;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR props{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 pdProps{};
    pdProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    pdProps.pNext = &props;

    vkGetPhysicalDeviceProperties2(rtx().physical, &pdProps);

    rtx().rt_props = props;
    rtx().rt_props_cached = true;
}

// =============================================================================
// Core Pipeline Setup
// =============================================================================

inline void pipeline_initialize() noexcept {
    cache_descriptor_properties();
    cache_ray_tracing_properties();

    rtx().dummy_tlas = create_dummy_tlas();

    uint64_t lwHandle = Memory::createBuffer(
        64,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "LivingWorldBuffer"
    );

    rtx().living_world_buffer_handle = lwHandle;

    LOG_SUCCESS_CAT("PIPELINE", "Core initialized — dummy TLAS and living world buffer ready");
}

inline void pipeline_create_pipeline_layout() noexcept {
    if (rtx().pipeline_layout != VK_NULL_HANDLE) return;

    VkDescriptorSetLayoutCreateInfo mainCI{};
    mainCI.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    mainCI.bindingCount = 9;
    mainCI.pBindings    = kMainBindings;
    mainCI.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    vkh.checker(vkCreateDescriptorSetLayout(rtx().device, &mainCI, nullptr, &mainLayout),
                "vkCreateDescriptorSetLayout (main)", "Failed");

    rtx().main_descriptor_layout = mainLayout;

    for (uint32_t i = 0; i < 9; ++i) {
        ext().vkGetDescriptorSetLayoutBindingOffsetEXT(rtx().device, mainLayout, i, &rtx().binding_offsets[i]);
    }

    VkDescriptorSetLayoutBinding texBind{};
    texBind.binding         = 0;
    texBind.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    texBind.descriptorCount = 1024;
    texBind.stageFlags      = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo texCI{};
    texCI.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    texCI.bindingCount = 1;
    texCI.pBindings    = &texBind;
    texCI.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout texLayout = VK_NULL_HANDLE;
    vkh.checker(vkCreateDescriptorSetLayout(rtx().device, &texCI, nullptr, &texLayout),
                "vkCreateDescriptorSetLayout (textures)", "Failed");

    rtx().tex_descriptor_layout = texLayout;

    VkDescriptorSetLayoutCreateInfo emptyCI{};
    emptyCI.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    emptyCI.bindingCount = 0;
    emptyCI.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    vkh.checker(vkCreateDescriptorSetLayout(rtx().device, &emptyCI, nullptr, &emptyLayout),
                "vkCreateDescriptorSetLayout (empty)", "Failed");

    rtx().empty_descriptor_layout = emptyLayout;

    const VkDescriptorSetLayout layouts[4] = {mainLayout, emptyLayout, texLayout, emptyLayout};

    VkPushConstantRange push{};
    push.stageFlags = FULL_PUSH_MASK;
    push.offset     = 0;
    push.size       = sizeof(float);

    VkPipelineLayoutCreateInfo plCI{};
    plCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCI.setLayoutCount         = 4;
    plCI.pSetLayouts            = layouts;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges    = &push;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    vkh.checker(vkCreatePipelineLayout(rtx().device, &plCI, nullptr, &layout),
                "vkCreatePipelineLayout", "Failed");

    rtx().pipeline_layout = layout;

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created");
}

inline void pipeline_create_compute_pipeline() noexcept {
    if (rtx().compute_pipeline != VK_NULL_HANDLE) return;

    VkShaderModule comp = load_shader("assets/shaders/compute/living_world.spv");
    vkh.checker(comp != VK_NULL_HANDLE, "load_shader", "Living world compute shader missing");

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = comp;
    stage.pName  = "main";

    VkComputePipelineCreateInfo ci{};
    ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ci.stage  = stage;
    ci.layout = rtx().pipeline_layout;
    ci.flags  = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkPipeline pipe = VK_NULL_HANDLE;
    vkh.checker(vkCreateComputePipelines(rtx().device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe),
                "vkCreateComputePipelines", "Failed");

    rtx().compute_pipeline = pipe;
    vkDestroyShaderModule(rtx().device, comp, nullptr);

    LOG_SUCCESS_CAT("PIPELINE", "Living world compute pipeline created");
}

inline void pipeline_create_ray_tracing_pipeline() noexcept {
    if (rtx().rt_pipeline != VK_NULL_HANDLE) return;

    auto load = [](const char* p) { return load_shader(std::string(p)); };

    VkShaderModule rgen = load("assets/shaders/raytracing/raygen.spv");
    VkShaderModule miss = load("assets/shaders/raytracing/miss.spv");
    VkShaderModule intr = load("assets/shaders/raytracing/intersection.spv");
    VkShaderModule chit = load("assets/shaders/raytracing/closest_hit.spv");

    vkh.checker(rgen && miss && intr && chit, "Shader loading", "Procedural shaders missing");

    VkPipelineShaderStageCreateInfo stages[4] = {};

    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[0].module = rgen;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[1].module = miss;
    stages[1].pName  = "main";

    stages[2].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[2].stage  = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    stages[2].module = intr;
    stages[2].pName  = "main";

    stages[3].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[3].stage  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stages[3].module = chit;
    stages[3].pName  = "main";

    VkRayTracingShaderGroupCreateInfoKHR groups[3]{};

    // Raygen group (GENERAL)
    groups[0].sType             = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[0].type              = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].generalShader     = 0;
    groups[0].closestHitShader  = VK_SHADER_UNUSED_KHR;
    groups[0].anyHitShader      = VK_SHADER_UNUSED_KHR;
    groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

    // Miss group (GENERAL)
    groups[1].sType             = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[1].type              = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[1].generalShader     = 1;
    groups[1].closestHitShader  = VK_SHADER_UNUSED_KHR;
    groups[1].anyHitShader      = VK_SHADER_UNUSED_KHR;
    groups[1].intersectionShader = VK_SHADER_UNUSED_KHR;

    // Procedural hit group
    groups[2].sType             = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[2].type              = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
    groups[2].intersectionShader = 2;
    groups[2].closestHitShader   = 3;
    groups[2].anyHitShader       = VK_SHADER_UNUSED_KHR;

    rtx().raygen_group_count = 1;
    rtx().miss_group_count   = 1;
    rtx().hit_group_count    = 1;

    VkRayTracingPipelineCreateInfoKHR ci{};
    ci.sType                      = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    ci.stageCount                 = 4;
    ci.pStages                    = stages;
    ci.groupCount                 = 3;
    ci.pGroups                    = groups;
    ci.maxPipelineRayRecursionDepth = 1;
    ci.layout                     = rtx().pipeline_layout;
    ci.flags                      = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkPipeline pipe = VK_NULL_HANDLE;
    vkh.checker(
        ext().vkCreateRayTracingPipelinesKHR(rtx().device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe),
        "vkCreateRayTracingPipelinesKHR",
        "Failed to create procedural ray tracing pipeline"
    );

    vkDestroyShaderModule(rtx().device, rgen, nullptr);
    vkDestroyShaderModule(rtx().device, miss, nullptr);
    vkDestroyShaderModule(rtx().device, intr, nullptr);
    vkDestroyShaderModule(rtx().device, chit, nullptr);

    rtx().rt_pipeline = pipe;

    LOG_SUCCESS_CAT("PIPELINE", "Procedural ray tracing pipeline created");
}

inline void pipeline_create_shader_binding_table(VkCommandPool pool = VK_NULL_HANDLE,
                                                 VkQueue queue = VK_NULL_HANDLE,
                                                 VkCommandBuffer cmd = VK_NULL_HANDLE) noexcept {
    if (rtx().eternal_sbt_forged) return;

    VkPipeline rtPipe = rtx().rt_pipeline;
    vkh.checker(rtPipe != VK_NULL_HANDLE, "Ray tracing pipeline", "Missing");

    VkCommandPool cmdPool = pool ? pool : rtx().transient_pool;
    vkh.checker(cmdPool != VK_NULL_HANDLE, "Command pool", "Missing");

    cache_ray_tracing_properties();
    const auto& props = rtx().rt_props;

    const VkDeviceSize handleSize  = props.shaderGroupHandleSize;
    const VkDeviceSize alignHandle = props.shaderGroupHandleAlignment;
    const VkDeviceSize alignBase   = props.shaderGroupBaseAlignment;

    const VkDeviceSize stride = Memory::align_up(handleSize, std::max(alignHandle, alignBase));

    const uint32_t total = rtx().raygen_group_count + rtx().miss_group_count + rtx().hit_group_count;

    const VkDeviceSize raygenSize = rtx().raygen_group_count * stride;
    const VkDeviceSize missSize   = rtx().miss_group_count   * stride;
    const VkDeviceSize hitSize    = rtx().hit_group_count    * stride;

    VkDeviceSize totalSize = raygenSize + missSize + hitSize;
    totalSize = Memory::align_up(totalSize, alignBase);

    uint64_t sbtHandle = Memory::createBuffer(
        totalSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        "EternalSBT"
    );

    vkh.checker(sbtHandle != 0, "Memory::createBuffer", "SBT allocation failed");

    VkDeviceAddress sbtAddr = Memory::getDeviceAddress(sbtHandle);

    std::vector<uint8_t> handles(total * handleSize);
    vkh.checker(
        ext().vkGetRayTracingShaderGroupHandlesKHR(rtx().device, rtPipe, 0, total, handles.size(), handles.data()),
        "vkGetRayTracingShaderGroupHandlesKHR",
        "Failed to get shader group handles"
    );

    VkCommandBuffer localCmd = cmd;
    bool ownsCmd = (cmd == VK_NULL_HANDLE);

    if (ownsCmd) {
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool        = cmdPool;
        alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        vkh.checker(vkAllocateCommandBuffers(rtx().device, &alloc, &localCmd),
                    "vkAllocateCommandBuffers", "Failed");

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkh.checker(vkBeginCommandBuffer(localCmd, &begin),
                    "vkBeginCommandBuffer", "Failed");
    }

    auto [stagingBuf, stagingMem] = Memory::uploadToBuffer(sbtHandle, handles.data(), handles.size(), localCmd);

    VkMemoryBarrier barrier{};
    barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(localCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);

    if (ownsCmd) {
        vkh.checker(vkEndCommandBuffer(localCmd), "vkEndCommandBuffer", "Failed");

        VkSubmitInfo submit{};
        submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &localCmd;

        vkh.checker(vkQueueSubmit(queue ? queue : rtx().graphics_queue, 1, &submit, VK_NULL_HANDLE),
                    "vkQueueSubmit", "Failed");

        vkQueueWaitIdle(queue ? queue : rtx().graphics_queue);

        if (stagingBuf != VK_NULL_HANDLE) {
            vkDestroyBuffer(rtx().device, stagingBuf, nullptr);
            vkFreeMemory(rtx().device, stagingMem, nullptr);
        }

        vkFreeCommandBuffers(rtx().device, cmdPool, 1, &localCmd);
    } else if (stagingBuf != VK_NULL_HANDLE) {
        LOG_WARNING_CAT("PIPELINE", "External SBT staging pending — caller must clean up buf={:016x} / mem={:016x}",
                        (uintptr_t)stagingBuf, (uintptr_t)stagingMem);
    }

    VkDeviceAddress raygenAddr = Memory::align_up(sbtAddr, alignBase);
    VkDeviceAddress missAddr   = Memory::align_up(raygenAddr + raygenSize, alignBase);
    VkDeviceAddress hitAddr    = Memory::align_up(missAddr + missSize, alignBase);

    rtx().sbt_address = sbtAddr;
    rtx().sbt_size    = totalSize;

    rtx().raygen_sbt_region = {raygenAddr, stride, raygenSize};
    rtx().miss_sbt_region   = {missAddr,   stride, missSize};
    rtx().hit_sbt_region    = {hitAddr,    stride, hitSize};

    rtx().eternal_sbt_forged = true;

    LOG_SUCCESS_CAT("PIPELINE", "Shader binding table created — total size {} bytes", totalSize);
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
    VkDeviceSize offset = 0;
    ext().vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                             rtx().pipeline_layout, 0, 1, &idx, &offset);

    vkCmdPushConstants(cmd, rtx().pipeline_layout, FULL_PUSH_MASK, 0, sizeof(float), &totalTime);

    vkCmdDispatch(cmd, 1, 1, 1);

    LOG_DEBUG_CAT("PIPELINE", "Living world compute dispatched");
}

inline void pipeline_trace_rays(VkCommandBuffer cmd, uint32_t width, uint32_t height) noexcept {
    vkh.checker(rtx().eternal_sbt_forged && rtx().sbt_address != 0, "SBT", "Not ready");

    vkh.checker(rtx().raygen_sbt_region.size && rtx().miss_sbt_region.size && rtx().hit_sbt_region.size,
                "SBT regions", "Invalid sizes");

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtx().rt_pipeline);

    VkStridedDeviceAddressRegionKHR callable{};
    ext().vkCmdTraceRaysKHR(cmd,
                            &rtx().raygen_sbt_region,
                            &rtx().miss_sbt_region,
                            &rtx().hit_sbt_region,
                            &callable,
                            width, height, 1);

    LOG_DEBUG_CAT("PIPELINE", "Ray tracing dispatched — {}x{}", width, height);
}

inline void pipeline_write_rt_descriptors(const RTDescriptorUpdate& update) noexcept {
    uint64_t handle = rtx().descriptor_buffer_handle;
    if (handle == 0) {
        constexpr VkDeviceSize INITIAL = 4096ULL * 4;
        handle = Memory::createBuffer(
            INITIAL,
            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            "EternalDescriptorBuffer",
            Memory::MemoryHint::DescriptorBuffer
        );

        rtx().descriptor_buffer_handle = handle;
        rtx().descriptor_buffer_address = Memory::getDeviceAddress(handle);
    }

    void* mapped = rtx().descriptor_mapped;
    if (!mapped) {
        auto it = rtx().buffers.find(handle);
        if (it == rtx().buffers.end()) return;

        BufferInfo& info = it->second;
        if (!info.mapped) {
            vkMapMemory(rtx().device, info.memory, 0, info.size, 0, &info.mapped);
        }
        mapped = info.mapped;
        rtx().descriptor_mapped = mapped;
    }

    uint8_t* base = static_cast<uint8_t*>(mapped);
    const auto& props = rtx().descriptor_props;

    // TLAS (0)
    if (update.tlas) {
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

    // Output image (1)
    if (update.rtOutputView) {
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

    // UBO (2)
    if (update.ubo && update.uboSize) {
        VkBufferDeviceAddressInfo info{};
        info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = update.ubo;
        VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(rtx().device, &info);

        VkDescriptorAddressInfoEXT addrInfo{};
        addrInfo.sType  = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        addrInfo.address = addr;
        addrInfo.range  = update.uboSize;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        get.data.pUniformBuffer = &addrInfo;

        ext().vkGetDescriptorEXT(rtx().device, &get, props.uniformBufferDescriptorSize,
                                 base + rtx().binding_offsets[2]);
    }

    // Materials (3)
    if (update.materialsBuffer && update.materialsSize) {
        VkBufferDeviceAddressInfo info{};
        info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = update.materialsBuffer;
        VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(rtx().device, &info);

        VkDescriptorAddressInfoEXT addrInfo{};
        addrInfo.sType  = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        addrInfo.address = addr;
        addrInfo.range  = update.materialsSize;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        get.data.pStorageBuffer = &addrInfo;

        ext().vkGetDescriptorEXT(rtx().device, &get, props.storageBufferDescriptorSize,
                                 base + rtx().binding_offsets[3]);
    }

    // Living world (7)
    if (rtx().living_world_buffer_handle) {
        VkBuffer buf = Memory::getBuffer(rtx().living_world_buffer_handle);

        VkBufferDeviceAddressInfo info{};
        info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = buf;
        VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(rtx().device, &info);

        VkDescriptorAddressInfoEXT addrInfo{};
        addrInfo.sType  = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        addrInfo.address = addr;
        addrInfo.range  = 64;

        VkDescriptorGetInfoEXT get{};
        get.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get.type  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        get.data.pStorageBuffer = &addrInfo;

        ext().vkGetDescriptorEXT(rtx().device, &get, props.storageBufferDescriptorSize,
                                 base + rtx().binding_offsets[7]);
    }

    LOG_DEBUG_CAT("PIPELINE", "Global RT descriptors updated");
}

inline void pipeline_shutdown() noexcept {
    if (rtx().descriptor_mapped) {
        auto it = rtx().buffers.find(rtx().descriptor_buffer_handle);
        if (it != rtx().buffers.end()) {
            vkUnmapMemory(rtx().device, it->second.memory);
        }
        rtx().descriptor_mapped = nullptr;
    }

    Memory::destroy(rtx().descriptor_buffer_handle);
    Memory::destroy(rtx().living_world_buffer_handle);

    if (rtx().main_descriptor_layout) {
        vkDestroyDescriptorSetLayout(rtx().device, rtx().main_descriptor_layout, nullptr);
        rtx().main_descriptor_layout = VK_NULL_HANDLE;
    }
    if (rtx().tex_descriptor_layout) {
        vkDestroyDescriptorSetLayout(rtx().device, rtx().tex_descriptor_layout, nullptr);
        rtx().tex_descriptor_layout = VK_NULL_HANDLE;
    }
    if (rtx().empty_descriptor_layout) {
        vkDestroyDescriptorSetLayout(rtx().device, rtx().empty_descriptor_layout, nullptr);
        rtx().empty_descriptor_layout = VK_NULL_HANDLE;
    }

    if (rtx().rt_pipeline) {
        vkDestroyPipeline(rtx().device, rtx().rt_pipeline, nullptr);
        rtx().rt_pipeline = VK_NULL_HANDLE;
    }
    if (rtx().compute_pipeline) {
        vkDestroyPipeline(rtx().device, rtx().compute_pipeline, nullptr);
        rtx().compute_pipeline = VK_NULL_HANDLE;
    }
    if (rtx().pipeline_layout) {
        vkDestroyPipelineLayout(rtx().device, rtx().pipeline_layout, nullptr);
        rtx().pipeline_layout = VK_NULL_HANDLE;
    }

    if (rtx().dummy_tlas) {
        ext().vkDestroyAccelerationStructureKHR(rtx().device, rtx().dummy_tlas, nullptr);
        rtx().dummy_tlas = VK_NULL_HANDLE;
    }
    if (rtx().dummy_accel_buffer) {
        vkDestroyBuffer(rtx().device, rtx().dummy_accel_buffer, nullptr);
        rtx().dummy_accel_buffer = VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline shutdown complete");
}