#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Pipeline (pure raymarched 3D)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// Pure raymarching pipeline — full controller + keyboard input to shader
// Loads CANVAS.spv (full-scene raymarcher from CANVAS.comp)
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "OptionsMenu.hpp"
#include "Camera.hpp"           // CAM singleton
#include "Materials.hpp"
#include "SDL3.hpp"             // INPUT access

#include <algorithm>
#include <vector>
#include <fstream>
#include <format>
#include <filesystem>
#include <cstdint>
#include <string>
#include <expected>
#include <cmath>

namespace Pipeline {

using u32 = std::uint32_t;

// ────────────────────────────────────────────────
// Input bitflags for shader (digital buttons)
// ────────────────────────────────────────────────
constexpr u32 INPUT_FORWARD     = 1u << 0;
constexpr u32 INPUT_BACKWARD    = 1u << 1;
constexpr u32 INPUT_LEFT        = 1u << 2;
constexpr u32 INPUT_RIGHT       = 1u << 3;
constexpr u32 INPUT_SPRINT      = 1u << 4;
constexpr u32 INPUT_CROUCH      = 1u << 5;
constexpr u32 INPUT_JUMP        = 1u << 6;
constexpr u32 INPUT_INTERACT    = 1u << 7;
constexpr u32 INPUT_SHOOT       = 1u << 8;

// ────────────────────────────────────────────────
// Descriptor bindings (matches shader set=0)
// ────────────────────────────────────────────────
struct CanvasBindings {
    static constexpr VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // output HDR image
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // previous HDR (accumulation/TAA)
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // geometry/instances (optional)
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // lights/environment
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // materials
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // textures / procedural data
    };
};

// ────────────────────────────────────────────────
// Push constants — 16-byte aligned, full controller support
// ────────────────────────────────────────────────
struct alignas(16) PushConstants {
    float       time;                   // Engine seconds
    u32         frameSeed;              // Per-frame RNG seed

    glm::vec3   cameraPos;              float pad0;
    glm::vec4   cameraQuat;             // Orientation quaternion
    float       cameraFovDeg;           // Vertical FOV degrees
    float       aspectRatio;
    float       exposure;

    glm::vec3   sunDir;                 float sunIntensity;
    glm::vec3   moonDir;                float moonIntensity;

    glm::vec3   windDir;                float windStrength;
    float       temperatureC;
    float       humidity;
    float       airPressureKPa;
    float       precipitationFactor;

    float       fogDensity;
    float       dayNightFactor;
    float       cloudCoverage;

    u32         debugFlags;

    // Raymarching quality controls
    float       raymarchMaxDist;
    float       raymarchEpsilon;
    u32         raymarchMaxSteps;

    // ─────────────── Controller / Input Controls ───────────────
    u32         controllerInput;        // bitfield: forward, sprint, jump, etc.
    float       leftStickX;             // [-1,1] strafe
    float       leftStickY;             // [-1,1] forward/back (inverted)
    float       rightStickX;            // [-1,1] look yaw
    float       rightStickY;            // [-1,1] look pitch
    float       leftTrigger;            // [0,1] secondary action / aim
    float       rightTrigger;           // [0,1] primary action / sprint
    float       mouseX;                 // normalized [0,1] screen space
    float       mouseY;                 // normalized [0,1]

    float       pad1[2];                // Padding to 16-byte alignment
};

// ────────────────────────────────────────────────
// Global pipeline objects
// ────────────────────────────────────────────────
inline VkDescriptorSetLayout main_descriptor_layout = VK_NULL_HANDLE;
inline VkPipelineLayout      pipeline_layout        = VK_NULL_HANDLE;
inline VkPipeline            canvas_pipeline        = VK_NULL_HANDLE;

