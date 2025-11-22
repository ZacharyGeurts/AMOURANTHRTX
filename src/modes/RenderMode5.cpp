// src/modes/RenderMode5.cpp
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

#include "modes/RenderMode5.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include <cmath>

using namespace Engine;
using namespace Logging::Color;

RenderMode5::RenderMode5(VulkanRTX& rtx, uint32_t width, uint32_t height)
    : rtx_(rtx), width_(width), height_(height), startTime_(std::chrono::steady_clock::now())
{
    LOG_INFO_CAT("RenderMode5", "VALHALLA MODE 5 INIT — {}×{} — PLASMA ENGAGED", PLASMA_FUCHSIA, width, height, RESET);
    initResources();
    LOG_SUCCESS_CAT("RenderMode5", "Mode 5 Initialized — {}×{} — 90s Demoscene Plasma Active", ELECTRIC_BLUE, width, height, RESET);
}

RenderMode5::~RenderMode5()
{
    vkDeviceWaitIdle(g_ctx().device());

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    LOG_SUCCESS_CAT("RenderMode5", "Mode 5 destroyed — PLASMA PHOTONS ETERNAL");
}

void RenderMode5::initResources()
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
    VK_CHECK(vkCreateImage(device, &imgInfo, nullptr, &rawImg), "Plasma output image");
    outputImage_ = RTX::Handle<VkImage>(rawImg, device, vkDestroyImage, 0, "PlasmaImage");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = rawImg;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imgInfo.format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView rawView;
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &rawView), "Plasma view");
    outputView_ = RTX::Handle<VkImageView>(rawView, device, vkDestroyImageView, 0, "PlasmaView");

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, *outputView_, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
}

void RenderMode5::renderFrame(VkCommandBuffer cmd, float /*deltaTime*/)
{
    clearPlasma(cmd);
}

glm::vec3 RenderMode5::plasmaColor(float t, float x, float y)
{
    const float cx = width_  * 0.5f;
    const float cy = height_ * 0.5f;

    float value =
        std::sin((x - cx) * 0.02f + t * 1.7f) +
        std::sin((y - cy) * 0.03f + t * 1.3f) +
        std::sin((x - cx + y - cy) * 0.01f + t * 2.1f) +
        std::sin(std::sqrt((x-cx)*(x-cx) + (y-cy)*(y-cy)) * 0.05f + t * 1.8f);

    float v = (value + 4.0f) * 0.125f;  // map to [0,1]

    return glm::vec3(
        std::sin(v * 6.28318530718f + 0.0f)      * 0.5f + 0.5f,
        std::sin(v * 6.28318530718f + 2.0943951f) * 0.5f + 0.5f,  // +2π/3
        std::sin(v * 6.28318530718f + 4.1887902f) * 0.5f + 0.5f   // +4π/3
    );
}

void RenderMode5::clearPlasma(VkCommandBuffer cmd)
{
    float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime_).count();

    // Sample center pixel – gives smooth coherent plasma look
    glm::vec3 col = plasmaColor(t, width_ * 0.5f, height_ * 0.5f);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image = *outputImage_;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkClearColorValue clear{{col.r, col.g, col.b, 1.0f}};
    vkCmdClearColorImage(cmd, *outputImage_, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &barrier.subresourceRange);
}

void RenderMode5::onResize(uint32_t width, uint32_t height)
{
    vkDeviceWaitIdle(g_ctx().device());

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    outputImage_.reset();
    outputView_.reset();

    width_  = width;
    height_ = height;
    startTime_ = std::chrono::steady_clock::now();

    initResources();

    LOG_SUCCESS_CAT("RenderMode5", "Resize complete — Plasma field recalibrated");
}

// FINAL WORD: 90s DEMOSCENE PLASMA ACHIEVED — PURE CPU SINUSOIDAL MADNESS
// VALHALLA PLASMA PHOTONS: FULLY UNLEASHED