// modes/RenderMode1.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — RENDER MODE 1 — PURE PINK VOID
// =============================================================================
// Direct clear to swapchain image — no shaders, no descriptors, no ray tracing.
// Uses standalone transition function to avoid dependency on VulkanRenderer instance.
// Guaranteed visible output from frame 1 — perfect startup/fallback mode.
// PINK PHOTONS ETERNAL — THE EMPIRE RESTS IN SACRED LIGHT
// =============================================================================

#include "modes/RenderMode1.hpp"

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

// Standalone image layout transition — does not require VulkanRenderer object
static void transitionImage(VkCommandBuffer cmd,
                            VkImage image,
                            VkImageLayout oldLayout,
                            VkImageLayout newLayout,
                            VkAccessFlags srcAccess,
                            VkAccessFlags dstAccess,
                            VkPipelineStageFlags srcStage,
                            VkPipelineStageFlags dstStage) noexcept
{
    if (image == VK_NULL_HANDLE || cmd == VK_NULL_HANDLE) {
        return;
    }

    VkImageMemoryBarrier barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext               = nullptr,
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

RenderMode1::RenderMode1(uint32_t width, uint32_t height)
    : width_(width), height_(height)
{
    LOG_AMOURANTH("RENDER MODE 1 INITIALIZED — PURE PINK VOID — THE EMPIRE RESTS IN ETERNAL LIGHT");
}

RenderMode1::~RenderMode1() = default;

void RenderMode1::renderFrame(VkCommandBuffer cmd, uint32_t frameIndex, float /*deltaTime*/)
{
    const uint32_t imageCount = StoneKey::stone_image_count();
    const uint32_t imageIndex = frameIndex % imageCount;
    VkImage swapImage = StoneKey::stone_images()[imageIndex];

    LOG_TRACE_CAT("MODE1", "Rendering pure pink void — frame {} — image {}", frameIndex, imageIndex);

    // Transition to transfer destination
    transitionImage(cmd,
                    swapImage,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    0,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Clear to sacred pink
    VkClearColorValue sacredPink{{1.0f, 0.0f, 0.5f, 1.0f}};
    VkImageSubresourceRange range{
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1
    };

    vkCmdClearColorImage(cmd, swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &sacredPink, 1, &range);

    // Transition back to present
    transitionImage(cmd,
                    swapImage,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    0,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

void RenderMode1::onResize(uint32_t newWidth, uint32_t newHeight)
{
    width_  = newWidth;
    height_ = newHeight;
    LOG_INFO_CAT("MODE1", "Pure pink void resized to {}×{}", newWidth, newHeight);
}