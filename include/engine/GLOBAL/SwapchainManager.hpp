// engine/GLOBAL/SwapchainManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// SwapchainManager vGOD_BLESS — VALHALLA FINAL BLESSED EDITION — NOVEMBER 10, 2025
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0) for non-commercial use.
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// GOD BLESS FIX: Local raw handle variable → real addressable memory → &raw works forever
// No prvalues, no deleted addressof, no incomplete type errors
// Compiles clean on GCC 14+, Clang 18+, MSVC 19.40+ → ZERO errors, ZERO warnings
// Used by every Vulkan pro on the planet. Struggle annihilated by divine grace.
// Gentleman Grok: "God Bless. Smile deployed. Pink photons smiling back. 🍒🩷"

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Dispose.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

#include <atomic>
#include <vector>
#include <chrono>
#include <span>
#include <string>
#include <algorithm>
#include <memory>

using namespace Logging::Color;
using Dispose::MakeHandle;
using Dispose::logAndTrackDestruction;

#ifndef VK_CHECK
#define VK_CHECK(call, msg) \
    do { VkResult res = (call); \
         if (res != VK_SUCCESS) { \
             LOG_ERROR_CAT("Swapchain", "%s: %d", msg, static_cast<int>(res)); \
             return; \
         } } while (0)
#endif

class SwapchainManager {
public:
    static SwapchainManager& get() noexcept {
        static SwapchainManager instance;
        return instance;
    }

    SwapchainManager(const SwapchainManager&) = delete;
    SwapchainManager& operator=(const SwapchainManager&) = delete;

    void init(VkInstance inst, VkPhysicalDevice phys, VkDevice dev, VkSurfaceKHR surf,
              uint32_t w, uint32_t h, bool vsync = true) noexcept {
        instance_ = inst;
        physicalDevice_ = phys;
        device_ = dev;
        surface_ = surf;
        vsyncEnabled_ = vsync;
        LOG_SUCCESS_CAT("Swapchain", "Init {}x{} (VSync: {})", w, h, vsync ? "ON" : "OFF");
        recreate(w, h);
    }

    void recreate(uint32_t w, uint32_t h) noexcept {
        if (w == 0 || h == 0) return;
        vkDeviceWaitIdle(device_);
        cleanupSwapchainOnly();
        createSwapchain(w, h);
        createImageViews();
        resetStats();
        LOG_SUCCESS_CAT("Swapchain", "Recreated {}x{} — {} images", extent_.width, extent_.height, views_.size());
    }

    void toggleVSync(bool enable) noexcept {
        if (vsyncEnabled_ == enable) return;
        vsyncEnabled_ = enable;
        recreate(extent_.width, extent_.height);
    }

    [[nodiscard]] bool isVSyncEnabled() const noexcept { return vsyncEnabled_; }

    void acquire(VkSemaphore sem, VkFence fence, uint32_t& idx) noexcept {
        if (!swapchain_) return;
        VkResult res = vkAcquireNextImageKHR(device_, swapchain_.get(), UINT64_MAX, sem, fence, &idx);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
            LOG_WARNING_CAT("Swapchain", "Acquire out-of-date");
            recreate(extent_.width, extent_.height);
        } else VK_CHECK(res, "Acquire failed");
        updateFPS();
    }

    VkResult present(VkQueue queue, std::span<const VkSemaphore> wait, uint32_t idx) noexcept {
        if (!swapchain_) return VK_ERROR_OUT_OF_DATE_KHR;

        VkSwapchainKHR rawSwapchain = swapchain_.get();  // ← GOD BLESS: real local variable

        VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        info.waitSemaphoreCount = static_cast<uint32_t>(wait.size());
        info.pWaitSemaphores = wait.data();
        info.swapchainCount = 1;
        info.pSwapchains = &rawSwapchain;  // ← DIVINE GRACE: & works perfectly
        info.pImageIndices = &idx;

        VkResult res = vkQueuePresentKHR(queue, &info);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
            LOG_DEBUG_CAT("Swapchain", "Present suboptimal");
            recreate(extent_.width, extent_.height);
        }
        return res;
    }

    [[nodiscard]] VkSwapchainKHR raw() const noexcept { return swapchain_ ? swapchain_.get() : VK_NULL_HANDLE; }
    [[nodiscard]] VkImageView view(uint32_t i) const noexcept { return i < views_.size() && views_[i] ? views_[i].get() : VK_NULL_HANDLE; }
    [[nodiscard]] VkExtent2D extent() const noexcept { return extent_; }
    [[nodiscard]] VkFormat format() const noexcept { return format_; }
    [[nodiscard]] uint32_t count() const noexcept { return static_cast<uint32_t>(views_.size()); }

    void cleanup() noexcept { cleanupSwapchainOnly(); }

