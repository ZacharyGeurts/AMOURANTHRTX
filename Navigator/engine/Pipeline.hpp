#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Pipeline (HYBRID: raymarched compute + full hardware RT with SBT + RTXGI ready)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "OptionsMenu.hpp"
#include "Camera.hpp"
#include "Materials.hpp"
#include "SDL3.hpp"

#include <algorithm>
#include <vector>
#include <fstream>
#include <format>
#include <filesystem>
#include <cstdint>
#include <string>
#include <expected>
#include <cmath>
#include <cstring>

namespace Pipeline {

using u32 = std::uint32_t;
using f32 = float;

// ────────────────────────────────────────────────
// Audio command constants
// ────────────────────────────────────────────────
constexpr f32 AUDIO_CMD_PLAY        = 0.8f;
constexpr f32 AUDIO_CMD_VOLUME      = 0.35f;
constexpr f32 AUDIO_CMD_STOP        = -0.5f;
constexpr f32 AUDIO_CMD_PAUSE       = -0.3f;

// ────────────────────────────────────────────────
// Push constant stage flags (used by compute + all ray tracing stages)
// ────────────────────────────────────────────────
constexpr VkShaderStageFlags PUSH_STAGE_FLAGS =
    VK_SHADER_STAGE_COMPUTE_BIT |
    VK_SHADER_STAGE_RAYGEN_BIT_KHR |
    VK_SHADER_STAGE_MISS_BIT_KHR |
    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

// ────────────────────────────────────────────────
// Descriptor bindings (set = 0) — shared between compute and ray tracing
// ────────────────────────────────────────────────
struct CanvasBindings {
    static constexpr VkDescriptorSetLayoutBinding bindings[] = {
        // 0 - Current output HDR image (write)
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},

        // 1 - Previous frame HDR image (read for accumulation / TAA / RTXGI)
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},

        // 2,3,5 - Reserved / future buffers
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},

        // 4 - Material library storage buffer
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | 
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},

        // 6 - Audio command buffer (shader → host)
        {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},

        // 7 - Top Level Acceleration Structure (TLAS) for hardware ray tracing + RTXGI
        {7, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
};

// ────────────────────────────────────────────────
// PushConstants — now includes RTXGI toggle and full hybrid control
// ────────────────────────────────────────────────
struct alignas(16) PushConstants {
    f32         time;                       // total elapsed time
    bool        enableHardwareRT;           // F2 — switches between compute raymarch and full hardware RT
    bool        enableRTXGI;                // F3 — enables RTX Global Illumination when hardware RT is active
    u32         frameSeed;                  // for noise / randomization

    glm::vec3   cameraPos;                  f32 pad0;
    glm::vec4   cameraQuat;
    f32         cameraFovDeg;
    f32         aspectRatio;
    f32         nearPlane;
    f32         farPlane;

    f32         exposure;
    f32         vignetteStrength;
    f32         bloomThreshold;
    f32         bloomIntensity;
    u32         tonemapMode;                // 0 = off, 1 = on
    f32         contrast;
    f32         saturation;
    f32         gamma;

    glm::vec3   sunDir;                     f32 sunIntensity;
    glm::vec3   moonDir;                    f32 moonIntensity;
    glm::vec3   windDir;                    f32 windStrength;
    f32         fogDensity;
    f32         dayNightFactor;
    f32         cloudCoverage;

    f32         raymarchMaxDist;
    f32         raymarchEpsilon;
    u32         raymarchMaxSteps;

    u32         controllerInput;            // bitfield for keyboard + gamepad
    f32         leftStickX;                 f32 leftStickY;
    f32         rightStickX;                f32 rightStickY;
    f32         leftTrigger;                f32 rightTrigger;

    glm::vec2   mouseDelta;
    glm::vec2   mouseNormalized;
    f32         mouseWheelDelta;

    f32         pad1[2];                    // keep 16-byte alignment
};

// ────────────────────────────────────────────────
// Audio command block (shader writes, host reads)
// ────────────────────────────────────────────────
struct alignas(16) AudioCommandBlock {
    f32 slotCommand[16];
    f32 slotValue[16];
    f32 reserved[16];
};

// ────────────────────────────────────────────────
// Global Vulkan objects for the pipeline
// ────────────────────────────────────────────────
inline VkDescriptorSetLayout    main_descriptor_layout  = VK_NULL_HANDLE;
inline VkPipelineLayout         pipeline_layout         = VK_NULL_HANDLE;
inline VkPipeline               canvas_pipeline         = VK_NULL_HANDLE;

