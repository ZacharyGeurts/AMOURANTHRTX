// src/engine/Vulkan/VulkanSwapchain.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// UNREAL-KILLER: Triple-buffer + MAILBOX + immediate fallback + full logging + zero-overhead + RAII-safe
// DEVELOPER CONFIG: SwapchainConfig::DESIRED_PRESENT_MODE, FORCE_VSYNC, FORCE_TRIPLE_BUFFER

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"  // for MAX_FRAMES_IN_FLIGHT
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <algorithm>
#include <format>
#include <bit>

using namespace VulkanRTX;

// ---------------------------------------------------------------------
//  Helper: sRGB preferred format
// ---------------------------------------------------------------------
static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available)
{
    for (const auto& fmt : available) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return fmt;
    }
    return available[0];
}

// ---------------------------------------------------------------------
//  CREATE SWAPCHAIN – DEVELOPER-CONFIGURABLE TRIPLE BUFFER + PRESENT MODE
// ---------------------------------------------------------------------
void Vulkan::Context::createSwapchain()
{
    LOG_INFO_CAT("Swapchain", "createSwapchain() START – developer-configurable swapchain");

    // ────────────────────── Surface Capabilities ──────────────────────
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps),
             "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    // ────────────────────── Surface Formats ──────────────────────
    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr),
             "surface format count");
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()),
             "surface formats");

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(formats);
    LOG_INFO_CAT("Swapchain", "Selected format: {} | colorSpace: {}",
                 static_cast<int>(surfaceFormat.format), static_cast<int>(surfaceFormat.colorSpace));

    // ────────────────────── Present Modes ──────────────────────
    uint32_t presentModeCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr),
             "present mode count");
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()),
             "present modes");

    bool mailboxAvailable = false;
    bool immediateAvailable = false;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)   mailboxAvailable = true;
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) immediateAvailable = true;
    }

    // ── APPLY DEVELOPER CONFIG FROM main.cpp ─────────────────────────────
    using namespace SwapchainConfig;

    VkPresentModeKHR chosenMode = VK_PRESENT_MODE_FIFO_KHR;
    std::string modeStr = "UNKNOWN";

    if (FORCE_VSYNC) {
        chosenMode = VK_PRESENT_MODE_FIFO_KHR;
        modeStr = "FIFO (VSync, 60 FPS cap)";
        LOG_INFO_CAT("Swapchain", "FORCE_VSYNC = true → using FIFO (60 FPS cap)");
    } else if (DESIRED_PRESENT_MODE == VK_PRESENT_MODE_MAILBOX_KHR && mailboxAvailable) {
        chosenMode = VK_PRESENT_MODE_MAILBOX_KHR;
        modeStr = "MAILBOX (triple-buffer, tear-free, uncapped)";
        LOG_INFO_CAT("Swapchain", "DESIRED_PRESENT_MODE = MAILBOX → using MAILBOX");
    } else if (DESIRED_PRESENT_MODE == VK_PRESENT_MODE_IMMEDIATE_KHR && immediateAvailable) {
        chosenMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        modeStr = "IMMEDIATE (uncapped, may tear)";
        LOG_INFO_CAT("Swapchain", "DESIRED_PRESENT_MODE = IMMEDIATE → using IMMEDIATE");
    } else if (DESIRED_PRESENT_MODE == VK_PRESENT_MODE_FIFO_KHR) {
        chosenMode = VK_PRESENT_MODE_FIFO_KHR;
        modeStr = "FIFO (VSync, 60 FPS)";
        LOG_INFO_CAT("Swapchain", "DESIRED_PRESENT_MODE = FIFO → using FIFO");
    } else {
        // Fallback
        if (mailboxAvailable) {
            chosenMode = VK_PRESENT_MODE_MAILBOX_KHR;
            modeStr = "MAILBOX (fallback)";
            LOG_INFO_CAT("Swapchain", "Fallback → MAILBOX available → using MAILBOX");
        } else if (immediateAvailable) {
            chosenMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            modeStr = "IMMEDIATE (fallback)";
            LOG_INFO_CAT("Swapchain", "Fallback → IMMEDIATE available → using IMMEDIATE");
        } else {
            chosenMode = VK_PRESENT_MODE_FIFO_KHR;
            modeStr = "FIFO (fallback)";
            LOG_INFO_CAT("Swapchain", "Fallback → using FIFO (VSync)");
        }
    }

    // ────────────────────── Extent (HiDPI) ──────────────────────
    if (caps.currentExtent.width != UINT32_MAX) {
        swapchainExtent = caps.currentExtent;
    } else {
        int w = width, h = height;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        swapchainExtent.width  = std::clamp(static_cast<uint32_t>(w), caps.minImageExtent.width,  caps.maxImageExtent.width);
        swapchainExtent.height = std::clamp(static_cast<uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    LOG_INFO_CAT("Swapchain", "Swapchain extent: {}x{}", swapchainExtent.width, swapchainExtent.height);

    // ────────────────────── Image Count: Force Triple Buffer ──────────────────────
    uint32_t imageCount = std::max(caps.minImageCount, 3u);
    if (FORCE_TRIPLE_BUFFER && imageCount < 3) {
        imageCount = 3;
        LOG_INFO_CAT("Swapchain", "FORCE_TRIPLE_BUFFER = true → requesting {} images", imageCount);
    }
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
        LOG_INFO_CAT("Swapchain", "Clamped image count to max: {}", imageCount);
    }
    LOG_INFO_CAT("Swapchain", "Image count: {} (min: {}, max: {})", imageCount, caps.minImageCount, caps.maxImageCount);

    // ────────────────────── Create Swapchain ──────────────────────
    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = chosenMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    uint32_t queueFamilyIndices[] = { graphicsQueueFamilyIndex, presentQueueFamilyIndex };
    if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
        LOG_INFO_CAT("Swapchain", "Sharing: CONCURRENT (graphics: {}, present: {})",
                     graphicsQueueFamilyIndex, presentQueueFamilyIndex);
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        LOG_INFO_CAT("Swapchain", "Sharing: EXCLUSIVE (QFI: {})", graphicsQueueFamilyIndex);
    }

    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain),
             "vkCreateSwapchainKHR");

    LOG_INFO_CAT("Swapchain", "Swapchain created: 0x{:016x}", std::bit_cast<uint64_t>(swapchain));

    // ────────────────────── Retrieve Images ──────────────────────
    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imgCount, nullptr),
             "swapchain image count");
    swapchainImages.resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imgCount, swapchainImages.data()),
             "swapchain images");

    swapchainImageFormat = surfaceFormat.format;
    LOG_INFO_CAT("Swapchain", "Retrieved {} swapchain images", swapchainImages.size());

    // ────────────────────── Create Image Views ──────────────────────
    swapchainImageViews.resize(swapchainImages.size());
    for (size_t i = 0; i < swapchainImages.size(); ++i) {
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainImageFormat,
            .components = {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };

        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]),
                 std::format("vkCreateImageView #{}", i));
    }

    LOG_INFO_CAT("Swapchain", "Created {} image views", swapchainImageViews.size());

    // ────────────────────── FINAL CONFIG SUMMARY (DEVELOPER-FACING) ──────────────────────
    if (SwapchainConfig::LOG_FINAL_CONFIG) {
        LOG_INFO_CAT("Swapchain", "createSwapchain() COMPLETE – DEVELOPER CONFIG:");
        LOG_INFO_CAT("Swapchain", "  • Desired Mode : {}", 
                     [DESIRED_PRESENT_MODE = SwapchainConfig::DESIRED_PRESENT_MODE]() -> std::string {
                         switch (DESIRED_PRESENT_MODE) {
                             case VK_PRESENT_MODE_MAILBOX_KHR:   return "MAILBOX";
                             case VK_PRESENT_MODE_IMMEDIATE_KHR:return "IMMEDIATE";
                             case VK_PRESENT_MODE_FIFO_KHR:     return "FIFO";
                             default:                           return "UNKNOWN";
                         }
                     }());
        LOG_INFO_CAT("Swapchain", "  • Force VSync  : {}", SwapchainConfig::FORCE_VSYNC ? "YES" : "NO");
        LOG_INFO_CAT("Swapchain", "  • Force Triple : {}", SwapchainConfig::FORCE_TRIPLE_BUFFER ? "YES" : "NO");
        LOG_INFO_CAT("Swapchain", "  • Final Mode   : {}", modeStr);
        LOG_INFO_CAT("Swapchain", "  • Images       : {} {}", swapchainImages.size(),
                     (swapchainImages.size() >= 3 ? "(TRIPLE BUFFER)" : "(DOUBLE BUFFER)"));
        LOG_INFO_CAT("Swapchain", "  • Extent       : {}x{}", swapchainExtent.width, swapchainExtent.height);
        LOG_INFO_CAT("Swapchain", "  • Format       : {} (sRGB)", static_cast<int>(swapchainImageFormat));
        LOG_INFO_CAT("Swapchain", "  • FPS          : {}", 
                     (chosenMode == VK_PRESENT_MODE_FIFO_KHR ? "60 (VSync)" : "UNLIMITED"));
    }
}

// ---------------------------------------------------------------------
//  DESTROY SWAPCHAIN – FULL RAII
// ---------------------------------------------------------------------
void Vulkan::Context::destroySwapchain()
{
    if (!device) return;

    LOG_INFO_CAT("Swapchain", "DESTROYING – {} image views", swapchainImageViews.size());

    for (auto view : swapchainImageViews) {
        if (view) vkDestroyImageView(device, view, nullptr);
    }
    swapchainImageViews.clear();

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
    swapchainImages.clear();

    LOG_INFO_CAT("Swapchain", "DESTROYED – resources released");
}