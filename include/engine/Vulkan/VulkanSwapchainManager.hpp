// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan swapchain management header.
// Dependencies: Vulkan 1.3+, VulkanCore.hpp, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#pragma once
#ifndef VULKANSWAPCHAINMANAGER_HPP
#define VULKANSWAPCHAINMANAGER_HPP

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <vector>

namespace VulkanRTX {

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager(Vulkan::Context& context, VkSurfaceKHR surface);
    ~VulkanSwapchainManager();

    void initializeSwapchain(int width, int height);
    void handleResize(int width, int height);
    void cleanupSwapchain();

    VkSwapchainKHR getSwapchain() const { return swapchain_; }
    VkFormat getSwapchainImageFormat() const { return swapchainImageFormat_; }
    VkExtent2D getSwapchainExtent() const { return swapchainExtent_; }
    const std::vector<VkImage>& getSwapchainImages() const { return swapchainImages_; }
    const std::vector<VkImageView>& getSwapchainImageViews() const { return swapchainImageViews_; }
    VkSemaphore getImageAvailableSemaphore(uint32_t currentFrame) const;
    VkSemaphore getRenderFinishedSemaphore(uint32_t currentFrame) const;
    VkFence getInFlightFence(uint32_t currentFrame) const;
    uint32_t getMaxFramesInFlight() const { return maxFramesInFlight_; }

private:
    Vulkan::Context& context_;
    VkSwapchainKHR swapchain_;
    VkFormat swapchainImageFormat_;
    VkExtent2D swapchainExtent_;
    uint32_t imageCount_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkSemaphore> imageAvailableSemaphores_;  // Per-frame
    std::vector<VkSemaphore> renderFinishedSemaphores_;  // Per-frame
    std::vector<VkFence> inFlightFences_;  // Per-frame
    uint32_t graphicsQueueFamilyIndex_;
    uint32_t presentQueueFamilyIndex_;
    uint32_t maxFramesInFlight_ = 0;  // Determined during initialization
};

} // namespace VulkanRTX

#endif // VULKANSWAPCHAINMANAGER_HPP