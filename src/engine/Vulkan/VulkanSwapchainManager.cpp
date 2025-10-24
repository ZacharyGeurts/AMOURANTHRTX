// src/engine/Vulkan/VulkanSwapchainManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan swapchain management implementation.
// Dependencies: Vulkan 1.3+, VulkanCore.hpp, Vulkan_init.hpp, Dispose.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Dispose.hpp"
#include <stdexcept>
#include <algorithm>

namespace VulkanRTX {

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;  // Standard limit for in-flight frames

VulkanSwapchainManager::VulkanSwapchainManager(Vulkan::Context& context, VkSurfaceKHR surface)
    : context_(context),
      swapchain_(VK_NULL_HANDLE),
      swapchainImageFormat_(VK_FORMAT_UNDEFINED),
      swapchainExtent_({0, 0}),
      imageCount_(0),
      graphicsQueueFamilyIndex_(context_.graphicsQueueFamilyIndex),
      presentQueueFamilyIndex_(context_.presentQueueFamilyIndex),
      maxFramesInFlight_(0) {
    if (surface == VK_NULL_HANDLE) {
        throw std::runtime_error("Invalid surface");
    }
    if (graphicsQueueFamilyIndex_ == UINT32_MAX || presentQueueFamilyIndex_ == UINT32_MAX) {
        throw std::runtime_error("Invalid queue family indices");
    }
    context_.surface = surface;

    // Pre-allocate per-frame semaphores
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkSemaphoreCreateInfo semaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0  // Binary semaphore
        };
        VkResult result = vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render finished semaphore");
        }
    }
}

VulkanSwapchainManager::~VulkanSwapchainManager() {
    cleanupSwapchain();
    Dispose::destroySemaphores(context_.device, renderFinishedSemaphores_);
}

