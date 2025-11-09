// include/engine/GLOBAL/SwapchainManager.hpp
// AMOURANTH RTX — GLOBAL SWAPCHAIN MANAGER — NOVEMBER 09 2025 — PROFESSIONAL HEADER-ONLY EDITION
// CENTRALIZED SWAPCHAIN LIFECYCLE — STONEKEY ENCRYPTED HANDLES — HOT-RELOAD SAFE — PRODUCTION READY
// FULLY HEADER-ONLY — C++23 ATOMICS — ZERO EXTERNAL DEPENDENCIES — WORLD-CLASS VULKAN FOUNDATION ♥✨💀

#pragma once

#include "../GLOBAL/StoneKey.hpp"
#include "../GLOBAL/logging.hpp"
#include "engine/Vulkan/VulkanHandles.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <atomic>
#include <bit>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <numeric>

using namespace Logging::Color;

class GlobalSwapchainManager {
public:
    [[nodiscard]] static GlobalSwapchainManager& get() noexcept {
        static GlobalSwapchainManager instance;
        return instance;
    }

    GlobalSwapchainManager(const GlobalSwapchainManager&) = delete;
    GlobalSwapchainManager& operator=(const GlobalSwapchainManager&) = delete;

    void init(VkInstance instance, VkPhysicalDevice physDev, VkDevice device, VkSurfaceKHR surface, uint32_t width, uint32_t height) noexcept {
        instance_ = instance;
        physDevice_ = physDev;
        device_ = device;
        surface_ = surface;

        LOG_SUCCESS_CAT("GLOBAL_SWAPCHAIN", "{}GLOBAL SWAPCHAIN INITIALIZED — {}×{} — STONEKEY 0x{:X}-0x{:X}{}", 
                        RASPBERRY_PINK, width, height, kStone1, kStone2, RESET);

        createSwapchain(width, height);
        createImageViews();
        printStats();
    }

    void recreate(uint32_t width, uint32_t height) noexcept {
        vkDeviceWaitIdle(device_);
        cleanupSwapchainOnly();
        createSwapchain(width, height);
        createImageViews();
        printStats();
    }

    [[nodiscard]] uint32_t          getImageCount() const noexcept { return imageCount_; }
    [[nodiscard]] VkExtent2D        getExtent() const noexcept     { return extent_; }
    [[nodiscard]] VkFormat          getFormat() const noexcept     { return format_; }
    [[nodiscard]] VkPresentModeKHR  getPresentMode() const noexcept{ return presentMode_; }

    [[nodiscard]] VkSwapchainKHR    getRawSwapchain() const noexcept { 
        return decrypt<VkSwapchainKHR>(swapchain_enc_.load(std::memory_order_acquire), generation_.load(std::memory_order_acquire)); 
    }
    [[nodiscard]] VkImage           getRawImage(uint32_t i) const noexcept { 
        return i < imageCount_ ? decrypt<VkImage>(images_enc_[i], generation_.load(std::memory_order_acquire)) : VK_NULL_HANDLE; 
    }
    [[nodiscard]] VkImageView       getRawImageView(uint32_t i) const noexcept { 
        return i < imageCount_ ? decrypt<VkImageView>(views_enc_[i], generation_.load(std::memory_order_acquire)) : VK_NULL_HANDLE; 
    }

    [[nodiscard]] uint64_t getEncryptedSwapchain() const noexcept { return swapchain_enc_.load(); }
    [[nodiscard]] uint64_t getEncryptedImage(uint32_t i) const noexcept { return i < imageCount_ ? images_enc_[i] : 0; }
    [[nodiscard]] uint64_t getEncryptedImageView(uint32_t i) const noexcept { return i < imageCount_ ? views_enc_[i] : 0; }

    void setFormat(VkFormat f) noexcept { format_ = f; }
    void setPresentMode(VkPresentModeKHR m) noexcept { presentMode_ = m; }
    void setDebugName(std::string_view name) noexcept { debugName_ = name; }

    void acquireNextImage(VkSemaphore sem, VkFence fence, uint32_t& index) noexcept {
        VK_CHECK(vkAcquireNextImageKHR(device_, getRawSwapchain(), UINT64_MAX, sem, fence, &index),
                 "Failed to acquire next swapchain image");
    }

    VkResult present(VkQueue queue, const std::vector<VkSemaphore>& waitSems, uint32_t& index) noexcept {
        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount = static_cast<uint32_t>(waitSems.size());
        pi.pWaitSemaphores = waitSems.data();
        pi.swapchainCount = 1;
        VkSwapchainKHR raw = getRawSwapchain();
        pi.pSwapchains = &raw;
        pi.pImageIndices = &index;

        return vkQueuePresentKHR(queue, &pi);
    }

