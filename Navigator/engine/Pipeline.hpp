#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Unified Pipeline (Raymarching + Hardware Ray Tracing)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// Supports:
// - Pure raymarching compute (CANVAS.spv)
// - Hardware ray tracing pipeline (raygen.rgen, miss.rmiss, closesthit.rchit, anyhit.rahit, callable.rcall)
// - Automatic fallback when RT not available or disabled
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "OptionsMenu.hpp"
#include "Camera.hpp"
#include "Materials.hpp"
#include "SDL3.hpp"

#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <format>
#include <expected>
#include <cstring>
#include <algorithm>

namespace Pipeline {

using u32 = std::uint32_t;
using f32 = float;

// ────────────────────────────────────────────────
// Input bitflags (keyboard & mouse buttons)
// ────────────────────────────────────────────────
constexpr u32 INPUT_FORWARD         = 1u << 0;
constexpr u32 INPUT_BACKWARD        = 1u << 1;
constexpr u32 INPUT_LEFT            = 1u << 2;
constexpr u32 INPUT_RIGHT           = 1u << 3;
constexpr u32 INPUT_SPRINT          = 1u << 4;
constexpr u32 INPUT_CROUCH          = 1u << 5;
constexpr u32 INPUT_JUMP            = 1u << 6;
constexpr u32 INPUT_INTERACT        = 1u << 7;
constexpr u32 INPUT_SHOOT           = 1u << 8;
constexpr u32 INPUT_MOUSE_LEFT      = 1u << 9;
constexpr u32 INPUT_MOUSE_RIGHT     = 1u << 10;
constexpr u32 INPUT_MOUSE_MIDDLE    = 1u << 11;

// ────────────────────────────────────────────────
// Audio command constants
// ────────────────────────────────────────────────
constexpr f32 AUDIO_CMD_PLAY        = 0.8f;
constexpr f32 AUDIO_CMD_VOLUME      = 0.35f;
constexpr f32 AUDIO_CMD_STOP        = -0.5f;
constexpr f32 AUDIO_CMD_PAUSE       = -0.3f;

// ────────────────────────────────────────────────
// Descriptor bindings (set = 0) — shared
// ────────────────────────────────────────────────
struct CanvasBindings {
    static constexpr VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR,   nullptr}, // output LDR
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR,   nullptr}, // prev frame
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr}, // TLAS / instances
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr}, // lights/env
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr}, // materials
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr}, // textures/data
        {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr}, // audio
    };
};

// ────────────────────────────────────────────────
// Push constants — shared layout
// ────────────────────────────────────────────────
struct alignas(16) PushConstants {
    f32         time;                       // total engine time (seconds)
    u32         frameSeed;                  // per-frame RNG seed

    glm::vec3   cameraPos;                  f32 pad0;
    glm::vec4   cameraQuat;                 // xyzw orientation quaternion
    f32         cameraFovDeg;               // vertical FOV in degrees
    f32         aspectRatio;                // width / height
    f32         nearPlane;
    f32         farPlane;

    f32         exposure;
    f32         vignetteStrength;
    f32         bloomThreshold;
    f32         bloomIntensity;
    u32         tonemapMode;
    f32         contrast;
    f32         saturation;

    glm::vec3   sunDir;                     f32 sunIntensity;
    glm::vec3   moonDir;                    f32 moonIntensity;
    glm::vec3   windDir;                    f32 windStrength;
    f32         fogDensity;
    f32         dayNightFactor;
    f32         cloudCoverage;

    f32         raymarchMaxDist;
    f32         raymarchEpsilon;
    u32         raymarchMaxSteps;

    u32         controllerInput;
    f32         leftStickX;                 f32 leftStickY;
    f32         rightStickX;                f32 rightStickY;
    f32         leftTrigger;                f32 rightTrigger;

    glm::vec2   mouseDelta;
    glm::vec2   mouseNormalized;
    f32         mouseWheelDelta;

    f32         pad1[3];
};

