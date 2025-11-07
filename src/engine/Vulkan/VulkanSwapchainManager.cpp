// VulkanSwapchainManager.cpp
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include <stdexcept>

void VulkanSwapchainManager::init(VkDevice device, VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    device_ = device;
    createSwapchain(width, height, surface);
    createImageViews();
}

void VulkanSwapchainManager::cleanup() {
    for (auto enc_view : swapchainImageViews_enc_) {
        VkImageView view = decrypt<VkImageView>(enc_view);
        vkDestroyImageView(device_, view, nullptr);
    }
    swapchainImageViews_enc_.clear();
    swapchainImages_enc_.clear();

    if (swapchain_enc_) {
        VkSwapchainKHR raw = decrypt<VkSwapchainKHR>(swapchain_enc_);
        vkDestroySwapchainKHR(device_, raw, nullptr);
        swapchain_enc_ = 0;
    }
}

void VulkanSwapchainManager::createSwapchain(uint32_t width, uint32_t height, VkSurfaceKHR surface) {
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = 3;
    createInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = {width, height};
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    createInfo.clipped = VK_TRUE;

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &newSwapchain));
    swapchain_enc_ = encrypt(newSwapchain);

    uint32_t imageCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device_, newSwapchain, &imageCount, nullptr));
    std::vector<VkImage> images(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device_, newSwapchain, &imageCount, images.data()));

    swapchainImages_enc_.clear();
    for (auto img : images) {
        swapchainImages_enc_.push_back(encrypt(img));
    }
}

void VulkanSwapchainManager::createImageViews() {
    swapchainImageViews_enc_.resize(swapchainImages_enc_.size());
    VkSwapchainKHR swapchain = decrypt<VkSwapchainKHR>(swapchain_enc_);

    for (size_t i = 0; i < swapchainImages_enc_.size(); ++i) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = decrypt<VkImage>(swapchainImages_enc_[i]);
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkImageView view = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImageView(device_, &createInfo, nullptr, &view));
        swapchainImageViews_enc_[i] = encrypt(view);
    }
}

void VulkanSwapchainManager::recreate(uint32_t width, uint32_t height) {
    vkDeviceWaitIdle(device_);
    cleanup();
    createSwapchain(width, height, /*surface from somewhere*/ nullptr);
    createImageViews();
}