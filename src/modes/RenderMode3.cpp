// src/modes/RenderMode3.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#include "modes/RenderMode3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

using namespace Engine;
using namespace Logging::Color;

RenderMode3::RenderMode3(VulkanRTX& rtx, uint32_t width, uint32_t height)
    : rtx_(rtx), width_(width), height_(height), gen_(0), rd_(), dist_(0.0f, 1.0f)
{
    gen_.seed(rd_());

    LOG_INFO_CAT("RenderMode3", "VALHALLA MODE 3 INIT — {}×{} — RANDOM CLEAR ENGAGED", PLASMA_FUCHSIA, width, height, RESET);
    initResources();
    LOG_SUCCESS_CAT("RenderMode3", "Mode 3 Initialized — {}×{} — Stochastic Chaos Active", ELECTRIC_BLUE, width, height, RESET);
}

RenderMode3::~RenderMode3() {
    LOG_INFO_CAT("RenderMode3", "Destructor invoked — Safe cleanup");
    vkDeviceWaitIdle(RTX::g_ctx().device());

    rtx_.updateRTXDescriptors(0,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    LOG_SUCCESS_CAT("RenderMode3", "Mode 3 destroyed — CHAOS PHOTONS ETERNAL");
}

void RenderMode3::initResources() {
    LOG_INFO_CAT("RenderMode3", "initResources() — Creating output image only");
    auto& ctx = RTX::g_ctx();
    VkDevice device = ctx.device();

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {width_, height_, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage rawImg;
    VK_CHECK(vkCreateImage(device, &imgInfo, nullptr, &rawImg), "Output image creation");
    outputImage_ = RTX::Handle<VkImage>(rawImg, device, vkDestroyImage, 0, "OutputImage");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = rawImg;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imgInfo.format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView rawView;
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &rawView), "Output view creation");
    outputView_ = RTX::Handle<VkImageView>(rawView, device, vkDestroyImageView, 0, "OutputView");

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, *outputView_, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    LOG_SUCCESS_CAT("RenderMode3", "initResources complete — Ready for pure chaos");
}

void RenderMode3::renderFrame(VkCommandBuffer cmd, float /*deltaTime*/) {
    clearRandom(cmd);
}

void RenderMode3::clearRandom(VkCommandBuffer cmd) {
    float r = dist_(gen_);
    float g = dist_(gen_);
    float b = dist_(gen_);

    LOG_DEBUG_CAT("RenderMode3", "Chaos clear → R={:.3f} G={:.3f} B={:.3f}", r, g, b);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image = *outputImage_;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkClearColorValue color{{r, g, b, 1.0f}};
    vkCmdClearColorImage(cmd, *outputImage_, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &barrier.subresourceRange);
}

void RenderMode3::onResize(uint32_t width, uint32_t height) {
    LOG_INFO_CAT("RenderMode3", "onResize() — New: {}×{} → Re-seeding chaos", width, height);

    vkDeviceWaitIdle(RTX::g_ctx().device());

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    outputImage_.reset();
    outputView_.reset();

    width_ = width;
    height_ = height;

    gen_.seed(rd_());

    initResources();
    LOG_SUCCESS_CAT("RenderMode3", "Resize complete — New chaos seed planted");
}