// ────────────────────────────────────────────────
// Audio command block
// ────────────────────────────────────────────────
struct alignas(16) AudioCommandBlock {
    f32 slotCommand[16];
    f32 slotValue[16];
    f32 reserved[16];
};

// ────────────────────────────────────────────────
// Globals
// ────────────────────────────────────────────────
inline VkDescriptorSetLayout    main_descriptor_layout  = VK_NULL_HANDLE;
inline VkPipelineLayout         pipeline_layout         = VK_NULL_HANDLE;

inline VkPipeline               canvas_pipeline         = VK_NULL_HANDLE;
inline VkPipeline               rt_pipeline             = VK_NULL_HANDLE;

inline VkBuffer                 sbt_buffer              = VK_NULL_HANDLE;
inline VkDeviceMemory           sbt_memory              = VK_NULL_HANDLE;
inline VkDeviceAddress          sbt_device_address      = 0;

inline VkStridedDeviceAddressRegionKHR  rgen_region     {};
inline VkStridedDeviceAddressRegionKHR  miss_region     {};
inline VkStridedDeviceAddressRegionKHR  hit_region      {};
inline VkStridedDeviceAddressRegionKHR  call_region     {};

inline VkBuffer                 audio_cmd_buffer        = VK_NULL_HANDLE;
inline VkDeviceMemory           audio_cmd_memory        = VK_NULL_HANDLE;
inline void*                    audio_cmd_mapped        = nullptr;

inline uint32_t                 shader_group_handle_size      = 0;
inline uint32_t                 shader_group_handle_alignment = 0;
inline uint32_t                 shader_group_base_alignment   = 0;

// ────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────
[[nodiscard]] inline uint32_t alignedSize(uint32_t size, uint32_t alignment) noexcept {
    return (size + alignment - 1u) & ~(alignment - 1u);
}

[[nodiscard]] inline glm::vec3 computeSunDirection(f32 todHours) noexcept {
    f32 angle = (todHours / 24.0f) * glm::two_pi<f32>() - glm::half_pi<f32>();
    return glm::normalize(glm::vec3(std::cos(angle)*0.8f, std::sin(angle), std::cos(angle)*0.6f));
}

[[nodiscard]] inline glm::vec3 computeMoonDirection(f32 todHours) noexcept {
    return -computeSunDirection(todHours);
}

[[nodiscard]] inline std::expected<VkShaderModule, std::string> load_spirv(
    const std::string& override_path = "") noexcept
{
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;

    // Add override first
    if (!override_path.empty()) {
        fs::path op(override_path);
        if (fs::exists(op) && fs::is_regular_file(op)) {
            candidates.push_back(op);
        }
    }

    // Try common locations
    fs::path exe_dir;
    try { exe_dir = fs::canonical("/proc/self/exe").parent_path(); }
    catch (...) { /* fallback */ }

    if (!exe_dir.empty()) {
        candidates.emplace_back(exe_dir / "assets/shaders/compute/CANVAS.spv");
        candidates.emplace_back(exe_dir / "CANVAS.spv");
    }

    fs::path cwd = fs::current_path();
    candidates.emplace_back(cwd / "assets/shaders/compute/CANVAS.spv");
    candidates.emplace_back(cwd / "CANVAS.spv");

    std::vector<uint32_t> code;
    for (const auto& p : candidates) {
        if (!fs::exists(p)) continue;
        std::ifstream file(p, std::ios::binary | std::ios::ate);
        if (!file) continue;
        auto size = file.tellg();
        file.seekg(0);
        code.resize(size / 4);
        file.read(reinterpret_cast<char*>(code.data()), size);
        if (file.good()) {
            LOG_SUCCESS_CAT("SHADER", "Loaded CANVAS.spv from {}", p.string());
            break;
        }
        code.clear();
    }

    if (code.empty()) {
        LOG_ERROR_CAT("SHADER", "Failed to load CANVAS.spv from any candidate path");
        return std::unexpected("Failed to load CANVAS.spv");
    }

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode    = code.data();

    VkShaderModule mod = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(rtx().device, &ci, nullptr, &mod);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("SHADER", "vkCreateShaderModule failed: {}", static_cast<int>(res));
        return std::unexpected("vkCreateShaderModule failed");
    }

    return mod;
}

