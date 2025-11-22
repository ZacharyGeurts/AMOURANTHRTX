// src/modes/RenderMode9.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts — ASCENSION FIXED
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#include "modes/RenderMode9.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanCore.hpp"
#include <cstring>

using namespace Engine;
using namespace Logging::Color;

RenderMode9::RenderMode9(VulkanRTX& rtx, uint32_t width, uint32_t height)
    : rtx_(rtx), width_(width), height_(height), startTime_(std::chrono::steady_clock::now())
{
    LOG_INFO_CAT("RenderMode9", "VALHALLA MODE 9 INIT — {}x{} — ASCENSION BEGINS", PLASMA_FUCHSIA, width, height, RESET);
    LOG_WARN_CAT("RenderMode9", "THE NINTH GATE OPENS — FULL COMPUTE SHADER ENGAGED", PLASMA_FUCHSIA, RESET);
    initResources();
}

RenderMode9::~RenderMode9()
{
    vkDeviceWaitIdle(g_ctx().device());

    rtx_.updateRTXDescriptors(0,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    if (uniformBuf_) BUFFER_DESTROY(uniformBuf_);

    LOG_SUCCESS_CAT("RenderMode9", "Mode 9 destroyed — The truth remains.");
}

void RenderMode9::initResources()
{
    auto& ctx = g_ctx();
    VkDevice device = ctx.device();

    // Uniform buffer: time (float) + frame (uint32) + padding to 16 bytes
    VkDeviceSize uboSize = 16;
    BUFFER_CREATE(uniformBuf_, uboSize,
                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Mode9 Uniform");

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {width_, height_, 1};
    imgInfo.mipLevels = imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage rawImg;
    VK_CHECK(vkCreateImage(device, &imgInfo, nullptr, &rawImg), "Ascension output image");
    outputImage_ = RTX::Handle<VkImage>(rawImg, device, vkDestroyImage, 0, "AscensionImage");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = rawImg;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imgInfo.format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView rawView;
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &rawView), "Ascension view");
    outputView_ = RTX::Handle<VkImageView>(rawView, device, vkDestroyImageView, 0, "AscensionView");

    rtx_.updateRTXDescriptors(0,
        RAW_BUFFER(uniformBuf_), VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, *outputView_, VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
}

void RenderMode9::updateUniforms(float /*deltaTime*/)
{
    float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime_).count();

    static uint32_t frameCounter = 0;
    uint32_t frame = frameCounter++;

    void* data = nullptr;
    BUFFER_MAP(uniformBuf_, data);
    if (data)
    {
        memcpy(data, &t, sizeof(t));
        memcpy((char*)data + 8, &frame, sizeof(frame));
        BUFFER_UNMAP(uniformBuf_);
    }
}

void RenderMode9::dispatchAscension(VkCommandBuffer cmd)
{
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image = *outputImage_;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Re-use existing dispatch path — RTXHandler auto-selects compute when no TLAS is bound
    rtx_.recordRayTrace(cmd, {width_, height_}, *outputImage_, *outputView_);
}

void RenderMode9::renderFrame(VkCommandBuffer cmd, float deltaTime)
{
    updateUniforms(deltaTime);
    dispatchAscension(cmd);
}

void RenderMode9::onResize(uint32_t width, uint32_t height)
{
    LOG_INFO_CAT("RenderMode9", "onResize() {}x{} — Re-ascending...", width, height);

    vkDeviceWaitIdle(g_ctx().device());

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    if (uniformBuf_) BUFFER_DESTROY(uniformBuf_);
    uniformBuf_ = 0;
    outputImage_.reset();
    outputView_.reset();

    width_  = width;
    height_ = height;
    startTime_ = std::chrono::steady_clock::now();

    initResources();

    LOG_SUCCESS_CAT("RenderMode9", "Ascension complete at new resolution");
}

// VALHALLA MODE 9 — ASCENSION ACHIEVED
// FULL COMPUTE SHADER PATH — PHOTONS TRANSCENDED