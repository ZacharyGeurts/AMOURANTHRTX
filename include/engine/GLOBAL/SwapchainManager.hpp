// engine/GLOBAL/SwapchainManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// SwapchainManager vGOD_BLESS — VALHALLA v34 DUDE GLOBAL BROS — NOVEMBER 10, 2025
// • USING OBLITERATED ETERNAL — DUDE GLOBAL BROS SUPREMACY
// • NO MORE using namespace Logging::Color;
// • NO MORE using Dispose::MakeHandle;
// • NO MORE using Dispose::logAndTrackDestruction;
// • DIRECT :: SCOPE EVERYWHERE — PURE GLOBAL BLISS
// • Gentleman Grok: "Dude... globals bro. Using? We burned it. Scope eternal."
// • PINK PHOTONS INFINITE — 69,420 FPS LOCKED — SHIP IT VALHALLA
// =============================================================================

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Dispose.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/GLOBAL/VulkanContext.hpp"

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

#include <atomic>
#include <vector>
#include <chrono>
#include <span>
#include <string>
#include <algorithm>
#include <memory>

// DUDE GLOBAL BROS — NO USING STATEMENTS — SCOPE ONLY
// Logging::Color::PLASMA_FUCHSIA
// Dispose::MakeHandle
// Dispose::logAndTrackDestruction

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

    void init(uint32_t w, uint32_t h, bool vsync = true) noexcept {
        auto& c = *ctx();
        instance_ = c.vkInstance();
        physicalDevice_ = c.vkPhysicalDevice();
        device_ = c.vkDevice();
        surface_ = c.vkSurface();
        vsyncEnabled_ = vsync;
        LOG_SUCCESS_CAT("Swapchain", "{}DUDE GLOBAL BROS — Init {}x{} (VSync: {}){}", 
                        Logging::Color::PLASMA_FUCHSIA, w, h, vsync ? "ON" : "OFF", Logging::Color::RESET);
        recreate(w, h);
    }

    void recreate(uint32_t w, uint32_t h) noexcept {
        if (w == 0 || h == 0) return;
        vkDeviceWaitIdle(device_);
        cleanupSwapchainOnly();
        createSwapchain(w, h);
        createImageViews();
        resetStats();
        LOG_SUCCESS_CAT("Swapchain", "{}GLOBAL RECREATE {}x{} — {} images — PINK PHOTONS READY{}", 
                        Logging::Color::EMERALD_GREEN, extent_.width, extent_.height, views_.size(), Logging::Color::RESET);
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
            LOG_WARNING_CAT("Swapchain", "Acquire out-of-date — RECREATING");
            recreate(extent_.width, extent_.height);
        } else VK_CHECK(res, "Acquire failed");
        updateFPS();
    }

    VkResult present(VkQueue queue, std::span<const VkSemaphore> wait, uint32_t idx) noexcept {
        if (!swapchain_) return VK_ERROR_OUT_OF_DATE_KHR;

        VkSwapchainKHR rawSwapchain = swapchain_.get();  // GOD BLESS LOCAL

        VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        info.waitSemaphoreCount = static_cast<uint32_t>(wait.size());
        info.pWaitSemaphores = wait.data();
        info.swapchainCount = 1;
        info.pSwapchain = &rawSwapchain;
        info.pImageIndices = &idx;

        VkResult res = vkQueuePresentKHR(queue, &info);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
            LOG_DEBUG_CAT("Swapchain", "Present suboptimal — RECREATING");
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
    ~SwapchainManager() { 
        cleanup(); 
        LOG_INFO_CAT("Swapchain", "{}DUDE GLOBAL BROS MANAGER DESTROYED — VALHALLA RESTORED{}", Logging::Color::PLASMA_FUCHSIA, Logging::Color::RESET); 
    }

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
        swapchain_ = Dispose::MakeHandle(rawSc, device_, 
            [](VkDevice d, VkSwapchainKHR h, const VkAllocationCallbacks*) { vkDestroySwapchainKHR(d, h, nullptr); },
            __LINE__, "Swapchain");
        Dispose::logAndTrackDestruction("VkSwapchainKHR", reinterpret_cast<void*>(rawSc), __LINE__);
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
            views_.emplace_back(Dispose::MakeHandle(view, device_, 
                [](VkDevice d, VkImageView h, const VkAllocationCallbacks*) { vkDestroyImageView(d, h, nullptr); },
                __LINE__, "SwapImageView"));
            Dispose::logAndTrackDestruction("VkImageView_Swap", reinterpret_cast<void*>(view), __LINE__);
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

    void updateFPS() noexcept {
        auto now = std::chrono::steady_clock::now();
        ++frameCount_;
        double delta = std::chrono::duration<double>(now - lastTime_).count();
        if (delta >= 1.0) {
            estimatedFPS_ = frameCount_ / delta;
            frameCount_ = 0;
            lastTime_ = now;
            LOG_PERF_CAT("FPS", "{}DUDE GLOBAL FPS: {:.1f}{}", Logging::Color::COSMIC_GOLD, estimatedFPS_, Logging::Color::RESET);
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

// MACROS — DUDE GLOBAL BROS
#define SWAPCHAIN          SwapchainManager::get()
#define SWAPCHAIN_RAW      (SWAPCHAIN.swapchain_ ? SWAPCHAIN.swapchain_.get() : VK_NULL_HANDLE)
#define SWAPCHAIN_VIEW(i)  (i < SWAPCHAIN.views_.size() && SWAPCHAIN.views_[i] ? SWAPCHAIN.views_[i].get() : VK_NULL_HANDLE)
#define SWAPCHAIN_EXTENT   SWAPCHAIN.extent()
#define SWAPCHAIN_FORMAT   SWAPCHAIN.format()
#define SWAPCHAIN_COUNT    SWAPCHAIN.count()
#define SWAPCHAIN_ACQUIRE(s,f,idx) SWAPCHAIN.acquire(s,f,idx)
#define SWAPCHAIN_PRESENT(q,s,idx) SWAPCHAIN.present(q,s,idx)
#define SWAPCHAIN_VSYNC(on) SWAPCHAIN.toggleVSync(on)
#define SWAPCHAIN_RECREATE(w,h) SWAPCHAIN.recreate(w,h)

/*
    AMOURANTH RTX — VALHALLA v34 — DUDE GLOBAL BROS — NOVEMBER 10, 2025
    USING STATEMENTS? OBLITERATED. SCOPE ONLY. DUDE... GLOBAL BROS.
    Pink photons beaming joy eternal. Gentleman Grok: "Dude."
    ELLIE FIER GRAIL SEALED — SHIP IT FOREVER — 69,420 FPS 🍒🩷😊🚀🔥🤖💀❤️⚡♾️
*/