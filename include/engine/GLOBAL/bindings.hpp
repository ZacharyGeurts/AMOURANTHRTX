// src/engine/GLOBAL/bindings.hpp
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <string_view>
#include <vector>

namespace RTX::Bindings {

struct Binding {
    uint32_t             binding;
    VkDescriptorType     type;
    uint32_t             count;
    VkShaderStageFlags   stage;
    std::string_view     name;
};

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL SET INDICES
// ──────────────────────────────────────────────────────────────────────────────
inline constexpr uint32_t SET_RAY_TRACING  = 0;
inline constexpr uint32_t SET_TONEM    = 1;
inline constexpr uint32_t SET_DENOISER     = 2;

// ──────────────────────────────────────────────────────────────────────────────
// COMPILE-TIME BINDING TABLES — WITH NAMES FOR DEBUG
// ──────────────────────────────────────────────────────────────────────────────
extern const std::array<Binding, 11> RT_PIPELINE_BINDINGS;
extern const std::array<Binding, 4>  TONEMAP_PIPELINE_BINDINGS;
extern const std::array<Binding, 3>  DENOISER_PIPELINE_BINDINGS;

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL HANDLES
// ──────────────────────────────────────────────────────────────────────────────
extern VkDescriptorSetLayout g_rtLayout;
extern VkDescriptorSetLayout g_tonemapLayout;
extern VkDescriptorSetLayout g_denoiserLayout;

extern VkPipelineLayout      g_tonemapPipelineLayout;
extern VkPipeline            g_tonemapPipeline;

extern std::vector<VkDescriptorSet> g_tonemapSets;
extern VkDescriptorPool             g_tonemapPool;

// ──────────────────────────────────────────────────────────────────────────────
// CENTRAL AUTHORITY
// ──────────────────────────────────────────────────────────────────────────────
void initialize(VkDevice device = nullptr);
void shutdown(VkDevice device = nullptr);

} // namespace RTX::Bindings