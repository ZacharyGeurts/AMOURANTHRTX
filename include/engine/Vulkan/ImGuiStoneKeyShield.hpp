// include/engine/Vulkan/ImGuiStoneKeyShield.hpp
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
#pragma once

#include <cstdint>
#include <memory>

struct ImDrawData;
using VkCommandBuffer = struct VkCommandBuffer_T*;

// Forward declare — we access via g_renderer (declared in main.cpp)
class VulkanRenderer;
extern std::unique_ptr<VulkanRenderer> g_renderer;  // ← BASTION LAW: visible here

namespace RTX {

struct ImGuiStoneKeyShield {
    static void newFrame();
    static void renderDrawData(ImDrawData* draw_data, VkCommandBuffer cmd);

private:
    static bool stonekey_active_;
    static uint64_t frameNumber();  // Returns g_renderer->getFrameNumber()
};

}  // namespace RTX