// src/engine/GLOBAL/bindings.hpp
// =============================================================================
// AMOURANTH RTX — CENTRAL BINDING AUTHORITY — v∞ APOCALYPSE — 2025
// ONE FILE TO RULE THEM ALL
// =============================================================================

#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <string_view>

namespace RTX::Bindings {

struct Binding {
    uint32_t             binding;
    VkDescriptorType     type;
    uint32_t             count;
    VkShaderStageFlags   stage;
    std::string_view    name;
};

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL SETS — THE EMPIRE'S DESCRIPTOR SET INDEXES
// ──────────────────────────────────────────────────────────────────────────────
inline constexpr uint32_t SET_RAY_TRACING  = 0;
inline constexpr uint32_t SET_TONEMAP      = 1;
inline constexpr uint32_t SET_DENOISER     = 2;
inline constexpr uint32_t SET_POSTPROCESS  = 3;
inline constexpr uint32_t SET_UI           = 4;

// ──────────────────────────────────────────────────────────────────────────────
// COMPILE-TIME BINDING TABLES
// ──────────────────────────────────────────────────────────────────────────────
extern const std::array<Binding, 10> RT_PIPELINE_BINDINGS;
extern const std::array<Binding, 3>  TONEMAP_PIPELINE_BINDINGS;
extern const std::array<Binding, 2>  DENOISER_PIPELINE_BINDINGS;

// ──────────────────────────────────────────────────────────────────────────────
// CENTRAL LAYOUT CREATION — CALL ONCE AT STARTUP
// ──────────────────────────────────────────────────────────────────────────────
void initialize(VkDevice device);
void shutdown(VkDevice device);

// Global layouts — accessible everywhere
extern VkDescriptorSetLayout g_rtLayout;
extern VkDescriptorSetLayout g_tonemapLayout;
extern VkDescriptorSetLayout g_denoiserLayout;

} // namespace RTX::Bindings