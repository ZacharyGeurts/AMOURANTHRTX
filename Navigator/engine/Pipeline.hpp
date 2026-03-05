#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Pipeline (fixed, C++23, developer-friendly)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "OptionsMenu.hpp"
#include "camera.hpp"           // ← added: for CAM singleton access

#include <algorithm>
#include <vector>
#include <fstream>
#include <format>
#include <filesystem>
#include <cstdint>
#include <string>
#include <expected>
#include <ranges>

namespace Pipeline {

using u32 = std::uint32_t;

// ────────────────────────────────────────────────
// Descriptor bindings (matches your current shader)
// ────────────────────────────────────────────────
inline constexpr VkDescriptorSetLayoutBinding kCanvasBindings[5] = {
    {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},  // outputImage
    {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},  // (unused or future)
    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},  // (unused or future)
    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},  // (unused or future)
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},  // (unused or future)
};

inline constexpr VkShaderStageFlags COMPUTE_PUSH_MASK = VK_SHADER_STAGE_COMPUTE_BIT;

// ────────────────────────────────────────────────
// Push constants — matches current shader layout
// ────────────────────────────────────────────────
struct PushConstants {
    float    time;          // pc.time
    u32      frameSeed;     // pc.frameSeed
    glm::vec3 cameraPos;    // pc.cameraPos
    float    _pad0;         // align vec3 to 16 bytes
    glm::vec4 cameraQuat;   // pc.cameraQuat (xyzw)
    float    cameraFov;     // pc.cameraFov
    float    _pad1[3];      // pad to 16-byte multiple
};

inline VkDescriptorSetLayout main_descriptor_layout = VK_NULL_HANDLE;
inline VkPipelineLayout      pipeline_layout        = VK_NULL_HANDLE;
inline VkPipeline            canvas_pipeline        = VK_NULL_HANDLE;

// ────────────────────────────────────────────────
// Load SPV shader — returns expected<module, error string>
// ────────────────────────────────────────────────
[[nodiscard]] inline std::expected<VkShaderModule, std::string> load_canvas_shader(
    const std::string& override_path = "",
    bool verbose = false) noexcept 
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
        if (sz <= 0 || sz % 4 != 0) {
            if (verbose) {
                LOG_WARNING_CAT("PIPELINE", "Skipping invalid file size: {} bytes from {}", 
                                static_cast<int64_t>(sz), path.string());
            }
            continue;
        }

        file.seekg(0);
        code.resize(static_cast<size_t>(sz / 4));
        file.read(reinterpret_cast<char*>(code.data()), sz);

        if (file.good()) {
            if (verbose) {
                LOG_SUCCESS_CAT("PIPELINE", "Loaded {} bytes from {}", 
                                static_cast<int64_t>(sz), path.string());
            }
            break;
        }
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
    if (main_descriptor_layout != VK_NULL_HANDLE) return;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = std::size(kCanvasBindings);
    ci.pBindings    = kCanvasBindings;

    vkh.checker(vkCreateDescriptorSetLayout(rtx().device, &ci, nullptr, &main_descriptor_layout),
                "DESCRIPTOR", "vkCreateDescriptorSetLayout");
}

// ────────────────────────────────────────────────
// Create pipeline layout with push constants
// ────────────────────────────────────────────────
inline void create_pipeline_layout() noexcept {
    if (pipeline_layout != VK_NULL_HANDLE) return;

    initialize();

    VkPushConstantRange push{};
    push.stageFlags = COMPUTE_PUSH_MASK;
    push.offset     = 0;
    push.size       = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo ci{};
    ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.setLayoutCount         = 1;
    ci.pSetLayouts            = &main_descriptor_layout;
    ci.pushConstantRangeCount = 1;
    ci.pPushConstantRanges    = &push;

    vkh.checker(vkCreatePipelineLayout(rtx().device, &ci, nullptr, &pipeline_layout),
                "PIPELINE", "vkCreatePipelineLayout");
}

// ────────────────────────────────────────────────
// Create / recreate compute pipeline
// ────────────────────────────────────────────────
inline void create_canvas_pipeline(const std::string& shader_override = "", bool verbose = false) noexcept {
    if (canvas_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr);
        canvas_pipeline = VK_NULL_HANDLE;
    }

    create_pipeline_layout();

    auto shader_result = load_canvas_shader(shader_override, verbose);
    if (!shader_result) {
        LOG_ERROR_CAT("PIPELINE", "Shader load failed: {}", shader_result.error());
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
                "PIPELINE", "vkCreateComputePipelines");

    vkDestroyShaderModule(rtx().device, shader, nullptr);
    LOG_SUCCESS_CAT("PIPELINE", "Canvas compute pipeline created");
}

// ────────────────────────────────────────────────
// Dispatch — now pushes real camera data
// ────────────────────────────────────────────────
inline void dispatch_canvas(VkCommandBuffer cmd, u32 width, u32 height, float totalTime) noexcept {
    if (canvas_pipeline == VK_NULL_HANDLE) {
        create_canvas_pipeline();
        if (canvas_pipeline == VK_NULL_HANDLE) return;
    }

    if (width == 0 || height == 0) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, canvas_pipeline);

    // Real push constants — synced with shader
    PushConstants pc{};
    pc.time       = totalTime;
    pc.frameSeed  = static_cast<u32>(totalTime * 1000.0f) ^ 0xDEADBEEFu;

    // Pull real camera data
    pc.cameraPos  = CAM.position();
    pc.cameraQuat = glm::vec4(CAM.orientation().x,
                              CAM.orientation().y,
                              CAM.orientation().z,
                              CAM.orientation().w);
    pc.cameraFov  = CAM.fov();

    vkCmdPushConstants(cmd, pipeline_layout, COMPUTE_PUSH_MASK,
                       0, sizeof(PushConstants), &pc);

    u32 dx = (width  + 15) / 16;
    u32 dy = (height + 15) / 16;

    vkCmdDispatch(cmd, dx, dy, 1);
}

// ────────────────────────────────────────────────
// Shutdown — clean up all resources
// ────────────────────────────────────────────────
inline void shutdown() noexcept {
    if (canvas_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr);
        canvas_pipeline = VK_NULL_HANDLE;
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

// ────────────────────────────────────────────────
// Developer hot-reload entry point
// ────────────────────────────────────────────────
inline void hot_reload_shader(const std::string& path = "compute/canvas.spv", bool verbose = false) noexcept {
    LOG_INFO_CAT("PIPELINE", "Hot-reloading canvas shader from: {}", path);
    create_canvas_pipeline(path, verbose);
}

} // namespace Pipeline