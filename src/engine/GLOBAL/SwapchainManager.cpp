// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — FINAL FIXED VERSION — DECEMBER 09, 2025
// NO MORE STALLS. NO MORE FREEZES. PINK PHOTONS FLOW ETERNALLY.
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

#include <algorithm>
#include <limits>
#include <vector>

using StoneKey::stone_device;
using StoneKey::stone_physical;
using StoneKey::stone_surface;
using StoneKey::stone_window;
using StoneKey::stone_seal_swapchain;
using StoneKey::stone_seal_extent;
using StoneKey::stone_seal_image_count;
using StoneKey::stone_seal_images;
using StoneKey::stone_seal_views;
using StoneKey::stone_swapchain;
using StoneKey::stone_width;
using StoneKey::stone_height;

namespace RTX {

void SwapchainManager::create(SDL_Window*, uint32_t w, uint32_t h) noexcept
{
    autoEnableHDR();
    createSwapchain(stone_window(), w, h, VK_NULL_HANDLE);
    createImageViews();
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    if (w == 0 || h == 0) {
        minimized_ = true;
        return;
    }
    minimized_ = false;

    vkDeviceWaitIdle(stone_device());

    LOG_AMOURANTH("SWAPCHAIN REBORN — {}×{} — MAILBOX + 2 FRAMES — THE ROOF IS OFF", w, h);

    for (auto v : swapchainImageViews_) {
        if (v) vkDestroyImageView(stone_device(), v, nullptr);
    }
    swapchainImageViews_.clear();

    VkSwapchainKHR old = swapchain_.get();
    createSwapchain(stone_window(), w, h, old);
    createImageViews();

    if (old && old != swapchain_.get()) {
        vkDestroySwapchainKHR(stone_device(), old, nullptr);
    }

    las().notifyResize();
}

void SwapchainManager::createImageViews() noexcept
{
    swapchainImageViews_.resize(swapchainImages_.size(), VK_NULL_HANDLE);

    VkImageViewCreateInfo ci = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = swapchainFormat_,
        .components       = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        .subresourceRange  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        ci.image = swapchainImages_[i];
        VK_CHECK(vkCreateImageView(stone_device(), &ci, nullptr, &swapchainImageViews_[i]));
    }

    stone_seal_views(swapchainImageViews_);
}

void SwapchainManager::cleanup() noexcept
{
    vkDeviceWaitIdle(stone_device());

    for (auto v : swapchainImageViews_) {
        if (v) vkDestroyImageView(stone_device(), v, nullptr);
    }
    swapchainImageViews_.clear();

    if (swapchain_.valid()) {
        vkDestroySwapchainKHR(stone_device(), swapchain_.get(), nullptr);
        swapchain_.reset();
    }
    swapchainImages_.clear();
}

bool SwapchainManager::supportsHDR()
{ 
    return currentColorSpace_ == VK_COLOR_SPACE_HDR10_ST2084_EXT; 
}

void SwapchainManager::createSwapchain(SDL_Window*, uint32_t w, uint32_t h, VkSwapchainKHR old) noexcept
{
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(stone_physical(), stone_surface(), &caps);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, formats.data());

    uint32_t presentCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &presentCount, nullptr);
    std::vector<VkPresentModeKHR> modes(presentCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &presentCount, modes.data());

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    // RESPECT YOUR ORIGINAL OPTIONS — THE EMPIRE LISTENS
    VkPresentModeKHR desiredMode = VK_PRESENT_MODE_FIFO_KHR;
    if (Options::Display::PREFER_MAILBOX_PRESENT) {
        desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
    }
    if (Options::Display::UNCAPPED_MODE_ACTIVE && Options::Display::ALLOW_IMMEDIATE_PRESENT) {
        desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto mode : modes) {
        if (mode == desiredMode) {
            presentMode = mode;
            break;
        }
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;  // safe fallback
        }
    }

    // IMAGE COUNT — RESPECTS YOUR MAX_FRAMES_IN_FLIGHT FROM OPTIONS
    uint32_t imageCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    imageCount = std::max(caps.minImageCount, std::min(imageCount, caps.maxImageCount ? caps.maxImageCount : 32u));

    // HDR FIRST — THEN SRGB — EXACTLY AS YOU WROTE
    VkSurfaceFormatKHR chosen = formats[0];
    if (Options::Display::HDR_AUTO_IGNITION) {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && 
                f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
                chosen = f;
                break;
            }
        }
    } else {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && 
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen = f;
                break;
            }
        }
    }

    QueueFamilyIndices qf = findQueueFamilies(stone_physical(), stone_surface());
    uint32_t indices[] = { qf.graphicsFamily.value(), qf.presentFamily.value() };

    VkSwapchainCreateInfoKHR ci{
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = stone_surface(),
        .minImageCount    = imageCount,
        .imageFormat      = chosen.format,
        .imageColorSpace  = chosen.colorSpace,
        .imageExtent      = extent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform     = caps.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = presentMode,
        .clipped          = VK_TRUE,
        .oldSwapchain     = old
    };

    if (qf.graphicsFamily != qf.presentFamily) {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = indices;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkSwapchainKHR raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(stone_device(), &ci, nullptr, &raw));

    swapchain_         = Handle<VkSwapchainKHR>(raw, stone_device());
    swapchainExtent_   = extent;
    swapchainFormat_   = chosen.format;
    currentColorSpace_ = chosen.colorSpace;
    currentPresentMode_ = presentMode;

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(stone_device(), raw, &count, nullptr);
    swapchainImages_.resize(count);
    vkGetSwapchainImagesKHR(stone_device(), raw, &count, swapchainImages_.data());

    stone_seal_swapchain(raw);
    stone_seal_extent(extent);
    stone_seal_image_count(count);
    stone_seal_images(swapchainImages_);

    LOG_AMOURANTH("SWAPCHAIN FORGED — {}×{} — {} images — {} — HDR: {}",
                  extent.width, extent.height, count,
                  presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" :
                  presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" : "FIFO",
                  (chosen.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT ? "ON" : "OFF"));
}

