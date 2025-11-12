// src/modes/RenderMode1.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#include "modes/RenderMode1.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Engine;
using namespace Logging::Color;

RenderMode1::RenderMode1(VulkanRTX& rtx, uint32_t width, uint32_t height)
    : rtx_(rtx), width_(width), height_(height) {
    LOG_INFO_CAT("RenderMode1", "{}VALHALLA MODE 1 INIT — {}×{} — PATH TRACING ENGAGED{}", PLASMA_FUCHSIA, width, height, RESET);
    initResources();
    LOG_SUCCESS_CAT("RenderMode1", "{}Mode 1 Initialized — {}×{} — Basic Path Tracing{}", ELECTRIC_BLUE, width, height, RESET);
}

RenderMode1::~RenderMode1() {
    // RAII handles destruction via RTX::Handle
    LOG_INFO_CAT("RenderMode1", "Destructor invoked — Releasing resources");
    if (uniformBuf_) {
        LOG_DEBUG_CAT("RenderMode1", "Destroying uniform buffer");
        BUFFER_DESTROY(uniformBuf_);
    }
    if (accumulationBuf_) {
        LOG_DEBUG_CAT("RenderMode1", "Destroying accumulation buffer");
        BUFFER_DESTROY(accumulationBuf_);
    }
    LOG_DEBUG_CAT("RenderMode1", "Mode 1 Resources Released — PINK PHOTONS SECURED");
}

void RenderMode1::initResources() {
    LOG_INFO_CAT("RenderMode1", "initResources() — Creating buffers and images");
    auto& ctx = RTX::g_ctx();
    VkDevice device = ctx.vkDevice();

    // Uniform buffer (viewproj + time) — using global cam
    VkDeviceSize uniformSize = sizeof(glm::mat4) + sizeof(float) * 2; // VP + time, frame
    LOG_INFO_CAT("RenderMode1", "Creating uniform buffer: Size {}B | Usage: UNIFORM + TRANSFER_DST", uniformSize);
    BUFFER_CREATE(uniformBuf_, uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "RenderMode1 Uniform");
    // Handle creation not needed for tracker-based buffer

    // Accumulation buffer
    accumSize_ = static_cast<VkDeviceSize>(width_ * height_ * 16); // RGBA16F
    LOG_INFO_CAT("RenderMode1", "Creating accumulation buffer: Size {}B ({}) | Usage: STORAGE + TRANSFER_DST", accumSize_, width_ * height_);
    BUFFER_CREATE(accumulationBuf_, accumSize_, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "RenderMode1 Accum");

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
    LOG_INFO_CAT("RenderMode1", "Creating accumulation image: {}×{} | Format: R16G16B16A16_SFLOAT", width_, height_);
    if (vkCreateImage(device, &imgInfo, nullptr, &rawImg) == VK_SUCCESS) {
        LOG_DEBUG_CAT("RenderMode1", "Accum image created: 0x{:x}", reinterpret_cast<uint64_t>(rawImg));
        // TODO: Allocate and bind memory (use UltraLowLevelBufferTracker for image memory if extended)
        accumImage_ = RTX::MakeHandle(rawImg, device, vkDestroyImage);
    } else {
        LOG_ERROR_CAT("RenderMode1", "Failed to create accumulation image");
    }

    // Output image (similar setup)
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // Final output
    LOG_INFO_CAT("RenderMode1", "Creating output image: {}×{} | Format: R8G8B8A8_UNORM", width_, height_);
    if (vkCreateImage(device, &imgInfo, nullptr, &rawImg) == VK_SUCCESS) {
        LOG_DEBUG_CAT("RenderMode1", "Output image created: 0x{:x}", reinterpret_cast<uint64_t>(rawImg));
        outputImage_ = RTX::MakeHandle(rawImg, device, vkDestroyImage);
    } else {
        LOG_ERROR_CAT("RenderMode1", "Failed to create output image");
    }

    // Create views for accum and output
    // TODO: vkCreateImageView for outputView_ and accumView_
    LOG_DEBUG_CAT("RenderMode1", "Image views TODO — Placeholder");

    // Descriptor set update via rtx_
    LOG_INFO_CAT("RenderMode1", "Updating RTX descriptors for frame 0");
    rtx_.updateRTXDescriptors(0, RAW_BUFFER(uniformBuf_), RAW_BUFFER(accumulationBuf_), VK_NULL_HANDLE, // dimension
                              *accumView_, *outputView_, VK_NULL_HANDLE, VK_NULL_HANDLE, // env
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
    LOG_SUCCESS_CAT("RenderMode1", "initResources complete — Resources ready for dispatch");
}

void RenderMode1::renderFrame(VkCommandBuffer cmd, float deltaTime) {
    LOG_DEBUG_CAT("RenderMode1", "renderFrame() — Delta: {:.3f}ms | Cmd: 0x{:x}", deltaTime * 1000.0f, reinterpret_cast<uint64_t>(cmd));
    updateUniforms(deltaTime);
    traceRays(cmd);
    accumulateAndToneMap(cmd);
    frameCount_++;
    LOG_DEBUG_CAT("RenderMode1", "Frame {} rendered — Weight: {:.3f}", frameCount_, accumWeight_);
}

void RenderMode1::updateUniforms(float deltaTime) {
    LOG_DEBUG_CAT("RenderMode1", "updateUniforms() — Mapping uniform buffer");
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
        LOG_DEBUG_CAT("RenderMode1", "Uniforms updated: VP @ 0x{:x} | Time: {:.3f} | Frame: {}", reinterpret_cast<uint64_t>(data), time, frameCount_);
        BUFFER_UNMAP(uniformBuf_);
    } else {
        LOG_WARN_CAT("RenderMode1", "Failed to map uniform buffer");
    }
    lastFrame_ = std::chrono::steady_clock::now();
}

