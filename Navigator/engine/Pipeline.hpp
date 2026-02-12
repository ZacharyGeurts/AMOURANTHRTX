#pragma once

// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
//
// SINGLE COMPUTE SHADER PIPELINE — canvas.spv is the ONLY shader
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"

#include <algorithm>
#include <array>
#include <vector>
#include <fstream>
#include <format>
#include <filesystem>

namespace Pipeline {

// ─────────────────────────────────────────────────────────────────────────────
// Bindings — optimized order: storage image at 0 (most accessed)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr VkDescriptorSetLayoutBinding kCanvasBindings[3] = {
    {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // HDR canvas image (primary output)
    {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Camera UBO
    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Optional world/primitives
};

static constexpr VkShaderStageFlags COMPUTE_PUSH_MASK = VK_SHADER_STAGE_COMPUTE_BIT;

// ─────────────────────────────────────────────────────────────────────────────
// Internal state (namespace-local)
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    VkPipelineLayout      pipeline_layout       = VK_NULL_HANDLE;
    VkDescriptorSetLayout main_descriptor_layout = VK_NULL_HANDLE;
    VkPipeline            canvas_pipeline       = VK_NULL_HANDLE;
}

// ─────────────────────────────────────────────────────────────────────────────
// Hardcoded loading of the single shader: canvas.spv
// ─────────────────────────────────────────────────────────────────────────────
inline VkShaderModule load_canvas_shader() {
    constexpr std::string_view shader_name = "canvas.spv";

    LOG_ATTEMPT_CAT("PIPELINE", "Loading the only shader: {}", shader_name);

    std::array<std::string, 5> search_roots = {
        "compute/",
        "build/bin/Linux/compute/",
        "build/bin/Windows/compute/"
    };

    std::string found_path;
    std::ifstream file;

    for (const auto& root : search_roots) {
        std::string candidate = root + std::string(shader_name);

        if (!std::filesystem::exists(candidate) || !std::filesystem::is_regular_file(candidate)) {
            continue;
        }

        file.open(candidate, std::ios::binary | std::ios::ate);
        if (file.is_open() && file.good()) {
            auto pos = file.tellg();
            if (pos != std::ifstream::pos_type(-1) && pos > 0) {
                found_path = candidate;
                break;
            }
        }

        // Clean up on failure
        if (file.is_open()) {
            file.close();
        }
        file.clear(); // reset any error flags for next attempt
    }

    if (!file.is_open() || found_path.empty()) {
        std::string checked_list;
        for (const auto& root : search_roots) {
            checked_list += std::format("  {}\n", root + std::string(shader_name));
        }
        std::string msg = std::format(
            "Shader '{}' not found, not readable, or empty after opening.\n"
            "Paths checked:\n{}",
            shader_name, checked_list
        );
        vkh.checker(false, "Shader file open", msg.c_str());
    }

    // At this point file is open, good, and tellg succeeded
    auto size_bytes = static_cast<std::streamsize>(file.tellg());

    constexpr std::streamsize MAX_SANE_SPIRV = 64LL * 1024 * 1024;

    vkh.checker(size_bytes > 0, "SPIR-V size > 0",
                std::format("File is empty: {}", found_path).c_str());

    vkh.checker(size_bytes % 4 == 0, "SPIR-V size multiple of 4",
                std::format("Size {} not multiple of 4 bytes: {}", size_bytes, found_path).c_str());

    vkh.checker(size_bytes <= MAX_SANE_SPIRV, "SPIR-V size sanity",
                std::format("File too large ({} bytes > {}): {}", size_bytes, MAX_SANE_SPIRV, found_path).c_str());

    std::vector<uint32_t> code(static_cast<size_t>(size_bytes / 4));

    file.seekg(0);
    vkh.checker(file.good(), "seekg(0) after size query",
                std::format("seek failed on: {}", found_path).c_str());

    file.read(reinterpret_cast<char*>(code.data()), size_bytes);

    auto bytes_read = file.gcount();
    vkh.checker(bytes_read == size_bytes && !file.fail() && !file.bad(),
                "SPIR-V file read complete",
                std::format("Failed to read {} bytes from {} (read {} bytes)",
                            size_bytes, found_path, bytes_read).c_str());

    file.close();

    LOG_SUCCESS_CAT("PIPELINE", "Loaded the only shader: {} ({} bytes / {} words) from {}",
                    shader_name, size_bytes, code.size(), found_path);

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode    = code.data();

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(rtx().device, &ci, nullptr, &module);

    vkh.checker(res == VK_SUCCESS,
                "vkCreateShaderModule",
                std::format("{} ({} bytes)", shader_name, ci.codeSize).c_str());

    return module;
}

// ─────────────────────────────────────────────────────────────────────────────
// Core setup — single canvas compute shader only
// ─────────────────────────────────────────────────────────────────────────────

inline void initialize() noexcept {
    LOG_SUCCESS_CAT("PIPELINE", "Single-shader pipeline initialized");
}

inline void create_pipeline_layout() noexcept {
    if (pipeline_layout != VK_NULL_HANDLE) return;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = std::size(kCanvasBindings);
    ci.pBindings    = kCanvasBindings;

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    vkh.checker(vkCreateDescriptorSetLayout(rtx().device, &ci, nullptr, &layout) == VK_SUCCESS,
                "vkCreateDescriptorSetLayout (canvas)", "Failed");

    main_descriptor_layout = layout;

    VkPushConstantRange push{};
    push.stageFlags = COMPUTE_PUSH_MASK;
    push.offset     = 0;
    push.size       = sizeof(float);  // totalTime

    VkPipelineLayoutCreateInfo plCI{};
    plCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCI.setLayoutCount         = 1;
    plCI.pSetLayouts            = &layout;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges    = &push;

    VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
    vkh.checker(vkCreatePipelineLayout(rtx().device, &plCI, nullptr, &pipeLayout) == VK_SUCCESS,
                "vkCreatePipelineLayout", "Failed");

    pipeline_layout = pipeLayout;

    LOG_SUCCESS_CAT("PIPELINE", "Single compute pipeline layout created");
}

inline void create_canvas_pipeline() noexcept {
    if (canvas_pipeline != VK_NULL_HANDLE) return;

    create_pipeline_layout();  // ensure layout exists

    VkShaderModule comp = load_canvas_shader();

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = comp;
    stage.pName  = "main";

    VkComputePipelineCreateInfo ci{};
    ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ci.stage  = stage;
    ci.layout = pipeline_layout;

    VkPipeline pipe = VK_NULL_HANDLE;
    vkh.checker(vkCreateComputePipelines(rtx().device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe) == VK_SUCCESS,
                "vkCreateComputePipelines", "Canvas pipeline failed");

    canvas_pipeline = pipe;

    vkDestroyShaderModule(rtx().device, comp, nullptr);

    LOG_SUCCESS_CAT("PIPELINE", "Single canvas compute pipeline created");
}

inline void dispatch_canvas(VkCommandBuffer cmd, uint32_t width, uint32_t height, float totalTime) noexcept {
    if (canvas_pipeline == VK_NULL_HANDLE) {
        create_canvas_pipeline();
    }

    vkh.checker(canvas_pipeline != VK_NULL_HANDLE, "Canvas pipeline", "Not created");

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, canvas_pipeline);

    vkCmdPushConstants(cmd, pipeline_layout, COMPUTE_PUSH_MASK, 0, sizeof(float), &totalTime);

    uint32_t gx = (width  + 15) / 16;
    uint32_t gy = (height + 15) / 16;
    vkCmdDispatch(cmd, gx, gy, 1);

    LOG_DEBUG_CAT("PIPELINE", "Canvas compute dispatched — {}x{} @ {:.3f}s", width, height, totalTime);
}

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

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline shutdown complete — single compute shader only");
}

} // namespace Pipeline