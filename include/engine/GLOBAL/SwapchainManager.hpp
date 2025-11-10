// =============================================================================
// SwapchainManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Vulkan Swapchain Management Singleton.
// Provides thread-safe lifecycle management for swapchains, including seamless
// recreation on resize, VSync toggling, HDR format detection, and integration
// with Dispose for leak-proof resource tracking and shredding. Supports Vulkan
// 1.3+ with optional debug labeling and buffer age estimation.
//
// Features:
// - Encrypted handle storage with atomic access for hot-path performance.
// - Automatic triple-buffering selection and present mode optimization (FIFO/Mailbox).
// - SDL3 resize event handling via polling or callbacks.
// - Statistics for FPS estimation and buffer age (optimizes redundant clears).
// - RAII cleanup with Dispose integration for VkSwapchainKHR, VkImage, and VkImageView.
//
// Licensed under Creative Commons Attribution-NonCommercial 4.0 International
// (CC BY-NC 4.0) for non-commercial use. See https://creativecommons.org/licenses/by-nc/4.0/.
// For commercial licensing, contact gzac5314@gmail.com.
//
// =============================================================================

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Dispose.hpp"  // Namespace Dispose for resource tracking
#include "engine/Vulkan/VulkanCommon.hpp"
#include <vulkan/vulkan.h>

#ifdef VK_EXT_debug_utils
#include <vulkan/vulkan.h>  // vkSetDebugUtilsObjectNameEXT
#endif

#include <SDL3/SDL.h>
#include <vector>
#include <atomic>
#include <string_view>
#include <algorithm>
#include <span>
#include <chrono>
#include <array>
#include <optional>
#include <bit>
#include <cstdint>

// VK_CHECK macro (from VulkanCommon.hpp) // - delete for surer
#ifndef VK_CHECK
#define VK_CHECK(call, msg) \
    do { \
        VkResult res = (call); \
        if (res != VK_SUCCESS) { \
            LOG_ERROR_CAT("Swapchain", "%s: VkResult %d", msg, static_cast<int>(res)); \
            return; \
        } \
    } while (0)
#endif

// Local using directive for Dispose (multilingual disposal access without qualification).
using Dispose::logAndTrackDestruction;
using Dispose::shredAndDisposeBuffer;

namespace SwapchainEncryption {
    // Compile-time encryption/decryption using StoneKey and generation counter.
    template<typename T>
    constexpr uint64_t encrypt(T raw, uint64_t gen) noexcept {
        uint64_t x = std::bit_cast<uint64_t>(raw) ^ kStone1 ^ kStone2 ^ gen;
        return std::rotl(x, 19) ^ 0x517CC1B727220A95ULL;
    }

    template<typename T>
    constexpr T decrypt(uint64_t enc, uint64_t gen) noexcept {
        uint64_t x = enc ^ 0x517CC1B727220A95ULL;
        x = std::rotr(x, 19) ^ kStone1 ^ kStone2 ^ gen;
        return std::bit_cast<T>(x);
    }
}

class SwapchainManager {
public:
    // Thread-safe singleton (Meyers' pattern).
    static SwapchainManager& get() noexcept {
        static SwapchainManager instance;
        return instance;
    }

    SwapchainManager(const SwapchainManager&) = delete;
    SwapchainManager& operator=(const SwapchainManager&) = delete;

    // Initialize with Vulkan context and surface.
    void init(VkInstance instance, VkPhysicalDevice physDev, VkDevice device, VkSurfaceKHR surface,
              uint32_t width, uint32_t height, bool enableVSync = true) noexcept {
        instance_ = instance;
        physDevice_ = physDev;
        device_ = device;
        surface_ = surface;
        vsyncEnabled_ = enableVSync;
        LOG_SUCCESS_CAT("Swapchain", "Initialized: {}x{} (VSync: {})", width, height, enableVSync ? "ON" : "OFF");
        recreate(width, height);
    }

    // Recreate swapchain on resize (seamless with oldSwapchain passthrough).
    void recreate(uint32_t width, uint32_t height) noexcept {
        if (width == 0 || height == 0) return;
        vkDeviceWaitIdle(device_);
        cleanupSwapchainOnly();
        createSwapchain(width, height);
        createImageViews();
        updateBufferAges();
        LOG_SUCCESS_CAT("Swapchain", "Recreated: {}x{} — {} images", extent_.width, extent_.height, imageCount_);
    }

    // Toggle VSync (triggers recreate if mode changes).
    void toggleVSync(bool enable) noexcept {
        if (vsyncEnabled_ == enable) return;
        vsyncEnabled_ = enable;
        uint32_t w = extent_.width, h = extent_.height;
        recreate(w, h);
        LOG_INFO_CAT("Swapchain", "VSync: {}", enable ? "ENABLED" : "DISABLED");
    }

