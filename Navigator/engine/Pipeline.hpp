#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Pipeline (pure raymarched 3D with full input & audio)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// Features:
// - Pure raymarching compute pipeline (CANVAS.spv from CANVAS.comp)
// - Full controller + keyboard + mouse input passed to shader via PushConstants
// - Shader handles final post-processing (exposure, vignette, tonemap, bloom, contrast, saturation)
// - Shader sends audio commands via small SSBO → CPU interprets & plays via SDL3System
// - All Options:: namespaces are read and applied where applicable
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "OptionsMenu.hpp"
#include "Camera.hpp"           // CAM singleton
#include "Materials.hpp"
#include "SDL3.hpp"             // INPUT access via SDL3System

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
// Audio command constants (values written by shader, interpreted by CPU)
// ────────────────────────────────────────────────
constexpr f32 AUDIO_CMD_PLAY        = 0.8f;     // > 0.5  → one-shot trigger (play)
constexpr f32 AUDIO_CMD_VOLUME      = 0.35f;    // 0.2–0.5 → continuous volume set (0.0–1.0)
constexpr f32 AUDIO_CMD_STOP        = -0.5f;    // < 0     → stop or pause track
constexpr f32 AUDIO_CMD_PAUSE       = -0.3f;    // alternative pause signal

// ────────────────────────────────────────────────
// Descriptor bindings (set = 0)
// ────────────────────────────────────────────────
struct CanvasBindings {
    static constexpr VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // output image (final LDR)
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // previous frame (accumulation / TAA)
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // optional geometry/instances
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // optional lights/environment
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // optional materials
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // optional textures/data
        {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // audio commands (small SSBO)
    };
};

// ────────────────────────────────────────────────
// Push constants — fully expanded with mouse & controller support
// Must stay aligned to 16 bytes; total size checked at compile time
// ────────────────────────────────────────────────
struct alignas(16) PushConstants {
    f32         time;                       // total engine time (seconds)
	uint        enableHardwareRT;           // Options::Rendering
    u32         frameSeed;                  // per-frame RNG seed

    glm::vec3   cameraPos;                  f32 pad0;
    glm::vec4   cameraQuat;                 // xyzw orientation quaternion
    f32         cameraFovDeg;               // vertical FOV in degrees
    f32         aspectRatio;                // width / height
    f32         nearPlane;
    f32         farPlane;

    // Post-process controls — shader applies these
    f32         exposure;                   // EV stops compensation
    f32         vignetteStrength;           // 0 = off, typical 0.3–0.8
    f32         bloomThreshold;             // brightness where glow begins
    f32         bloomIntensity;             // self-bloom strength (0 = disabled)
    u32         tonemapMode;                // 0 = linear/raw, 1 = simple filmic, 2 = ACES approx
    f32         contrast;                   // 0.8–1.4 typical
    f32         saturation;                 // 0.0–2.0 typical
    f32         gamma;                      // 1.0 typical

    // Environment / atmosphere
    glm::vec3   sunDir;                     f32 sunIntensity;
    glm::vec3   moonDir;                    f32 moonIntensity;
    glm::vec3   windDir;                    f32 windStrength;
    f32         fogDensity;
    f32         dayNightFactor;
    f32         cloudCoverage;

    // Raymarching quality
    f32         raymarchMaxDist;
    f32         raymarchEpsilon;
    u32         raymarchMaxSteps;

    // Full input state — sent every frame
    u32         controllerInput;            // bitfield (keyboard + mouse buttons)
    f32         leftStickX;                 f32 leftStickY;
    f32         rightStickX;                f32 rightStickY;
    f32         leftTrigger;                f32 rightTrigger;

    // Mouse input
    glm::vec2   mouseDelta;                 // raw pixel delta this frame
    glm::vec2   mouseNormalized;            // [0,1] screen space (0,0 = top-left)
    f32         mouseWheelDelta;            // accumulated scroll this frame

    f32         pad1[3];                    // pad to 16-byte alignment
};

// ────────────────────────────────────────────────
// Audio command block — small SSBO (written by shader, read by CPU)
// ────────────────────────────────────────────────
struct alignas(16) AudioCommandBlock {
    f32 slotCommand[16];        // command code per slot (play, volume, stop, pause, ...)
    f32 slotValue[16];          // parameter (volume 0–1, pitch multiplier, etc.)
    f32 reserved[16];           // future: pan, reverb send, effect index, ...
};