// Hardware ray tracing pipeline + Shader Binding Table
inline VkPipeline               rt_pipeline             = VK_NULL_HANDLE;
inline u32                      shader_group_handle_size = 0;
inline u32                      shader_group_alignment  = 0;
inline VkBuffer                 sbt_buffer              = VK_NULL_HANDLE;
inline VkDeviceMemory           sbt_memory              = VK_NULL_HANDLE;

inline VkStridedDeviceAddressRegionKHR raygen_region = {};
inline VkStridedDeviceAddressRegionKHR miss_region   = {};
inline VkStridedDeviceAddressRegionKHR chit_region   = {};
inline VkStridedDeviceAddressRegionKHR ahit_region   = {};

struct RTShaderModules {
    VkShaderModule raygen      = VK_NULL_HANDLE;
    VkShaderModule miss        = VK_NULL_HANDLE;
    VkShaderModule closestHit  = VK_NULL_HANDLE;
};
inline RTShaderModules rt_shaders;

// Audio command storage buffer
inline VkBuffer                 audio_cmd_buffer        = VK_NULL_HANDLE;
inline VkDeviceMemory           audio_cmd_memory        = VK_NULL_HANDLE;
inline void*                    audio_cmd_mapped        = nullptr;

// ────────────────────────────────────────────────
// Helper functions
// ────────────────────────────────────────────────
[[nodiscard]] inline glm::vec3 computeSunDirection(f32 todHours) noexcept {
    f32 angle = (todHours / 24.0f) * glm::two_pi<f32>() - glm::half_pi<f32>();
    return glm::normalize(glm::vec3(std::cos(angle) * 0.8f, std::sin(angle), std::cos(angle) * 0.6f));
}

[[nodiscard]] inline glm::vec3 computeMoonDirection(f32 todHours) noexcept {
    return -computeSunDirection(todHours);
}

// ────────────────────────────────────────────────
// Shader module loader — searches multiple paths for .spv files
// ────────────────────────────────────────────────
[[nodiscard]] inline std::expected<VkShaderModule, std::string> load_shader_module(
    const std::string& shader_name) noexcept
{
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;

    // Try executable directory
    fs::path exe_dir;
    try { 
        exe_dir = fs::canonical("/proc/self/exe").parent_path(); 
    } catch (...) {}

    if (!exe_dir.empty()) {
        candidates.emplace_back(exe_dir / "assets/shaders/compute" / (shader_name + ".spv"));
        candidates.emplace_back(exe_dir / "assets/shaders/raytracing" / (shader_name + ".spv"));
        candidates.emplace_back(exe_dir / (shader_name + ".spv"));
    }

    // Try current working directory
    fs::path cwd = fs::current_path();
    candidates.emplace_back(cwd / "assets/shaders/compute" / (shader_name + ".spv"));
    candidates.emplace_back(cwd / "assets/shaders/raytracing" / (shader_name + ".spv"));
    candidates.emplace_back(cwd / (shader_name + ".spv"));

    std::vector<uint32_t> code;
    for (const auto& p : candidates) {
        if (!fs::exists(p)) continue;

        std::ifstream file(p, std::ios::binary | std::ios::ate);
        if (!file.is_open()) continue;

        auto size = file.tellg();
        file.seekg(0);
        code.resize(size / 4);
        file.read(reinterpret_cast<char*>(code.data()), size);

        if (file.good() && !code.empty()) {
            LOG_SUCCESS_CAT("PIPELINE", "Loaded shader from: {}", p.string());
            break;
        }
        code.clear();
    }

    if (code.empty()) {
        return std::unexpected(std::format("Failed to load shader: {}.spv from any search path", shader_name));
    }

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode    = code.data();

    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(rtx().device, &ci, nullptr, &mod) != VK_SUCCESS) {
        return std::unexpected("vkCreateShaderModule failed for " + shader_name);
    }

    return mod;
}

// Query ray tracing pipeline properties (run once)
inline void query_ray_tracing_properties() noexcept {
    if (shader_group_handle_size > 0) return;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;

    vkGetPhysicalDeviceProperties2(rtx().physical, &props2);

    shader_group_handle_size = rtProps.shaderGroupHandleSize;
    shader_group_alignment   = rtProps.shaderGroupHandleAlignment;
}

