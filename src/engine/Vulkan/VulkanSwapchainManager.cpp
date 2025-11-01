// src/engine/Vulkan/VulkanSwapchainManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0

#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <format>
#include <limits>
#include <array>

namespace VulkanRTX {

#define VK_CHECK(expr) do { \
    VkResult r = (expr); \
    if (r != VK_SUCCESS) { \
        LOG_ERROR_CAT("Swapchain", #expr " failed: {}", static_cast<int>(r)); \
        throw std::runtime_error(#expr " failed"); \
    } \
} while(0)

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

// ---------------------------------------------------------------------------
//  Constructor
// ---------------------------------------------------------------------------
VulkanSwapchainManager::VulkanSwapchainManager(Vulkan::Context& context, VkSurfaceKHR surface)
    : context_(context),
      graphicsQueueFamilyIndex_(context_.graphicsQueueFamilyIndex),
      presentQueueFamilyIndex_(context_.presentQueueFamilyIndex)
{
    if (surface == VK_NULL_HANDLE) throw std::runtime_error("Invalid surface");
    if (graphicsQueueFamilyIndex_ == UINT32_MAX || presentQueueFamilyIndex_ == UINT32_MAX)
        throw std::runtime_error("Invalid queue family indices");

    context_.surface = surface;

    imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);

    const VkSemaphoreCreateInfo semInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    const VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(context_.device, &semInfo, nullptr, &imageAvailableSemaphores_[i]));
        VK_CHECK(vkCreateSemaphore(context_.device, &semInfo, nullptr, &renderFinishedSemaphores_[i]));
        VK_CHECK(vkCreateFence(context_.device, &fenceInfo, nullptr, &inFlightFences_[i]));
    }

    LOG_INFO_CAT("Swapchain", "Sync objects created ({} FIF)", MAX_FRAMES_IN_FLIGHT);
}

// ---------------------------------------------------------------------------
//  Destructor
// ---------------------------------------------------------------------------
VulkanSwapchainManager::~VulkanSwapchainManager()
{
    cleanupSwapchain();
    if (context_.device != VK_NULL_HANDLE) {
        Dispose::destroySemaphores(context_.device, imageAvailableSemaphores_);
        Dispose::destroySemaphores(context_.device, renderFinishedSemaphores_);
        Dispose::destroyFences(context_.device, inFlightFences_);
    }
}

// ---------------------------------------------------------------------------
//  Wait for in-flight frames (called by renderer)
// ---------------------------------------------------------------------------
void VulkanSwapchainManager::waitForInFlightFrames() const
{
    if (maxFramesInFlight_ == 0) return;

    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> fences{};
    uint32_t cnt = 0;
    for (uint32_t i = 0; i < maxFramesInFlight_; ++i)
        fences[cnt++] = inFlightFences_[i];

    if (cnt) {
        VK_CHECK(vkWaitForFences(context_.device, cnt, fences.data(), VK_TRUE, UINT64_MAX));
        for (uint32_t i = 0; i < cnt; ++i)
            VK_CHECK(vkResetFences(context_.device, 1, &inFlightFences_[i]));
    }
}