// ────────────────────────────────────────────────
// Descriptor & pipeline layout (shared)
// ────────────────────────────────────────────────
inline void initialize_descriptors_and_layout() noexcept {
    if (main_descriptor_layout != VK_NULL_HANDLE) return;

    VkDescriptorSetLayoutCreateInfo dslCI{};
    dslCI.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslCI.bindingCount = static_cast<uint32_t>(std::size(CanvasBindings::bindings));
    dslCI.pBindings    = CanvasBindings::bindings;

    VkResult res = vkCreateDescriptorSetLayout(rtx().device, &dslCI, nullptr, &main_descriptor_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create descriptor layout: {}", static_cast<int>(res));
        return;
    }

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
                    | VK_SHADER_STAGE_RAYGEN_BIT_KHR
                    | VK_SHADER_STAGE_MISS_BIT_KHR
                    | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                    | VK_SHADER_STAGE_ANY_HIT_BIT_KHR
                    | VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    push.offset     = 0;
    push.size       = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo plCI{};
    plCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCI.setLayoutCount         = 1;
    plCI.pSetLayouts            = &main_descriptor_layout;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges    = &push;

    res = vkCreatePipelineLayout(rtx().device, &plCI, nullptr, &pipeline_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create pipeline layout: {}", static_cast<int>(res));
        return;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Descriptor layout and pipeline layout created");
}

// ────────────────────────────────────────────────
// Audio command buffer
// ────────────────────────────────────────────────
inline void create_audio_command_buffer() noexcept {
    if (audio_cmd_buffer != VK_NULL_HANDLE) return;

    VkBufferCreateInfo bufCI{};
    bufCI.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.size        = sizeof(AudioCommandBlock);
    bufCI.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkCreateBuffer(rtx().device, &bufCI, nullptr, &audio_cmd_buffer);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("AUDIO", "vkCreateBuffer failed: {}", static_cast<int>(res));
        return;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, audio_cmd_buffer, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize  = req.size;
    alloc.memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    res = vkAllocateMemory(rtx().device, &alloc, nullptr, &audio_cmd_memory);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("AUDIO", "vkAllocateMemory failed: {}", static_cast<int>(res));
        return;
    }

    vkBindBufferMemory(rtx().device, audio_cmd_buffer, audio_cmd_memory, 0);

    res = vkMapMemory(rtx().device, audio_cmd_memory, 0, sizeof(AudioCommandBlock), 0, &audio_cmd_mapped);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("AUDIO", "vkMapMemory failed: {}", static_cast<int>(res));
        return;
    }

    std::memset(audio_cmd_mapped, 0, sizeof(AudioCommandBlock));
    LOG_SUCCESS_CAT("AUDIO", "Audio command buffer created & mapped");
}

// ────────────────────────────────────────────────
// Compute (raymarching) pipeline
// ────────────────────────────────────────────────
inline void create_canvas_pipeline(const std::string& override_path = "") noexcept {
    if (canvas_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr);
        canvas_pipeline = VK_NULL_HANDLE;
    }

    initialize_descriptors_and_layout();
    create_audio_command_buffer();

    auto shader_res = load_spirv(override_path);
    if (!shader_res) return;

    VkShaderModule module = *shader_res;

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = module;
    stage.pName  = "main";

    VkComputePipelineCreateInfo ci{};
    ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ci.stage  = stage;
    ci.layout = pipeline_layout;

    VkResult res = vkCreateComputePipelines(rtx().device, VK_NULL_HANDLE, 1, &ci, nullptr, &canvas_pipeline);
    vkDestroyShaderModule(rtx().device, module, nullptr);

    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "vkCreateComputePipelines failed: {}", static_cast<int>(res));
        return;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Raymarching compute pipeline created");
}