// ────────────────────────────────────────────────
// Sun/moon helpers
// ────────────────────────────────────────────────
[[nodiscard]] inline glm::vec3 computeSunDirection(float todHours) noexcept {
    float angle = (todHours / 24.0f) * glm::two_pi<float>() - glm::half_pi<float>();
    return glm::normalize(glm::vec3(
        std::cos(angle) * 0.8f,
        std::sin(angle),
        std::cos(angle) * 0.6f
    ));
}

[[nodiscard]] inline glm::vec3 computeMoonDirection(float todHours) noexcept {
    return -computeSunDirection(todHours);
}

// ────────────────────────────────────────────────
// Load SPIR-V shader (default: CANVAS.spv)
// ────────────────────────────────────────────────
[[nodiscard]] inline std::expected<VkShaderModule, std::string> load_canvas_shader(
    const std::string& override_path = "") noexcept
{
    constexpr std::string_view default_name = "CANVAS.spv";
    constexpr std::string_view default_rel_path = "assets/shaders/compute/CANVAS.spv";

    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;

    // 1. Explicit user override (highest priority)
    if (!override_path.empty()) {
        fs::path op(override_path);
        if (fs::exists(op) && fs::is_regular_file(op)) {
            candidates.push_back(std::move(op));
        } else {
            LOG_WARNING_CAT("PIPELINE", "Override path does not exist: {}", override_path);
        }
    }

    // 2. Get executable directory reliably
    fs::path exe_dir;
    try {
        exe_dir = fs::canonical("/proc/self/exe").parent_path();
    } catch (const fs::filesystem_error&) {
        // Fallback: try argv[0] or current path if /proc/self/exe fails
        LOG_WARNING_CAT("PIPELINE", "Failed to get exe path via /proc/self/exe");
    }

    if (!exe_dir.empty()) {
        // Modern expected location (what CMake now produces)
        candidates.emplace_back(exe_dir / default_rel_path);

        // Common alternatives during dev/build
        candidates.emplace_back(exe_dir / "assets" / "shaders" / "compute" / default_name);
        candidates.emplace_back(exe_dir / "compute" / default_name);
        candidates.emplace_back(exe_dir / default_name);
    }

    // 3. Current working directory fallbacks
    fs::path cwd = fs::current_path();
    candidates.emplace_back(cwd / default_rel_path);
    candidates.emplace_back(cwd / "assets" / "shaders" / "compute" / default_name);
    candidates.emplace_back(cwd / "compute" / default_name);

    // 4. Project root heuristic (last resort)
    fs::path root = cwd;
    for (int i = 0; i < 10; ++i) {  // increased slightly
        if (fs::exists(root / "CMakeLists.txt") || fs::exists(root / "Navigator")) {
            break;
        }
        if (root == root.parent_path()) break;
        root = root.parent_path();
    }
    if (fs::exists(root / "CMakeLists.txt")) {
        candidates.emplace_back(root / "build" / "bin" / "Linux" / default_rel_path);
        candidates.emplace_back(root / "assets" / "shaders" / "compute" / default_name);
    }

    // ── Try loading from candidates ─────────────────────────────────────
    std::vector<uint32_t> code;

    for (const auto& path : candidates) {
        if (!fs::exists(path) || !fs::is_regular_file(path)) {
            continue;
        }

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            LOG_WARNING_CAT("PIPELINE", "Cannot open: {}", path.string());
            continue;
        }

        auto size = file.tellg();

        file.seekg(0);
        code.resize(static_cast<size_t>(size) / 4);
        file.read(reinterpret_cast<char*>(code.data()), size);

        if (file.good()) {
            LOG_SUCCESS_CAT("PIPELINE", "Loaded CANVAS.spv from: {}", path.string());
            break;
        }

        LOG_WARNING_CAT("PIPELINE", "Failed to read fully: {}", path.string());
        code.clear();
    }

    if (code.empty()) {
        LOG_ERROR_CAT("PIPELINE", "Failed to find/load CANVAS.spv");
        LOG_ERROR_CAT("PIPELINE", "Checked {} locations:", candidates.size());
        for (const auto& p : candidates) {
            LOG_ERROR_CAT("PIPELINE", "  - {}", p.string());
        }
        return std::unexpected("CANVAS.spv not found in any expected location");
    }

    // ── Create shader module ────────────────────────────────────────────
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode    = code.data();

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(rtx().device, &ci, nullptr, &module);

    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "vkCreateShaderModule failed: {}", static_cast<int>(res));
        return std::unexpected(std::format("vkCreateShaderModule failed: {}", static_cast<int>(res)));
    }

    return module;
}

