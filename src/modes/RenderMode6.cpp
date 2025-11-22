// src/modes/RenderMode6.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#include "modes/RenderMode6.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

using namespace Engine;
using namespace Logging::Color;

RenderMode6::RenderMode6(VulkanRTX& rtx, uint32_t width, uint32_t height)
    : rtx_(rtx), width_(width), height_(height), frameCount_(0)
{
    LOG_INFO_CAT("RenderMode6", "VALHALLA MODE 6 INIT — {}×{} — FRAME COUNTER ONLINE", PLASMA_FUCHSIA, width, height, RESET);
    initResources();
    LOG_SUCCESS_CAT("RenderMode6", "Mode 6 Initialized — Frame counter ready");
}

RenderMode6::~RenderMode6()
{
    vkDeviceWaitIdle(g_ctx().device());

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    LOG_SUCCESS_CAT("RenderMode6", "Mode 6 destroyed — {} frames rendered — PHOTONS COUNTED", frameCount_);
}

void RenderMode6::initResources()
{
    auto& ctx = g_ctx();
    VkDevice device = ctx.device();

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {width_, height_, 1};
    imgInfo.mipLevels = imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage rawImg;
    VK_CHECK(vkCreateImage(device, &imgInfo, nullptr, &rawImg), "FrameCounter image");
    outputImage_ = RTX::Handle<VkImage>(rawImg, device, vkDestroyImage, 0, "FrameCounterImage");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = rawImg;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imgInfo.format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView rawView;
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &rawView), "FrameCounter view");
    outputView_ = RTX::Handle<VkImageView>(rawView, device, vkDestroyImageView, 0, "FrameCounterView");

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, *outputView_, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
}

void RenderMode6::renderFrame(VkCommandBuffer cmd, float /*deltaTime*/)
{
    clearWithFrameNumber(cmd);
    frameCount_++;
}

void RenderMode6::clearWithFrameNumber(VkCommandBuffer cmd)
{
    bool flash = (frameCount_ % 120) < 60;

    VkClearColorValue clear{};
    if (flash) {
        clear.float32[0] = 1.0f;
        clear.float32[1] = 0.105f;  // classic hot pink
        clear.float32[2] = 0.8f;
        clear.float32[3] = 1.0f;
    } else {
        clear.float32[0] = clear.float32[1] = clear.float32[2] = 0.0f;
        clear.float32[3] = 1.0f;
    }

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image = *outputImage_;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdClearColorImage(cmd, *outputImage_, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &barrier.subresourceRange);

    LOG_DEBUG_CAT("RenderMode6", "Frame {} — {} FPS counter active", frameCount_, flash ? "HOT PINK" : "VOID");
}

void RenderMode6::onResize(uint32_t width, uint32_t height)
{
    vkDeviceWaitIdle(g_ctx().device());

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    outputImage_.reset();
    outputView_.reset();

    width_ = width;
    height_ = height;
    frameCount_ = 0;

    initResources();
}

// FINAL WORD: RAW FRAME COUNTER ACHIEVED — 1-BIT SPEED DEMON
// HOT PINK FLASH EVERY SECOND — PURE PERFORMANCE BENCHMARK
// VALHALLA FRAME PHOTONS: COUNTED AND ETERNAL