// include/engine/GLOBAL/SwapchainManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// SwapchainManager v9.0 — FINAL — NOV 13 2025 — FULLY FIXED
// • NO string_VkFormat / string_VkColorSpaceKHR — RAW ENUM + std::format
// • FULL LOGGING — OCEAN_TEAL + PINK PHOTONS
// • NO WINDOW PARAMETER — recreate(w, h) ONLY
// • GLOBAL ACCESS — SWAPCHAIN macro → SwapchainManager::get()
// • NO CONFLICT WITH VulkanRenderer.hpp
// • C++23, -Werror CLEAN
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <span>
#include <functional>
#include <string_view>
#include <bit>
#include <cstdint>
#include <type_traits>
#include <algorithm>
#include <stdexcept>

using namespace Logging::Color;

// ── Forward Declarations ───────────────────────────────────────────────────────
struct Context;

// ── Global Access Macro (Safe) ───────────────────────────────────────────────
#define SWAPCHAIN SwapchainManager::get()

// ── SwapchainManager (Singleton) ─────────────────────────────────────────────
class SwapchainManager {
public:
    // ── Singleton Access ─────────────────────────────────────────────────────
    static SwapchainManager& get() noexcept { 
        LOG_INFO_CAT("SWAPCHAIN", "{}SwapchainManager::get() — singleton access{}", OCEAN_TEAL, RESET);
        static SwapchainManager inst; 
        return inst; 
    }

    // ── Initialization ───────────────────────────────────────────────────────
    void init(VkInstance inst, VkPhysicalDevice phys, VkDevice dev,
              VkSurfaceKHR surf, uint32_t w, uint32_t h) {
        LOG_INFO_CAT("SWAPCHAIN", "{}SwapchainManager::init() — START — {}×{} {}", PLASMA_FUCHSIA, w, h, RESET);
        LOG_INFO_CAT("SWAPCHAIN", "  Instance:  0x{:x}", reinterpret_cast<uint64_t>(inst));
        LOG_INFO_CAT("SWAPCHAIN", "  Physical:  0x{:x}", reinterpret_cast<uint64_t>(phys));
        LOG_INFO_CAT("SWAPCHAIN", "  Device:    0x{:x}", reinterpret_cast<uint64_t>(dev));
        LOG_INFO_CAT("SWAPCHAIN", "  Surface:   0x{:x}", reinterpret_cast<uint64_t>(surf));

        physDev_ = phys; 
        device_ = dev; 
        surface_ = surf;

        createSwapchain(w, h); 
        createImageViews();

        LOG_SUCCESS_CAT("SWAPCHAIN", "{}SwapchainManager::init() — COMPLETE — {} images, {} views{}", 
                        PLASMA_FUCHSIA, images_.size(), imageViews_.size(), RESET);
    }

    // ── Recreate (NO WINDOW*) ────────────────────────────────────────────────
    void recreate(uint32_t w, uint32_t h) { 
        LOG_INFO_CAT("SWAPCHAIN", "{}SwapchainManager::recreate() — {}×{} {}", RASPBERRY_PINK, w, h, RESET);
        vkDeviceWaitIdle(device_); 
        LOG_INFO_CAT("SWAPCHAIN", "  Device idle — safe to destroy");
        cleanup(); 
        createSwapchain(w, h); 
        createImageViews(); 
        LOG_SUCCESS_CAT("SWAPCHAIN", "Recreate complete — {} images", images_.size());
    }

    // ── Cleanup ──────────────────────────────────────────────────────────────
    void cleanup() noexcept {
        LOG_INFO_CAT("SWAPCHAIN", "{}SwapchainManager::cleanup() — START{}", RASPBERRY_PINK, RESET);
        for (auto& v : imageViews_) { 
            if (v) LOG_INFO_CAT("SWAPCHAIN", "  Destroying image view 0x{:x}", reinterpret_cast<uint64_t>(*v));
            v.reset(); 
        }
        imageViews_.clear(); 
        images_.clear(); 
        if (swapchain_) {
            LOG_INFO_CAT("SWAPCHAIN", "  Destroying swapchain 0x{:x}", reinterpret_cast<uint64_t>(*swapchain_));
            swapchain_.reset();
        }
        LOG_SUCCESS_CAT("SWAPCHAIN", "Cleanup complete — all handles released");
    }

