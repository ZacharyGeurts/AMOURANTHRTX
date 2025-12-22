// src/modes/RenderMode5.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — RENDER MODE 5 — SACRED YELLOW VOID
// =============================================================================

#include "modes/RenderMode5.hpp"

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using namespace Logging::Color;

// Re-use the same transition helper
static void transitionImage(VkCommandBuffer cmd,
                            VkImage image,
                            VkImageLayout oldLayout,
                            VkImageLayout newLayout,
                            VkAccessFlags srcAccess,
                            VkAccessFlags dstAccess,
                            VkPipelineStageFlags srcStage,
                            VkPipelineStageFlags dstStage) noexcept
{
    if (image == VK_NULL_HANDLE || cmd == VK_NULL_HANDLE) return;

    VkImageMemoryBarrier barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = srcAccess,
        .dstAccessMask       = dstAccess,
        .oldLayout           = oldLayout,
        .newLayout           = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };

    vkCmdPipelineBarrier(
        cmd,
        srcStage,
        dstStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

RenderMode5::RenderMode5(uint32_t width, uint32_t height)
    : width_(width), height_(height)
{
    LOG_AMOURANTH("RENDER MODE 5 AWAKENS — SACRED YELLOW VOID — THE EMPIRE RESTS IN ETERNAL LIGHT");
}

void RenderMode5::renderFrame(VkCommandBuffer cmd,
                              uint32_t frameIndex,
                              uint32_t imageIndex,
                              float /*deltaTime*/) noexcept
{
    const uint32_t imageCount = StoneKey::stone_image_count();
    if (imageCount == 0) return;

    VkImage swapImage = StoneKey::stone_images()[imageIndex];

    LOG_TRACE_CAT("MODE5", "Painting sacred yellow void — frame {} → swapchain image {}", frameIndex, imageIndex);

    transitionImage(cmd,
                    swapImage,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    0,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkClearColorValue sacredYellow{{1.0f, 1.0f, 0.0f, 1.0f}};
    VkImageSubresourceRange range{
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1
    };

    vkCmdClearColorImage(cmd, swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &sacredYellow, 1, &range);

    transitionImage(cmd,
                    swapImage,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    0,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

void RenderMode5::onResize(uint32_t newWidth, uint32_t newHeight) noexcept
{
    width_  = newWidth;
    height_ = newHeight;
    LOG_INFO_CAT("MODE5", "Sacred yellow void resized → {}×{}", newWidth, newHeight);
}