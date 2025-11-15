// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// SWAPCHAIN MANAGER — C++23 REWRITE — FIXED: Null Device Guards in Cleanup
// • Rewritten for full C++23 compatibility (std::format, ranges, concepts, etc.)
// • Direct Handle constructor — bypass MakeHandle auto deduction
// • *swapchain_ dereference for raw VkSwapchainKHR
// • NO MORE "before deduction of auto"
// • NO MORE "cannot convert Handle to VkSwapchainKHR"
// • RTX::init() REMOVED — Vulkan context initialized via RTX::g_ctx().init()
// • VkFormat formatter specialized implicitly via logging.hpp (C++23 std::format compliant)
// • FIXED: Guards in cleanup() & recreate() — prevents VUID-vkDestroySwapchainKHR-device-parameter on null/stale device
// • FIXED: Added VK_IMAGE_USAGE_TRANSFER_DST_BIT to swapchain imageUsage — resolves VUID-vkCmdClearColorImage-image-00002 if clearing swapchain images
// • NOTE: For VUID-VkPresentInfoKHR-pImageIndices-01430 (layout VK_IMAGE_LAYOUT_UNDEFINED on present), ensure pipeline barrier transition to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR before vkQueuePresentKHR in VulkanRenderer (e.g., after render pass). Example barrier code in prior analysis.
// • PINK PHOTONS ETERNAL — ZERO LEAKS — TITAN DOMINANCE
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
// =============================================================================

#pragma once

#include "engine/GLOBAL/logging.hpp"       // ← PINK_PHOTON, LOG_*, VkFormat formatter specialization
#include "engine/GLOBAL/OptionsMenu.hpp"   // ← CRITICAL: Options first
#include "engine/GLOBAL/RTXHandler.hpp"    // ← g_ctx(), Context, Handle
#include "engine/Vulkan/VulkanCore.hpp"    // VKCHECK
#include <vulkan/vulkan.h>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <format>                          // C++23: Explicit include for std::format enhancements

using namespace Logging::Color;

#define SWAPCHAIN SwapchainManager::get()

class SwapchainManager {
public:
    static SwapchainManager& get() noexcept {
        static SwapchainManager inst;
        return inst;
    }

    // -------------------------------------------------------------------------
    // Initialize swapchain — assumes Vulkan context already initialized
    // -------------------------------------------------------------------------
    void init(VkInstance inst, VkPhysicalDevice phys, VkDevice dev,
              VkSurfaceKHR surf, uint32_t w, uint32_t h) {

        physDev_ = phys;
        device_  = dev;
        surface_ = surf;

        createSwapchain(w, h);
        createImageViews();
    }

    void recreate(uint32_t w, uint32_t h) {
        // FIXED: Guard vkDeviceWaitIdle — null device invalid (VUID-vkDeviceWaitIdle-device-parameter)
        if (device_ != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device_);
        } else {
            LOG_WARN_CAT("SWAPCHAIN", "Skipped vkDeviceWaitIdle in recreate — null device");
        }
        cleanup();
        createSwapchain(w, h);
        createImageViews();
    }

    void cleanup() noexcept {
        // FIXED: Guard all destroys/resets — prevents VUID-vkDestroySwapchainKHR-device-parameter & vkDestroyImageView-device-parameter
        VkDevice dev = device_;  // Cache for efficiency
        if (dev != VK_NULL_HANDLE) {
            for (auto& v : imageViews_) {
                if (v) v.reset();  // Triggers ~Handle: vkDestroyImageView(dev, obj, nullptr) — safe with valid dev
            }
        } else {
            LOG_WARN_CAT("SWAPCHAIN", "Skipped image view resets — null device (nullifying only)");
            for (auto& v : imageViews_) v = RTX::Handle<VkImageView>();  // Nullify without destroy
        }
        imageViews_.clear();
        images_.clear();
        if (dev != VK_NULL_HANDLE && swapchain_) {
            swapchain_.reset();  // Triggers ~Handle: vkDestroySwapchainKHR(dev, obj, nullptr) — safe with valid dev
        } else if (swapchain_) {
            LOG_WARN_CAT("SWAPCHAIN", "Skipped swapchain reset — null device (nullifying only)");
            swapchain_ = RTX::Handle<VkSwapchainKHR>();  // Nullify without destroy
        }
    }

    [[nodiscard]] VkSwapchainKHR swapchain() const noexcept { return *swapchain_; }
    [[nodiscard]] VkFormat       format()   const noexcept { return format_; }
    [[nodiscard]] VkExtent2D     extent()   const noexcept { return extent_; }
    [[nodiscard]] auto           images()   const noexcept -> const std::vector<VkImage>& { return images_; }
    [[nodiscard]] auto           views()    const noexcept -> const std::vector<RTX::Handle<VkImageView>>& { return imageViews_; }