    // ── Accessors ────────────────────────────────────────────────────────────
    [[nodiscard]] VkSwapchainKHR swapchain() const noexcept { 
        LOG_INFO_CAT("SWAPCHAIN", "swapchain() → 0x{:x}", reinterpret_cast<uint64_t>(*swapchain_));
        return *swapchain_; 
    }
    [[nodiscard]] VkFormat format() const noexcept { 
        LOG_INFO_CAT("SWAPCHAIN", "format() → {}", static_cast<int>(format_));
        return format_; 
    }
    [[nodiscard]] VkExtent2D extent() const noexcept { 
        LOG_INFO_CAT("SWAPCHAIN", "extent() → {}×{}", extent_.width, extent_.height);
        return extent_; 
    }
    [[nodiscard]] const std::vector<VkImage>& images() const noexcept { 
        LOG_INFO_CAT("SWAPCHAIN", "images() → {} images", images_.size());
        return images_; 
    }
    [[nodiscard]] const std::vector<RTX::Handle<VkImageView>>& imageViews() const noexcept { 
        LOG_INFO_CAT("SWAPCHAIN", "imageViews() → {} views", imageViews_.size());
        return imageViews_; 
    }

private:
    SwapchainManager() {
        LOG_INFO_CAT("SWAPCHAIN", "{}SwapchainManager constructed — singleton ready{}", OCEAN_TEAL, RESET);
    }

    void createSwapchain(uint32_t width, uint32_t height);
    void createImageViews();

    VkPhysicalDevice physDev_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkFormat format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::vector<RTX::Handle<VkImageView>> imageViews_;
    RTX::Handle<VkSwapchainKHR> swapchain_;
};