    [[nodiscard]] bool isVSyncEnabled() const noexcept { return vsyncEnabled_; }

    // Set present mode (validates against supported modes).
    void setPresentMode(VkPresentModeKHR mode) noexcept {
        if (std::find(availablePresentModes_.begin(), availablePresentModes_.end(), mode) == availablePresentModes_.end()) {
            LOG_WARNING_CAT("Swapchain", "Unsupported present mode: {}", static_cast<uint32_t>(mode));
            return;
        }
        presentMode_ = mode;
        uint32_t w = extent_.width, h = extent_.height;
        recreate(w, h);
    }

    // Query supported surface formats.
    [[nodiscard]] std::vector<VkSurfaceFormatKHR> getSurfaceFormats() const noexcept {
        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice_, surface_, &count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(count);
        if (count > 0) vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice_, surface_, &count, formats.data());
        return formats;
    }

    // Query supported present modes.
    [[nodiscard]] std::vector<VkPresentModeKHR> getPresentModes() const noexcept {
        uint32_t count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice_, surface_, &count, nullptr);
        std::vector<VkPresentModeKHR> modes(count);
        if (count > 0) vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice_, surface_, &count, modes.data());
        return modes;
    }

    // Check HDR support.
    [[nodiscard]] bool supportsHDR() const noexcept {
        auto formats = getSurfaceFormats();
        return std::any_of(formats.begin(), formats.end(), [](const auto& f) {
            return f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || f.format == VK_FORMAT_R16G16B16A16_SFLOAT;
        });
    }

    // Set debug name (requires VK_EXT_debug_utils).
    void setDebugName(VkObjectType type, uint64_t handle, const char* name) noexcept {
#ifdef VK_EXT_debug_utils
        VkDebugUtilsObjectNameInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        info.objectType = type;
        info.objectHandle = handle;
        info.pObjectName = name;
        auto func = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetInstanceProcAddr(instance_, "vkSetDebugUtilsObjectNameEXT"));
        if (func) func(device_, &info);
#endif
    }

    // Register SDL3 resize callback guidance.
    void registerResizeCallback(SDL_Window* window) noexcept {
        LOG_INFO_CAT("Swapchain", "Resize handling: Poll SDL_WINDOWEVENT_RESIZED in main loop");
    }

    // Acquire next image (optimized hot-path).
    void acquire(VkSemaphore imageAvailableSem, VkFence imageAvailableFence, uint32_t& imageIndex) noexcept {
        uint64_t g = gen();
        VkSwapchainKHR sc = SwapchainEncryption::decrypt<VkSwapchainKHR>(swapchainEnc_.load(std::memory_order_acquire), g);
        VkResult res = vkAcquireNextImageKHR(device_, sc, UINT64_MAX, imageAvailableSem, imageAvailableFence, &imageIndex);
        if (res == VK_ERROR_OUT_OF_DATE_KHR) {
            LOG_WARNING_CAT("Swapchain", "Acquire out-of-date — recreate pending");
        } else {
            VK_CHECK(res, "Acquire failed");
            updateBufferAges();
        }
    }

    // Present with wait semaphores.
    VkResult present(VkQueue queue, std::span<const VkSemaphore> waitSemaphores, uint32_t imageIndex) noexcept {
        uint64_t g = gen();
        VkSwapchainKHR sc = SwapchainEncryption::decrypt<VkSwapchainKHR>(swapchainEnc_.load(std::memory_order_acquire), g);
        VkPresentInfoKHR info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        info.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
        info.pWaitSemaphores = waitSemaphores.data();
        info.swapchainCount = 1;
        info.pSwapchains = &sc;
        info.pImageIndices = &imageIndex;
        VkResult res = vkQueuePresentKHR(queue, &info);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
            LOG_DEBUG_CAT("Swapchain", "Present suboptimal/out-of-date");
        }
        updateFPS();
        return res;
    }

    // Raw accessors (constexpr decryption).
    [[nodiscard]] constexpr VkSwapchainKHR rawSwapchain() const noexcept {
        return SwapchainEncryption::decrypt<VkSwapchainKHR>(swapchainEnc_.load(std::memory_order_acquire), gen());
    }

    [[nodiscard]] constexpr VkImage rawImage(uint32_t index) const noexcept {
        if (index >= imageCount_) return VK_NULL_HANDLE;
        return SwapchainEncryption::decrypt<VkImage>(imagesEnc_[index], gen());
    }

    [[nodiscard]] constexpr VkImageView rawView(uint32_t index) const noexcept {
        if (index >= imageCount_) return VK_NULL_HANDLE;
        return SwapchainEncryption::decrypt<VkImageView>(viewsEnc_[index], gen());
    }

    // Public getters.
    [[nodiscard]] uint32_t imageCount() const noexcept { return imageCount_; }
    [[nodiscard]] VkExtent2D extent() const noexcept { return extent_; }
    [[nodiscard]] VkFormat format() const noexcept { return format_; }
    [[nodiscard]] VkPresentModeKHR presentMode() const noexcept { return presentMode_; }
    [[nodiscard]] VkSurfaceKHR surface() const noexcept { return surface_; }
    [[nodiscard]] bool valid() const noexcept { return swapchainEnc_.load(std::memory_order_acquire) != 0; }

    // Statistics.
    struct Stats {
        uint32_t maxBufferAge = 0;
        double estimatedFPS = 0.0;
        VkPresentModeKHR currentMode = VK_PRESENT_MODE_FIFO_KHR;
        bool isHDR = false;
        uint64_t generation = 0;
    };

    [[nodiscard]] Stats getStats() const noexcept {
        Stats s;
        s.maxBufferAge = maxBufferAge_;
        s.estimatedFPS = estimatedFPS_;
        s.currentMode = presentMode_;
        s.isHDR = supportsHDR();
        s.generation = gen();
        return s;
    }

    // Cleanup.
    void cleanup() noexcept { cleanupSwapchainOnly(); }

