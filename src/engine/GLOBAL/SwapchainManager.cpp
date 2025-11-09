// src/engine/GLOBAL/SwapchainManager.cpp
// AMOURANTH RTX — HYPER-SECURE STONEKEYED SWAPCHAIN — PERFECT LOGGING — NOVEMBER 08 2025
// GETTER/SETTER PERFECTION | HACKER-IMPENETRABLE | TOASTER-SECURE

#include "engine/GLOBAL/SwapchainManager.hpp"
#include <algorithm>
#include <set>
#include <iomanip>

#define VK_CHECK_NOMSG(call) do {                    \
    VkResult __res = (call);                         \
    if (__res != VK_SUCCESS) {                       \
        VulkanSwapchainManager::vkError(__res, "Vulkan call failed", __FILE__, __LINE__); \
    }                                                \
} while (0)

#define VK_CHECK(call, msg) do {                     \
    VkResult __res = (call);                         \
    if (__res != VK_SUCCESS) {                       \
        VulkanSwapchainManager::vkError(__res, msg, __FILE__, __LINE__); \
    }                                                \
} while (0)

void VulkanSwapchainManager::init(VkInstance instance, VkPhysicalDevice physDev, VkDevice device,
                                  VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    instance_ = instance;
    physDevice_ = physDev;
    device_ = device;
    surface_ = surface;

    LOG_SUCCESS_CAT("Swapchain", "{}ULTIMATE STONKEYED INIT — {}x{} — {} — PINK PHOTONS AWAKEN 🩷🚀{}",
                    Logging::Color::DIAMOND_WHITE, width, height, debugName_, Logging::Color::RESET);

    createSwapchain(width, height);
    createImageViews();
    printStats();
}

void VulkanSwapchainManager::createSwapchain(uint32_t width, uint32_t height) {
    VK_CHECK(vkDeviceWaitIdle(device_), "Device not idle before swapchain creation");

    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice_, surface_, &caps));

    swapchainExtent_ = selectSwapchainExtent(caps, width, height);
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    std::vector<VkSurfaceFormatKHR> formats;
    uint32_t formatCount;
    VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice_, surface_, &formatCount, nullptr));
    formats.resize(formatCount);
    VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice_, surface_, &formatCount, formats.data()));

    std::vector<VkPresentModeKHR> presentModes;
    uint32_t presentModeCount;
    VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice_, surface_, &presentModeCount, nullptr));
    presentModes.resize(presentModeCount);
    VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice_, surface_, &presentModeCount, presentModes.data()));

    VkSurfaceFormatKHR surfaceFormat = selectSwapchainFormat(formats);
    VkPresentModeKHR presentMode = selectSwapchainPresentMode(presentModes);

    VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapchainExtent_;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | extraUsage_;

    uint32_t queueFamilyIndices[] = {0, 0};  // Assume single queue family
    if (false) {  // Placeholder: implement proper queue family check
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = getRawSwapchain();

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &newSwapchain), "STONKEYED swapchain creation FAILED — VALHALLA DENIED");
    swapchain_enc_ = encrypt(newSwapchain);
    swapchainFormat_ = surfaceFormat.format;
    presentMode_ = presentMode;

    uint32_t imgCount = 0;
    VK_CHECK_NOMSG(vkGetSwapchainImagesKHR(device_, newSwapchain, &imgCount, nullptr));
    std::vector<VkImage> images(imgCount);
    VK_CHECK_NOMSG(vkGetSwapchainImagesKHR(device_, newSwapchain, &imgCount, images.data()));

    swapchainImages_enc_.resize(imgCount);
    for (uint32_t i = 0; i < imgCount; ++i) {
        swapchainImages_enc_[i] = encrypt(images[i]);
    }

    LOG_SUCCESS_CAT("Swapchain", "{}STONKEYED SWAPCHAIN FORGED — {}x{} — {} images — Mode: {} (0x{:X}) — HACKERS BLIND 🩷🔥{}",
                    Logging::Color::EMERALD_GREEN, swapchainExtent_.width, swapchainExtent_.height, imgCount,
                    presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" : "FIFO",
                    static_cast<uint32_t>(presentMode), Logging::Color::RESET);
}

void VulkanSwapchainManager::createImageViews() {
    swapchainImageViews_enc_.resize(swapchainImages_enc_.size());

    for (size_t i = 0; i < swapchainImages_enc_.size(); ++i) {
        VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        createInfo.image = decrypt<VkImage>(swapchainImages_enc_[i]);
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainFormat_;
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
        VK_CHECK(vkCreateImageView(device_, &createInfo, nullptr, &view), "STONKEYED ImageView creation FAILED — RENDER IMPOSSIBLE");
        swapchainImageViews_enc_[i] = encrypt(view);
    }

    LOG_SUCCESS_CAT("Swapchain", "{}STONKEYED {} IMAGE VIEWS — FORMAT 0x{:X} — TOASTER-PROOF — VALHALLA SEALED 🩷⚡{}",
                    Logging::Color::OCEAN_TEAL, swapchainImageViews_enc_.size(), static_cast<uint32_t>(swapchainFormat_), Logging::Color::RESET);
}

