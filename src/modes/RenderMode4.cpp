// src/modes/RenderMode4.cpp
// MINIMAL STABLE MODE — NO RESOURCES — NO CRASH — JUST BLACK
#include "modes/RenderMode4.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using namespace Logging::Color;

RenderMode4::RenderMode4(uint32_t, uint32_t)
{
    LOG_SUCCESS_CAT("RenderMode4", "MINIMAL STABLE MODE — NO RENDERING — BLACK VOID — NO CRASH");
}

RenderMode4::~RenderMode4() = default;

void RenderMode4::renderFrame(VkCommandBuffer cmd, float)
{
    // Clear to pure black — sacred silence
    const VkClearColorValue black = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    // Use the swapchain image directly (already bound by VulkanRenderer)
    vkCmdClearColorImage(cmd,
        StoneKey::stone_images()[0],  // any valid swapchain image
        VK_IMAGE_LAYOUT_GENERAL,
        &black, 1, &range);
}

void RenderMode4::onResize(uint32_t, uint32_t)
{
    // Do nothing — no resources to recreate
}