// ────────────────────────────────────────────────
// Initialize descriptor layout (once)
// ────────────────────────────────────────────────
inline void initialize() noexcept {
    if (main_descriptor_layout) return;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = std::size(CanvasBindings::bindings);
    ci.pBindings    = CanvasBindings::bindings;

    VkResult res = vkCreateDescriptorSetLayout(rtx().device, &ci, nullptr, &main_descriptor_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("DESCRIPTOR", "Failed to create main layout: {}", static_cast<int>(res));
        return;
    }

    LOG_SUCCESS_CAT("DESCRIPTOR", "Main layout created");
}

// ────────────────────────────────────────────────
// Create pipeline layout with push constants
// ────────────────────────────────────────────────
inline void create_pipeline_layout() noexcept {
    if (pipeline_layout) return;

    initialize();

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
        LOG_FATAL_CAT("PIPELINE", "Failed to create pipeline layout: {}", static_cast<int>(res));
        return;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout ready");
}

// ────────────────────────────────────────────────
// Create or recreate the canvas compute pipeline
// ────────────────────────────────────────────────
inline void create_canvas_pipeline(const std::string& shader_override = "") noexcept {
    if (canvas_pipeline) {
        vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr);
        canvas_pipeline = VK_NULL_HANDLE;
    }

    create_pipeline_layout();

    auto shader_result = load_canvas_shader(shader_override);
    if (!shader_result) {
        LOG_ERROR_CAT("PIPELINE", "Failed to load CANVAS.spv: {}", shader_result.error());
        return;
    }

    VkShaderModule shader = *shader_result;

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = shader;
    stage.pName  = "main";

    VkComputePipelineCreateInfo ci{};
    ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ci.stage  = stage;
    ci.layout = pipeline_layout;

    VkResult res = vkCreateComputePipelines(rtx().device, VK_NULL_HANDLE, 1, &ci, nullptr, &canvas_pipeline);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "vkCreateComputePipelines failed: {}", static_cast<int>(res));
        vkDestroyShaderModule(rtx().device, shader, nullptr);
        return;
    }

    vkDestroyShaderModule(rtx().device, shader, nullptr);
    LOG_SUCCESS_CAT("PIPELINE", "CANVAS pipeline ready (from CANVAS.comp → CANVAS.spv)");
}

