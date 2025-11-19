// include/engine/Vulkan/ImGuiStoneKeyShield.hpp
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