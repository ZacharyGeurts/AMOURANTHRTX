// include/engine/Vulkan/VulkanSwapchainManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0

#pragma once
#ifndef VULKANSWAPCHAINMANAGER_HPP
#define VULKANSWAPCHAINMANAGER_HPP

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <array>

namespace VulkanRTX {

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager(Vulkan::Context& context, VkSurfaceKHR surface);
    ~VulkanSwapchainManager();

    void initializeSwapchain(int width, int height);
    void handleResize(int width, int height);
    void cleanupSwapchain();

    // --- Accessors ---
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
    void waitForInFlightFrames() const;
    VkSwapchainKHR createNewSwapchain(int width, int height, VkSwapchainKHR oldSwapchain);

    Vulkan::Context& context_;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_ = {0, 0};
    uint32_t imageCount_ = 0;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;

    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;

    const uint32_t graphicsQueueFamilyIndex_;
    const uint32_t presentQueueFamilyIndex_;
    uint32_t maxFramesInFlight_ = 0;
};

} // namespace VulkanRTX

#endif // VULKANSWAPCHAINMANAGER_HPP