// ────────────────────────────────────────────────
// Ray Tracing pipeline
// ────────────────────────────────────────────────
inline bool create_ray_tracing_pipeline() {
    if (rt_pipeline != VK_NULL_HANDLE) return true;

    initialize_descriptors_and_layout();
    create_audio_command_buffer();

    auto rgen   = load_spirv("assets/shaders/raytracing/raygen.rgen");
    auto rmiss  = load_spirv("assets/shaders/raytracing/miss.rmiss");
    auto rchit  = load_spirv("assets/shaders/raytracing/closesthit.rchit");
    auto rahit  = load_spirv("assets/shaders/raytracing/anyhit.rahit");
    auto rcall  = load_spirv("assets/shaders/raytracing/callable.rcall");

    if (!rgen || !rmiss || !rchit || !rahit || !rcall) {
        LOG_ERROR_CAT("PIPELINE", "Failed to load one or more RT shaders");
        return false;
    }

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR,   *rgen,  "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR,     *rmiss, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, *rchit, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR,  *rahit, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CALLABLE_BIT_KHR, *rcall, "main", nullptr},
    };

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr},
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr},
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, 3, VK_SHADER_UNUSED_KHR, nullptr},
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 4, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr},
    };

    if (shader_group_handle_size == 0) {
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR props{};
        props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

        VkPhysicalDeviceProperties2 pdp2{};
        pdp2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        pdp2.pNext = &props;

        vkGetPhysicalDeviceProperties2(rtx().physical, &pdp2);

        shader_group_handle_size      = props.shaderGroupHandleSize;
        shader_group_handle_alignment = props.shaderGroupHandleAlignment;
        shader_group_base_alignment   = props.shaderGroupBaseAlignment;
    }

    VkRayTracingPipelineCreateInfoKHR pipeCI{};
    pipeCI.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipeCI.stageCount                   = static_cast<uint32_t>(stages.size());
    pipeCI.pStages                      = stages.data();
    pipeCI.groupCount                   = static_cast<uint32_t>(groups.size());
    pipeCI.pGroups                      = groups.data();
    pipeCI.maxPipelineRayRecursionDepth = Options::Rendering::MaxRayRecursion;
    pipeCI.layout                       = pipeline_layout;

    VkResult res = ext().vkCreateRayTracingPipelinesKHR(rtx().device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &rt_pipeline);

    vkDestroyShaderModule(rtx().device, *rgen,  nullptr);
    vkDestroyShaderModule(rtx().device, *rmiss, nullptr);
    vkDestroyShaderModule(rtx().device, *rchit, nullptr);
    vkDestroyShaderModule(rtx().device, *rahit, nullptr);
    vkDestroyShaderModule(rtx().device, *rcall, nullptr);

    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "vkCreateRayTracingPipelinesKHR failed: {}", static_cast<int>(res));
        return false;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Hardware RT pipeline created successfully");
    return true;
}