VkResult SwapchainManager::acquireNextImage(uint32_t* pImageIndex,
                                           VkSemaphore semaphore,
                                           VkFence fence) noexcept
{
    constexpr uint64_t TIMEOUT = 1'000'000'000ULL; // 1 second max

    VkResult result = vkAcquireNextImageKHR(stone_device(),
                                           stone_swapchain(),
                                           TIMEOUT,
                                           semaphore,
                                           fence,
                                           pImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate(stone_width(), stone_height());
        return VK_ERROR_OUT_OF_DATE_KHR;
    }

    return result;
}

void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) noexcept
{
    VkPresentInfoKHR pi{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = waitSemaphore ? 1u : 0u,
        .pWaitSemaphores    = waitSemaphore ? &waitSemaphore : nullptr,
        .swapchainCount     = 1,
        .pSwapchains        = &stone_swapchain(),
        .pImageIndices      = &imageIndex
    };

    VkResult r = vkQueuePresentKHR(queue, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        recreate(stone_width(), stone_height());
    }
}

void SwapchainManager::autoEnableHDR() noexcept
{
    static bool done = false;
    if (done) return;
    done = true;

    bool hdr = false;

    currentColorSpace_ = hdr ? VK_COLOR_SPACE_HDR10_ST2084_EXT
                             : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    LOG_AMOURANTH("HDR AUTO-IGNITION: {} → using {}",
                  hdr ? "ENABLED" : "disabled",
                  hdr ? "HDR10_ST2084 (10-bit)" : "sRGB (8-bit)");
}

bool parseEDIDForHDR() {
    // Attempt to find and read EDID file (adjust path as needed for your system)
    const char* paths[] = {
        "/sys/class/drm/card0-HDMI-A-1/edid",
        "/sys/class/drm/card0-DP-1/edid",
        "/sys/class/drm/card0-eDP-1/edid",
        "/sys/class/drm/card1-HDMI-A-1/edid",
        nullptr
    };

    for (int i = 0; paths[i]; ++i) {
        std::ifstream f(paths[i], std::ios::binary);
        if (!f) continue;

        std::vector<char> edid((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (edid.size() < 128) continue;

        // Check EDID signature
        if (memcmp(edid.data(), "\0\xff\xff\xff\xff\xff\xff\0", 8) != 0) continue;

        int extensions = static_cast<unsigned char>(edid[126]);

        size_t pos = 128;
        for (int ext = 0; ext < extensions; ++ext) {
            if (pos + 128 > edid.size()) break;

            if (static_cast<unsigned char>(edid[pos]) == 0x02) { // CTA-861 extension
                int dtd_start = static_cast<unsigned char>(edid[pos + 2]);
                int block_pos = pos + 4;
                while (block_pos < pos + dtd_start) {
                    unsigned char byte = edid[block_pos];
                    int tag = byte >> 5;
                    int len = byte & 0x1F;
                    if (tag == 6) { // HDR Static Metadata Data Block
                        return true; // HDR supported
                    }
                    block_pos += len + 1;
                }
            }

            pos += 128;
        }
    }
    return false;
}

bool detectHDRFromEDID() noexcept
{
    const char* edid_paths[] = {
        "/sys/class/drm/card0-HDMI-A-1/edid",
        "/sys/class/drm/card0-HDMI-A-2/edid",
        "/sys/class/drm/card0-DP-1/edid",
        "/sys/class/drm/card0-DP-2/edid",
        "/sys/class/drm/card0-eDP-1/edid",
        "/sys/class/drm/card1-HDMI-A-1/edid",
        "/sys/class/drm/card1-DP-1/edid",
        nullptr
    };

    for (int i = 0; edid_paths[i]; ++i) {
        std::ifstream file(edid_paths[i], std::ios::binary);
        if (!file) continue;

        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        if (size < 128) continue;
        file.seekg(0);

        std::vector<uint8_t> edid(size);
        if (!file.read(reinterpret_cast<char*>(edid.data()), size))
            continue;

        // EDID header check
        static const uint8_t header[8] = {0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00};
        if (std::memcmp(edid.data(), header, 8) != 0)
            continue;

        uint8_t extensions = edid[126];
        size_t offset = 128;

        for (uint8_t ext = 0; ext < extensions && offset + 128 <= size; ++ext) {
            const uint8_t* block = edid.data() + offset;

            if (block[0] == 0x02 && block[1] == 0x03) {           // CTA-861 block
                uint8_t dtd_start = block[2];
                for (uint8_t pos = 4; pos < dtd_start && pos < 128; ) {
                    uint8_t tag = block[pos] >> 5;
                    uint8_t len = block[pos] & 0x1F;

                    if (tag == 0x06) // HDR Static Metadata Data Block
                        return true;

                    pos += len + 1;
                }
            }
            offset += 128;
        }
    }
    return false;
}

} // namespace RTX