#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Pipeline (pure raymarched 3D)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// Pure raymarching pipeline — no 2D SDF canvas layer anymore
// Loads raymarch.spv (full-scene raymarcher)
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "OptionsMenu.hpp"
#include "Camera.hpp"           // CAM singleton
#include "Materials.hpp"

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
// Descriptor bindings (matches shader set=0)
// Order must match shader bindings exactly
// ────────────────────────────────────────────────
struct RaymarchBindings {
    static constexpr VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // output HDR image
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // previous HDR (for accumulation/TAA)
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // geometry/instances (optional)
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // lights/environment
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // materials
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // textures / procedural data
    };
};

// ────────────────────────────────────────────────
// Push constants — same as before (camera + raymarch tunables)
// Must stay 16-byte aligned
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

    float       pad1[3];                // Padding to 16-byte alignment
};

// ────────────────────────────────────────────────
// Global pipeline objects
// ────────────────────────────────────────────────
inline VkDescriptorSetLayout main_descriptor_layout = VK_NULL_HANDLE;
inline VkPipelineLayout      pipeline_layout        = VK_NULL_HANDLE;
inline VkPipeline            raymarch_pipeline      = VK_NULL_HANDLE;  // renamed from canvas_pipeline

// ────────────────────────────────────────────────
// Sun/moon direction helpers (unchanged)
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
// Load SPIR-V (default: raymarch.spv)
// ────────────────────────────────────────────────
[[nodiscard]] inline std::expected<VkShaderModule, std::string> load_raymarch_shader(
    const std::string& override_path = "") noexcept
{
    constexpr std::string_view shader_name = "CANVAS.spv";
    namespace fs = std::filesystem;

    std::vector<fs::path> candidates;

    if (!override_path.empty()) {
        fs::path op(override_path);
        if (fs::exists(op)) candidates.push_back(std::move(op));
    }

    fs::path exe_dir;
    try { exe_dir = fs::canonical("/proc/self/exe").parent_path(); } catch (...) {}
    if (!exe_dir.empty()) {
        candidates.emplace_back(exe_dir / "compute" / shader_name);
        candidates.emplace_back(exe_dir / shader_name);
    }

    fs::path cwd = fs::current_path();
    candidates.emplace_back(cwd / "compute" / shader_name);
    candidates.emplace_back(cwd / "bin" / "Linux" / "compute" / shader_name);

    fs::path root = cwd;
    for (int i = 0; i < 6; ++i) {
        if (fs::exists(root / "CMakeLists.txt") || fs::exists(root / "Navigator")) break;
        if (root == root.parent_path()) break;
        root = root.parent_path();
    }
    candidates.emplace_back(root / "compute" / shader_name);
    candidates.emplace_back(root / "bin" / "Linux" / "compute" / shader_name);

    std::vector<uint32_t> code;

    for (const auto& path : candidates) {
        if (!fs::exists(path)) continue;

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) continue;

        std::streamoff sz = file.tellg();
        if (sz <= 0 || sz % 4 != 0) continue;

        file.seekg(0);
        code.resize(static_cast<size_t>(sz / 4));
        file.read(reinterpret_cast<char*>(code.data()), sz);

        if (file.good()) break;
        code.clear();
    }

    if (code.empty()) {
        return std::unexpected(std::format("raymarch.spv not found — checked {} paths", candidates.size()));
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
// Initialize descriptor layout (once)
// ────────────────────────────────────────────────
inline void initialize() noexcept {
    if (main_descriptor_layout) return;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = std::size(RaymarchBindings::bindings);
    ci.pBindings    = RaymarchBindings::bindings;

    vkh.checker(vkCreateDescriptorSetLayout(rtx().device, &ci, nullptr, &main_descriptor_layout),
                "DESCRIPTOR", "main layout");
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

    vkh.checker(vkCreatePipelineLayout(rtx().device, &ci, nullptr, &pipeline_layout),
                "PIPELINE", "layout");
}

// ────────────────────────────────────────────────
// Create or recreate the raymarching compute pipeline
// ────────────────────────────────────────────────
inline void create_raymarch_pipeline(const std::string& shader_override = "") noexcept {
    if (raymarch_pipeline) {
        vkDestroyPipeline(rtx().device, raymarch_pipeline, nullptr);
        raymarch_pipeline = VK_NULL_HANDLE;
    }

    create_pipeline_layout();

    auto shader_result = load_raymarch_shader(shader_override);
    if (!shader_result) {
        LOG_ERROR_CAT("PIPELINE", "Failed to load raymarch shader: {}", shader_result.error());
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

    vkh.checker(vkCreateComputePipelines(rtx().device, VK_NULL_HANDLE, 1, &ci, nullptr, &raymarch_pipeline),
                "PIPELINE", "raymarch compute");

    vkDestroyShaderModule(rtx().device, shader, nullptr);
    LOG_SUCCESS_CAT("PIPELINE", "Raymarching pipeline ready");
}

// ────────────────────────────────────────────────
// Dispatch the raymarching compute shader
// ────────────────────────────────────────────────
inline void dispatch_raymarch(VkCommandBuffer cmd,
                              int width, int height,
                              float totalTime) noexcept
{
    if (!raymarch_pipeline) {
        create_raymarch_pipeline();
        if (!raymarch_pipeline) return;
    }

    if (width <= 0 || height <= 0) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, raymarch_pipeline);

    PushConstants pc{};
    pc.time         = totalTime;
    pc.frameSeed    = static_cast<u32>(totalTime * 987654.321f) ^ 0xCAFEBABEu;

    // Camera
    pc.cameraPos    = CAM.position();
    pc.cameraQuat   = glm::vec4(CAM.orientation().x, CAM.orientation().y, CAM.orientation().z, CAM.orientation().w);
    pc.cameraFovDeg = CAM.fov();
    pc.aspectRatio  = static_cast<float>(width) / static_cast<float>(height);
    pc.exposure     = Options::Rendering::EXPOSURE;

    // Living World
    float tod = Options::LivingWorld::CurrentTimeOfDay;
    float todFrac = tod / 24.0f;

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
    pc.dayNightFactor      = todFrac;
    pc.cloudCoverage       = Options::LivingWorld::CloudCoverage;
    pc.debugFlags          = Options::LivingWorld::DebugFlags;

    // Raymarching tunables
    pc.raymarchMaxDist     = Options::Rendering::RaymarchMaxDistance;
    pc.raymarchEpsilon     = Options::Rendering::RaymarchEpsilon;
    pc.raymarchMaxSteps    = Options::Rendering::RaymarchMaxSteps;

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
    if (raymarch_pipeline) {
        vkDestroyPipeline(rtx().device, raymarch_pipeline, nullptr);
        raymarch_pipeline = VK_NULL_HANDLE;
    }
    if (pipeline_layout) {
        vkDestroyPipelineLayout(rtx().device, pipeline_layout, nullptr);
        pipeline_layout = VK_NULL_HANDLE;
    }
    if (main_descriptor_layout) {
        vkDestroyDescriptorSetLayout(rtx().device, main_descriptor_layout, nullptr);
        main_descriptor_layout = VK_NULL_HANDLE;
    }
}

// ────────────────────────────────────────────────
// Hot-reload shader
// ────────────────────────────────────────────────
inline void hot_reload_shader(const std::string& path = "compute/raymarch.spv") noexcept {
    LOG_INFO_CAT("PIPELINE", "Hot-reloading raymarch shader: {}", path);
    create_raymarch_pipeline(path);
}

} // namespace Pipeline