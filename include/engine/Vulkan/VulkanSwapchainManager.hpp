// include/engine/Vulkan/VulkanSwapchainManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FINAL: cleanup() added, inline accessors, owned by Context, full RAII

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

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager(std::shared_ptr<Vulkan::Context> context,
                           SDL_Window* window,
                           int width,
                           int height);
    ~VulkanSwapchainManager();

    void initializeSwapchain(int width, int height);
    void handleResize(int width, int height);
    void cleanup();  // ← NEW: RAII cleanup

    // --- Accessors: NON-CONST + CONST (RETURN BY REFERENCE) ---
    VkSwapchainKHR&         getSwapchain()                     { return swapchain_; }
    const VkSwapchainKHR&   getSwapchain() const               { return swapchain_; }

    VkFormat                getSwapchainImageFormat() const    { return swapchainImageFormat_; }
    VkExtent2D              getSwapchainExtent() const         { return swapchainExtent_; }

    const std::vector<VkImage>&     getSwapchainImages() const     { return swapchainImages_; }
    const std::vector<VkImageView>& getSwapchainImageViews() const { return swapchainImageViews_; }

    // Sync objects — non-const + const overloads
    VkSemaphore& getImageAvailableSemaphore(uint32_t currentFrame) {
        return imageAvailableSemaphores_[currentFrame % maxFramesInFlight_];
    }
    const VkSemaphore& getImageAvailableSemaphore(uint32_t currentFrame) const {
        return imageAvailableSemaphores_[currentFrame % maxFramesInFlight_];
    }

    VkSemaphore& getRenderFinishedSemaphore(uint32_t currentFrame) {
        return renderFinishedSemaphores_[currentFrame % maxFramesInFlight_];
    }
    const VkSemaphore& getRenderFinishedSemaphore(uint32_t currentFrame) const {
        return renderFinishedSemaphores_[currentFrame % maxFramesInFlight_];
    }

    VkFence& getInFlightFence(uint32_t currentFrame) {
        return inFlightFences_[currentFrame % maxFramesInFlight_];
    }
    const VkFence& getInFlightFence(uint32_t currentFrame) const {
        return inFlightFences_[currentFrame % maxFramesInFlight_];
    }

    uint32_t getMaxFramesInFlight() const { return maxFramesInFlight_; }

private:
    void        waitForInFlightFrames() const;
    VkSurfaceKHR createSurface(SDL_Window* window);
    void        cleanupSwapchain();  // ← internal

    // SEXY SWAPCHAIN LOGGING – ONE LINE, ONE OCEAN-TEAL BLAST
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

    uint32_t maxFramesInFlight_ = 0;
};

} // namespace VulkanRTX

#endif // VULKANSWAPCHAINMANAGER_HPP