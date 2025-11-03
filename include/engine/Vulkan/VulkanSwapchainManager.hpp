// include/engine/Vulkan/VulkanSwapchainManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: Triple buffer, RAII, mutable config, cleanup() noexcept

#pragma once
#ifndef VULKANSWAPCHAINMANAGER_HPP
#define VULKANSWAPCHAINMANAGER_HPP

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <SDL3/SDL_vulkan.h>

namespace VulkanRTX {

// ── MUTABLE DEVELOPER CONFIG (CLI OVERRIDE) ───────────────────────────────
namespace SwapchainConfig {
    inline VkPresentModeKHR DESIRED_PRESENT_MODE = VK_PRESENT_MODE_MAILBOX_KHR;
    inline bool FORCE_VSYNC = false;
    inline bool FORCE_TRIPLE_BUFFER = true;
    inline bool LOG_FINAL_CONFIG = true;
}

class VulkanSwapchainManager {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    VulkanSwapchainManager(std::shared_ptr<Vulkan::Context> context,
                           SDL_Window* window,
                           int width,
                           int height);
    ~VulkanSwapchainManager();

    void initializeSwapchain(int width, int height);
    void handleResize(int width, int height);
    void cleanup() noexcept;  // RAII

    // Accessors
    VkSwapchainKHR&         getSwapchain()                     { return swapchain_; }
    const VkSwapchainKHR&   getSwapchain() const               { return swapchain_; }

    VkFormat                getSwapchainImageFormat() const    { return swapchainImageFormat_; }
    VkExtent2D              getSwapchainExtent() const         { return swapchainExtent_; }

    const std::vector<VkImage>&     getSwapchainImages() const     { return swapchainImages_; }
    const std::vector<VkImageView>& getSwapchainImageViews() const { return swapchainImageViews_; }

    VkSemaphore& getImageAvailableSemaphore(uint32_t currentFrame) {
        return imageAvailableSemaphores_[currentFrame % MAX_FRAMES_IN_FLIGHT];
    }
    const VkSemaphore& getImageAvailableSemaphore(uint32_t currentFrame) const {
        return imageAvailableSemaphores_[currentFrame % MAX_FRAMES_IN_FLIGHT];
    }

    VkSemaphore& getRenderFinishedSemaphore(uint32_t currentFrame) {
        return renderFinishedSemaphores_[currentFrame % MAX_FRAMES_IN_FLIGHT];
    }
    const VkSemaphore& getRenderFinishedSemaphore(uint32_t currentFrame) const {
        return renderFinishedSemaphores_[currentFrame % MAX_FRAMES_IN_FLIGHT];
    }

    VkFence& getInFlightFence(uint32_t currentFrame) {
        return inFlightFences_[currentFrame % MAX_FRAMES_IN_FLIGHT];
    }
    const VkFence& getInFlightFence(uint32_t currentFrame) const {
        return inFlightFences_[currentFrame % MAX_FRAMES_IN_FLIGHT];
    }

    uint32_t getMaxFramesInFlight() const { return MAX_FRAMES_IN_FLIGHT; }

private:
    void        waitForInFlightFrames() const;
    VkSurfaceKHR createSurface(SDL_Window* window);
    void        cleanupSwapchain() noexcept;  // internal

    void logSwapchainInfo(const char* prefix) const;

    std::shared_ptr<Vulkan::Context> context_;
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
};

} // namespace VulkanRTX

#endif // VULKANSWAPCHAINMANAGER_HPP