// ────────────────────────────────────────────────
// Global Vulkan objects
// ────────────────────────────────────────────────
inline VkDescriptorSetLayout    main_descriptor_layout  = VK_NULL_HANDLE;
inline VkPipelineLayout         pipeline_layout         = VK_NULL_HANDLE;
inline VkPipeline               canvas_pipeline         = VK_NULL_HANDLE;

// Audio command buffer (persistent host-visible mapping)
inline VkBuffer                 audio_cmd_buffer        = VK_NULL_HANDLE;
inline VkDeviceMemory           audio_cmd_memory        = VK_NULL_HANDLE;
inline void*                    audio_cmd_mapped        = nullptr;

// ────────────────────────────────────────────────
// Sun / moon direction helpers
// ────────────────────────────────────────────────
[[nodiscard]] inline glm::vec3 computeSunDirection(f32 todHours) noexcept {
    f32 angle = (todHours / 24.0f) * glm::two_pi<f32>() - glm::half_pi<f32>();
    return glm::normalize(glm::vec3(
        std::cos(angle) * 0.8f,
        std::sin(angle),
        std::cos(angle) * 0.6f
    ));
}

[[nodiscard]] inline glm::vec3 computeMoonDirection(f32 todHours) noexcept {
    return -computeSunDirection(todHours);
}

// ────────────────────────────────────────────────
// Load shader module (CANVAS.spv)
// ────────────────────────────────────────────────
[[nodiscard]] inline std::expected<VkShaderModule, std::string> load_canvas_shader(
    const std::string& override_path = "") noexcept
{
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;

    if (!override_path.empty()) {
        fs::path op(override_path);
        if (fs::exists(op) && fs::is_regular_file(op)) candidates.push_back(op);
    }

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
            LOG_SUCCESS_CAT("PIPELINE", "Loaded shader from {}", p.string());
            break;
        }
        code.clear();
    }

    if (code.empty()) {
        return std::unexpected("Failed to load CANVAS.spv from any candidate path");
    }

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode    = code.data();

    VkShaderModule mod = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(rtx().device, &ci, nullptr, &mod);
    if (res != VK_SUCCESS) {
        return std::unexpected(std::format("vkCreateShaderModule failed: {}", static_cast<int>(res)));
    }

    return mod;
}

// ────────────────────────────────────────────────
// Initialize descriptor layout
// ────────────────────────────────────────────────
inline void initialize_descriptors() noexcept {
    if (main_descriptor_layout) return;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = std::size(CanvasBindings::bindings);
    ci.pBindings    = CanvasBindings::bindings;

    VkResult res = vkCreateDescriptorSetLayout(rtx().device, &ci, nullptr, &main_descriptor_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("DESCRIPTOR", "Failed to create descriptor layout: {}", static_cast<int>(res));
        return;
    }

    LOG_SUCCESS_CAT("DESCRIPTOR", "Descriptor layout created (7 bindings incl. audio commands)");
}

// ────────────────────────────────────────────────
// Create pipeline layout
// ────────────────────────────────────────────────
inline void create_pipeline_layout() noexcept {
    if (pipeline_layout) return;

    initialize_descriptors();

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
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
        LOG_FATAL_CAT("PIPELINE", "vkCreatePipelineLayout failed: {}", static_cast<int>(res));
        return;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout ready with push constants");
}

// ────────────────────────────────────────────────
// Create persistent host-visible audio command buffer
// ────────────────────────────────────────────────
inline void create_audio_command_buffer() noexcept {
    LOG_AMOURANTH("Audio command buffer creation and mapping.");
    if (audio_cmd_buffer) return;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = sizeof(AudioCommandBlock);
    bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(rtx().device, &bufInfo, nullptr, &audio_cmd_buffer);

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, audio_cmd_buffer, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize  = req.size;
    alloc.memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(rtx().device, &alloc, nullptr, &audio_cmd_memory);
    vkBindBufferMemory(rtx().device, audio_cmd_buffer, audio_cmd_memory, 0);
    vkMapMemory(rtx().device, audio_cmd_memory, 0, sizeof(AudioCommandBlock), 0, &audio_cmd_mapped);

    std::memset(audio_cmd_mapped, 0, sizeof(AudioCommandBlock));
}