// Create ray tracing pipeline + Shader Binding Table (SBT)
inline void create_ray_tracing_pipeline() noexcept {
    if (rt_pipeline != VK_NULL_HANDLE) return;

    query_ray_tracing_properties();

    // Load ray tracing shaders
    auto raygen_res = load_shader_module("raygen");
    if (!raygen_res) {
        LOG_ERROR_CAT("PIPELINE", "Failed to load raygen.spv");
        return;
    }
    rt_shaders.raygen = *raygen_res;

    auto miss_res = load_shader_module("miss");
    if (!miss_res) {
        LOG_ERROR_CAT("PIPELINE", "Failed to load miss.spv");
        return;
    }
    rt_shaders.miss = *miss_res;

    auto chit_res = load_shader_module("closesthit");
    if (!chit_res) {
        LOG_ERROR_CAT("PIPELINE", "Failed to load closesthit.spv");
        return;
    }
    rt_shaders.closestHit = *chit_res;

    // Shader stages
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages(3);
    shaderStages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                        VK_SHADER_STAGE_RAYGEN_BIT_KHR, rt_shaders.raygen, "main", nullptr };

    shaderStages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                        VK_SHADER_STAGE_MISS_BIT_KHR, rt_shaders.miss, "main", nullptr };

    shaderStages[2] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rt_shaders.closestHit, "main", nullptr };

    // Shader groups
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups(3);

    shaderGroups[0].sType               = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroups[0].type                = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shaderGroups[0].generalShader       = 0;
    shaderGroups[0].closestHitShader    = VK_SHADER_UNUSED_KHR;
    shaderGroups[0].anyHitShader        = VK_SHADER_UNUSED_KHR;
    shaderGroups[0].intersectionShader  = VK_SHADER_UNUSED_KHR;

    shaderGroups[1].sType               = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroups[1].type                = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shaderGroups[1].generalShader       = 1;
    shaderGroups[1].closestHitShader    = VK_SHADER_UNUSED_KHR;
    shaderGroups[1].anyHitShader        = VK_SHADER_UNUSED_KHR;
    shaderGroups[1].intersectionShader  = VK_SHADER_UNUSED_KHR;

    shaderGroups[2].sType               = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroups[2].type                = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    shaderGroups[2].generalShader       = VK_SHADER_UNUSED_KHR;
    shaderGroups[2].closestHitShader    = 2;
    shaderGroups[2].anyHitShader        = VK_SHADER_UNUSED_KHR;
    shaderGroups[2].intersectionShader  = VK_SHADER_UNUSED_KHR;

    // Create ray tracing pipeline
    VkRayTracingPipelineCreateInfoKHR ci{};
    ci.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    ci.stageCount                   = 3;
    ci.pStages                      = shaderStages.data();
    ci.groupCount                   = 3;
    ci.pGroups                      = shaderGroups.data();
    ci.maxPipelineRayRecursionDepth = 1;                    // RTXGI usually needs 1-2
    ci.layout                       = pipeline_layout;

    if (ext().vkCreateRayTracingPipelinesKHR(rtx().device, VK_NULL_HANDLE, VK_NULL_HANDLE,
        1, &ci, nullptr, &rt_pipeline) != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to create hardware ray tracing pipeline");
        return;
    }

    // Build Shader Binding Table (SBT)
    u32 numGroups = 3;
    u32 handleSizeAligned = ((shader_group_handle_size + shader_group_alignment - 1) / shader_group_alignment) 
                            * shader_group_alignment;
    VkDeviceSize sbtTotalSize = numGroups * handleSizeAligned;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = sbtTotalSize;
    bufInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    vkCreateBuffer(rtx().device, &bufInfo, nullptr, &sbt_buffer);

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, sbt_buffer, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(rtx().device, &alloc, nullptr, &sbt_memory);
    vkBindBufferMemory(rtx().device, sbt_buffer, sbt_memory, 0);

    // Get shader group handles
    std::vector<uint8_t> handles(numGroups * shader_group_handle_size);
    ext().vkGetRayTracingShaderGroupHandlesKHR(rtx().device, rt_pipeline, 0, numGroups, handles.size(), handles.data());

    // Copy handles into SBT with proper alignment
    void* mapped = nullptr;
    vkMapMemory(rtx().device, sbt_memory, 0, sbtTotalSize, 0, &mapped);
    uint8_t* dst = static_cast<uint8_t*>(mapped);
    for (u32 i = 0; i < numGroups; ++i) {
        std::memcpy(dst + i * handleSizeAligned, 
                    handles.data() + i * shader_group_handle_size, 
                    shader_group_handle_size);
    }
    vkUnmapMemory(rtx().device, sbt_memory);

    // Get device address for regions
    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = sbt_buffer;
    VkDeviceAddress sbtDevAddr = vkGetBufferDeviceAddress(rtx().device, &addrInfo);

    raygen_region.deviceAddress = sbtDevAddr;
    raygen_region.stride        = handleSizeAligned;
    raygen_region.size          = handleSizeAligned;

    miss_region.deviceAddress   = sbtDevAddr + handleSizeAligned;
    miss_region.stride          = handleSizeAligned;
    miss_region.size            = handleSizeAligned;

    chit_region.deviceAddress   = sbtDevAddr + 2 * handleSizeAligned;
    chit_region.stride          = handleSizeAligned;
    chit_region.size            = handleSizeAligned;

    LOG_SUCCESS_CAT("PIPELINE", "Hardware ray tracing pipeline + SBT created successfully (RTXGI ready)");
}

