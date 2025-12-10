// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
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

    // NO vkDeviceWaitIdle() — WE ARE FREE — DRIVER REUSES oldSwapchain

    for (auto v : swapchainImageViews_)
        if (v) vkDestroyImageView(stone_device(), v, nullptr);
    swapchainImageViews_.clear();

    VkSwapchainKHR old = swapchain_.get();
    createSwapchain(stone_window(), w, h, old);
    createImageViews();
    las().notifyResize();
}

void SwapchainManager::createImageViews() noexcept
{
    swapchainImageViews_.resize(swapchainImages_.size());

    VkImageViewCreateInfo ci{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = swapchainFormat_,
        .components       = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        ci.image = swapchainImages_[i];
        vkCreateImageView(stone_device(), &ci, nullptr, &swapchainImageViews_[i]);
    }

    stone_seal_views(swapchainImageViews_);
}

const std::vector<VkImageView>& SwapchainManager::getImageViews() const noexcept { return swapchainImageViews_; }
VkImageView SwapchainManager::getImageView(uint32_t index) const noexcept { return index < swapchainImageViews_.size() ? swapchainImageViews_[index] : VK_NULL_HANDLE; }

void SwapchainManager::cleanup() noexcept
{
    for (auto v : swapchainImageViews_)
        if (v) vkDestroyImageView(stone_device(), v, nullptr);
    swapchainImageViews_.clear();

    if (swapchain_.valid()) {
        vkDestroySwapchainKHR(stone_device(), swapchain_.get(), nullptr);
        swapchain_.reset();
    }
    swapchainImages_.clear();
}

bool SwapchainManager::supportsHDR() noexcept { return currentColorSpace_ == VK_COLOR_SPACE_HDR10_ST2084_EXT; }

void SwapchainManager::createSwapchain(SDL_Window*, uint32_t w, uint32_t h, VkSwapchainKHR old) noexcept
{
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(stone_physical(), stone_surface(), &caps);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount) vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, formats.data());

    uint32_t presentCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &presentCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentCount);
    if (presentCount) vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &presentCount, presentModes.data());

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == std::numeric_limits<uint32_t>::max()) {
        extent.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = std::min(caps.minImageCount + 1, caps.maxImageCount > 0 ? caps.maxImageCount : 8u);

    VkPresentModeKHR desired = Options::Display::UNCAPPED_MODE_ACTIVE ? VK_PRESENT_MODE_IMMEDIATE_KHR :
                               Options::Performance::PREFER_MAILBOX_PRESENT ? VK_PRESENT_MODE_MAILBOX_KHR :
                               VK_PRESENT_MODE_FIFO_KHR;

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto mode : { desired, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR }) {
        if (std::find(presentModes.begin(), presentModes.end(), mode) != presentModes.end()) {
            presentMode = mode;
            break;
        }
    }

    VkSurfaceFormatKHR chosen = formats[0];
    if (supportsHDR()) {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
                chosen = f;
                break;
            }
        }
    } else {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen = f;
                break;
            }
        }
    }

    QueueFamilyIndices qf = findQueueFamilies(stone_physical(), stone_surface());
    uint32_t queueFamilyIndices[] = { qf.graphicsFamily.value(), qf.presentFamily.value() };

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
        ci.pQueueFamilyIndices   = queueFamilyIndices;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkSwapchainKHR raw = VK_NULL_HANDLE;
    vkCreateSwapchainKHR(stone_device(), &ci, nullptr, &raw);

    if (old && old != raw) {
        vkDestroySwapchainKHR(stone_device(), old, nullptr);
    }

    swapchain_         = Handle<VkSwapchainKHR>(raw, stone_device());
    swapchainExtent_   = extent;
    swapchainFormat_   = chosen.format;
    currentColorSpace_ = chosen.colorSpace;
    currentPresentMode_ = presentMode;

    uint32_t imgCount = 0;
    vkGetSwapchainImagesKHR(stone_device(), raw, &imgCount, nullptr);
    swapchainImages_.resize(imgCount);
    vkGetSwapchainImagesKHR(stone_device(), raw, &imgCount, swapchainImages_.data());

    stone_seal_swapchain(raw);
    stone_seal_extent(extent);
    stone_seal_image_count(imgCount);
    stone_seal_images(swapchainImages_);
}

VkResult SwapchainManager::acquireNextImage(uint32_t* pImageIndex,
                                            VkSemaphore semaphore,
                                            VkFence fence) noexcept
{
    constexpr uint64_t TIMEOUT = 100'000'000ULL;  // 100ms — NO DEADLOCK DENIED

    VkResult result = vkAcquireNextImageKHR(stone_device(),
                                            stone_swapchain(),
                                            TIMEOUT,
                                            semaphore,
                                            fence,
                                            pImageIndex);

    if (result == VK_TIMEOUT || result == VK_NOT_READY ||
        result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
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

    bool hdr = false;
    if (Options::Display::HDR_AUTO_IGNITION)
    {
        const char* paths[] = {
            "/sys/class/drm/card0-DP-1/edid",
            "/sys/class/drm/card0-HDMI-A-1/edid",
            "/sys/class/drm/card0-eDP-1/edid",
            "/sys/class/drm/card1-DP-1/edid"
        };

        std::vector<char> edid;
        for (const auto* p : paths)
        {
            std::ifstream f(p, std::ios::binary);
            if (!f) continue;
            f.seekg(0, std::ios::end);
            auto sz = f.tellg();
            if (sz < 256) continue;
            edid.resize(static_cast<size_t>(sz));
            f.seekg(0);
            if (!f.read(edid.data(), sz)) continue;

            for (size_t i = 128; i + 128 <= edid.size(); i += 128)
            {
                if (edid[i] == 0x02 && edid[i + 3] >= 0x06)
                {
                    hdr = true;
                    break;
                }
            }
            if (hdr) break;
        }
    }

    currentColorSpace_ = hdr ? VK_COLOR_SPACE_HDR10_ST2084_EXT : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    done = true;
}

} // namespace RTX