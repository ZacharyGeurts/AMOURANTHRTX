#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Pipeline (Living World Edition)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
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
// Descriptor bindings (matches shader)
struct CanvasBindings {
    static constexpr VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // output HDR
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // scene globals (future)
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // geometry/instances
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // lights/environment
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // materials
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // textures/procedural
    };
};

// ────────────────────────────────────────────────
// Push constants — full living world + camera + adaptive
// Must stay aligned to 16 bytes
// ────────────────────────────────────────────────
struct alignas(16) PushConstants {
    float       time;                   // total engine seconds
    u32         frameSeed;              // RNG seed (frame-dependent)

    glm::vec3   cameraPos;              float pad0;
    glm::vec4   cameraQuat;             // orientation xyzw
    float       cameraFovDeg;
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
    float       dayNightFactor;         // 0 = midnight, 0.5 = noon, etc.
    float       cloudCoverage;
    u32         debugFlags;

    // Adaptive / quality controls
    int         samplesPerPixel;
    float       temporalBlendStrength;
    int         maxRayRecursion;

    float       pad1[3];                // keep multiple of 16 bytes
};

// ────────────────────────────────────────────────
inline VkDescriptorSetLayout main_descriptor_layout = VK_NULL_HANDLE;
inline VkPipelineLayout      pipeline_layout        = VK_NULL_HANDLE;
inline VkPipeline            canvas_pipeline        = VK_NULL_HANDLE;

// ────────────────────────────────────────────────
// Procedural sun/moon direction from time-of-day
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
// Load SPIR-V shader module
// ────────────────────────────────────────────────
[[nodiscard]] inline std::expected<VkShaderModule, std::string> load_canvas_shader(
    [[maybe_unused]] const std::string& override_path = "",
    [[maybe_unused]] bool verbose = false) noexcept
{
    constexpr std::string_view shader_name = "canvas.spv";
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

    // Project root heuristic
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
        return std::unexpected(std::format("canvas.spv not found — checked {} paths", candidates.size()));
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
inline void initialize() noexcept {
    if (main_descriptor_layout) return;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = std::size(CanvasBindings::bindings);
    ci.pBindings    = CanvasBindings::bindings;

    vkh.checker(vkCreateDescriptorSetLayout(rtx().device, &ci, nullptr, &main_descriptor_layout),
                "DESCRIPTOR", "main layout");
}

// ────────────────────────────────────────────────
// Pipeline layout with push constants
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
// Create/recreate compute pipeline
// ────────────────────────────────────────────────
inline void create_canvas_pipeline(const std::string& shader_override = "", bool verbose = false) noexcept {
    if (canvas_pipeline) {
        vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr);
        canvas_pipeline = VK_NULL_HANDLE;
    }

    create_pipeline_layout();

    auto shader_result = load_canvas_shader(shader_override, verbose);
    if (!shader_result) {
        LOG_ERROR_CAT("PIPELINE", "Failed to load canvas shader: {}", shader_result.error());
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

    vkh.checker(vkCreateComputePipelines(rtx().device, VK_NULL_HANDLE, 1, &ci, nullptr, &canvas_pipeline),
                "PIPELINE", "canvas compute");

    vkDestroyShaderModule(rtx().device, shader, nullptr);
    LOG_SUCCESS_CAT("PIPELINE", "Living World canvas pipeline ready");
}

// ────────────────────────────────────────────────
// Dispatch — pushes full living world state
// ────────────────────────────────────────────────
inline void dispatch_canvas(VkCommandBuffer cmd,
                            int width, int height,
                            float totalTime) noexcept
{
    if (!canvas_pipeline) {
        create_canvas_pipeline();
        if (!canvas_pipeline) return;
    }

    if (width <= 0 || height <= 0) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, canvas_pipeline);

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
    float tod = Options::LivingWorld::CurrentTimeOfDay;  // 0..24
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

    pc.samplesPerPixel     = Options::Rendering::MAX_SAMPLES_PER_PIXEL;
    pc.temporalBlendStrength = Options::Rendering::TemporalBlendStrength;
    pc.maxRayRecursion     = Options::Rendering::MAX_RAY_RECURSION;

    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(PushConstants), &pc);

    // Safe unsigned dispatch grid calculation
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
}

// ────────────────────────────────────────────────
// Hot-reload support
// ────────────────────────────────────────────────
inline void hot_reload_shader(const std::string& path = "compute/canvas.spv",
                              [[maybe_unused]] bool verbose = false) noexcept {
    LOG_INFO_CAT("PIPELINE", "Hot-reloading shader: {}", path);
    create_canvas_pipeline(path, verbose);
}

} // namespace Pipeline