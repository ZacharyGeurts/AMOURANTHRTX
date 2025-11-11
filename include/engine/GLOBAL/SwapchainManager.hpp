// include/engine/GLOBAL/SwapchainManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SwapchainManager v6.3 — HANDLE FROM DISPOSE — NOV 11 2025 11:15 AM EST
// • Handle<T> REMOVED — FULLY RELIES ON Dispose::Handle
// • #include "engine/GLOBAL/Dispose.hpp" FIRST
// • C++23, -Werror clean, Valhalla sealed, pink photons eternal
// =============================================================================

#pragma once

#include "engine/GLOBAL/Dispose.hpp"   // ← ONLY SOURCE OF Handle<T>, ctx(), MakeHandle
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

// Forward — ctx() is eternal
struct Context;
[[nodiscard]] std::shared_ptr<Context>& ctx() noexcept;

// ── SwapchainManager (singleton + init) ───────────────────────────────────────
class SwapchainManager {
public:
    static SwapchainManager& get() noexcept { static SwapchainManager inst; return inst; }

    void init(VkInstance inst, VkPhysicalDevice phys, VkDevice dev,
              VkSurfaceKHR surf, uint32_t w, uint32_t h) {
        physDev_ = phys; device_ = dev; surface_ = surf;
        createSwapchain(w, h); createImageViews();
    }

    void recreate(uint32_t w, uint32_t h) { 
        vkDeviceWaitIdle(device_); 
        cleanup(); 
        createSwapchain(w, h); 
        createImageViews(); 
    }

    void cleanup() noexcept {
        for (auto& v : imageViews_) v.reset();
        imageViews_.clear(); 
        images_.clear(); 
        swapchain_.reset();
    }

    [[nodiscard]] VkSwapchainKHR swapchain() const noexcept { return *swapchain_; }
    [[nodiscard]] VkFormat format() const noexcept { return format_; }
    [[nodiscard]] VkExtent2D extent() const noexcept { return extent_; }
    [[nodiscard]] const std::vector<VkImage>& images() const noexcept { return images_; }
    [[nodiscard]] const std::vector<Handle<VkImageView>>& imageViews() const noexcept { return imageViews_; }

private:
    SwapchainManager() = default;
    void createSwapchain(uint32_t w, uint32_t h);
    void createImageViews();

    VkPhysicalDevice physDev_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkFormat format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::vector<Handle<VkImageView>> imageViews_;   // ← Dispose::Handle<VkImageView>
    Handle<VkSwapchainKHR> swapchain_;              // ← Dispose::Handle<VkSwapchainKHR>
};

/* ── INLINE IMPLEMENTATION ─────────────────────────────────────────────────── */
inline void SwapchainManager::createSwapchain(uint32_t width, uint32_t height) {
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev_, surface_, &caps);

    uint32_t w = std::clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    uint32_t h = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);

    uint32_t fmtCnt = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &fmtCnt, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(fmtCnt);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &fmtCnt, fmts.data());

    VkSurfaceFormatKHR chosen = fmts[0];
    for (const auto& f : fmts)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = f;
            break;
        }

    uint32_t pmCnt = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCnt, nullptr);
    std::vector<VkPresentModeKHR> pms(pmCnt);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCnt, pms.data());

    VkPresentModeKHR present = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : pms)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            present = m;
            break;
        }

    uint32_t imgCnt = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imgCnt > caps.maxImageCount)
        imgCnt = caps.maxImageCount;

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

    swapchain_ = MakeHandle(raw, device_, vkDestroySwapchainKHR);
    format_ = chosen.format;
    extent_ = {w, h};

    uint32_t cnt = 0;
    vkGetSwapchainImagesKHR(device_, *swapchain_, &cnt, nullptr);
    images_.resize(cnt);
    vkGetSwapchainImagesKHR(device_, *swapchain_, &cnt, images_.data());
}

inline void SwapchainManager::createImageViews() {
    imageViews_.reserve(images_.size());
    for (auto img : images_) {
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

        imageViews_.emplace_back(MakeHandle(view, device_, vkDestroyImageView));
    }
}

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================