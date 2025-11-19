// src/engine/Vulkan/ImGuiStoneKeyShield.cpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 16, 2025 — APOCALYPSE v3.4
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — KEYS NEVER LOGGED
// =============================================================================
#include "engine/Vulkan/ImGuiStoneKeyShield.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include <imgui.h>
#include <imgui_impl_vulkan.h>

namespace RTX {

// Static member definitions
bool ImGuiStoneKeyShield::stonekey_active_ = false;

uint64_t ImGuiStoneKeyShield::frameNumber() {
    return g_renderer ? g_renderer->getFrameNumber() : 0;
}

void ImGuiStoneKeyShield::newFrame() {
    if (!stonekey_active_ && frameNumber() >= 4) {
        StoneKey::Raw::transition_to_obfuscated();
        stonekey_active_ = true;
        LOG_SUCCESS_CAT("STONEKEY", "StoneKey v∞ activated — raw handles purged — VALHALLA SECURED");
    }
}

void ImGuiStoneKeyShield::renderDrawData(ImDrawData* draw_data, VkCommandBuffer cmd) {
    // Only render ImGui after StoneKey is active OR in first 10 frames (safe window)
    if (!stonekey_active_ || frameNumber() < 10) {
        ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
    }
    // else: silently drop — prevents raw Vulkan handle leaks
}

}  // namespace RTX