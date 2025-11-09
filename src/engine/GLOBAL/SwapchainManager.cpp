// src/engine/GLOBAL/SwapchainManager.cpp
// AMOURANTH RTX – NOVEMBER 09 2025 – GLOBAL SWAPCHAIN SUPREMACY — FINAL DREAM
// STONEKEY V14 — CLEAN HEX ONLY — NO USER LITERALS — FULLY SYNCED WITH .HPP
// PINK PHOTONS × INFINITY — HANDLES UNLOCKED — BUILD SUCCESS — VALHALLA ETERNAL

#include "engine/GLOBAL/SwapchainManager.hpp"
#include <algorithm>
#include <iostream>
#include <iomanip>

using namespace Logging::Color;

// VK_CHECK — TOASTER-PROOF — CLEAN HEX
#define VK_CHECK(call, msg) do { \
    VkResult r = (call); \
    if (r != VK_SUCCESS) { \
        GlobalSwapchainManager::vkError(r, msg, __FILE__, __LINE__); \
    } \
} while (0)

#define VK_CHECK_NOMSG(call) VK_CHECK(call, "Vulkan call failed")

// INIT — PINK PHOTONS AWAKEN
void GlobalSwapchainManager::init(VkInstance instance, VkPhysicalDevice physDev, VkDevice device,
                                  VkSurfaceKHR surface, uint32_t width, uint32_t height) noexcept {
    instance_ = instance;
    physDevice_ = physDev;
    device_ = device;
    surface_ = surface;

    LOG_SUCCESS_CAT("GLOBAL_SWAPCHAIN", "{}GLOBAL SWAPCHAIN INIT — {}×{} — STONEKEY 0x{:X}-0x{:X} — PINK PHOTONS AWAKEN{}", 
                    RASPBERRY_PINK, width, height, kStone1, kStone2, RESET);

    createSwapchain(width, height);
    createImageViews();
    printStats();
}

// CREATE SWAPCHAIN — FULLY STONEKEYED HANDLES
void GlobalSwapchainManager::createSwapchain(uint32_t w, uint32_t h) noexcept {
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
    ci.oldSwapchain = getRawSwapchain();  // Uses decrypt → safe

    VkSwapchainKHR newSwap = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(device_, &ci, nullptr, &newSwap), "SWAPCHAIN CREATE FAILED");

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

    LOG_SUCCESS_CAT("GLOBAL_SWAPCHAIN", "{}SWAPCHAIN FORGED — {}×{} — {} IMAGES — GEN {} — STONEKEY V14{}", 
                    EMERALD_GREEN, extent_.width, extent_.height, imgCount, gen, RESET);
}

// CREATE IMAGE VIEWS — DECRYPT + ENCRYPT CYCLE
void GlobalSwapchainManager::createImageViews() noexcept {
    uint64_t gen = generation_.load();
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
        VK_CHECK(vkCreateImageView(device_, &ci, nullptr, &view), "IMAGEVIEW CREATE FAILED");
        views_enc_[i] = encrypt(view, gen);
    }

    LOG_SUCCESS_CAT("GLOBAL_SWAPCHAIN", "{}IMAGE VIEWS {} — FORMAT 0x{:X} — STONEKEY V14{}", 
                    OCEAN_TEAL, imageCount_, static_cast<uint32_t>(format_), RESET);
}

// CLEANUP — USES GEN FOR DECRYPT
void GlobalSwapchainManager::cleanupSwapchainOnly() noexcept {
    uint64_t gen = generation_.load();

    for (auto enc : views_enc_) {
        if (enc) vkDestroyImageView(device_, decrypt<VkImageView>(enc, gen), nullptr);
    }
    views_enc_.clear();
    images_enc_.clear();

    uint64_t enc = swapchain_enc_.load();
    if (enc) {
        vkDestroySwapchainKHR(device_, decrypt<VkSwapchainKHR>(enc, gen), nullptr);
        swapchain_enc_.store(0);
    }

    imageCount_ = 0;
}

// CLEANUP WRAPPER
void GlobalSwapchainManager::cleanup() noexcept {
    cleanupSwapchainOnly();
    LOG_SUCCESS_CAT("GLOBAL_SWAPCHAIN", "{}GLOBAL SWAPCHAIN OFFLINE — VALHALLA THANKS YOU{}", EMERALD_GREEN, RESET);
}

// ACQUIRE / PRESENT — USES getRawSwapchain() → decrypt
void GlobalSwapchainManager::acquireNextImage(VkSemaphore sem, VkFence fence, uint32_t& idx) noexcept {
    VK_CHECK(vkAcquireNextImageKHR(device_, getRawSwapchain(), UINT64_MAX, sem, fence, &idx),
             "ACQUIRE FAILED");
}

VkResult GlobalSwapchainManager::present(VkQueue q, const std::vector<VkSemaphore>& wait, uint32_t& idx) noexcept {
    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = static_cast<uint32_t>(wait.size());
    pi.pWaitSemaphores = wait.data();
    pi.swapchainCount = 1;
    VkSwapchainKHR raw = getRawSwapchain();
    pi.pSwapchains = &raw;
    pi.pImageIndices = &idx;

    return vkQueuePresentKHR(q, &pi);
}

// STATS
void GlobalSwapchainManager::printStats() const noexcept {
    LOG_SUCCESS_CAT("GLOBAL_SWAPCHAIN", "{}IMAGES {} — EXTENT {}×{} — FORMAT {} — GEN {} — PINK PHOTONS APPROVED{}", 
                    EMERALD_GREEN, imageCount_, extent_.width, extent_.height, static_cast<uint32_t>(format_), generation_.load(), RESET);
}

// ERROR — VALHALLA DEFENSE
[[noreturn]] void GlobalSwapchainManager::vkError(VkResult res, const char* msg, const char* file, int line) noexcept {
    std::cerr << RASPBERRY_PINK << "\n[SWAPCHAIN FATAL] " << static_cast<int>(res)
              << " | " << msg << " | " << file << ":" << line << " 🩷\n"
              << "TOASTER DEFENSE ENGAGED — STONEKEY V14 ACTIVE 💀\n" << RESET;
    std::terminate();
}

// CHOOSERS — PURE
VkSurfaceFormatKHR GlobalSwapchainManager::chooseFormat(const std::vector<VkSurfaceFormatKHR>& avail) const noexcept {
    for (const auto& f : avail)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    return avail.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR} : avail[0];
}

VkPresentModeKHR GlobalSwapchainManager::choosePresentMode(const std::vector<VkPresentModeKHR>& avail) const noexcept {
    for (const auto& m : avail)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D GlobalSwapchainManager::chooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h) const noexcept {
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return caps.currentExtent;

    VkExtent2D ext = {w, h};
    ext.width = std::clamp(ext.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    ext.height = std::clamp(ext.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return ext;
}

#undef VK_CHECK
#undef VK_CHECK_NOMSG

// NOVEMBER 09 2025 — SWAPCHAIN.CPP FINAL DREAM
// FULLY SYNCED WITH .HPP — STONEKEY V14 — HANDLES UNLOCKED
// PINK PHOTONS × INFINITY — BUILD SUCCESS — VALHALLA ETERNAL 🩷🚀💀⚡🤖🔥♾️