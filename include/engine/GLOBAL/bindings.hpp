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
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 28, 2025 — APOCALYPSE FINAL v3.1
// BINDING 31 IS GOD — PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED — VALHALLA UNBREACHABLE
// THE LONG NAME IS DECLARED — CARMACK-APPROVED — CID IS PROUD — NICK NODS
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <span>
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
// GLOBAL SET INDICES — THE SACRED TRINITY
// ──────────────────────────────────────────────────────────────────────────────
inline constexpr uint32_t SET_RAY_TRACING = 0;
inline constexpr uint32_t SET_TONEMAP     = 1;
inline constexpr uint32_t SET_DENOISER    = 2;

// ──────────────────────────────────────────────────────────────────────────────
// COMPILE-TIME BINDING TABLES — IMMORTAL, WITH NAMES FOR DEBUG
// ──────────────────────────────────────────────────────────────────────────────
extern const std::array<Binding, 11> RT_PIPELINE_BINDINGS;
extern const std::array<Binding, 4>  TONEMAP_PIPELINE_BINDINGS;
extern const std::array<Binding, 3>  DENOISER_PIPELINE_BINDINGS;

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL HANDLES — THE EMPIRE'S CROWN JEWELS
// ──────────────────────────────────────────────────────────────────────────────
extern VkDescriptorSetLayout g_rtLayout;
extern VkDescriptorSetLayout g_tonemapLayout;
extern VkDescriptorSetLayout g_denoiserLayout;

extern VkPipelineLayout      g_tonemapPipelineLayout;
extern VkPipeline            g_tonemapPipeline;

extern std::vector<VkDescriptorSet> g_tonemapSets;
extern VkDescriptorPool             g_tonemapPool;

// ──────────────────────────────────────────────────────────────────────────────
// CENTRAL AUTHORITY — INITIALIZE ONCE, RULE FOREVER
// ──────────────────────────────────────────────────────────────────────────────
void initialize(VkDevice device = nullptr);
void shutdown(VkDevice device = nullptr);

// ──────────────────────────────────────────────────────────────────────────────
// THE ONE TRUE FUNCTION — LONG NAME — PUBLIC — CARMACK-APPROVED — BEAUTIFUL
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device, std::span<const Binding> bindings);

} // namespace RTX::Bindings

// PINK PHOTONS ETERNAL — BINDING 31 IS GOD — STONEKEY v∞ ACTIVE
// THE LONG NAME IS DECLARED — THE EMPIRE IS PURE — FIRST LIGHT ACHIEVED
// JOHN CARMACK: "Good."
// CID: "I can finally sleep."
// NICK: "Perfect."
// CAPTAIN AMOURANTH: "The photons are pleased."
// KEANU REEVES: "…."

// NOW SHIP IT — THE RAID IS COMPLETE — VALHALLA IS OURS