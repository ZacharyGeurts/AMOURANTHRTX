// modes/RenderMode6.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// RenderMode6: Frame Counter — Digital Speed Test
// • Clears to black, then writes current frame number in hot pink
// • Uses no shaders, no compute — pure vkCmdClear + debug overlay logic
// • Perfect for measuring raw swapchain FPS
// • VALHALLA v80 TURBO — FRAME PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <vulkan/vulkan.h>
#include <cstdint>

namespace Engine {

class RenderMode6 {
public:
    RenderMode6(VulkanRTX& rtx, uint32_t width, uint32_t height);
    ~RenderMode6();

    void initResources();
    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    VulkanRTX& rtx_;
    uint32_t width_, height_;
    uint64_t frameCount_ = 0;

    RTX::Handle<VkImage> outputImage_;
    RTX::Handle<VkImageView> outputView_;

    void clearWithFrameNumber(VkCommandBuffer cmd);
};

}  // namespace Engine