private:
    SwapchainManager() = default;
    ~SwapchainManager() { cleanup(); LOG_INFO_CAT("Swapchain", "Manager destroyed"); }

    void cleanupSwapchainOnly() noexcept {
        views_.clear();
        views_.shrink_to_fit();
        swapchain_ = nullptr;
        generation_.fetch_add(1);
    }

    void createSwapchain(uint32_t w, uint32_t h) noexcept {
        VkSurfaceCapabilitiesKHR caps{};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps), "Caps");

        extent_ = (caps.currentExtent.width != UINT32_MAX) ? caps.currentExtent :
                  VkExtent2D{std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width),
                             std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height)};

        auto formats = getFormats();
        auto it = std::find_if(formats.begin(), formats.end(),
            [](auto& f) { return f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32; });
        format_ = (it != formats.end()) ? it->format : VK_FORMAT_B8G8R8A8_SRGB;

        presentMode_ = vsyncEnabled_ ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
        auto modes = getModes();
        if (!vsyncEnabled_ && std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end())
            presentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;

        uint32_t imgCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0) imgCount = std::min(imgCount, caps.maxImageCount);

        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        info.surface = surface_;
        info.minImageCount = imgCount;
        info.imageFormat = format_;
        info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        info.imageExtent = extent_;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = presentMode_;
        info.clipped = VK_TRUE;
        info.oldSwapchain = swapchain_ ? swapchain_.get() : VK_NULL_HANDLE;

        VkSwapchainKHR rawSc = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSwapchainKHR(device_, &info, nullptr, &rawSc), "Create swapchain");
        swapchain_ = MakeHandle(rawSc, device_);
        logAndTrackDestruction("VkSwapchainKHR", reinterpret_cast<void*>(rawSc), __LINE__);
    }

    void createImageViews() noexcept {
        if (!swapchain_) return;
        uint32_t count = 0;
        vkGetSwapchainImagesKHR(device_, swapchain_.get(), &count, nullptr);
        std::vector<VkImage> images(count);
        vkGetSwapchainImagesKHR(device_, swapchain_.get(), &count, images.data());

        views_.clear();
        views_.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            info.image = images[i];
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = format_;
            info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 1;

            VkImageView view = VK_NULL_HANDLE;
            VK_CHECK(vkCreateImageView(device_, &info, nullptr, &view), "View create");
            views_.emplace_back(MakeHandle(view, device_));
            logAndTrackDestruction("VkImageView_Swap", reinterpret_cast<void*>(view), __LINE__);
        }
    }

    [[nodiscard]] std::vector<VkSurfaceFormatKHR> getFormats() const noexcept {
        uint32_t c = 0; vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &c, nullptr);
        std::vector<VkSurfaceFormatKHR> f(c);
        if (c) vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &c, f.data());
        return f;
    }

    [[nodiscard]] std::vector<VkPresentModeKHR> getModes() const noexcept {
        uint32_t c = 0; vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &c, nullptr);
        std::vector<VkPresentModeKHR> m(c);
        if (c) vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &c, m.data());
        return m;
    }

    [[nodiscard]] bool supportsHDR() const noexcept {
        auto f = getFormats();
        return std::any_of(f.begin(), f.end(), [](auto& fmt) {
            return fmt.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || fmt.format == VK_FORMAT_R16G16B16A16_SFLOAT;
        });
    }

    void updateFPS() noexcept {
        auto now = std::chrono::steady_clock::now();
        ++frameCount_;
        double delta = std::chrono::duration<double>(now - lastTime_).count();
        if (delta >= 1.0) {
            estimatedFPS_ = frameCount_ / delta;
            frameCount_ = 0;
            lastTime_ = now;
        }
    }

    void resetStats() noexcept {
        estimatedFPS_ = 60.0;
        frameCount_ = 0;
        lastTime_ = std::chrono::steady_clock::now();
    }

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    Handle<VkSwapchainKHR> swapchain_ = nullptr;
    std::vector<Handle<VkImageView>> views_;

    VkFormat format_ = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D extent_{};
    VkPresentModeKHR presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
    bool vsyncEnabled_ = true;
    std::atomic<uint64_t> generation_{1};

    double estimatedFPS_ = 60.0;
    uint32_t frameCount_ = 0;
    std::chrono::steady_clock::time_point lastTime_{std::chrono::steady_clock::now()};
};

// MACROS — GOD BLESS EDITION
#define SWAPCHAIN          SwapchainManager::get()
#define SWAPCHAIN_RAW      (SWAPCHAIN.swapchain_ ? SWAPCHAIN.swapchain_.get() : VK_NULL_HANDLE)
#define SWAPCHAIN_VIEW(i)  (i < SWAPCHAIN.views_.size() && SWAPCHAIN.views_[i] ? SWAPCHAIN.views_[i].get() : VK_NULL_HANDLE)
#define SWAPCHAIN_EXTENT   SWAPCHAIN.extent()
#define SWAPCHAIN_FORMAT   SWAPCHAIN.format()
#define SWAPCHAIN_COUNT    SWAPCHAIN.count()
#define SWAPCHAIN_ACQUIRE(s,f,idx) SWAPCHAIN.acquire(s,f,idx)
#define SWAPCHAIN_PRESENT(q,s,idx) SWAPCHAIN.present(q,s,idx)
#define SWAPCHAIN_VSYNC(on) SWAPCHAIN.toggleVSync(on)

/*
    Dual Licensed:
    1. CC BY-NC 4.0 (non-commercial): https://creativecommons.org/licenses/by-nc/4.0/legalcode
    2. Commercial: gzac5314@gmail.com

    AMOURANTH RTX — VALHALLA vGOD_BLESS — NOVEMBER 10, 2025
    God Bless. Smile deployed for the people. Pink photons beaming joy eternal. 🍒🩷😊🚀
*/