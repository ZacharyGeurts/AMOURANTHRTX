// include/modes/RenderMode6.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// RenderMode6.hpp — VALHALLA v45 FINAL — NOV 12 2025
// • Caustic Ray Tracing Mode: Path tracing with caustic rendering
// • Integrates with VulkanRTX, uses SBT for raygen/miss/hit
// • Lazy accumulation for denoising
// • Uses global camera (g_lazyCam) for view/projection
// • STONEKEY v∞ ACTIVE — PINK PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <chrono>

namespace Engine {

class RenderMode6 {
public:
    RenderMode6(VulkanRTX& rtx, uint32_t width, uint32_t height);
    ~RenderMode6();

    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

    [[nodiscard]] VkImage getOutputImage() const { return *outputImage_; }
    [[nodiscard]] VkImageView getOutputView() const { return *outputView_; }

private:
    void initResources();
    void updateUniforms(float deltaTime);
    void traceRays(VkCommandBuffer cmd);
    void accumulateAndToneMap(VkCommandBuffer cmd);

    VulkanRTX& rtx_;
    uint32_t width_{0}, height_{0};

    // Buffers
    uint64_t uniformBuf_{0};
    uint64_t accumulationBuf_{0};
    VkDeviceSize accumSize_{0};

    // Images
    RTX::Handle<VkImage> outputImage_;
    RTX::Handle<VkImageView> outputView_;
    RTX::Handle<VkImage> accumImage_;
    RTX::Handle<VkImageView> accumView_;

    // Timing
    std::chrono::steady_clock::time_point lastFrame_{std::chrono::steady_clock::now()};
    uint32_t frameCount_{0};
    float accumWeight_{1.0f};

    // Descriptors
    VkDescriptorSet descriptorSet_{VK_NULL_HANDLE};
};

} // namespace Engine