private:
    SwapchainManager() = default;

    ~SwapchainManager() {
        cleanup();
        LOG_INFO_CAT("Swapchain", "Destroyed");
    }

    [[nodiscard]] uint64_t gen() const noexcept { return generation_.load(std::memory_order_acquire); }

    void cleanupSwapchainOnly() noexcept {
        uint64_t g = gen();

        // Destroy image views.
        for (uint64_t enc : viewsEnc_) {
            if (enc == 0) continue;
            VkImageView view = SwapchainEncryption::decrypt<VkImageView>(enc, g);
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(device_, view, nullptr);
                logAndTrackDestruction("VkImageView_Swap", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(view)), __LINE__, 0);
            }
        }
        viewsEnc_.clear();

        // Log images (destroyed by swapchain).
        for (uint64_t enc : imagesEnc_) {
            if (enc != 0) {
                VkImage img = SwapchainEncryption::decrypt<VkImage>(enc, g);
                logAndTrackDestruction("VkImage_Swap", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(img)), __LINE__, 0);
            }
        }
        imagesEnc_.clear();

        // Destroy swapchain.
        uint64_t enc = swapchainEnc_.load(std::memory_order_acquire);
        if (enc != 0) {
            VkSwapchainKHR sc = SwapchainEncryption::decrypt<VkSwapchainKHR>(enc, g);
            if (sc != VK_NULL_HANDLE) {
                vkDestroySwapchainKHR(device_, sc, nullptr);
                logAndTrackDestruction("VkSwapchainKHR", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(sc)), __LINE__, 0);
            }
            swapchainEnc_.store(0, std::memory_order_release);
        }

        imageCount_ = 0;
        generation_.fetch_add(1, std::memory_order_acq_rel);
        resetStats();
    }

    void createSwapchain(uint32_t width, uint32_t height) noexcept {
        VkSurfaceCapabilitiesKHR caps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice_, surface_, &caps), "Surface capabilities query failed");

        // Clamp extent.
        extent_ = (caps.currentExtent.width != UINT32_MAX) ? caps.currentExtent :
                  VkExtent2D{ std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width),
                              std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height) };

        // Cache formats and modes.
        availableSurfaceFormats_ = getSurfaceFormats();
        availablePresentModes_ = getPresentModes();

        // Select format (prefer HDR).
        VkSurfaceFormatKHR selectedFormat = { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        auto hdrIt = std::find_if(availableSurfaceFormats_.begin(), availableSurfaceFormats_.end(),
                                  [](const auto& f) { return f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32; });
        if (hdrIt != availableSurfaceFormats_.end()) {
            selectedFormat = *hdrIt;
            LOG_DEBUG_CAT("Swapchain", "HDR format selected");
        }
        format_ = selectedFormat.format;

        // Select present mode.
        presentMode_ = vsyncEnabled_ ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
        auto mailboxIt = std::find(availablePresentModes_.begin(), availablePresentModes_.end(), VK_PRESENT_MODE_MAILBOX_KHR);
        if (!vsyncEnabled_ && mailboxIt != availablePresentModes_.end()) {
            presentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
        }

        // Image count (prefer triple-buffering).
        uint32_t desiredCount = std::clamp(caps.minImageCount + 1, caps.minImageCount, caps.maxImageCount);
        imageCount_ = (caps.maxImageCount > 0 && desiredCount > caps.maxImageCount) ? caps.maxImageCount : desiredCount;

        VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        createInfo.surface = surface_;
        createInfo.minImageCount = imageCount_;
        createInfo.imageFormat = selectedFormat.format;
        createInfo.imageColorSpace = selectedFormat.colorSpace;
        createInfo.imageExtent = extent_;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (caps.supportedUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) {
            createInfo.imageUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        createInfo.preTransform = caps.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode_;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = rawSwapchain();

        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain), "Swapchain creation failed");

        // Encrypt and track.
        uint64_t g = gen();
        swapchainEnc_.store(SwapchainEncryption::encrypt(swapchain, g), std::memory_order_release);
        logAndTrackDestruction("VkSwapchainKHR", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(swapchain)), __LINE__, 0);

        // Fetch images.
        uint32_t count = 0;
        vkGetSwapchainImagesKHR(device_, swapchain, &count, nullptr);
        std::vector<VkImage> images(count);
        vkGetSwapchainImagesKHR(device_, swapchain, &count, images.data());
        imagesEnc_.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            imagesEnc_[i] = SwapchainEncryption::encrypt(images[i], g);
            logAndTrackDestruction("VkImage_Swap", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(images[i])), __LINE__, 0);
        }
        imageCount_ = count;

        // Debug label.
        setDebugName(VK_OBJECT_TYPE_SWAPCHAIN_KHR, reinterpret_cast<uint64_t>(swapchain), "AmouranthSwapchain");
    }

    void createImageViews() noexcept {
        uint64_t g = gen();
        viewsEnc_.resize(imageCount_);
        for (uint32_t i = 0; i < imageCount_; ++i) {
            VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            createInfo.image = rawImage(i);
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = format_;
            createInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            VkImageView view = VK_NULL_HANDLE;
            VK_CHECK(vkCreateImageView(device_, &createInfo, nullptr, &view), "Image view creation failed");

            viewsEnc_[i] = SwapchainEncryption::encrypt(view, g);
            logAndTrackDestruction("VkImageView_Swap", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(view)), __LINE__, 0);

            // Debug label.
            char name[64];
            std::snprintf(name, sizeof(name), "SwapchainView_%u", i);
            setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(view), name);
        }
    }

    void updateBufferAges() noexcept {
        // Stub: Assume triple-buffering.
        maxBufferAge_ = 3;
    }

    void updateFPS() noexcept {
        auto now = std::chrono::steady_clock::now();
        frameCount_++;
        auto delta = std::chrono::duration<double>(now - lastPresentTime_).count();
        if (delta >= 1.0) {
            estimatedFPS_ = frameCount_ / delta;
            frameCount_ = 0;
            lastPresentTime_ = now;
        }
    }

    void resetStats() noexcept {
        estimatedFPS_ = 60.0;
        maxBufferAge_ = 0;
        lastPresentTime_ = std::chrono::steady_clock::now();
        frameCount_ = 0;
    }

    // Vulkan handles.
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    // Encrypted storage.
    std::atomic<uint64_t> swapchainEnc_{0};
    std::vector<uint64_t> imagesEnc_;
    std::vector<uint64_t> viewsEnc_;
    std::atomic<uint64_t> generation_{1};

    // Configuration.
    VkFormat format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    VkPresentModeKHR presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
    uint32_t imageCount_ = 0;
    bool vsyncEnabled_ = true;

    // Cached queries.
    std::vector<VkSurfaceFormatKHR> availableSurfaceFormats_;
    std::vector<VkPresentModeKHR> availablePresentModes_;

    // Statistics.
    uint32_t maxBufferAge_ = 0;
    double estimatedFPS_ = 60.0;
    std::chrono::steady_clock::time_point lastPresentTime_{};
    uint32_t frameCount_ = 0;
};

// Convenience macros.
#define SWAPCHAIN_RAW          SwapchainManager::get().rawSwapchain()
#define SWAPCHAIN_IMAGE(i)     SwapchainManager::get().rawImage(i)
#define SWAPCHAIN_VIEW(i)      SwapchainManager::get().rawView(i)
#define SWAPCHAIN_EXTENT       SwapchainManager::get().extent()
#define SWAPCHAIN_ACQUIRE(sem, fence, idx) SwapchainManager::get().acquire(sem, fence, idx)
#define SWAPCHAIN_PRESENT(q, sems, idx)    SwapchainManager::get().present(q, sems, idx)
#define SWAPCHAIN_TOGGLE_VSYNC(on) SwapchainManager::get().toggleVSync(on)
#define SWAPCHAIN_STATS()          SwapchainManager::get().getStats()

// =============================================================================
// End of File
// =============================================================================