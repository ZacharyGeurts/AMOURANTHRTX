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
#include <filesystem>
#include <cstdint>

namespace Pipeline {

// ─────────────────────────────────────────────────────────────────────────────
// Bindings — matches the current shader (5 bindings)
// 0: storage image (HDR write)
// 1: uniform buffer (camera)
// 2: storage buffer (living world)
// 3: storage buffer (materials)
// 4: storage buffer (primitives / scene objects)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr VkDescriptorSetLayoutBinding kCanvasBindings[5] = {
    {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // 0: HDR output
    {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // 1: Camera UBO
    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // 2: LivingWorldBuffer
    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // 3: Materials buffer
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // 4: Primitives buffer
};

inline constexpr VkShaderStageFlags COMPUTE_PUSH_MASK = VK_SHADER_STAGE_COMPUTE_BIT;

// ─────────────────────────────────────────────────────────────────────────────
// Exposed pipeline objects & functions
// ─────────────────────────────────────────────────────────────────────────────
inline VkDescriptorSetLayout main_descriptor_layout = VK_NULL_HANDLE;
inline VkPipelineLayout      pipeline_layout        = VK_NULL_HANDLE;
inline VkPipeline            canvas_pipeline        = VK_NULL_HANDLE;

// ─────────────────────────────────────────────────────────────────────────────
// Push constant struct — EXACTLY matches the shader (8 bytes total)
// ─────────────────────────────────────────────────────────────────────────────
struct PushConstants {
    float time;
    uint  frameSeed;
};

// ─────────────────────────────────────────────────────────────────────────────
// Load canvas.spv — single compute shader
// ─────────────────────────────────────────────────────────────────────────────
[[nodiscard]] inline VkShaderModule load_canvas_shader() noexcept {
    constexpr std::string_view shader_name = "canvas.spv";

    std::array<std::string, 7> search_paths = {
        "compute/canvas.spv",
        "build/bin/Linux/compute/canvas.spv",
        "build/bin/Windows/compute/canvas.spv"
    };

    std::vector<uint32_t> code;

    for (const auto& path : search_paths) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) continue;

        auto size_bytes = file.tellg();
        if (size_bytes <= 0 || size_bytes % 4 != 0) continue;

        file.seekg(0, std::ios::beg);
        code.resize(static_cast<size_t>(size_bytes / 4));

        file.read(reinterpret_cast<char*>(code.data()), size_bytes);

        if (file.good() && !file.fail()) {
            int64_t printable_size = static_cast<int64_t>(size_bytes);
            LOG_SUCCESS_CAT("PIPELINE", "Loaded canvas.spv ({} bytes) from {}", 
                            printable_size, path);
            break;
        }

        code.clear();
    }

    if (code.empty()) {
        LOG_FATAL_CAT("PIPELINE", "canvas.spv not found or corrupted in any search path");
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
// Initialize descriptor layout (called once)
// ─────────────────────────────────────────────────────────────────────────────
inline void initialize() noexcept {
    if (main_descriptor_layout != VK_NULL_HANDLE) return;

    VkDescriptorSetLayoutCreateInfo layoutCI{};
    layoutCI.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCI.bindingCount = std::size(kCanvasBindings);
    layoutCI.pBindings    = kCanvasBindings;

    vkh.checker(vkCreateDescriptorSetLayout(rtx().device, &layoutCI, nullptr, &main_descriptor_layout),
                "vkCreateDescriptorSetLayout", "canvas main layout (5 bindings)");

    LOG_SUCCESS_CAT("PIPELINE", "Descriptor layout created with 5 bindings");
}

// ─────────────────────────────────────────────────────────────────────────────
// Create pipeline layout with 8-byte push constants (matches shader)
// ─────────────────────────────────────────────────────────────────────────────
inline void create_pipeline_layout() noexcept {
    if (pipeline_layout != VK_NULL_HANDLE) return;

    initialize(); // ensure descriptor layout exists

    VkPushConstantRange push{};
    push.stageFlags = COMPUTE_PUSH_MASK;
    push.offset     = 0;
    push.size       = sizeof(PushConstants);  // 8 bytes (float + uint)

    VkPipelineLayoutCreateInfo plCI{};
    plCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCI.setLayoutCount         = 1;
    plCI.pSetLayouts            = &main_descriptor_layout;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges    = &push;

    vkh.checker(vkCreatePipelineLayout(rtx().device, &plCI, nullptr, &pipeline_layout),
                "vkCreatePipelineLayout", "canvas");

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created with 8-byte push constants");
}

// ─────────────────────────────────────────────────────────────────────────────
// Lazy-create the compute pipeline
// ─────────────────────────────────────────────────────────────────────────────
inline void create_canvas_pipeline() noexcept {
    if (canvas_pipeline != VK_NULL_HANDLE) return;

    create_pipeline_layout();  // ensure layout exists

    VkShaderModule shader = load_canvas_shader();
    if (shader == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Cannot create pipeline — shader load failed");
        return;
    }

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
                "vkCreateComputePipelines", "canvas");

    vkDestroyShaderModule(rtx().device, shader, nullptr);

    if (canvas_pipeline != VK_NULL_HANDLE) {
        LOG_SUCCESS_CAT("PIPELINE", "Canvas compute pipeline created successfully");
    } else {
        LOG_FATAL_CAT("PIPELINE", "vkCreateComputePipelines failed — check validation layers");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Dispatch — now pushes 8 bytes (time + frameSeed) to match shader
// ─────────────────────────────────────────────────────────────────────────────
inline void dispatch_canvas(VkCommandBuffer cmd, uint32_t width, uint32_t height, float totalTime) noexcept {
    if (canvas_pipeline == VK_NULL_HANDLE) {
        create_canvas_pipeline();
        if (canvas_pipeline == VK_NULL_HANDLE) {
            LOG_ERROR_CAT("PIPELINE", "Dispatch aborted — pipeline creation failed");
            return;
        }
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, canvas_pipeline);

    // Pack both values (matches shader exactly)
    PushConstants pc{};
    pc.time       = totalTime;
    pc.frameSeed  = static_cast<uint>(totalTime * 1000.0f) ^ 0xDEADBEEFu;  // deterministic per-frame seed

    vkCmdPushConstants(cmd, pipeline_layout, COMPUTE_PUSH_MASK,
                       0, sizeof(PushConstants), &pc);

    uint32_t dispatchX = (width  + 15) / 16;
    uint32_t dispatchY = (height + 15) / 16;

    vkCmdDispatch(cmd, dispatchX, dispatchY, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Cleanup — called from RayCanvas destructor
// ─────────────────────────────────────────────────────────────────────────────
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

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline shutdown complete");
}

} // namespace Pipeline