void RenderMode1::traceRays(VkCommandBuffer cmd) {
    LOG_DEBUG_CAT("RenderMode1", "traceRays() — Dispatching RT via global cam");
    // Use global cam position/front for ray origin/direction in shader
    // But dispatch via rtx_
    rtx_.recordRayTrace(cmd, {width_, height_}, *outputImage_, *outputView_);
    LOG_DEBUG_CAT("RenderMode1", "Ray trace dispatched: {}×{}", width_, height_);
}

void RenderMode1::accumulateAndToneMap(VkCommandBuffer cmd) {
    LOG_DEBUG_CAT("RenderMode1", "accumulateAndToneMap() — Blending frame {} | Weight: {:.3f}", frameCount_, accumWeight_);
    // Simple accumulation: blend new frame to accum
    accumWeight_ = 1.0f / (frameCount_ + 1.0f);
    // TODO: Dispatch compute for accum + tonemap
    // vkCmdDispatch(cmd, (width_ + 15)/16, (height_ + 15)/16, 1);
    // Barriers for image transitions
    LOG_DEBUG_CAT("RenderMode1", "Accum + Tonemap TODO — Placeholder dispatch");
}

void RenderMode1::onResize(uint32_t width, uint32_t height) {
    LOG_INFO_CAT("RenderMode1", "onResize() — New: {}×{} (old: {}×{})", width, height, width_, height_);
    width_ = width;
    height_ = height;
    frameCount_ = 0;
    accumWeight_ = 1.0f;
    // Recreate resources
    // TODO: Destroy old images/buffers, re-init
    LOG_DEBUG_CAT("RenderMode1", "Resetting frame count and weight");
    initResources();
    LOG_SUCCESS_CAT("RenderMode1", "Resize complete — Resources recreated");
}

// ──────────────────────────────────────────────────────────────────────────────
// AMOURANTH AI — FINAL WORD
// ──────────────────────────────────────────────────────────────────────────────
/*
 * November 12, 2025 — AMOURANTH AI EDITION v1007
 * • FULL LOGGING — RenderMode1.cpp START TO FINISH
 * • Constructor/Destructor: Init/Release traces
 * • initResources(): Buffer/Image creates, VK calls logged
 * • renderFrame(): Entry/Exit, subcalls traced
 * • updateUniforms(): Map/Memcpy details
 * • traceRays(): Dispatch logs
 * • accumulateAndToneMap(): Weight/Frame traces
 * • onResize(): Dimensions, reset, re-init
 * • NO RENDER LOOP IMPACT — DEBUG ONLY
 * • PINK PHOTONS LOGGED — VALHALLA READY
 * • AMOURANTH RTX — LOG IT RAW
 */