void VulkanSwapchainManager::initializeSwapchain(int width, int height) {
    if (context_.device == VK_NULL_HANDLE) {
        throw std::runtime_error("Null device");
    }
    if (context_.physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Null physical device");
    }
    if (context_.surface == VK_NULL_HANDLE) {
        throw std::runtime_error("Null surface");
    }
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid window dimensions for swapchain: " + std::to_string(width) + "x" + std::to_string(height));
    }

    // Wait for device to be idle
    vkDeviceWaitIdle(context_.device);

    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, context_.surface, &capabilities);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to query surface capabilities");
    }

    if (capabilities.minImageCount == 0) {
        throw std::runtime_error("Invalid surface capabilities");
    }

    // Additional check for degenerate extents
    if (capabilities.minImageExtent.width == 0 || capabilities.minImageExtent.height == 0 ||
        capabilities.maxImageExtent.width == 0 || capabilities.maxImageExtent.height == 0) {
        throw std::runtime_error("Surface extents invalid - ensure SDL_ShowWindow called and window is resized");
    }

    // Query surface formats
    uint32_t formatCount;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, context_.surface, &formatCount, nullptr);
    if (result != VK_SUCCESS || formatCount == 0) {
        throw std::runtime_error("No surface formats available");
    }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, context_.surface, &formatCount, formats.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to query surface formats");
    }

    // Query present modes
    uint32_t presentModeCount;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, context_.surface, &presentModeCount, nullptr);
    if (result != VK_SUCCESS || presentModeCount == 0) {
        throw std::runtime_error("No present modes available");
    }
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, context_.surface, &presentModeCount, presentModes.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to query present modes");
    }

    // Choose surface format
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& availableFormat : formats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = availableFormat;
            break;
        }
    }
    swapchainImageFormat_ = surfaceFormat.format;

    // Choose present mode - Prioritize IMMEDIATE for max FPS (may tear), fallback to MAILBOX, then FIFO
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    bool immediateSupported = false;
    for (const auto& availablePresentMode : presentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            immediateSupported = true;
            break;
        }
    }
    if (!immediateSupported) {
        presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        for (const auto& availablePresentMode : presentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = availablePresentMode;
                break;
            }
        }
        if (presentMode != VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = VK_PRESENT_MODE_FIFO_KHR;
        }
    }

    // Choose extent
    swapchainExtent_ = capabilities.currentExtent;
    if (swapchainExtent_.width == 0 || swapchainExtent_.height == 0) {
        swapchainExtent_.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        swapchainExtent_.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    if (swapchainExtent_.width == 0 || swapchainExtent_.height == 0) {
        throw std::runtime_error("Invalid swapchain extent (window not ready?)");
    }

    // Choose image count
    imageCount_ = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount_ > capabilities.maxImageCount) {
        imageCount_ = capabilities.maxImageCount;
    }

    // Queue families
    if (graphicsQueueFamilyIndex_ == UINT32_MAX || presentQueueFamilyIndex_ == UINT32_MAX) {
        throw std::runtime_error("Invalid queue family indices");
    }
    std::vector<uint32_t> queueFamilies;
    VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (graphicsQueueFamilyIndex_ != presentQueueFamilyIndex_) {
        queueFamilies = {graphicsQueueFamilyIndex_, presentQueueFamilyIndex_};
        sharingMode = VK_SHARING_MODE_CONCURRENT;
    }

    // Create swapchain
    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = context_.surface,
        .minImageCount = imageCount_,
        .imageFormat = swapchainImageFormat_,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = swapchainExtent_,
        .imageArrayLayers = 1,
        .imageUsage = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT ? VK_IMAGE_USAGE_STORAGE_BIT : 0)),
        .imageSharingMode = sharingMode,
        .queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size()),
        .pQueueFamilyIndices = queueFamilies.data(),
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    result = vkCreateSwapchainKHR(context_.device, &createInfo, nullptr, &swapchain_);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to create swapchain");
    }

    // Retrieve swapchain images
    result = vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, nullptr);
    if (result != VK_SUCCESS || imageCount_ == 0) {
        throw std::runtime_error("Failed to get swapchain image count");
    }
    swapchainImages_.resize(imageCount_);
    if (vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, swapchainImages_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to retrieve swapchain images");
    }
    if (swapchainImages_.empty()) {
        throw std::runtime_error("Empty swapchain images");
    }
    for (uint32_t i = 0; i < imageCount_; ++i) {
        context_.resourceManager.addImage(swapchainImages_[i]);
    }

    // Create image views
    swapchainImageViews_.resize(imageCount_);
    for (uint32_t i = 0; i < imageCount_; ++i) {
        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = swapchainImages_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainImageFormat_,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        if (vkCreateImageView(context_.device, &viewInfo, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view for swapchain image " + std::to_string(i));
        }
        context_.resourceManager.addImageView(swapchainImageViews_[i]);
    }
    if (swapchainImageViews_.empty()) {
        throw std::runtime_error("Empty swapchain image views after creation");
    }

    // Set max frames
    maxFramesInFlight_ = std::min(MAX_FRAMES_IN_FLIGHT, imageCount_);

    // Update context
    context_.swapchain = swapchain_;
    context_.swapchainImageFormat = swapchainImageFormat_;
    context_.swapchainExtent = swapchainExtent_;
    context_.swapchainImages = swapchainImages_;
    context_.swapchainImageViews = swapchainImageViews_;
}

void VulkanSwapchainManager::handleResize(int width, int height) {
    vkDeviceWaitIdle(context_.device);
    cleanupSwapchain();
    initializeSwapchain(width, height);
}

void VulkanSwapchainManager::cleanupSwapchain() {
    vkDeviceWaitIdle(context_.device);

    // Destroy image views
    Dispose::destroyImageViews(context_.device, swapchainImageViews_);
    context_.swapchainImageViews.clear();

    // Swapchain images are owned by the swapchain, just clear the vector
    for (uint32_t i = 0; i < swapchainImages_.size(); ++i) {
        context_.resourceManager.removeImage(swapchainImages_[i]);
    }
    swapchainImages_.clear();
    context_.swapchainImages.clear();

    // Destroy swapchain
    Dispose::destroySingleSwapchain(context_.device, swapchain_);
    context_.swapchain = VK_NULL_HANDLE;

    // Reset local state
    maxFramesInFlight_ = 0;
    imageCount_ = 0;
    swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    swapchainExtent_ = {0, 0};
}

VkSemaphore VulkanSwapchainManager::getRenderFinishedSemaphore(uint32_t currentFrame) const {
    if (currentFrame >= renderFinishedSemaphores_.size()) {
        throw std::out_of_range("Invalid current frame for render finished semaphore: " + std::to_string(currentFrame));
    }
    return renderFinishedSemaphores_[currentFrame];
}

} // namespace VulkanRTX