// ────────────────────────────────────────────────
// Build SBT
// ────────────────────────────────────────────────
inline bool build_shader_binding_table() {
    if (sbt_buffer != VK_NULL_HANDLE) return true;

    uint32_t groupCount = 4;

    uint32_t handleSize   = shader_group_handle_size;
    uint32_t alignHandle  = shader_group_handle_alignment;
    uint32_t alignBase    = shader_group_base_alignment;

    uint32_t rgenEntrySize  = alignedSize(handleSize, alignHandle);
    uint32_t missEntrySize  = alignedSize(handleSize, alignHandle);
    uint32_t hitEntrySize   = alignedSize(handleSize, alignHandle);
    uint32_t callEntrySize  = alignedSize(handleSize, alignHandle);

    uint32_t rgenRegionSize  = alignedSize(rgenEntrySize,  alignBase);
    uint32_t missRegionSize  = alignedSize(missEntrySize * 1, alignBase);
    uint32_t hitRegionSize   = alignedSize(hitEntrySize  * 1, alignBase);
    uint32_t callRegionSize  = alignedSize(callEntrySize * 1, alignBase);

    uint32_t totalSize = rgenRegionSize + missRegionSize + hitRegionSize + callRegionSize;

    VkBufferCreateInfo bufCI{};
    bufCI.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.size        = totalSize;
    bufCI.usage       = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkCreateBuffer(rtx().device, &bufCI, nullptr, &sbt_buffer);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("SBT", "vkCreateBuffer failed: {}", static_cast<int>(res));
        return false;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, sbt_buffer, &req);

    VkMemoryAllocateFlagsInfo flags{};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocCI{};
    allocCI.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocCI.pNext           = &flags;
    allocCI.allocationSize  = req.size;
    allocCI.memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    res = vkAllocateMemory(rtx().device, &allocCI, nullptr, &sbt_memory);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("SBT", "vkAllocateMemory failed: {}", static_cast<int>(res));
        return false;
    }

    vkBindBufferMemory(rtx().device, sbt_buffer, sbt_memory, 0);

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = sbt_buffer;
    sbt_device_address = ext().vkGetBufferDeviceAddress(rtx().device, &addrInfo);

    std::vector<uint8_t> handles(groupCount * handleSize);
    res = ext().vkGetRayTracingShaderGroupHandlesKHR(rtx().device, rt_pipeline, 0, groupCount, handles.size(), handles.data());
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("SBT", "vkGetRayTracingShaderGroupHandlesKHR failed: {}", static_cast<int>(res));
        return false;
    }

    void* mapped = nullptr;
    vkMapMemory(rtx().device, sbt_memory, 0, totalSize, 0, &mapped);
    uint8_t* dst = static_cast<uint8_t*>(mapped);

    uint64_t offset = 0;

    std::memcpy(dst + offset, handles.data() + 0 * handleSize, handleSize);
    rgen_region.deviceAddress = sbt_device_address + offset;
    rgen_region.stride        = rgenEntrySize;
    rgen_region.size          = rgenEntrySize;
    offset += rgenRegionSize;

    std::memcpy(dst + offset, handles.data() + 1 * handleSize, handleSize);
    miss_region.deviceAddress = sbt_device_address + offset;
    miss_region.stride        = missEntrySize;
    miss_region.size          = missEntrySize;
    offset += missRegionSize;

    std::memcpy(dst + offset, handles.data() + 2 * handleSize, handleSize);
    hit_region.deviceAddress = sbt_device_address + offset;
    hit_region.stride        = hitEntrySize;
    hit_region.size          = hitEntrySize;
    offset += hitRegionSize;

    std::memcpy(dst + offset, handles.data() + 3 * handleSize, handleSize);
    call_region.deviceAddress = sbt_device_address + offset;
    call_region.stride        = callEntrySize;
    call_region.size          = callEntrySize;

    vkUnmapMemory(rtx().device, sbt_memory);

    LOG_SUCCESS_CAT("SBT", "Shader Binding Table built — total size {} bytes", totalSize);
    return true;
}