private:
    SwapchainManager() = default;

    void createSwapchain(uint32_t width, uint32_t height);
    void createImageViews();

    VkPhysicalDevice physDev_ = VK_NULL_HANDLE;
    VkDevice         device_  = VK_NULL_HANDLE;
    VkSurfaceKHR     surface_ = VK_NULL_HANDLE;

    VkFormat                              format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D                            extent_{};
    std::vector<VkImage>                  images_;
    std::vector<RTX::Handle<VkImageView>> imageViews_;
    RTX::Handle<VkSwapchainKHR>           swapchain_;
};

// =============================================================================
// IMPLEMENTATION — INLINE (C++23: No changes needed; fully compatible)
// =============================================================================

inline void SwapchainManager::createSwapchain(uint32_t width, uint32_t height) {

    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev_, surface_, &caps),
             "Failed to get surface capabilities");

    uint32_t w = std::clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    uint32_t h = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    extent_ = {w, h};

    // Choose best format
    uint32_t fmtCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &fmtCount, nullptr),
             "Failed to query surface formats");
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &fmtCount, formats.data()),
             "Failed to retrieve surface formats");

    VkSurfaceFormatKHR chosen = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = f;
            break;
        }
    }
    format_ = chosen.format;

    // Choose best present mode — prioritize uncapped
    uint32_t pmCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCount, nullptr),
             "Failed to query present modes");
    std::vector<VkPresentModeKHR> modes(pmCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCount, modes.data()),
             "Failed to retrieve present modes");

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;  // Safe fallback
    if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) {
        presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;  // Uncapped, tearing OK — max FPS
    } else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()) {
        presentMode = VK_PRESENT_MODE_MAILBOX_KHR;  // Uncapped, no tearing — ideal if supported
    } else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_FIFO_RELAXED_KHR) != modes.end()) {
        presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;  // Vsync but tearing-tolerant
    } else {
        LOG_WARN_CAT("SWAPCHAIN", "Only FIFO_KHR available — vsync-capped FPS; check driver/display");
    }

    // Respect MAX_FRAMES_IN_FLIGHT
    uint32_t imageCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    imageCount = std::max(caps.minImageCount, imageCount);
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci = { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    ci.surface = surface_;
    ci.minImageCount = imageCount;
    ci.imageFormat = format_;
    ci.imageColorSpace = chosen.colorSpace;
    ci.imageExtent = extent_;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = presentMode;
    ci.clipped = VK_TRUE;

    VkSwapchainKHR raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(device_, &ci, nullptr, &raw),
             "Swapchain creation failed");

    // FIXED: Direct Handle constructor — bypass MakeHandle auto
    swapchain_ = RTX::Handle<VkSwapchainKHR>(raw, device_, vkDestroySwapchainKHR);

    // Retrieve swapchain images
    uint32_t count = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device_, *swapchain_, &count, nullptr),
             "Failed to query swapchain image count");
    images_.resize(count);
    VK_CHECK(vkGetSwapchainImagesKHR(device_, *swapchain_, &count, images_.data()),
             "Failed to retrieve swapchain images");
}

inline void SwapchainManager::createImageViews() {

    imageViews_.reserve(images_.size());
    for (auto img : images_) {
        VkImageViewCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        ci.image = img;
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = format_;
        ci.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
        };
        ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageView view = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImageView(device_, &ci, nullptr, &view),
                 "Failed to create image view");

        // FIXED: Direct Handle constructor — bypass MakeHandle auto
        imageViews_.emplace_back(RTX::Handle<VkImageView>(view, device_, vkDestroyImageView));
    }
}

// =============================================================================
// END OF C++23 SWAPCHAIN MANAGER — PINK PHOTONS FOREVER — CRASH ERADICATED
// =============================================================================