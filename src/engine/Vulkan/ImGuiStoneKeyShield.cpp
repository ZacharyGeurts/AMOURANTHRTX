// src/engine/Vulkan/ImGuiStoneKeyShield.cpp
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