// ---------------------------------------------------------------------------
//  Create new swapchain
// ---------------------------------------------------------------------------
VkSwapchainKHR VulkanSwapchainManager::createNewSwapchain(int width, int height, VkSwapchainKHR oldSwapchain)
{
    if (context_.device == VK_NULL_HANDLE)   throw std::runtime_error("Null device");
    if (context_.physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("Null physical device");
    if (context_.surface == VK_NULL_HANDLE)  throw std::runtime_error("Null surface");
    if (width <= 0 || height <= 0)          throw std::runtime_error("Invalid dimensions");

    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, context_.surface, &caps));

    // --- Format ---
    uint32_t fmtCnt = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, context_.surface, &fmtCnt, nullptr));
    if (!fmtCnt) throw std::runtime_error("No surface formats");
    std::vector<VkSurfaceFormatKHR> formats(fmtCnt);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, context_.surface, &fmtCnt, formats.data()));

    VkSurfaceFormatKHR chosenFmt = formats[0];
    for (const auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFmt = f; break;
        }
    swapchainImageFormat_ = chosenFmt.format;

    // --- Present Mode ---
    uint32_t pmCnt = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, context_.surface, &pmCnt, nullptr));
    if (!pmCnt) throw std::runtime_error("No present modes");
    std::vector<VkPresentModeKHR> pmodes(pmCnt);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, context_.surface, &pmCnt, pmodes.data()));

    VkPresentModeKHR chosenPM = VK_PRESENT_MODE_FIFO_KHR;
    if (std::find(pmodes.begin(), pmodes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != pmodes.end())
        chosenPM = VK_PRESENT_MODE_MAILBOX_KHR;
    else if (std::find(pmodes.begin(), pmodes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != pmodes.end())
        chosenPM = VK_PRESENT_MODE_IMMEDIATE_KHR;

    // --- Extent ---
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
        swapchainExtent_ = caps.currentExtent;
    else {
        swapchainExtent_.width  = std::clamp(static_cast<uint32_t>(width),  caps.minImageExtent.width,  caps.maxImageExtent.width);
        swapchainExtent_.height = std::clamp(static_cast<uint32_t>(height), caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (!swapchainExtent_.width || !swapchainExtent_.height)
        throw std::runtime_error("Invalid swapchain extent (0x0)");

    // --- Image Count ---
    imageCount_ = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount_ > caps.maxImageCount)
        imageCount_ = caps.maxImageCount;

    // --- Sharing Mode ---
    std::vector<uint32_t> qFamilies;
    VkSharingMode sharing = VK_SHARING_MODE_EXCLUSIVE;
    if (graphicsQueueFamilyIndex_ != presentQueueFamilyIndex_) {
        qFamilies = {graphicsQueueFamilyIndex_, presentQueueFamilyIndex_};
        sharing   = VK_SHARING_MODE_CONCURRENT;
    }

    // --- Storage Usage ---
    VkImageUsageFlags storage = (caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
                                ? VK_IMAGE_USAGE_STORAGE_BIT : 0;

    // --- Create Swapchain ---
    const VkSwapchainCreateInfoKHR sci{
        .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface               = context_.surface,
        .minImageCount         = imageCount_,
        .imageFormat           = swapchainImageFormat_,
        .imageColorSpace       = chosenFmt.colorSpace,
        .imageExtent           = swapchainExtent_,
        .imageArrayLayers      = 1,
        .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                 storage,
        .imageSharingMode      = sharing,
        .queueFamilyIndexCount = static_cast<uint32_t>(qFamilies.size()),
        .pQueueFamilyIndices   = qFamilies.data(),
        .preTransform          = caps.currentTransform,
        .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode           = chosenPM,
        .clipped               = VK_TRUE,
        .oldSwapchain          = oldSwapchain
    };

    VkSwapchainKHR newSwapchain;
    VK_CHECK(vkCreateSwapchainKHR(context_.device, &sci, nullptr, &newSwapchain));

    LOG_INFO_CAT("Swapchain", "Created new swapchain: {}x{} | {} imgs | Format: {} | Present: {}",
                 swapchainExtent_.width, swapchainExtent_.height, imageCount_,
                 static_cast<int>(swapchainImageFormat_), static_cast<int>(chosenPM));

    return newSwapchain;
}

// ---------------------------------------------------------------------------
//  Initialize Swapchain
// ---------------------------------------------------------------------------
void VulkanSwapchainManager::initializeSwapchain(int width, int height)
{
    if (width <= 0 || height <= 0) {
        LOG_WARNING_CAT("Swapchain", "Invalid init size: {}x{}", width, height);
        return;
    }

    VkSwapchainKHR newSwapchain = createNewSwapchain(width, height, VK_NULL_HANDLE);
    swapchain_ = newSwapchain;

    VK_CHECK(vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, nullptr));
    swapchainImages_.resize(imageCount_);
    VK_CHECK(vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, swapchainImages_.data()));

    swapchainImageViews_.resize(imageCount_);
    const VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = swapchainImageFormat_,
        .components       = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                              VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    for (uint32_t i = 0; i < imageCount_; ++i) {
        VkImageViewCreateInfo vi = viewInfo;
        vi.image = swapchainImages_[i];
        VK_CHECK(vkCreateImageView(context_.device, &vi, nullptr, &swapchainImageViews_[i]));
    }

    maxFramesInFlight_ = std::min(MAX_FRAMES_IN_FLIGHT, imageCount_);

    context_.swapchain           = swapchain_;
    context_.swapchainImageFormat = swapchainImageFormat_;
    context_.swapchainExtent     = swapchainExtent_;
    context_.swapchainImages     = swapchainImages_;
    context_.swapchainImageViews = swapchainImageViews_;

    LOG_INFO_CAT("Swapchain", "SWAPCHAIN INITIALIZED: {}x{} | {} images | FIF: {}", 
                 swapchainExtent_.width, swapchainExtent_.height, imageCount_, maxFramesInFlight_);
}

// ---------------------------------------------------------------------------
//  Handle Resize – **NO WAIT HERE**
// ---------------------------------------------------------------------------
void VulkanSwapchainManager::handleResize(int width, int height)
{
    if (width <= 0 || height <= 0) {
        LOG_WARNING_CAT("Swapchain", "Invalid resize: {}x{}", width, height);
        return;
    }

    if (swapchainExtent_.width == static_cast<uint32_t>(width) &&
        swapchainExtent_.height == static_cast<uint32_t>(height)) {
        LOG_DEBUG_CAT("Swapchain", "Resize to same size ignored");
        return;
    }

    LOG_INFO_CAT("Swapchain", "RESIZING: {}x{} → {}x{}", swapchainExtent_.width, swapchainExtent_.height, width, height);

    // --- DO NOT WAIT HERE — renderer already waited in applyResize() ---

    VkSwapchainKHR oldSwapchain = swapchain_;

    try {
        VkSwapchainKHR newSwapchain = createNewSwapchain(width, height, oldSwapchain);
        swapchain_ = newSwapchain;

        // Get new images
        VK_CHECK(vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, nullptr));
        swapchainImages_.resize(imageCount_);
        VK_CHECK(vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, swapchainImages_.data()));

        // Destroy old image views *before* creating new ones
        if (!swapchainImageViews_.empty()) {
            Dispose::destroyImageViews(context_.device, swapchainImageViews_);
            swapchainImageViews_.clear();
            context_.swapchainImageViews.clear();
        }

        // Create new image views
        swapchainImageViews_.resize(imageCount_);
        const VkImageViewCreateInfo viewInfo{
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = swapchainImageFormat_,
            .components       = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                  VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };

        for (uint32_t i = 0; i < imageCount_; ++i) {
            VkImageViewCreateInfo vi = viewInfo;
            vi.image = swapchainImages_[i];
            VK_CHECK(vkCreateImageView(context_.device, &vi, nullptr, &swapchainImageViews_[i]));
        }

        // Update FIF
        maxFramesInFlight_ = std::min(MAX_FRAMES_IN_FLIGHT, imageCount_);

        // Update context
        context_.swapchain           = swapchain_;
        context_.swapchainImageFormat = swapchainImageFormat_;
        context_.swapchainExtent     = swapchainExtent_;
        context_.swapchainImages     = swapchainImages_;
        context_.swapchainImageViews = swapchainImageViews_;

        // Destroy old swapchain *after* new one is ready
        if (oldSwapchain != VK_NULL_HANDLE && oldSwapchain != newSwapchain) {
            vkDestroySwapchainKHR(context_.device, oldSwapchain, nullptr);
        }

        LOG_INFO_CAT("Swapchain", "RESIZE SUCCESS: {}x{}", swapchainExtent_.width, swapchainExtent_.height);
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("Swapchain", "Resize failed: {}", e.what());
        swapchain_ = oldSwapchain;
        throw;
    }
}