// Descriptor layout creation
inline void initialize_descriptors() noexcept {
    if (main_descriptor_layout != VK_NULL_HANDLE) return;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = std::size(CanvasBindings::bindings);
    ci.pBindings    = CanvasBindings::bindings;

    VkResult res = vkCreateDescriptorSetLayout(rtx().device, &ci, nullptr, &main_descriptor_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create main descriptor set layout");
        return;
    }
    LOG_SUCCESS_CAT("PIPELINE", "Descriptor set layout created");
}

// Pipeline layout (push constants + descriptor set)
inline void create_pipeline_layout() noexcept {
    if (pipeline_layout != VK_NULL_HANDLE) return;
    initialize_descriptors();

    VkPushConstantRange push{};
    push.stageFlags = PUSH_STAGE_FLAGS;
    push.offset     = 0;
    push.size       = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo ci{};
    ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.setLayoutCount         = 1;
    ci.pSetLayouts            = &main_descriptor_layout;
    ci.pushConstantRangeCount = 1;
    ci.pPushConstantRanges    = &push;

    VkResult res = vkCreatePipelineLayout(rtx().device, &ci, nullptr, &pipeline_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create pipeline layout");
        return;
    }
    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created");
}

// Audio command buffer
inline void create_audio_command_buffer() noexcept {
    if (audio_cmd_buffer != VK_NULL_HANDLE) return;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = sizeof(AudioCommandBlock);
    bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(rtx().device, &bufInfo, nullptr, &audio_cmd_buffer);

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, audio_cmd_buffer, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(rtx().device, &alloc, nullptr, &audio_cmd_memory);
    vkBindBufferMemory(rtx().device, audio_cmd_buffer, audio_cmd_memory, 0);

    vkMapMemory(rtx().device, audio_cmd_memory, 0, sizeof(AudioCommandBlock), 0, &audio_cmd_mapped);
    std::memset(audio_cmd_mapped, 0, sizeof(AudioCommandBlock));
}

// Create compute pipeline (CANVAS)
inline void create_canvas_pipeline() noexcept {
    if (canvas_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr);
        canvas_pipeline = VK_NULL_HANDLE;
    }

    create_pipeline_layout();
    create_audio_command_buffer();

    auto shader_res = load_shader_module("CANVAS");
    if (!shader_res) {
        LOG_ERROR_CAT("PIPELINE", "Failed to load CANVAS.spv");
        return;
    }

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
        LOG_ERROR_CAT("PIPELINE", "Failed to create compute pipeline");
        return;
    }

    // Always create RT pipeline at startup so toggling doesn't cause long freeze
    create_ray_tracing_pipeline();
}

// Process audio commands written by shader
inline void process_shader_audio_commands() noexcept {
    if (!audio_cmd_mapped) return;

    AudioCommandBlock* cmd = static_cast<AudioCommandBlock*>(audio_cmd_mapped);

    for (int slot = 0; slot < 16; ++slot) {
        f32 command = cmd->slotCommand[slot];

        if (command > 0.51f) {
            std::string file = "assets/audio/splash" + std::to_string(slot) + ".wav";
            INPUT.playSound(file, "play", slot);
        }
        else if (command < -0.1f) {
            INPUT.playSound("", "stop", slot);
        }
    }

    std::memset(cmd, 0, sizeof(AudioCommandBlock));
}

