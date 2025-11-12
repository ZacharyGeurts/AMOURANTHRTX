// src/modes/RenderMode5.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// RenderMode5.cpp — VALHALLA v45 FINAL — NOV 12 2025
// • Ray tracing dispatch with lazy accumulation
// • Ultimate all-features for mode 5
// • Uses g_lazyCam for camera access — GLOBAL_CAM under the hood
// • STONEKEY v∞ ACTIVE — PINK PHOTONS ETERNAL
// =============================================================================

#include "modes/RenderMode5.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Engine;
using namespace Logging::Color;

RenderMode5::RenderMode5(VulkanRTX& rtx, uint32_t width, uint32_t height)
    : rtx_(rtx), width_(width), height_(height) {
    initResources();
    LOG_INFO_CAT("RenderMode5", "{}Mode 5 Initialized — {}×{} — Ultimate Path Tracing{}", ELECTRIC_BLUE, width, height, RESET);
}

RenderMode5::~RenderMode5() {
    // RAII handles destruction via RTX::Handle
    if (uniformBuf_) BUFFER_DESTROY(uniformBuf_);
    if (accumulationBuf_) BUFFER_DESTROY(accumulationBuf_);
    LOG_DEBUG_CAT("RenderMode5", "Mode 5 Resources Released");
}

void RenderMode5::initResources() {
    auto& ctx = RTX::g_ctx();
    VkDevice device = ctx.vkDevice();

    // Uniform buffer (viewproj + time) — using global cam
    VkDeviceSize uniformSize = sizeof(glm::mat4) + sizeof(float) * 2; // VP + time, frame
    BUFFER_CREATE(uniformBuf_, uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "RenderMode5 Uniform");
    // Handle creation not needed for tracker-based buffer

    // Accumulation buffer
    accumSize_ = static_cast<VkDeviceSize>(width_ * height_ * 16); // RGBA16F
    BUFFER_CREATE(accumulationBuf_, accumSize_, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "RenderMode5 Accum");

    // Accumulation image
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
    if (vkCreateImage(device, &imgInfo, nullptr, &rawImg) == VK_SUCCESS) {
        // TODO: Allocate and bind memory (use UltraLowLevelBufferTracker for image memory if extended)
        accumImage_ = RTX::MakeHandle(rawImg, device, vkDestroyImage);
    }

    // Output image (similar setup)
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // Final output
    if (vkCreateImage(device, &imgInfo, nullptr, &rawImg) == VK_SUCCESS) {
        outputImage_ = RTX::MakeHandle(rawImg, device, vkDestroyImage);
    }

    // Create views for accum and output
    // TODO: vkCreateImageView for outputView_ and accumView_

    // Descriptor set update via rtx_
    rtx_.updateRTXDescriptors(0, RAW_BUFFER(uniformBuf_), RAW_BUFFER(accumulationBuf_), VK_NULL_HANDLE, // dimension
                              *accumView_, *outputView_, VK_NULL_HANDLE, VK_NULL_HANDLE, // env
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
}

void RenderMode5::renderFrame(VkCommandBuffer cmd, float deltaTime) {
    updateUniforms(deltaTime);
    traceRays(cmd);
    accumulateAndToneMap(cmd);
    frameCount_++;
}

void RenderMode5::updateUniforms(float deltaTime) {
    // Map uniform buffer — using g_lazyCam
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
    // Use global cam position/front for ray origin/direction in shader
    // But dispatch via rtx_
    rtx_.recordRayTrace(cmd, {width_, height_}, *outputImage_, *outputView_);
}

void RenderMode5::accumulateAndToneMap(VkCommandBuffer cmd) {
    // Simple accumulation: blend new frame to accum
    accumWeight_ = 1.0f / (frameCount_ + 1.0f);
    // TODO: Dispatch compute for accum + tonemap
    // vkCmdDispatch(cmd, (width_ + 15)/16, (height_ + 15)/16, 1);
    // Barriers for image transitions
}

void RenderMode5::onResize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    frameCount_ = 0;
    accumWeight_ = 1.0f;
    // Recreate resources
    // TODO: Destroy old images/buffers, re-init
    initResources();
}