// ────────────────────────────────────────────────
// Create or recreate compute pipeline
// ────────────────────────────────────────────────
inline void create_canvas_pipeline(const std::string& override_path = "") noexcept {
    if (canvas_pipeline) {
        vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr);
        canvas_pipeline = VK_NULL_HANDLE;
    }

    create_pipeline_layout();
    create_audio_command_buffer();

    auto shader_res = load_canvas_shader(override_path);
    if (!shader_res) {
        LOG_ERROR_CAT("PIPELINE", "Cannot load shader: {}", shader_res.error());
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
        LOG_ERROR_CAT("PIPELINE", "vkCreateComputePipelines failed: {}", static_cast<int>(res));
        return;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Compute pipeline created (CANVAS.spv loaded)");
}

// ────────────────────────────────────────────────
// Process audio commands from shader (call after compute dispatch completes)
// ────────────────────────────────────────────────
inline void process_shader_audio_commands() noexcept {
    if (!audio_cmd_mapped) return;

    AudioCommandBlock* cmd = static_cast<AudioCommandBlock*>(audio_cmd_mapped);

    for (int slot = 0; slot < 16; ++slot) {
        f32 command = cmd->slotCommand[slot];
        f32 value   = cmd->slotValue[slot];

        if (command > 0.51f) {
            // Trigger play
            std::string file = "assets/audio/sfx_slot_" + std::to_string(slot) + ".wav";
            INPUT.playSound(file, "play", slot);
            LOG_INFO_CAT("AUDIO_SHADER", "Play triggered on slot {} (value={})", slot, value);
        }
        else if (command >= 0.20f && command <= 0.50f) {
            // Continuous volume
            f32 normalized_vol = glm::clamp(value, 0.0f, 1.2f);
            // TODO: INPUT.setTrackVolume(slot, normalized_vol);
            LOG_INFO_CAT("AUDIO_SHADER", "Volume set on slot {} → {:.3f}", slot, normalized_vol);
        }
        else if (command < -0.1f) {
            // Stop / pause
            INPUT.playSound("", "stop", slot);
            LOG_INFO_CAT("AUDIO_SHADER", "Stop requested on slot {}", slot);
        }
    }

    // Clear for next frame
    std::memset(cmd, 0, sizeof(AudioCommandBlock));
}

