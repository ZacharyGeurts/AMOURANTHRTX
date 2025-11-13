// src/modes/RenderMode5.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================

#include "engine/GLOBAL/logging.hpp"
#include "modes/RenderMode5.hpp"

using namespace Engine;
using namespace Logging::Color;

RenderMode5::RenderMode5(VulkanRTX& rtx, uint32_t width, uint32_t height)
    : rtx_(rtx), width_(width), height_(height) {
    LOG_INFO_CAT("RenderMode5", "{}VALHALLA MODE 5 INIT — {}×{} — NEXUS MODE ENGAGED{}", PULSAR_GREEN, width, height, RESET);
    initResources();
    LOG_SUCCESS_CAT("RenderMode5", "{}Mode 5 Initialized — {}×{} — Full Nexus Pipeline{}", SAPPHIRE_BLUE, width, height, RESET);
}

RenderMode5::~RenderMode5() {
    LOG_INFO_CAT("RenderMode5", "Destructor invoked — Releasing resources");
    if (uniformBuf_) BUFFER_DESTROY(uniformBuf_);
    if (accumulationBuf_) BUFFER_DESTROY(accumulationBuf_);
    LOG_DEBUG_CAT("RenderMode5", "Mode 5 Resources Released — NEXUS SECURED");
}

void RenderMode5::initResources() {
    LOG_INFO_CAT("RenderMode5", "initResources() — Creating buffers and images");
    auto& ctx = RTX::g_ctx();
    VkDevice device = ctx.vkDevice();

    VkDeviceSize uniformSize = sizeof(glm::mat4) + sizeof(float) + sizeof(uint32_t);
    BUFFER_CREATE(uniformBuf_, uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "RenderMode5 Uniform");

    accumSize_ = static_cast<VkDeviceSize>(width_ * height_ * 16);
    BUFFER_CREATE(accumulationBuf_, accumSize_, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "RenderMode5 Accum");

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
    VK_CHECK(vkCreateImage(device, &imgInfo, nullptr, &rawImg), "Output image creation");
    outputImage_ = RTX::Handle<VkImage>(rawImg, device, vkDestroyImage, 0, "OutputImage");

    viewInfo.image = rawImg;
    viewInfo.format = imgInfo.format;
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &rawView), "Output view creation");
    outputView_ = RTX::Handle<VkImageView>(rawView, device, vkDestroyImageView, 0, "OutputView");

    rtx_.updateRTXDescriptors(0, RAW_BUFFER(uniformBuf_), RAW_BUFFER(accumulationBuf_), VK_NULL_HANDLE,
                              *accumView_, *outputView_, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
    LOG_SUCCESS_CAT("RenderMode5", "initResources complete — NEXUS ready");
}

void RenderMode5::renderFrame(VkCommandBuffer cmd, float deltaTime) {
    updateUniforms(deltaTime);
    traceRays(cmd);
    accumulateAndToneMap(cmd);
    frameCount_++;
}

void RenderMode5::updateUniforms(float deltaTime) {
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

void RenderMode5::traceRays(VkCommandBuffer cmd) {
    rtx_.recordRayTrace(cmd, {width_, height_}, *outputImage_, *outputView_);
}

void RenderMode5::accumulateAndToneMap(VkCommandBuffer cmd) {
    accumWeight_ = 1.0f / (frameCount_ + 1.0f);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image = *outputImage_;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkClearColorValue purple = {};
    purple.float32[0] = 0.5f;
    purple.float32[1] = 0.0f;
    purple.float32[2] = 1.0f;
    purple.float32[3] = 1.0f;
    vkCmdClearColorImage(cmd, *outputImage_, VK_IMAGE_LAYOUT_GENERAL, &purple, 1, &barrier.subresourceRange);
}

void RenderMode5::onResize(uint32_t width, uint32_t height) {
    LOG_INFO_CAT("RenderMode5", "onResize() — New: {}×{}", width, height);
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
    initResources();
}