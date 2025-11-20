// modes/RenderMode8.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// RenderMode8: PURE BLACK — The Void. The End. The Final Mode.
// • Clears to absolute black every frame
// • No light. No color. No mercy.
// • For when you need to test swapchain, input, or just contemplate existence
// • VALHALLA v80 TURBO — VOID PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <vulkan/vulkan.h>

using namespace RTX;

namespace Engine {

class RenderMode8 {
public:
    RenderMode8(VulkanRTX& rtx, uint32_t width, uint32_t height);
    ~RenderMode8();

    void initResources();
    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    VulkanRTX& rtx_;
    uint32_t width_, height_;

    RTX::Handle<VkImage> outputImage_;
    RTX::Handle<VkImageView> outputView_;

    void enterTheVoid(VkCommandBuffer cmd);
};

}  // namespace Engine