// ────────────────────────────────────────────────
// Main dispatch — full input to shader, camera from singleton, audio feedback
// ────────────────────────────────────────────────
inline void dispatch_canvas(VkCommandBuffer cmd,
                            int width, int height,
                            float totalTime) noexcept
{
    if (!canvas_pipeline) {
        create_canvas_pipeline();
        if (!canvas_pipeline) return;
    }

    if (width <= 0 || height <= 0) {
        LOG_WARNING_CAT("PIPELINE", "Invalid dispatch size: {}x{}", width, height);
        return;
    }

    PushConstants pc{};
    pc.time             = totalTime;
	pc.enableHardwareRT = Options::Rendering::EnableHardwareRayTracing ? 1u : 0u;
    pc.frameSeed        = static_cast<u32>(totalTime * 987654.321f) ^ 0xCAFEBABEu;

    // ── Camera (use singleton methods for runtime state) ─────────────────────
    pc.cameraPos        = CAM.position();
    pc.cameraQuat       = glm::vec4(CAM.orientation().x, CAM.orientation().y,
                                    CAM.orientation().z, CAM.orientation().w);
    pc.cameraFovDeg     = CAM.fovDeg();
    pc.aspectRatio      = static_cast<f32>(width) / static_cast<f32>(height);
    pc.nearPlane        = Options::Camera::NearPlane;
    pc.farPlane         = Options::Camera::FarPlane;

    // ── Post-process (shader applies) ────────────────────────────────────────
    pc.exposure         = Options::Rendering::Exposure;
    pc.vignetteStrength = Options::Rendering::VignetteStrength;
    pc.bloomThreshold   = Options::Rendering::BloomThreshold;
    pc.bloomIntensity   = Options::Rendering::BloomIntensity;
    pc.tonemapMode      = Options::Rendering::EnableTonemapping;
    pc.contrast         = Options::Rendering::Contrast;
    pc.saturation       = Options::Rendering::Saturation;
    pc.gamma            = Options::Rendering::Gamma;

    // ── Environment / TOD (Howie©) ───────────────────────────────────────────
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

    // ── Raymarching quality ──────────────────────────────────────────────────
    pc.raymarchMaxDist  = Options::Rendering::RaymarchMaxDistance;
    pc.raymarchEpsilon  = Options::Rendering::RaymarchEpsilon;
    pc.raymarchMaxSteps = Options::Rendering::RaymarchMaxSteps;

    // ── Full input state (keyboard + mouse + controller) ─────────────────────
    pc.controllerInput = 0u;

    // Keyboard actions (string-based → bitflag)
    if (INPUT.down("move_forward"))  pc.controllerInput |= Options::Input::Flags::FORWARD;
    if (INPUT.down("move_backward")) pc.controllerInput |= Options::Input::Flags::BACKWARD;
    if (INPUT.down("move_left"))     pc.controllerInput |= Options::Input::Flags::LEFT;
    if (INPUT.down("move_right"))    pc.controllerInput |= Options::Input::Flags::RIGHT;

    if (INPUT.down("sprint"))        pc.controllerInput |= Options::Input::Flags::SPRINT;
    if (INPUT.down("crouch"))        pc.controllerInput |= Options::Input::Flags::CROUCH;
    if (INPUT.down("jump"))          pc.controllerInput |= Options::Input::Flags::JUMP;
    if (INPUT.down("interact"))      pc.controllerInput |= Options::Input::Flags::INTERACT;
    if (INPUT.down("shoot"))         pc.controllerInput |= Options::Input::Flags::SHOOT;

    // Mouse buttons
    Uint32 mouse_state = SDL_GetMouseState(nullptr, nullptr);
    if (mouse_state & SDL_BUTTON_LMASK) pc.controllerInput |= Options::Input::Flags::MOUSE_LEFT;
    if (mouse_state & SDL_BUTTON_RMASK) pc.controllerInput |= Options::Input::Flags::MOUSE_RIGHT;
    if (mouse_state & SDL_BUTTON_MMASK) pc.controllerInput |= Options::Input::Flags::MOUSE_MIDDLE;

    // Analog input (sticks & triggers)
    int ctrl_slot = 0; // primary controller
    pc.leftStickX   = INPUT.leftStickX(ctrl_slot);
    pc.leftStickY   = INPUT.leftStickY(ctrl_slot);
    pc.rightStickX  = INPUT.rightStickX(ctrl_slot);
    pc.rightStickY  = INPUT.rightStickY(ctrl_slot);
    pc.leftTrigger  = INPUT.leftTrigger(ctrl_slot);
    pc.rightTrigger = INPUT.rightTrigger(ctrl_slot);

    // Mouse delta & normalized position
    glm::vec2 delta = INPUT.mouseDelta();
    pc.mouseDelta       = delta;
    pc.mouseNormalized  = glm::vec2(
        (delta.x + static_cast<f32>(width)  * 0.5f) / static_cast<f32>(width),
        (delta.y + static_cast<f32>(height) * 0.5f) / static_cast<f32>(height)
    );
    pc.mouseWheelDelta  = 0.0f; // TODO: accumulate from SDL wheel events if needed

    // ── Vulkan dispatch ──────────────────────────────────────────────────────
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, canvas_pipeline);
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(PushConstants), &pc);

    // Assume descriptor set already bound (output, prev frame, audio SSBO, etc.)

    u32 dx = (static_cast<u32>(width)  + 15u) / 16u;
    u32 dy = (static_cast<u32>(height) + 15u) / 16u;
    vkCmdDispatch(cmd, dx, dy, 1u);

    // Audio feedback (after fence/wait in real usage)
    process_shader_audio_commands();
}

// ────────────────────────────────────────────────
// Cleanup everything
// ────────────────────────────────────────────────
inline void shutdown() noexcept {
    process_shader_audio_commands(); // flush any last commands

    if (audio_cmd_mapped) {
        vkUnmapMemory(rtx().device, audio_cmd_memory);
        audio_cmd_mapped = nullptr;
    }
    if (audio_cmd_buffer) {
        vkDestroyBuffer(rtx().device, audio_cmd_buffer, nullptr);
        audio_cmd_buffer = VK_NULL_HANDLE;
    }
    if (audio_cmd_memory) {
        vkFreeMemory(rtx().device, audio_cmd_memory, nullptr);
        audio_cmd_memory = VK_NULL_HANDLE;
    }

    if (canvas_pipeline) {
        vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr);
        canvas_pipeline = VK_NULL_HANDLE;
    }
    if (pipeline_layout) {
        vkDestroyPipelineLayout(rtx().device, pipeline_layout, nullptr);
        pipeline_layout = VK_NULL_HANDLE;
    }
    if (main_descriptor_layout) {
        vkDestroyDescriptorSetLayout(rtx().device, main_descriptor_layout, nullptr);
        main_descriptor_layout = VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Full shutdown complete — canvas, audio buffer, descriptors released");
}

} // namespace Pipeline