// ────────────────────────────────────────────────
// MAIN DISPATCH — hybrid compute vs hardware RT (no freeze on toggle)
// ────────────────────────────────────────────────
inline void dispatch_canvas(VkCommandBuffer cmd, int width, int height, float totalTime) noexcept {
    if (width <= 0 || height <= 0) return;

    PushConstants pc{};
    pc.time             = totalTime;
    pc.enableHardwareRT = Options::Rendering::EnableHardwareRayTracing;
    pc.enableRTXGI      = Options::Rendering::EnableRTXGI;
    pc.frameSeed        = static_cast<u32>(totalTime * 987654.321f) ^ 0xCAFEBABEu;

    pc.cameraPos        = CAM.position();
    pc.cameraQuat       = glm::vec4(CAM.orientation().x, CAM.orientation().y, CAM.orientation().z, CAM.orientation().w);
    pc.cameraFovDeg     = CAM.fovDeg();
    pc.aspectRatio      = static_cast<f32>(width) / static_cast<f32>(height);
    pc.nearPlane        = Options::Camera::NearPlane;
    pc.farPlane         = Options::Camera::FarPlane;

    pc.exposure         = Options::Rendering::Exposure;
    pc.vignetteStrength = Options::Rendering::VignetteStrength;
    pc.bloomThreshold   = Options::Rendering::BloomThreshold;
    pc.bloomIntensity   = Options::Rendering::BloomIntensity;
    pc.tonemapMode      = Options::Rendering::EnableTonemapping ? 1u : 0u;
    pc.contrast         = Options::Rendering::Contrast;
    pc.saturation       = Options::Rendering::Saturation;
    pc.gamma            = Options::Rendering::Gamma;

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

    // Input bitfield packing
    pc.controllerInput = 0u;

    if (INPUT.down("move_forward"))  pc.controllerInput |= Options::Input::Flags::MOVE_FORWARD;
    if (INPUT.down("move_backward")) pc.controllerInput |= Options::Input::Flags::MOVE_BACKWARD;
    if (INPUT.down("move_left"))     pc.controllerInput |= Options::Input::Flags::MOVE_LEFT;
    if (INPUT.down("move_right"))    pc.controllerInput |= Options::Input::Flags::MOVE_RIGHT;

    if (INPUT.down("gp_south"))      pc.controllerInput |= Options::Input::Flags::GAMEPAD_SOUTH;
    if (INPUT.down("gp_east"))       pc.controllerInput |= Options::Input::Flags::GAMEPAD_EAST;
    if (INPUT.down("gp_west"))       pc.controllerInput |= Options::Input::Flags::GAMEPAD_WEST;
    if (INPUT.down("gp_north"))      pc.controllerInput |= Options::Input::Flags::GAMEPAD_NORTH;

    if (INPUT.down("gp_left_shoulder"))  pc.controllerInput |= Options::Input::Flags::GAMEPAD_LEFT_SHOULDER;
    if (INPUT.down("gp_right_shoulder")) pc.controllerInput |= Options::Input::Flags::GAMEPAD_RIGHT_SHOULDER;

    if (INPUT.down("gp_left_stick"))     pc.controllerInput |= Options::Input::Flags::GAMEPAD_LEFT_STICK;
    if (INPUT.down("gp_right_stick"))    pc.controllerInput |= Options::Input::Flags::GAMEPAD_RIGHT_STICK;

    if (INPUT.down("gp_left_paddle1"))   pc.controllerInput |= Options::Input::Flags::GAMEPAD_LEFT_PADDLE1;
    if (INPUT.down("gp_left_paddle2"))   pc.controllerInput |= Options::Input::Flags::GAMEPAD_LEFT_PADDLE2;
    if (INPUT.down("gp_right_paddle1"))  pc.controllerInput |= Options::Input::Flags::GAMEPAD_RIGHT_PADDLE1;
    if (INPUT.down("gp_right_paddle2"))  pc.controllerInput |= Options::Input::Flags::GAMEPAD_RIGHT_PADDLE2;

    Uint32 mouse_state = SDL_GetMouseState(nullptr, nullptr);
    if (mouse_state & SDL_BUTTON_LMASK) pc.controllerInput |= Options::Input::Flags::MOUSE_LEFT;
    if (mouse_state & SDL_BUTTON_RMASK) pc.controllerInput |= Options::Input::Flags::MOUSE_RIGHT;
    if (mouse_state & SDL_BUTTON_MMASK) pc.controllerInput |= Options::Input::Flags::MOUSE_MIDDLE;

    int ctrl_slot = 0;
    pc.leftStickX   = INPUT.leftStickX(ctrl_slot);
    pc.leftStickY   = INPUT.leftStickY(ctrl_slot);
    pc.rightStickX  = INPUT.rightStickX(ctrl_slot);
    pc.rightStickY  = INPUT.rightStickY(ctrl_slot);
    pc.leftTrigger  = INPUT.leftTrigger(ctrl_slot);
    pc.rightTrigger = INPUT.rightTrigger(ctrl_slot);

    glm::vec2 delta = INPUT.mouseDelta();
    pc.mouseDelta       = delta;
    pc.mouseNormalized  = glm::vec2(
        (delta.x + static_cast<f32>(width)  * 0.5f) / static_cast<f32>(width),
        (delta.y + static_cast<f32>(height) * 0.5f) / static_cast<f32>(height)
    );
    pc.mouseWheelDelta  = 0.0f;

    // ── Hybrid Dispatch Logic ──
    if (pc.enableHardwareRT && rt_pipeline != VK_NULL_HANDLE) {
        // Hardware ray tracing path (with RTXGI support via closesthit/raygen)
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline);
        vkCmdPushConstants(cmd, pipeline_layout, PUSH_STAGE_FLAGS, 0, sizeof(PushConstants), &pc);

        ext().vkCmdTraceRaysKHR(cmd,
            &raygen_region,
            &miss_region,
            &chit_region,
            &ahit_region,
            static_cast<u32>(width),
            static_cast<u32>(height),
            1u);
    } else {
        // Fallback compute raymarching path
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, canvas_pipeline);
        vkCmdPushConstants(cmd, pipeline_layout, PUSH_STAGE_FLAGS, 0, sizeof(PushConstants), &pc);

        u32 dx = (static_cast<u32>(width)  + 15u) / 16u;
        u32 dy = (static_cast<u32>(height) + 15u) / 16u;
        vkCmdDispatch(cmd, dx, dy, 1u);
    }

    process_shader_audio_commands();
}