// ────────────────────────────────────────────────
// Unified dispatch
// ────────────────────────────────────────────────
inline void dispatch(VkCommandBuffer cmd, int width, int height, float totalTime) noexcept {
    if (width <= 0 || height <= 0) return;

    PushConstants pc{};
    pc.time             = totalTime;
    pc.frameSeed        = static_cast<u32>(totalTime * 987654.321f) ^ 0xCAFEBABEu;

    pc.cameraPos        = CAM.position();
    pc.cameraQuat       = glm::vec4(CAM.orientation().x, CAM.orientation().y, CAM.orientation().z, CAM.orientation().w);
    pc.cameraFovDeg     = CAM.fov();
    pc.aspectRatio      = static_cast<f32>(width) / static_cast<f32>(height);
    pc.nearPlane        = Options::Camera::NearPlane;
    pc.farPlane         = Options::Camera::FarPlane;

    pc.exposure         = Options::Rendering::Exposure;
    pc.vignetteStrength = Options::Rendering::VignetteStrength;
    pc.bloomThreshold   = Options::Rendering::BloomThreshold;
    pc.bloomIntensity   = Options::Rendering::BloomIntensity;
    pc.tonemapMode      = Options::Rendering::EnableTonemapping ? 2u : 0u;
    pc.contrast         = Options::Rendering::Contrast;
    pc.saturation       = Options::Rendering::Saturation;

    f32 tod = Options::LivingWorld::CurrentTimeOfDay;
    pc.sunDir           = computeSunDirection(tod);
    pc.moonDir          = computeMoonDirection(tod);
    pc.sunIntensity     = Options::LivingWorld::SunIntensityDay;
    pc.moonIntensity    = Options::LivingWorld::MoonIntensity;
    pc.windDir          = glm::normalize(Options::LivingWorld::WindDirection);
    pc.windStrength     = Options::LivingWorld::WindStrength;
    pc.fogDensity       = Options::LivingWorld::FogDensity;
    pc.dayNightFactor   = tod / 24.0f;
    pc.cloudCoverage    = Options::LivingWorld::CloudCoverage;

    pc.raymarchMaxDist  = Options::Rendering::RaymarchMaxDistance;
    pc.raymarchEpsilon  = Options::Rendering::RaymarchEpsilon;
    pc.raymarchMaxSteps = Options::Rendering::RaymarchMaxSteps;

    // Input state
    pc.controllerInput = 0;
    if (SDL3System::get().down("move_forward"))  pc.controllerInput |= INPUT_FORWARD;
    if (SDL3System::get().down("move_backward")) pc.controllerInput |= INPUT_BACKWARD;
    if (SDL3System::get().down("move_left"))     pc.controllerInput |= INPUT_LEFT;
    if (SDL3System::get().down("move_right"))    pc.controllerInput |= INPUT_RIGHT;
    if (SDL3System::get().down("sprint"))        pc.controllerInput |= INPUT_SPRINT;
    if (SDL3System::get().down("crouch"))        pc.controllerInput |= INPUT_CROUCH;
    if (SDL3System::get().down("jump"))          pc.controllerInput |= INPUT_JUMP;
    if (SDL3System::get().down("interact"))      pc.controllerInput |= INPUT_INTERACT;
    if (SDL3System::get().down("shoot"))         pc.controllerInput |= INPUT_SHOOT;

    Uint32 mouse_state = SDL_GetMouseState(nullptr, nullptr);
    if (mouse_state & SDL_BUTTON_LMASK) pc.controllerInput |= INPUT_MOUSE_LEFT;
    if (mouse_state & SDL_BUTTON_RMASK) pc.controllerInput |= INPUT_MOUSE_RIGHT;
    if (mouse_state & SDL_BUTTON_MMASK) pc.controllerInput |= INPUT_MOUSE_MIDDLE;

    int ctrl_slot = 0;
    pc.leftStickX   = SDL3System::get().leftStickX(ctrl_slot);
    pc.leftStickY   = SDL3System::get().leftStickY(ctrl_slot);
    pc.rightStickX  = SDL3System::get().rightStickX(ctrl_slot);
    pc.rightStickY  = SDL3System::get().rightStickY(ctrl_slot);
    pc.leftTrigger  = SDL3System::get().leftTrigger(ctrl_slot);
    pc.rightTrigger = SDL3System::get().rightTrigger(ctrl_slot);

    glm::vec2 delta = SDL3System::get().mouseDelta();
    pc.mouseDelta       = delta;
    pc.mouseNormalized  = glm::vec2(
        (delta.x + 0.5f * static_cast<f32>(width))  / static_cast<f32>(width),
        (delta.y + 0.5f * static_cast<f32>(height)) / static_cast<f32>(height)
    );
    pc.mouseWheelDelta  = 0.0f; // fill if tracked

    // ────────────────────────────────────────────────
    // Choose path
    // ────────────────────────────────────────────────
    bool use_rt = Options::Rendering::EnableHardwareRayTracing &&
                  (Options::Rendering::CurrentTechnique == Options::Rendering::RenderTechnique::HardwareRayTracing) &&
                  rtx().rayTracingSupported &&
                  create_ray_tracing_pipeline() &&
                  build_shader_binding_table();

    if (use_rt) {
        LOG_INFO_CAT("PIPELINE", "Dispatching hardware ray tracing ({}x{})", width, height);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline);
        vkCmdPushConstants(cmd, pipeline_layout,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                           VK_SHADER_STAGE_CALLABLE_BIT_KHR,
                           0, sizeof(PushConstants), &pc);

        ext().vkCmdTraceRaysKHR(cmd, &rgen_region, &miss_region, &hit_region, &call_region,
                                static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1u);
    }
    else {
        if (!canvas_pipeline) {
            create_canvas_pipeline();  // uses default path
            if (!canvas_pipeline) {
                LOG_ERROR_CAT("PIPELINE", "Raymarching pipeline creation failed — cannot dispatch");
                return;
            }
        }
		
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, canvas_pipeline);
        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(PushConstants), &pc);

        u32 dx = (static_cast<u32>(width)  + 15u) / 16u;
        u32 dy = (static_cast<u32>(height) + 15u) / 16u;
        vkCmdDispatch(cmd, dx, dy, 1u);
    }

    // Process audio commands
    if (audio_cmd_mapped) {
        auto* cmds = static_cast<AudioCommandBlock*>(audio_cmd_mapped);
        for (int slot = 0; slot < 16; ++slot) {
            f32 command = cmds->slotCommand[slot];
            // f32 value   = cmds->slotValue[slot]; // commented — avoid unused warning

            if (command > 0.51f) {
                std::string file = "assets/audio/sfx_slot_" + std::to_string(slot) + ".wav";
                SDL3System::get().playSound(file, "play", slot);
            }
            else if (command >= 0.20f && command <= 0.50f) {
                // Volume set — implement when needed
            }
            else if (command < -0.1f) {
                SDL3System::get().playSound("", "stop", slot);
            }
        }
        std::memset(cmds, 0, sizeof(AudioCommandBlock));
    }
}