void VulkanSwapchainManager::cleanupSwapchainOnly() noexcept {
    for (auto enc : swapchainImageViews_enc_) {
        if (enc != 0) {
            vkDestroyImageView(device_, decrypt<VkImageView>(enc), nullptr);
        }
    }
    swapchainImageViews_enc_.clear();
    swapchainImages_enc_.clear();

    if (swapchain_enc_ != 0) {
        vkDestroySwapchainKHR(device_, decrypt<VkSwapchainKHR>(swapchain_enc_), nullptr);
        swapchain_enc_ = 0;
    }
}

void VulkanSwapchainManager::cleanup() noexcept {
    VK_CHECK(vkDeviceWaitIdle(device_), "Cleanup wait idle failed");
    cleanupSwapchainOnly();
    LOG_SUCCESS_CAT("Swapchain", "{}STONKEYED SWAPCHAIN PURGED — {} HANDLES OBLITERATED — COSMIC VOID ACHIEVED 🩷💀{}",
                    Logging::Color::CRIMSON_MAGENTA, getImageCount() + 1, Logging::Color::RESET);
}

void VulkanSwapchainManager::recreate(uint32_t width, uint32_t height) {
    if (!isValid()) {
        LOG_WARN_CAT("Swapchain", "Recreate called on invalid swapchain — skipping");
        return;
    }

    VK_CHECK(vkDeviceWaitIdle(device_), "Recreate wait idle failed");
    cleanupSwapchainOnly();
    createSwapchain(width, height);
    createImageViews();
    printStats();

    LOG_SUCCESS_CAT("Swapchain", "{}SWAPCHAIN REBORN — {}x{} — {} — RASPBERRY_PINK SUPREMACY RESTORED 🩷🚀🔥{}",
                    Logging::Color::LIME_YELLOW, width, height, debugName_, Logging::Color::RESET);
}

// ────── ADVANCED IMPLEMENTATIONS ──────
std::optional<VkSurfaceFormatKHR> VulkanSwapchainManager::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const noexcept {
    // Improved selection: prefer SRGB, fallback to first
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB || format.format == VK_FORMAT_R8G8B8A8_SRGB) {
            return format;
        }
    }
    return availableFormats.empty() ? std::nullopt : std::optional(availableFormats[0]);
}

std::optional<VkPresentModeKHR> VulkanSwapchainManager::choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const noexcept {
    // Prefer mailbox for tear-free, fallback to FIFO
    for (const auto& mode : availablePresentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
    }
    for (const auto& mode : availablePresentModes) {
        if (mode == VK_PRESENT_MODE_FIFO_KHR) return mode;
    }
    return availablePresentModes.empty() ? std::nullopt : std::optional(availablePresentModes[0]);
}

void VulkanSwapchainManager::acquireNextImage(VkSemaphore imageAvailableSemaphore, VkFence imageAvailableFence, uint32_t& imageIndex) noexcept {
    VK_CHECK(vkAcquireNextImageKHR(device_, getRawSwapchain(), UINT64_MAX,
                                   imageAvailableSemaphore, imageAvailableFence, &imageIndex),
             "Acquire next image failed — FRAME STALLED");
}

VkResult VulkanSwapchainManager::present(VkQueue presentQueue, const std::vector<VkSemaphore>& waitSemaphores, uint32_t& imageIndex) noexcept {
    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    presentInfo.pWaitSemaphores = waitSemaphores.data();
    presentInfo.swapchainCount = 1;
    VkSwapchainKHR rawSwapchain = getRawSwapchain();
    presentInfo.pSwapchains = &rawSwapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;  // Single swapchain

    return vkQueuePresentKHR(presentQueue, &presentInfo);
}

void VulkanSwapchainManager::printStats() const noexcept {
    LOG_SUCCESS_CAT("Swapchain", "{}STONKEYED STATS — {}x{} | {} images | Format: 0x{:X} | Mode: 0x{:X} | Usage: 0x{:X} | {} ENCRYPTED 🩷{}",
                    Logging::Color::DIAMOND_WHITE, swapchainExtent_.width, swapchainExtent_.height,
                    getImageCount(), static_cast<uint32_t>(swapchainFormat_), static_cast<uint32_t>(presentMode_),
                    static_cast<uint32_t>(getImageUsage()), debugName_, Logging::Color::RESET);
}

void VulkanSwapchainManager::dumpAllHandles() const noexcept {
    LOG_DEBUG_CAT("Swapchain", "DUMPING ENCRYPTED HANDLES:");
    LOG_DEBUG_CAT("Swapchain", "  Swapchain: 0x{:016X}", swapchain_enc_);
    for (size_t i = 0; i < swapchainImages_enc_.size(); ++i) {
        LOG_DEBUG_CAT("Swapchain", "  Image[{}]: 0x{:016X}", i, swapchainImages_enc_[i]);
    }
    for (size_t i = 0; i < swapchainImageViews_enc_.size(); ++i) {
        LOG_DEBUG_CAT("Swapchain", "  View[{}]: 0x{:016X}", i, swapchainImageViews_enc_[i]);
    }
}

// ────── PRIVATE HELPERS ──────
VkSurfaceFormatKHR VulkanSwapchainManager::selectSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const noexcept {
    auto opt = chooseSurfaceFormat(availableFormats);
    return opt.value_or(availableFormats[0]);
}

VkPresentModeKHR VulkanSwapchainManager::selectSwapchainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const noexcept {
    auto opt = choosePresentMode(availablePresentModes);
    return opt.value_or(availablePresentModes[0]);
}

VkExtent2D VulkanSwapchainManager::selectSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) const noexcept {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = { width, height };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}