// ────────────────────────────────────────────────
// Dispatch the canvas compute shader — full controller support
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
        LOG_WARNING_CAT("PIPELINE", "Invalid dispatch dimensions: {}x{}", width, height);
        return;
    }

    PushConstants pc{};
    pc.time         = totalTime;
    pc.frameSeed    = static_cast<u32>(totalTime * 987654.321f) ^ 0xCAFEBABEu;

    // Camera
    pc.cameraPos    = CAM.position();
    pc.cameraQuat   = glm::vec4(CAM.orientation().x, CAM.orientation().y, CAM.orientation().z, CAM.orientation().w);
    pc.cameraFovDeg = CAM.fov();
    pc.aspectRatio  = static_cast<float>(width) / static_cast<float>(height);
    pc.exposure     = Options::Rendering::Exposure;

    // Environment / Living World
    float tod = Options::LivingWorld::CurrentTimeOfDay;
    pc.sunDir       = computeSunDirection(tod);
    pc.moonDir      = computeMoonDirection(tod);

    float sunHeight = pc.sunDir.y;
    pc.sunIntensity = Options::LivingWorld::SunIntensityDay * std::max(0.0f, sunHeight) +
                      Options::LivingWorld::SunIntensityNight * (1.0f - std::max(0.0f, sunHeight));

    pc.moonIntensity = Options::LivingWorld::MoonIntensity * (1.0f - std::max(0.0f, sunHeight));

    pc.windDir             = glm::normalize(Options::LivingWorld::WindDirection);
    pc.windStrength        = Options::LivingWorld::WindStrength;
    pc.temperatureC        = Options::LivingWorld::TemperatureC;
    pc.humidity            = Options::LivingWorld::Humidity;
    pc.airPressureKPa      = Options::LivingWorld::AirPressureKPa;
    pc.precipitationFactor = Options::LivingWorld::PrecipitationFactor;
    pc.fogDensity          = Options::LivingWorld::FogDensity;
    pc.dayNightFactor      = tod / 24.0f;
    pc.cloudCoverage       = Options::LivingWorld::CloudCoverage;
    pc.debugFlags          = Options::Rendering::DebugFlags;

    // Raymarching quality controls
    pc.raymarchMaxDist     = Options::Rendering::RaymarchMaxDistance;
    pc.raymarchEpsilon     = Options::Rendering::RaymarchEpsilon;
    pc.raymarchMaxSteps    = Options::Rendering::RaymarchMaxSteps;

    // Controller / Input — full analog support
    int slot = 0; // primary controller

    pc.controllerInput = 0;
    if (INPUT.down("move_forward"))  pc.controllerInput |= INPUT_FORWARD;
    if (INPUT.down("move_backward")) pc.controllerInput |= INPUT_BACKWARD;
    if (INPUT.down("move_left"))     pc.controllerInput |= INPUT_LEFT;
    if (INPUT.down("move_right"))    pc.controllerInput |= INPUT_RIGHT;
    if (INPUT.down("sprint"))        pc.controllerInput |= INPUT_SPRINT;
    if (INPUT.down("crouch"))        pc.controllerInput |= INPUT_CROUCH;
    if (INPUT.down("jump"))          pc.controllerInput |= INPUT_JUMP;
    if (INPUT.down("interact"))      pc.controllerInput |= INPUT_INTERACT;
    if (INPUT.down("shoot"))         pc.controllerInput |= INPUT_SHOOT;

    pc.leftStickX   = INPUT.leftStickX(slot);
    pc.leftStickY   = INPUT.leftStickY(slot);
    pc.rightStickX  = INPUT.rightStickX(slot);
    pc.rightStickY  = INPUT.rightStickY(slot);
    pc.leftTrigger  = INPUT.leftTrigger(slot);
    pc.rightTrigger = INPUT.rightTrigger(slot);

    // Mouse (normalized screen space)
    glm::vec2 mouseDelta = INPUT.mouseDelta();
    pc.mouseX = mouseDelta.x / static_cast<float>(width);
    pc.mouseY = mouseDelta.y / static_cast<float>(height);

    // Submit to GPU
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, canvas_pipeline);

    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(PushConstants), &pc);

    u32 dx = (static_cast<u32>(width)  + 15u) / 16u;
    u32 dy = (static_cast<u32>(height) + 15u) / 16u;
    vkCmdDispatch(cmd, dx, dy, 1u);
}

// ────────────────────────────────────────────────
// Cleanup
// ────────────────────────────────────────────────
inline void shutdown() noexcept {
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

    LOG_SUCCESS_CAT("PIPELINE", "Shutdown complete");
}

// ────────────────────────────────────────────────
// Hot-reload shader
// ────────────────────────────────────────────────
inline void hot_reload_shader(const std::string& path = "assets/shaders/compute/CANVAS.spv") noexcept {
    LOG_INFO_CAT("PIPELINE", "Hot-reloading CANVAS shader: {}", path);
    create_canvas_pipeline(path);
}

} // namespace Pipeline