// ────────────────────────────────────────────────
// Cleanup — separate assignments to avoid type errors
// ────────────────────────────────────────────────
inline void shutdown() noexcept {
    if (audio_cmd_mapped) {
        vkUnmapMemory(rtx().device, audio_cmd_memory);
        audio_cmd_mapped = nullptr;
    }

    if (audio_cmd_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(rtx().device, audio_cmd_buffer, nullptr);
        audio_cmd_buffer = VK_NULL_HANDLE;
    }
    if (audio_cmd_memory != VK_NULL_HANDLE) {
        vkFreeMemory(rtx().device, audio_cmd_memory, nullptr);
        audio_cmd_memory = VK_NULL_HANDLE;
    }

    if (canvas_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr);
        canvas_pipeline = VK_NULL_HANDLE;
    }
    if (rt_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtx().device, rt_pipeline, nullptr);
        rt_pipeline = VK_NULL_HANDLE;
    }
    if (sbt_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(rtx().device, sbt_buffer, nullptr);
        sbt_buffer = VK_NULL_HANDLE;
    }
    if (sbt_memory != VK_NULL_HANDLE) {
        vkFreeMemory(rtx().device, sbt_memory, nullptr);
        sbt_memory = VK_NULL_HANDLE;
    }

    if (pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rtx().device, pipeline_layout, nullptr);
        pipeline_layout = VK_NULL_HANDLE;
    }
    if (main_descriptor_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtx().device, main_descriptor_layout, nullptr);
        main_descriptor_layout = VK_NULL_HANDLE;
    }
}

} // namespace Pipeline