// ---------------------------------------------------------------------------
//  Cleanup
// ---------------------------------------------------------------------------
void VulkanSwapchainManager::cleanupSwapchain()
{
    if (context_.device == VK_NULL_HANDLE) return;

    // Wait for GPU — safe because called from renderer cleanup
    waitForInFlightFrames();

    if (!swapchainImageViews_.empty()) {
        Dispose::destroyImageViews(context_.device, swapchainImageViews_);
        swapchainImageViews_.clear();
        context_.swapchainImageViews.clear();
    }
    swapchainImages_.clear();
    context_.swapchainImages.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context_.device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }

    maxFramesInFlight_ = 0;
    imageCount_ = 0;
    swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    swapchainExtent_ = {0, 0};

    LOG_DEBUG_CAT("Swapchain", "Swapchain cleaned up");
}

// ---------------------------------------------------------------------------
//  Accessors
// ---------------------------------------------------------------------------
VkSemaphore VulkanSwapchainManager::getImageAvailableSemaphore(uint32_t frame) const
{
    return imageAvailableSemaphores_[frame % maxFramesInFlight_];
}

VkSemaphore VulkanSwapchainManager::getRenderFinishedSemaphore(uint32_t frame) const
{
    return renderFinishedSemaphores_[frame % maxFramesInFlight_];
}

VkFence VulkanSwapchainManager::getInFlightFence(uint32_t frame) const
{
    return inFlightFences_[frame % maxFramesInFlight_];
}

} // namespace VulkanRTX