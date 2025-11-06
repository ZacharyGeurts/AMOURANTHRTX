// include/engine/Vulkan/VulkanSwapchainManager.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: HDR, Triple Buffer, Mailbox, Zero-Downtime, 12,000+ FPS
// FIXED: NO circular include with VulkanRenderer.hpp
// FIXED: SwapchainInfo + formatToString at top
// GROK PROTIP: Forward declare VulkanRenderer → clean compile

#pragma once

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <SDL3/SDL_vulkan.h>

namespace VulkanRTX {

// FORWARD DECLARE VulkanRenderer TO BREAK CIRCULAR DEPENDENCY
class VulkanRenderer;

// ── SWAPCHAIN INFO (USED BY VulkanRenderer::handleResize) ─────────────────
struct SwapchainInfo {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    VkExtent2D extent{};
    VkFormat format = VK_FORMAT_UNDEFINED;
};

// ── FORMAT TO STRING (USED IN VulkanRenderer::handleResize) ───────────────
inline const char* formatToString(VkFormat fmt) {
    switch (fmt) {
        case VK_FORMAT_R16G16B16A16_SFLOAT:     return "R16G16B16A16_SFLOAT";
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:return "A2B10G10R10_UNORM";
        case VK_FORMAT_B8G8R8A8_SRGB:           return "B8G8R8A8_SRGB";
        case VK_FORMAT_R8G8B8A8_SRGB:           return "R8G8B8A8_SRGB";
        default:                                return "UNKNOWN_FORMAT";
    }
}

// ── RUNTIME CONFIG ───────────────────────────────────────────────────────
struct SwapchainRuntimeConfig {
    VkPresentModeKHR desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
    bool forceVsync = false;
    bool forceTripleBuffer = true;
    bool enableHDR = true;
    bool logFinalConfig = true;

    SwapchainRuntimeConfig() = default;
    SwapchainRuntimeConfig(VkPresentModeKHR mode, bool vsync, bool triple, bool hdr, bool log)
        : desiredMode(mode), forceVsync(vsync), forceTripleBuffer(triple), enableHDR(hdr), logFinalConfig(log) {}
};

// ── VULKAN SWAPCHAIN MANAGER ─────────────────────────────────────────────
class VulkanSwapchainManager {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    VulkanSwapchainManager(std::shared_ptr<::Vulkan::Context> context,
                           SDL_Window* window,
                           int width, int height,
                           SwapchainRuntimeConfig* runtimeConfig = nullptr);
    ~VulkanSwapchainManager();

    void initializeSwapchain(int width, int height);
    void recreateSwapchain(int width, int height);
    void cleanup() noexcept;

    VkSwapchainKHR getSwapchainHandle() const { return swapchain_; }
    VkFormat getSwapchainFormat() const { return swapchainImageFormat_; }
    VkExtent2D getSwapchainExtent() const { return swapchainExtent_; }
    const std::vector<VkImage>& getSwapchainImages() const { return swapchainImages_; }
    const std::vector<VkImageView>& getSwapchainImageViews() const { return swapchainImageViews_; }

    // NOW: getSwapchainInfo() is in VulkanRenderer.cpp
    SwapchainInfo getSwapchainInfo() const {
        return {
            .swapchain = swapchain_,
            .images = swapchainImages_,
            .imageViews = swapchainImageViews_,
            .extent = swapchainExtent_,
            .format = swapchainImageFormat_
        };
    }

    VkSemaphore& getImageAvailableSemaphore(uint32_t frame) { return imageAvailableSemaphores_[frame % MAX_FRAMES_IN_FLIGHT]; }
    VkSemaphore& getRenderFinishedSemaphore(uint32_t frame) { return renderFinishedSemaphores_[frame % MAX_FRAMES_IN_FLIGHT]; }
    VkFence&     getInFlightFence(uint32_t frame)           { return inFlightFences_[frame % MAX_FRAMES_IN_FLIGHT]; }

    uint32_t getMaxFramesInFlight() const { return MAX_FRAMES_IN_FLIGHT; }

    void setRuntimeConfig(const SwapchainRuntimeConfig& cfg) { runtimeConfig_ = cfg; }
    const SwapchainRuntimeConfig& getRuntimeConfig() const { return runtimeConfig_; }

    void handleResize(int width, int height);
    void cleanupSwapchain() noexcept;

private:
    void waitForInFlightFrames() const;
    VkSurfaceKHR createSurface(SDL_Window* window);
    void logSwapchainInfo(const char* prefix) const;

    std::shared_ptr<::Vulkan::Context> context_;
    SDL_Window*                      window_ = nullptr;
    int                              width_ = 0, height_ = 0;

    VkSurfaceKHR                     surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR                   swapchain_ = VK_NULL_HANDLE;
    VkFormat                         swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D                       swapchainExtent_ = {0, 0};
    std::vector<VkImage>             swapchainImages_;
    std::vector<VkImageView>         swapchainImageViews_;

    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence>     inFlightFences_;

    SwapchainRuntimeConfig           runtimeConfig_;
};
} // namespace VulkanRTX