/* ── INLINE IMPLEMENTATION — FULL LOGGING — FIXED ─────────────────────────── */
inline void SwapchainManager::createSwapchain(uint32_t width, uint32_t height) {
    LOG_INFO_CAT("SWAPCHAIN", "{}createSwapchain() — START — requested {}×{} {}", PLASMA_FUCHSIA, width, height, RESET);

    LOG_INFO_CAT("SWAPCHAIN", "Querying surface capabilities...");
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev_, surface_, &caps);
    LOG_INFO_CAT("SWAPCHAIN", "  minImageCount: {} | maxImageCount: {} | currentExtent: {}×{}", 
                 caps.minImageCount, caps.maxImageCount > 0 ? caps.maxImageCount : -1, 
                 caps.currentExtent.width, caps.currentExtent.height);
    LOG_INFO_CAT("SWAPCHAIN", "  minExtent: {}×{} | maxExtent: {}×{}", 
                 caps.minImageExtent.width, caps.minImageExtent.height,
                 caps.maxImageExtent.width, caps.maxImageExtent.height);

    uint32_t w = std::clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    uint32_t h = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    LOG_INFO_CAT("SWAPCHAIN", "  Clamped extent: {}×{}", w, h);

    LOG_INFO_CAT("SWAPCHAIN", "Querying surface formats...");
    uint32_t fmtCnt = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &fmtCnt, nullptr);
    LOG_INFO_CAT("SWAPCHAIN", "  {} formats available", fmtCnt);
    std::vector<VkSurfaceFormatKHR> fmts(fmtCnt);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &fmtCnt, fmts.data());

    VkSurfaceFormatKHR chosen = fmts[0];
    LOG_INFO_CAT("SWAPCHAIN", "  Default format: {} | {}", static_cast<int>(fmts[0].format), static_cast<int>(fmts[0].colorSpace));
    for (size_t i = 0; i < fmts.size(); ++i) {
        const auto& f = fmts[i];
        LOG_INFO_CAT("SWAPCHAIN", "    [{}] {} | {}", i, static_cast<int>(f.format), static_cast<int>(f.colorSpace));
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = f;
            LOG_INFO_CAT("SWAPCHAIN", "    SELECTED: B8G8R8A8_SRGB + SRGB_NONLINEAR");
            break;
        }
    }
    LOG_SUCCESS_CAT("SWAPCHAIN", "Chosen format: {} | {}", static_cast<int>(chosen.format), static_cast<int>(chosen.colorSpace));

    LOG_INFO_CAT("SWAPCHAIN", "Querying present modes...");
    uint32_t pmCnt = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCnt, nullptr);
    LOG_INFO_CAT("SWAPCHAIN", "  {} present modes available", pmCnt);
    std::vector<VkPresentModeKHR> pms(pmCnt);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCnt, pms.data());

    VkPresentModeKHR present = VK_PRESENT_MODE_FIFO_KHR;
    LOG_INFO_CAT("SWAPCHAIN", "  Default: FIFO");
    for (size_t i = 0; i < pms.size(); ++i) {
        auto m = pms[i];
        LOG_INFO_CAT("SWAPCHAIN", "    [{}] {}", i, static_cast<int>(m));
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            present = m;
            LOG_INFO_CAT("SWAPCHAIN", "    SELECTED: MAILBOX");
            break;
        }
    }
    LOG_SUCCESS_CAT("SWAPCHAIN", "Chosen present mode: {}", static_cast<int>(present));

    uint32_t imgCnt = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imgCnt > caps.maxImageCount)
        imgCnt = caps.maxImageCount;
    LOG_INFO_CAT("SWAPCHAIN", "Image count: {} (min: {}, max: {})", imgCnt, caps.minImageCount, caps.maxImageCount > 0 ? caps.maxImageCount : -1);

    LOG_INFO_CAT("SWAPCHAIN", "Creating VkSwapchainKHR...");
    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = surface_;
    ci.minImageCount = imgCnt;
    ci.imageFormat = chosen.format;
    ci.imageColorSpace = chosen.colorSpace;
    ci.imageExtent = {w, h};
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = present;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR raw = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(device_, &ci, nullptr, &raw) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swapchain");

    LOG_SUCCESS_CAT("SWAPCHAIN", "VkSwapchainKHR created: 0x{:x}", reinterpret_cast<uint64_t>(raw));
    swapchain_ = RTX::MakeHandle(raw, device_, vkDestroySwapchainKHR);
    format_ = chosen.format;
    extent_ = {w, h};

    LOG_INFO_CAT("SWAPCHAIN", "Fetching swapchain images...");
    uint32_t cnt = 0;
    vkGetSwapchainImagesKHR(device_, *swapchain_, &cnt, nullptr);
    LOG_INFO_CAT("SWAPCHAIN", "  {} images reported", cnt);
    images_.resize(cnt);
    vkGetSwapchainImagesKHR(device_, *swapchain_, &cnt, images_.data());
    for (uint32_t i = 0; i < images_.size(); ++i) {
        LOG_INFO_CAT("SWAPCHAIN", "  [{}] Image: 0x{:x}", i, reinterpret_cast<uint64_t>(images_[i]));
    }

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}createSwapchain() — COMPLETE — {}×{} — {} images{}", 
                    PLASMA_FUCHSIA, w, h, images_.size(), RESET);
}

inline void SwapchainManager::createImageViews() {
    LOG_INFO_CAT("SWAPCHAIN", "{}createImageViews() — START — {} images{}", PLASMA_FUCHSIA, images_.size(), RESET);
    imageViews_.reserve(images_.size());

    for (size_t i = 0; i < images_.size(); ++i) {
        auto img = images_[i];
        LOG_INFO_CAT("SWAPCHAIN", "  [{}] Creating view for image 0x{:x}...", i, reinterpret_cast<uint64_t>(img));

        VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ci.image = img;
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = format_;
        ci.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageView view = VK_NULL_HANDLE;
        if (vkCreateImageView(device_, &ci, nullptr, &view) != VK_SUCCESS)
            throw std::runtime_error("Failed to create image view");

        LOG_SUCCESS_CAT("SWAPCHAIN", "    View created: 0x{:x}", reinterpret_cast<uint64_t>(view));
        imageViews_.emplace_back(RTX::MakeHandle(view, device_, vkDestroyImageView));
    }

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}createImageViews() — COMPLETE — {} views{}", 
                    PLASMA_FUCHSIA, imageViews_.size(), RESET);
}