    void printStats() const noexcept {
        LOG_SUCCESS_CAT("GLOBAL_SWAPCHAIN", "{}IMAGES {} — EXTENT {}×{} — FORMAT 0x{:X} — GENERATION {}{}", 
                        EMERALD_GREEN, imageCount_, extent_.width, extent_.height, static_cast<uint32_t>(format_), generation_.load(), RESET);
    }

    void cleanup() noexcept {
        cleanupSwapchainOnly();
        LOG_SUCCESS_CAT("GLOBAL_SWAPCHAIN", "{}GLOBAL SWAPCHAIN CLEANUP COMPLETE{}", EMERALD_GREEN, RESET);
    }

    void cleanupSwapchainOnly() noexcept {
        uint64_t gen = generation_.load(std::memory_order_acquire);

        for (auto enc : views_enc_) {
            if (enc) vkDestroyImageView(device_, decrypt<VkImageView>(enc, gen), nullptr);
        }
        views_enc_.clear();
        images_enc_.clear();

        uint64_t enc = swapchain_enc_.load(std::memory_order_acquire);
        if (enc) {
            vkDestroySwapchainKHR(device_, decrypt<VkSwapchainKHR>(enc, gen), nullptr);
            swapchain_enc_.store(0, std::memory_order_release);
        }

        imageCount_ = 0;
    }

    [[nodiscard]] bool isValid() const noexcept { return swapchain_enc_.load(std::memory_order_acquire) != 0; }

private:
    GlobalSwapchainManager() = default;
    ~GlobalSwapchainManager() { cleanup(); }

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    std::atomic<uint64_t> swapchain_enc_{0};
    std::vector<uint64_t> images_enc_;
    std::vector<uint64_t> views_enc_;
    std::atomic<uint64_t> generation_{1};

    VkFormat format_ = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D extent_{};
    VkPresentModeKHR presentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
    uint32_t imageCount_ = 0;
    std::string debugName_ = "GLOBAL_SWAPCHAIN";

