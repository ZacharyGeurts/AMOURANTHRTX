// src/modes/RenderMode1.cpp
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

#include "modes/RenderMode1.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

using namespace Engine;
using namespace Logging::Color;

RenderMode1::RenderMode1(VulkanRTX& rtx, uint32_t width, uint32_t height)
    : rtx_(rtx), width_(width), height_(height), lastFrame_(std::chrono::steady_clock::now()) {
    LOG_INFO_CAT("RenderMode1", "{}VALHALLA MODE 1 INIT — {}×{} — PATH TRACING ENGAGED{}", PLASMA_FUCHSIA, width, height, RESET);
    initResources();
    LOG_SUCCESS_CAT("RenderMode1", "{}Mode 1 Initialized — {}×{} — Basic Path Tracing{}", ELECTRIC_BLUE, width, height, RESET);
}

RenderMode1::~RenderMode1() {
    LOG_INFO_CAT("RenderMode1", "Destructor invoked — Safe cleanup");

    vkDeviceWaitIdle(RTX::g_ctx().vkDevice());

    rtx_.updateRTXDescriptors(0,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    if (uniformBuf_)      BUFFER_DESTROY(uniformBuf_);
    if (accumulationBuf_) BUFFER_DESTROY(accumulationBuf_);

    LOG_SUCCESS_CAT("RenderMode1", "Mode 1 destroyed — PINK PHOTONS ETERNAL");
}

void RenderMode1::initResources() {
    LOG_INFO_CAT("RenderMode1", "initResources() — Creating buffers and images");
    auto& ctx = RTX::g_ctx();
    VkDevice device = ctx.vkDevice();

    VkDeviceSize uniformSize = sizeof(glm::mat4) + sizeof(float) + sizeof(uint32_t);
    LOG_INFO_CAT("RenderMode1", "Creating uniform buffer: Size {}B | Usage: UNIFORM + TRANSFER_DST", uniformSize);
    BUFFER_CREATE(uniformBuf_, uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "RenderMode1 Uniform");

    accumSize_ = static_cast<VkDeviceSize>(width_ * height_ * 16);
    LOG_INFO_CAT("RenderMode1", "Creating accumulation buffer: Size {}B ({}) | Usage: STORAGE + TRANSFER_DST", accumSize_, width_ * height_);
    BUFFER_CREATE(accumulationBuf_, accumSize_, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "RenderMode1 Accum");

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imgInfo.extent = {width_, height_, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage rawImg;
    LOG_INFO_CAT("RenderMode1", "Creating accumulation image: {}×{} | Format: R16G16B16A16_SFLOAT", width_, height_);
    VK_CHECK(vkCreateImage(device, &imgInfo, nullptr, &rawImg), "Accum image creation");
    accumImage_ = RTX::Handle<VkImage>(rawImg, device, vkDestroyImage, 0, "AccumImage");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = rawImg;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imgInfo.format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView rawView;
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &rawView), "Accum view creation");
    accumView_ = RTX::Handle<VkImageView>(rawView, device, vkDestroyImageView, 0, "AccumView");

    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    LOG_INFO_CAT("RenderMode1", "Creating output image: {}×{} | Format: R8G8B8A8_UNORM", width_, height_);
    VK_CHECK(vkCreateImage(device, &imgInfo, nullptr, &rawImg), "Output image creation");
    outputImage_ = RTX::Handle<VkImage>(rawImg, device, vkDestroyImage, 0, "OutputImage");

    viewInfo.image = rawImg;
    viewInfo.format = imgInfo.format;
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &rawView), "Output view creation");
    outputView_ = RTX::Handle<VkImageView>(rawView, device, vkDestroyImageView, 0, "OutputView");

    LOG_INFO_CAT("RenderMode1", "Updating RTX descriptors for frame 0");
    rtx_.updateRTXDescriptors(0, RAW_BUFFER(uniformBuf_), RAW_BUFFER(accumulationBuf_), VK_NULL_HANDLE,
                              *accumView_, *outputView_, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
    LOG_SUCCESS_CAT("RenderMode1", "initResources complete — Resources ready for dispatch");
}

void RenderMode1::renderFrame(VkCommandBuffer cmd, float deltaTime) {
    LOG_DEBUG_CAT("RenderMode1", "renderFrame() — Delta: {:.3f}ms", deltaTime * 1000.0f);
    updateUniforms(deltaTime);
    traceRays(cmd);
    accumulateAndToneMap(cmd);
    frameCount_++;
}

void RenderMode1::updateUniforms(float deltaTime) {
    void* data = nullptr;
    BUFFER_MAP(uniformBuf_, data);
    if (data) {
        float aspect = static_cast<float>(width_) / height_;
        glm::mat4 vp = g_lazyCam.proj(aspect) * g_lazyCam.view();
        float time = std::chrono::duration<float>(std::chrono::steady_clock::now() - lastFrame_).count();
        memcpy(data, glm::value_ptr(vp), sizeof(vp));
        memcpy(static_cast<char*>(data) + sizeof(vp), &time, sizeof(time));
        memcpy(static_cast<char*>(data) + sizeof(vp) + sizeof(time), &frameCount_, sizeof(frameCount_));
        BUFFER_UNMAP(uniformBuf_);
    }
    lastFrame_ = std::chrono::steady_clock::now();
}

void RenderMode1::traceRays(VkCommandBuffer cmd) {
    rtx_.recordRayTrace(cmd, {width_, height_}, *outputImage_, *outputView_);
}

void RenderMode1::accumulateAndToneMap(VkCommandBuffer cmd) {
    accumWeight_ = 1.0f / (frameCount_ + 1.0f);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image = *outputImage_;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkClearColorValue pink{{1.0f, 0.2f, 0.8f, 1.0f}};
    vkCmdClearColorImage(cmd, *outputImage_, VK_IMAGE_LAYOUT_GENERAL, &pink, 1, &barrier.subresourceRange);
}

void RenderMode1::onResize(uint32_t width, uint32_t height) {
    LOG_INFO_CAT("RenderMode1", "onResize() — New: {}×{} (old: {}×{})", width, height, width_, height_);

    auto& ctx = RTX::g_ctx();
    VkDevice device = ctx.vkDevice();
    VK_CHECK(vkDeviceWaitIdle(device), "vkDeviceWaitIdle failed in onResize");

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    if (uniformBuf_) BUFFER_DESTROY(uniformBuf_);
    if (accumulationBuf_) BUFFER_DESTROY(accumulationBuf_);
    accumImage_.reset();
    outputImage_.reset();
    accumView_.reset();
    outputView_.reset();
    uniformBuf_ = 0;
    accumulationBuf_ = 0;
    accumSize_ = 0;
    width_ = width;
    height_ = height;
    frameCount_ = 0;
    accumWeight_ = 1.0f;
    lastFrame_ = std::chrono::steady_clock::now();
    initResources();

    LOG_SUCCESS_CAT("RenderMode1", "Resize complete — Resources recreated");
}