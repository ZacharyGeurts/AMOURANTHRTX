// src/engine/Vulkan/VulkanSwapchainManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// STONEKEY FINAL — OPAQUE HANDLE SAFE — NOVEMBER 08 2025

#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/logging.hpp"
#include <algorithm>

void VulkanSwapchainManager::init(VkInstance instance, VkPhysicalDevice physDev, VkDevice device,
                                  VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    instance_ = instance;
    physDevice_ = physDev;
    device_ = device;
    surface_ = surface;

    LOG_SUCCESS_CAT("Swapchain", "{}STONKEYED Manager INIT — {}x{} — RASPBERRY_PINK 🩷{}",
                    Logging::Color::DIAMOND_WHITE, width, height, Logging::Color::RESET);

    createSwapchain(width, height);
    createImageViews();
}

void VulkanSwapchainManager::createSwapchain(uint32_t width, uint32_t height) {
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice_, surface_, &caps));

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == ~0u) {
        extent = { std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width),
                   std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height) };
    }
    swapchainExtent_ = extent;

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface_,
        .minImageCount = imageCount,
        .imageFormat = swapchainFormat_,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_MAILBOX_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = getRawSwapchain()
    };

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &newSwapchain));
    swapchain_enc_ = encrypt(newSwapchain);

    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device_, newSwapchain, &imgCount, nullptr));
    std::vector<VkImage> images(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device_, newSwapchain, &imgCount, images.data()));

    swapchainImages_enc_.assign(imgCount, 0);
    for (uint32_t i = 0; i < imgCount; ++i) {
        swapchainImages_enc_[i] = encrypt(images[i]);
    }

    LOG_SUCCESS_CAT("Swapchain", "{}STONKEYED swapchain — {}x{} — {} images — HACKERS BLIND 🩷{}",
                    Logging::Color::EMERALD_GREEN, extent.width, extent.height, imgCount, Logging::Color::RESET);
}

void VulkanSwapchainManager::createImageViews() {
    swapchainImageViews_enc_.resize(swapchainImages_enc_.size());

    for (size_t i = 0; i < swapchainImages_enc_.size(); ++i) {
        VkImageViewCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = decrypt<VkImage>(swapchainImages_enc_[i]),
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainFormat_,
            .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };

        VkImageView view = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImageView(device_, &createInfo, nullptr, &view));
        swapchainImageViews_enc_[i] = encrypt(view);
    }

    LOG_SUCCESS_CAT("Swapchain", "{}STONKEYED {} image views — VALHALLA SECURE 🩷{}",
                    Logging::Color::OCEAN_TEAL, swapchainImageViews_enc_.size(), Logging::Color::RESET);
}

void VulkanSwapchainManager::cleanupSwapchainOnly() noexcept {
    for (auto enc : swapchainImageViews_enc_) {
        vkDestroyImageView(device_, decrypt<VkImageView>(enc), nullptr);
    }
    swapchainImageViews_enc_.clear();
    swapchainImages_enc_.clear();

    if (swapchain_enc_) {
        vkDestroySwapchainKHR(device_, decrypt<VkSwapchainKHR>(swapchain_enc_), nullptr);
        swapchain_enc_ = 0;
    }
}

void VulkanSwapchainManager::cleanup() noexcept {
    cleanupSwapchainOnly();
    LOG_SUCCESS_CAT("Swapchain", "{}STONKEYED swapchain purged — COSMIC VOID 🩷{}", Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET);
}

void VulkanSwapchainManager::recreate(uint32_t width, uint32_t height) {
    VK_CHECK(vkDeviceWaitIdle(device_));
    cleanupSwapchainOnly();
    createSwapchain(width, height);
    createImageViews();

    LOG_SUCCESS_CAT("Swapchain", "{}Swapchain RECREATED — {}x{} — RASPBERRY_PINK REBIRTH 🩷{}",
                    Logging::Color::EMERALD_GREEN, width, height, Logging::Color::RESET);
}