// ────────────────────────────────────────────────
// Full shutdown
// ────────────────────────────────────────────────
inline void shutdown() noexcept {
    process_shader_audio_commands();

    if (rt_pipeline)            { vkDestroyPipeline(rtx().device, rt_pipeline, nullptr); rt_pipeline = VK_NULL_HANDLE; }
    if (sbt_buffer)             { vkDestroyBuffer(rtx().device, sbt_buffer, nullptr); sbt_buffer = VK_NULL_HANDLE; }
    if (sbt_memory)             { vkFreeMemory(rtx().device, sbt_memory, nullptr); sbt_memory = VK_NULL_HANDLE; }
    if (rt_shaders.raygen)      vkDestroyShaderModule(rtx().device, rt_shaders.raygen, nullptr);
    if (rt_shaders.miss)        vkDestroyShaderModule(rtx().device, rt_shaders.miss, nullptr);
    if (rt_shaders.closestHit)  vkDestroyShaderModule(rtx().device, rt_shaders.closestHit, nullptr);

    if (canvas_pipeline)        { vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr); canvas_pipeline = VK_NULL_HANDLE; }
    if (pipeline_layout)        { vkDestroyPipelineLayout(rtx().device, pipeline_layout, nullptr); pipeline_layout = VK_NULL_HANDLE; }
    if (main_descriptor_layout) { vkDestroyDescriptorSetLayout(rtx().device, main_descriptor_layout, nullptr); main_descriptor_layout = VK_NULL_HANDLE; }

    if (audio_cmd_mapped)       { vkUnmapMemory(rtx().device, audio_cmd_memory); audio_cmd_mapped = nullptr; }
    if (audio_cmd_buffer)       { vkDestroyBuffer(rtx().device, audio_cmd_buffer, nullptr); audio_cmd_buffer = VK_NULL_HANDLE; }
    if (audio_cmd_memory)       { vkFreeMemory(rtx().device, audio_cmd_memory, nullptr); audio_cmd_memory = VK_NULL_HANDLE; }

    LOG_SUCCESS_CAT("PIPELINE", "Full shutdown complete — hybrid canvas + RT + RTXGI released");
}

} // namespace Pipeline