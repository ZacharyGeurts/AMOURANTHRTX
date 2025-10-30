// src/engine/Vulkan/VulkanSwapchainManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <format>

namespace VulkanRTX {

#define VK_CHECK(x) do { \
    VkResult result = (x); \
    if (result != VK_SUCCESS) { \
        LOG_ERROR_CAT("Swapchain", #x " failed: {}", static_cast<int>(result)); \
        throw std::runtime_error(#x " failed"); \
    } \
} while(0)

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

VulkanSwapchainManager::VulkanSwapchainManager(Vulkan::Context& context, VkSurfaceKHR surface)
    : context_(context),
      swapchain_(VK_NULL_HANDLE),
      swapchainImageFormat_(VK_FORMAT_UNDEFINED),
      swapchainExtent_({0, 0}),
      imageCount_(0),
      graphicsQueueFamilyIndex_(context_.graphicsQueueFamilyIndex),
      presentQueueFamilyIndex_(context_.presentQueueFamilyIndex),
      maxFramesInFlight_(0)
{
    if (surface == VK_NULL_HANDLE) {
        throw std::runtime_error("Invalid surface");
    }
    if (graphicsQueueFamilyIndex_ == UINT32_MAX || presentQueueFamilyIndex_ == UINT32_MAX) {
        throw std::runtime_error("Invalid queue family indices");
    }
    context_.surface = surface;

    imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);

    const VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };

    const VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]));
        VK_CHECK(vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]));
        VK_CHECK(vkCreateFence(context_.device, &fenceInfo, nullptr, &inFlightFences_[i]));
    }
}

VulkanSwapchainManager::~VulkanSwapchainManager()
{
    cleanupSwapchain();
    Dispose::destroySemaphores(context_.device, imageAvailableSemaphores_);
    Dispose::destroySemaphores(context_.device, renderFinishedSemaphores_);
    Dispose::destroyFences(context_.device, inFlightFences_);
}

