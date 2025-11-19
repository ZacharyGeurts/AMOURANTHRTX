// include/modes/RenderMode1.hpp
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

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include <vulkan/vulkan.h>
#include <chrono>

class RenderMode1 {
public:
    RenderMode1(VulkanRTX& rtx, uint32_t width, uint32_t height);
    ~RenderMode1();

    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    VulkanRTX& rtx_;
    uint32_t width_, height_;
    uint64_t uniformBuf_ = 0;
    uint64_t accumulationBuf_ = 0;
    VkDeviceSize accumSize_ = 0;
    RTX::Handle<VkImage> accumImage_;
    RTX::Handle<VkImageView> accumView_;
    RTX::Handle<VkImage> outputImage_;
    RTX::Handle<VkImageView> outputView_;
    float accumWeight_ = 1.0f;
    uint32_t frameCount_ = 0;
    std::chrono::steady_clock::time_point lastFrame_;

RTX::Handle<VkDeviceMemory> accumMem_;
RTX::Handle<VkDeviceMemory> outputMem_;

    void initResources();
    void updateUniforms(float deltaTime);
    void traceRays(VkCommandBuffer cmd);
    void accumulateAndToneMap(VkCommandBuffer cmd);
};