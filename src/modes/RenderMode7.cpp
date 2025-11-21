// filename: src/modes/RenderMode7.cpp
// src/modes/RenderMode7.cpp
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

#include "modes/RenderMode7.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include <cmath>

using namespace Engine;
using namespace Logging::Color;

RenderMode7::RenderMode7(VulkanRTX& rtx, uint32_t width, uint32_t height)
    : rtx_(rtx), width_(width), height_(height), startTime_(std::chrono::steady_clock::now())
{
    LOG_INFO_CAT("RenderMode7", "VALHALLA MODE 7 INIT — {}×{} — VORTEX SPINNING UP", PLASMA_FUCHSIA, width, height, RESET);
    initResources();
    LOG_SUCCESS_CAT("RenderMode7", "Mode 7 Initialized — Infinite spiral engaged");
}

RenderMode7::~RenderMode7()
{
    vkDeviceWaitIdle(RTX::g_ctx().device());

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    LOG_SUCCESS_CAT("RenderMode7", "Mode 7 destroyed — VORTEX PHOTONS CONSUMED");
}

void RenderMode7::initResources()
{
    auto& ctx = RTX::g_ctx();
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
    VK_CHECK(vkCreateImage(device, &imgInfo, nullptr, &rawImg), "Vortex image");
    outputImage_ = RTX::Handle<VkImage>(rawImg, device, vkDestroyImage, 0, "VortexImage");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = rawImg;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imgInfo.format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView rawView;
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &rawView), "Vortex view");
    outputView_ = RTX::Handle<VkImageView>(rawView, device, vkDestroyImageView, 0, "VortexView");

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, *outputView_, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
}

glm::vec3 RenderMode7::hsvToRgb(float h, float s, float v)
{
    h = std::fmod(h, 1.0f);
    float c = v * s;
    float hp = h * 6.0f;
    float x = c * (1.0f - std::abs(std::fmod(hp, 2.0f) - 1.0f));
    glm::vec3 rgb;

    if (hp < 1.0f)      rgb = glm::vec3(c, x, 0.0f);
    else if (hp < 2.0f) rgb = glm::vec3(x, c, 0.0f);
    else if (hp < 3.0f) rgb = glm::vec3(0.0f, c, x);
    else if (hp < 4.0f) rgb = glm::vec3(0.0f, x, c);
    else if (hp < 5.0f) rgb = glm::vec3(x, 0.0f, c);
    else                rgb = glm::vec3(c, 0.0f, x);

    return rgb + glm::vec3(v - c);
}

void RenderMode7::renderFrame(VkCommandBuffer cmd, float /*deltaTime*/)
{
    clearVortex(cmd);
}

void RenderMode7::clearVortex(VkCommandBuffer cmd)
{
    float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime_).count() * 0.3f;

    const float cx = width_  * 0.5f;
    const float cy = height_ * 0.5f;
    const float px = cx;
    const float py = cy;

    const float dx = px - cx;
    const float dy = py - cy;
    const float dist = std::sqrt(dx*dx + dy*dy);
    const float angle = (dist > 0.0f) ? std::atan2(dy, dx) / (2.0f * 3.14159265359f) : 0.0f;

    float hue = angle + t + dist * 0.0005f;
    float sat = 1.0f - std::exp(-dist * 0.0008f);
    float val = 1.0f;

    glm::vec3 col = hsvToRgb(hue, sat, val);

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

void RenderMode7::onResize(uint32_t width, uint32_t height)
{
    vkDeviceWaitIdle(RTX::g_ctx().device());

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    outputImage_.reset();
    outputView_.reset();

    width_  = width;
    height_ = height;
    startTime_ = std::chrono::steady_clock::now();

    initResources();
}