void VulkanSwapchainManager::initializeSwapchain(int width, int height)
{
    if (context_.device == VK_NULL_HANDLE)   throw std::runtime_error("Null device");
    if (context_.physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("Null physical device");
    if (context_.surface == VK_NULL_HANDLE)  throw std::runtime_error("Null surface");
    if (width <= 0 || height <= 0)          throw std::runtime_error("Invalid window dimensions");

    vkDeviceWaitIdle(context_.device);

    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, context_.surface, &caps));

    if (caps.minImageCount == 0 ||
        caps.minImageExtent.width == 0 || caps.minImageExtent.height == 0 ||
        caps.maxImageExtent.width == 0 || caps.maxImageExtent.height == 0)
        throw std::runtime_error("Invalid surface capabilities");

    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, context_.surface, &formatCount, nullptr));
    if (formatCount == 0) throw std::runtime_error("No surface formats available");
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, context_.surface, &formatCount, formats.data()));

    uint32_t presentModeCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, context_.surface, &presentModeCount, nullptr));
    if (presentModeCount == 0) throw std::runtime_error("No present modes available");
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, context_.surface, &presentModeCount, presentModes.data()));

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            break;
        }
    }
    swapchainImageFormat_ = chosenFormat.format;

    VkPresentModeKHR chosenPresent = VK_PRESENT_MODE_FIFO_KHR;
    if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end())
        chosenPresent = VK_PRESENT_MODE_MAILBOX_KHR;
    else if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != presentModes.end())
        chosenPresent = VK_PRESENT_MODE_IMMEDIATE_KHR;

    swapchainExtent_ = caps.currentExtent;
    if (swapchainExtent_.width == 0 || swapchainExtent_.height == 0) {
        swapchainExtent_.width  = std::clamp(static_cast<uint32_t>(width),  caps.minImageExtent.width,  caps.maxImageExtent.width);
        swapchainExtent_.height = std::clamp(static_cast<uint32_t>(height), caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (swapchainExtent_.width == 0 || swapchainExtent_.height == 0)
        throw std::runtime_error("Invalid swapchain extent");

    imageCount_ = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount_ > caps.maxImageCount)
        imageCount_ = caps.maxImageCount;

    std::vector<uint32_t> queueFamilies;
    VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (graphicsQueueFamilyIndex_ != presentQueueFamilyIndex_) {
        queueFamilies = {graphicsQueueFamilyIndex_, presentQueueFamilyIndex_};
        sharingMode   = VK_SHARING_MODE_CONCURRENT;
    }

    VkImageUsageFlags storageUsage = ((caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) != 0)
        ? VK_IMAGE_USAGE_STORAGE_BIT
        : static_cast<VkImageUsageFlags>(0);

    const VkSwapchainCreateInfoKHR sci{
        .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext                 = nullptr,
        .flags                 = 0,
        .surface               = context_.surface,
        .minImageCount         = imageCount_,
        .imageFormat           = swapchainImageFormat_,
        .imageColorSpace       = chosenFormat.colorSpace,
        .imageExtent           = swapchainExtent_,
        .imageArrayLayers      = 1,
        .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                 storageUsage,
        .imageSharingMode      = sharingMode,
        .queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size()),
        .pQueueFamilyIndices   = queueFamilies.data(),
        .preTransform          = caps.currentTransform,
        .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode           = chosenPresent,
        .clipped               = VK_TRUE,
        .oldSwapchain          = VK_NULL_HANDLE
    };

    VK_CHECK(vkCreateSwapchainKHR(context_.device, &sci, nullptr, &swapchain_));

    VK_CHECK(vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, nullptr));
    swapchainImages_.resize(imageCount_);
    VK_CHECK(vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, swapchainImages_.data()));

    swapchainImageViews_.resize(imageCount_);
    const VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = 0,
        .image            = VK_NULL_HANDLE,
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

    context_.swapchain          = swapchain_;
    context_.swapchainImageFormat = swapchainImageFormat_;
    context_.swapchainExtent    = swapchainExtent_;
    context_.swapchainImages    = swapchainImages_;
    context_.swapchainImageViews = swapchainImageViews_;

    LOG_INFO_CAT("Swapchain", "Created {}x{} swapchain with {} images", 
                 swapchainExtent_.width, swapchainExtent_.height, imageCount_);
}

void VulkanSwapchainManager::handleResize(int width, int height)
{
    LOG_INFO_CAT("Swapchain", "Resizing swapchain: {}x{} → {}x{}", 
                 swapchainExtent_.width, swapchainExtent_.height, width, height);
    vkDeviceWaitIdle(context_.device);
    cleanupSwapchain();
    initializeSwapchain(width, height);
}

void VulkanSwapchainManager::cleanupSwapchain()
{
    if (context_.device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(context_.device);

    Dispose::destroyImageViews(context_.device, swapchainImageViews_);
    context_.swapchainImageViews.clear();
    swapchainImages_.clear();
    context_.swapchainImages.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context_.device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }

    maxFramesInFlight_ = 0;
    imageCount_        = 0;
    swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    swapchainExtent_ = {0, 0};
}

VkSemaphore VulkanSwapchainManager::getImageAvailableSemaphore(uint32_t frame) const
{
    if (frame >= imageAvailableSemaphores_.size())
        throw std::out_of_range("Invalid frame index for image-available semaphore");
    return imageAvailableSemaphores_[frame];
}

VkSemaphore VulkanSwapchainManager::getRenderFinishedSemaphore(uint32_t frame) const
{
    if (frame >= renderFinishedSemaphores_.size())
        throw std::out_of_range("Invalid frame index for render-finished semaphore");
    return renderFinishedSemaphores_[frame];
}

VkFence VulkanSwapchainManager::getInFlightFence(uint32_t frame) const
{
    if (frame >= inFlightFences_.size())
        throw std::out_of_range("Invalid frame index for in-flight fence");
    return inFlightFences_[frame];
}

} // namespace VulkanRTX