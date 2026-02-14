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
// Bindings — storage image first (most frequent access)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr VkDescriptorSetLayoutBinding kCanvasBindings[3] = {
    {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // 0: HDR output (write)
    {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // 1: Camera UBO
    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // 2: World / primitives buffer
};

inline constexpr VkShaderStageFlags COMPUTE_PUSH_MASK = VK_SHADER_STAGE_COMPUTE_BIT;

// ─────────────────────────────────────────────────────────────────────────────
// Namespace-local pipeline objects
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    VkDescriptorSetLayout main_descriptor_layout = VK_NULL_HANDLE;
    VkPipelineLayout      pipeline_layout        = VK_NULL_HANDLE;
    VkPipeline            canvas_pipeline        = VK_NULL_HANDLE;
}

// ─────────────────────────────────────────────────────────────────────────────
// Load the single compute shader: canvas.spv
// ─────────────────────────────────────────────────────────────────────────────
[[nodiscard]] inline VkShaderModule load_canvas_shader() noexcept {
    constexpr std::string_view shader_name = "canvas.spv";

    std::array<std::string, 5> search_roots = {
        "compute/", "build/bin/Linux/compute/", "build/bin/Windows/compute/",
        "shaders/", "./"
    };

    std::vector<uint32_t> code;

    for (const auto& root : search_roots) {
        std::string path = root + std::string(shader_name);
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open() || !file.good()) continue;

        // Get size safely
        file.seekg(0, std::ios::end);
        auto pos = file.tellg();
        file.seekg(0, std::ios::beg);

        if (pos <= 0 || pos % 4 != 0) {
            continue;
        }

        auto size_bytes = static_cast<std::streamsize>(pos);
        code.resize(static_cast<size_t>(size_bytes / 4));

        file.read(reinterpret_cast<char*>(code.data()), size_bytes);

        if (file.good() && !file.fail()) {
            // SAFE: cast to int64_t before formatting
            int64_t printable_size = static_cast<int64_t>(size_bytes);
            LOG_SUCCESS_CAT("PIPELINE", "Loaded canvas.spv ({} bytes) from {}", 
                            printable_size, path);
            break;
        }

        code.clear();
        file.close();
    }

    if (code.empty()) {
        LOG_ERROR_CAT("PIPELINE", "canvas.spv not found or corrupted in any search path");
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode    = code.data();

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(rtx().device, &ci, nullptr, &module);
    vkh.checker(res, "vkCreateShaderModule", "canvas.spv");

    return module;
}

// ─────────────────────────────────────────────────────────────────────────────
// One-time initialization — creates descriptor layout + pipeline layout
// ─────────────────────────────────────────────────────────────────────────────
inline void initialize() noexcept {
    if (main_descriptor_layout != VK_NULL_HANDLE) return;

    VkDescriptorSetLayoutCreateInfo layoutCI{};
    layoutCI.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCI.bindingCount = std::size(kCanvasBindings);
    layoutCI.pBindings    = kCanvasBindings;

    vkh.checker(vkCreateDescriptorSetLayout(rtx().device, &layoutCI, nullptr, &main_descriptor_layout),
                "vkCreateDescriptorSetLayout", "main canvas layout");

    VkPushConstantRange push{};
    push.stageFlags = COMPUTE_PUSH_MASK;
    push.offset     = 0;
    push.size       = sizeof(float);  // matches shader: float totalTime

    VkPipelineLayoutCreateInfo plCI{};
    plCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCI.setLayoutCount         = 1;
    plCI.pSetLayouts            = &main_descriptor_layout;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges    = &push;

    vkh.checker(vkCreatePipelineLayout(rtx().device, &plCI, nullptr, &pipeline_layout),
                "vkCreatePipelineLayout", "canvas");

    LOG_SUCCESS_CAT("PIPELINE", "Descriptor layout + pipeline layout created");
}

// ─────────────────────────────────────────────────────────────────────────────
// Create the single compute pipeline (lazy — called on first dispatch)
// ─────────────────────────────────────────────────────────────────────────────
inline void create_canvas_pipeline() noexcept {
    if (canvas_pipeline != VK_NULL_HANDLE) return;

    initialize();  // ensure layout exists

    VkShaderModule shader = load_canvas_shader();
    if (shader == VK_NULL_HANDLE) return;

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
                "vkCreateComputePipelines", "canvas compute pipeline");

    vkDestroyShaderModule(rtx().device, shader, nullptr);

    LOG_SUCCESS_CAT("PIPELINE", "Single canvas compute pipeline created");
}

// ─────────────────────────────────────────────────────────────────────────────
// Dispatch the canvas shader — called from RayCanvas
// ─────────────────────────────────────────────────────────────────────────────
inline void dispatch_canvas(VkCommandBuffer cmd, uint32_t width, uint32_t height, float totalTime) noexcept {
    if (canvas_pipeline == VK_NULL_HANDLE) {
        create_canvas_pipeline();
    }

    if (canvas_pipeline == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Cannot dispatch — pipeline creation failed");
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, canvas_pipeline);

    vkCmdPushConstants(cmd, pipeline_layout, COMPUTE_PUSH_MASK,
                       0, sizeof(float), &totalTime);

    uint32_t dispatchX = (width  + 15) / 16;
    uint32_t dispatchY = (height + 15) / 16;

    vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

    LOG_DEBUG_CAT("PIPELINE", "Dispatched canvas compute — {}×{} grid for {}×{} pixels @ t={:.3f}",
                  dispatchX, dispatchY, width, height, totalTime);
}

// ─────────────────────────────────────────────────────────────────────────────
// Cleanup — called from RayCanvas destructor
// ─────────────────────────────────────────────────────────────────────────────
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

    LOG_SUCCESS_CAT("PIPELINE", "Single compute pipeline shutdown complete");
}

inline void create_pipeline_layout() noexcept {
    if (pipeline_layout != VK_NULL_HANDLE) return;

    VkPushConstantRange push{};
    push.stageFlags = COMPUTE_PUSH_MASK;
    push.offset     = 0;
    push.size       = sizeof(float);  // totalTime

    VkPipelineLayoutCreateInfo plCI{};
    plCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCI.setLayoutCount         = 1;
    plCI.pSetLayouts            = &main_descriptor_layout;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges    = &push;

    vkh.checker(vkCreatePipelineLayout(rtx().device, &plCI, nullptr, &pipeline_layout),
                "vkCreatePipelineLayout", "canvas");

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout with push constants created");
}

} // namespace Pipeline