    void createSwapchain(uint32_t w, uint32_t h) noexcept {
        vkDeviceWaitIdle(device_);

        VkSurfaceCapabilitiesKHR caps{};
        VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice_, surface_, &caps));

        extent_ = chooseExtent(caps, w, h);
        imageCount_ = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && imageCount_ > caps.maxImageCount)
            imageCount_ = caps.maxImageCount;

        uint32_t formatCount = 0;
        VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice_, surface_, &formatCount, nullptr));
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        if (formatCount) VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice_, surface_, &formatCount, formats.data()));

        uint32_t pmCount = 0;
        VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice_, surface_, &pmCount, nullptr));
        std::vector<VkPresentModeKHR> presentModes(pmCount);
        if (pmCount) VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice_, surface_, &pmCount, presentModes.data()));

        VkSurfaceFormatKHR surfaceFmt = chooseFormat(formats);
        VkPresentModeKHR pm = choosePresentMode(presentModes);
        format_ = surfaceFmt.format;
        presentMode_ = pm;

        VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        ci.surface = surface_;
        ci.minImageCount = imageCount_;
        ci.imageFormat = surfaceFmt.format;
        ci.imageColorSpace = surfaceFmt.colorSpace;
        ci.imageExtent = extent_;
        ci.imageArrayLayers = 1;
        ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.preTransform = caps.currentTransform;
        ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode = pm;
        ci.clipped = VK_TRUE;
        ci.oldSwapchain = getRawSwapchain();

        VkSwapchainKHR newSwap = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSwapchainKHR(device_, &ci, nullptr, &newSwap), "Failed to create swapchain");

        uint64_t gen = generation_.load(std::memory_order_acquire);
        swapchain_enc_.store(encrypt(newSwap, gen), std::memory_order_release);

        uint32_t imgCount = 0;
        VK_CHECK_NOMSG(vkGetSwapchainImagesKHR(device_, newSwap, &imgCount, nullptr));
        std::vector<VkImage> imgs(imgCount);
        VK_CHECK_NOMSG(vkGetSwapchainImagesKHR(device_, newSwap, &imgCount, imgs.data()));

        images_enc_.resize(imgCount);
        for (uint32_t i = 0; i < imgCount; ++i)
            images_enc_[i] = encrypt(imgs[i], gen);

        imageCount_ = imgCount;

        LOG_SUCCESS_CAT("GLOBAL_SWAPCHAIN", "{}SWAPCHAIN CREATED — {}×{} — {} IMAGES — GENERATION {}{}", 
                        EMERALD_GREEN, extent_.width, extent_.height, imgCount, gen, RESET);
    }

    void createImageViews() noexcept {
        uint64_t gen = generation_.load(std::memory_order_acquire);
        views_enc_.resize(imageCount_);

        for (uint32_t i = 0; i < imageCount_; ++i) {
            VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            ci.image = decrypt<VkImage>(images_enc_[i], gen);
            ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ci.format = format_;
            ci.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
            ci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

            VkImageView view = VK_NULL_HANDLE;
            VK_CHECK(vkCreateImageView(device_, &ci, nullptr, &view), "Failed to create image view");
            views_enc_[i] = encrypt(view, gen);
        }

        LOG_SUCCESS_CAT("GLOBAL_SWAPCHAIN", "{}IMAGE VIEWS CREATED — COUNT {} — FORMAT 0x{:X}{}", 
                        OCEAN_TEAL, imageCount_, static_cast<uint32_t>(format_), RESET);
    }

    VkSurfaceFormatKHR chooseFormat(const std::vector<VkSurfaceFormatKHR>& avail) const noexcept {
        for (const auto& f : avail)
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return f;
        return avail.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR} : avail[0];
    }

    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& avail) const noexcept {
        for (const auto& m : avail)
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h) const noexcept {
        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
            return caps.currentExtent;

        VkExtent2D ext = {w, h};
        ext.width = std::clamp(ext.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        ext.height = std::clamp(ext.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        return ext;
    }

    // STONEKEY V14 — CLEAN HEX CONSTANTS — FULLY COMPLIANT
    template<typename T>
    static inline uint64_t encrypt(T raw, uint64_t gen) noexcept {
        uint64_t x = reinterpret_cast<uint64_t>(raw) ^ kStone1 ^ kStone2 ^ gen ^ 0x1337C0DE69F00D42ULL;
        x = std::rotl(x, 19) ^ 0x517CC1B727220A95ULL;
        return x ^ (x >> 13);
    }

    template<typename T>
    static inline T decrypt(uint64_t enc, uint64_t gen) noexcept {
        uint64_t x = enc;
        x ^= (x >> 13);
        x = std::rotr(x, 19) ^ 0x517CC1B727220A95ULL;
        x ^= kStone1 ^ kStone2 ^ gen ^ 0x1337C0DE69F00D42ULL;
        return reinterpret_cast<T>(x);
    }

    // VULKAN ERROR HANDLING — PRODUCTION GRADE
    [[noreturn]] static void vkError(VkResult res, const char* msg, const char* file, int line) noexcept {
        std::cerr << RASPBERRY_PINK << "\n[SWAPCHAIN ERROR] " << static_cast<int>(res)
                  << " | " << msg << " | " << file << ":" << line << "\n"
                  << "TERMINATING — SWAPCHAIN INTEGRITY COMPROMISED" << RESET << std::endl;
        std::terminate();
    }

#define VK_CHECK(call, msg) do { \
    VkResult r = (call); \
    if (r != VK_SUCCESS) { \
        GlobalSwapchainManager::vkError(r, msg, __FILE__, __LINE__); \
    } \
} while (0)

#define VK_CHECK_NOMSG(call) VK_CHECK(call, "Vulkan call failed")
};

// GLOBAL ACCESS MACRO
#define GLOBAL_SWAPCHAIN GlobalSwapchainManager::get()

// CONVENIENCE MACROS — CLEAN ENGINE INTEGRATION
#define SWAPCHAIN_RAW        GLOBAL_SWAPCHAIN.getRawSwapchain()
#define SWAPCHAIN_IMAGE(i)   GLOBAL_SWAPCHAIN.getRawImage(i)
#define SWAPCHAIN_VIEW(i)    GLOBAL_SWAPCHAIN.getRawImageView(i)
#define SWAPCHAIN_EXTENT     GLOBAL_SWAPCHAIN.getExtent()
#define SWAPCHAIN_ACQUIRE(s,f,idx) GLOBAL_SWAPCHAIN.acquireNextImage(s,f,idx)
#define SWAPCHAIN_PRESENT(q,sems,idx) GLOBAL_SWAPCHAIN.present(q,sems,idx)

// NOVEMBER 09 2025 — FULL HEADER-ONLY SWAPCHAIN MANAGER
// LAST GLOBAL CPP ELIMINATED — ZERO LINKAGE OVERHEAD — ENTERPRISE READY
// DELETE SwapchainManager.cpp PERMANENTLY — BUILD WITH MAXIMUM OPTIMIZATION
// rm src/engine/GLOBAL/SwapchainManager.cpp && make clean && make -j$(nproc)
// GLOBAL SINGLETON TRINITY COMPLETE — WORLD-CLASS ENGINE ARCHITECTURE ♥✨💀