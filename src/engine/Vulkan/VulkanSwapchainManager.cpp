// src/engine/Vulkan/VulkanSwapchainManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan swapchain management implementation.
// Dependencies: Vulkan 1.3+, VulkanCore.hpp, Vulkan_init.hpp, logging.hpp, Dispose.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Dispose.hpp"
#include <stdexcept>
#include <algorithm>
#include <format>

#define VK_CHECK(result, msg) if ((result) != VK_SUCCESS) { \
    LOG_ERROR_CAT("Swapchain", "{} (VkResult: {})", (msg), static_cast<int>(result)); \
    throw std::runtime_error(msg); \
}

namespace VulkanRTX {

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;  // Standard limit for in-flight frames

VulkanSwapchainManager::VulkanSwapchainManager(Vulkan::Context& context, VkSurfaceKHR surface)
    : context_(context),
      swapchain_(VK_NULL_HANDLE),
      swapchainImageFormat_(VK_FORMAT_UNDEFINED),
      swapchainExtent_({0, 0}),
      imageCount_(0),
      graphicsQueueFamilyIndex_(context_.graphicsQueueFamilyIndex),
      presentQueueFamilyIndex_(context_.presentQueueFamilyIndex),
      maxFramesInFlight_(0) {
    LOG_INFO_CAT("Swapchain", "Entering VulkanSwapchainManager constructor with surface={:p}", static_cast<void*>(surface));
    LOG_DEBUG_CAT("Swapchain", "Step 1: Validating inputs");
    if (surface == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Swapchain", "Invalid surface provided (surface={:p})", static_cast<void*>(surface));
        throw std::runtime_error("Invalid surface");
    }
    if (graphicsQueueFamilyIndex_ == UINT32_MAX || presentQueueFamilyIndex_ == UINT32_MAX) {
        LOG_ERROR_CAT("Swapchain", "Invalid queue family indices: graphicsQueueFamilyIndex_={}, presentQueueFamilyIndex_={}",
                  graphicsQueueFamilyIndex_, presentQueueFamilyIndex_);
        throw std::runtime_error("Invalid queue family indices");
    }
    LOG_DEBUG_CAT("Swapchain", "Step 2: Assigning surface to context");
    context_.surface = surface;
    LOG_DEBUG_CAT("Swapchain", "Assigned surface to context: surface={:p}", static_cast<void*>(context_.surface));

    // Pre-allocate per-frame semaphores
    LOG_DEBUG_CAT("Swapchain", "Step 3: Resizing renderFinishedSemaphores_ to {}", MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    LOG_DEBUG_CAT("Swapchain", "Resized renderFinishedSemaphores_ to size={} (MAX_FRAMES_IN_FLIGHT={})", renderFinishedSemaphores_.size(), MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        LOG_DEBUG_CAT("Swapchain", "Step 4.{}: Creating render finished semaphore for frame index {}", i + 1, i);
        VkSemaphoreCreateInfo semaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0  // Binary semaphore
        };
        VkResult result = vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("Swapchain", "Failed to create render finished semaphore for frame index {}: VkResult={}", i, static_cast<int>(result));
            throw std::runtime_error("Failed to create render finished semaphore");
        }
        LOG_DEBUG_CAT("Swapchain", "Created render finished semaphore[{}] at {:p}", i, static_cast<void*>(renderFinishedSemaphores_[i]));
    }
    LOG_INFO_CAT("Swapchain", "VulkanSwapchainManager constructed successfully with {} semaphores", MAX_FRAMES_IN_FLIGHT);
}

VulkanSwapchainManager::~VulkanSwapchainManager() {
    LOG_INFO_CAT("Swapchain", "Entering VulkanSwapchainManager destructor");
    cleanupSwapchain();
    Dispose::destroySemaphores(context_.device, renderFinishedSemaphores_);
    LOG_INFO_CAT("Swapchain", "VulkanSwapchainManager destructed successfully");
}

void VulkanSwapchainManager::initializeSwapchain(int width, int height) {
    LOG_INFO_CAT("Swapchain", "Entering initializeSwapchain with width={}, height={}", width, height);
    LOG_DEBUG_CAT("Swapchain", "Step 1: Validating Vulkan context");
    if (context_.device == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Swapchain", "Device is null in swapchain initialization (device={:p})", static_cast<void*>(context_.device));
        throw std::runtime_error("Null device");
    }
    if (context_.physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Swapchain", "Physical device is null in swapchain initialization (physicalDevice={:p})", static_cast<void*>(context_.physicalDevice));
        throw std::runtime_error("Null physical device");
    }
    if (context_.surface == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Swapchain", "Surface is null in swapchain initialization (surface={:p})", static_cast<void*>(context_.surface));
        throw std::runtime_error("Null surface");
    }
    if (width <= 0 || height <= 0) {
        LOG_ERROR_CAT("Swapchain", "Invalid window dimensions for swapchain initialization: width={}, height={}", width, height);
        throw std::runtime_error("Invalid window dimensions for swapchain: " + std::to_string(width) + "x" + std::to_string(height));
    }
    LOG_DEBUG_CAT("Swapchain", "Validated Vulkan context: device={:p}, physicalDevice={:p}, surface={:p}",
              static_cast<void*>(context_.device), static_cast<void*>(context_.physicalDevice), static_cast<void*>(context_.surface));

    // Wait for device to be idle
    LOG_DEBUG_CAT("Swapchain", "Step 2: Calling vkDeviceWaitIdle before swapchain initialization");
    vkDeviceWaitIdle(context_.device);

    // Query surface capabilities
    LOG_DEBUG_CAT("Swapchain", "Step 3: Querying surface capabilities");
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, context_.surface, &capabilities);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Swapchain", "Failed to query surface capabilities: VkResult={}", static_cast<int>(result));
        throw std::runtime_error("Failed to query surface capabilities");
    }
    LOG_DEBUG_CAT("Swapchain", "Surface capabilities: minImageCount={}, maxImageCount={}, currentExtent={}x{}, minExtent={}x{}, maxExtent={}x{}, currentTransform={:x}, supportedTransforms={:x}, supportedCompositeAlpha={:x}, supportedUsageFlags={:x}",
              capabilities.minImageCount, capabilities.maxImageCount,
              capabilities.currentExtent.width, capabilities.currentExtent.height,
              capabilities.minImageExtent.width, capabilities.minImageExtent.height,
              capabilities.maxImageExtent.width, capabilities.maxImageExtent.height,
              static_cast<uint32_t>(capabilities.currentTransform), static_cast<uint32_t>(capabilities.supportedTransforms),
              static_cast<uint32_t>(capabilities.supportedCompositeAlpha), static_cast<uint32_t>(capabilities.supportedUsageFlags));

    LOG_DEBUG_CAT("Swapchain", "Step 4: Validating minimum image count");
    if (capabilities.minImageCount == 0) {
        LOG_ERROR_CAT("Swapchain", "Surface capabilities report zero minimum images, which is invalid");
        throw std::runtime_error("Invalid surface capabilities");
    }

    // Check for storage image support
    LOG_DEBUG_CAT("Swapchain", "Step 5: Checking for VK_IMAGE_USAGE_STORAGE_BIT support");
    if (!(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)) {
        LOG_WARNING_CAT("Swapchain", "Swapchain does not support VK_IMAGE_USAGE_STORAGE_BIT; compute operations may be limited");
    }

    // Additional check for degenerate extents
    if (capabilities.minImageExtent.width == 0 || capabilities.minImageExtent.height == 0 ||
        capabilities.maxImageExtent.width == 0 || capabilities.maxImageExtent.height == 0) {
        LOG_ERROR_CAT("Swapchain", "Degenerate surface extents: min={}x{}, max={}x{}", 
                  capabilities.minImageExtent.width, capabilities.minImageExtent.height,
                  capabilities.maxImageExtent.width, capabilities.maxImageExtent.height);
        throw std::runtime_error("Surface extents invalid - ensure SDL_ShowWindow called and window is resized");
    }

    // Query surface formats
    LOG_DEBUG_CAT("Swapchain", "Step 6: Querying surface formats (first pass to get count)");
    uint32_t formatCount;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, context_.surface, &formatCount, nullptr);
    if (result != VK_SUCCESS || formatCount == 0) {
        LOG_ERROR_CAT("Swapchain", "Failed to query surface formats: VkResult={}, formatCount={}", static_cast<int>(result), formatCount);
        throw std::runtime_error("No surface formats available");
    }
    LOG_DEBUG_CAT("Swapchain", "Queried {} available surface formats", formatCount);
    LOG_DEBUG_CAT("Swapchain", "Step 7: Allocating formats vector with size={}", formatCount);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    LOG_DEBUG_CAT("Swapchain", "Step 8: Retrieving surface formats");
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, context_.surface, &formatCount, formats.data()),
             "Failed to query surface formats");
    LOG_DEBUG_CAT("Swapchain", "Retrieved {} surface formats", formatCount);
    for (uint32_t i = 0; i < formatCount; ++i) {
        LOG_DEBUG_CAT("Swapchain", "Surface format[{}]: format={}, colorSpace={}", i, static_cast<uint32_t>(formats[i].format), static_cast<uint32_t>(formats[i].colorSpace));
    }

    // Query present modes
    LOG_DEBUG_CAT("Swapchain", "Step 9: Querying present modes (first pass to get count)");
    uint32_t presentModeCount;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, context_.surface, &presentModeCount, nullptr);
    if (result != VK_SUCCESS || presentModeCount == 0) {
        LOG_ERROR_CAT("Swapchain", "Failed to query present modes: VkResult={}, presentModeCount={}", static_cast<int>(result), presentModeCount);
        throw std::runtime_error("No present modes available");
    }
    LOG_DEBUG_CAT("Swapchain", "Queried {} available present modes", presentModeCount);
    LOG_DEBUG_CAT("Swapchain", "Step 10: Allocating presentModes vector with size={}", presentModeCount);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    LOG_DEBUG_CAT("Swapchain", "Step 11: Retrieving present modes");
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, context_.surface, &presentModeCount, presentModes.data()),
             "Failed to query present modes");
    LOG_DEBUG_CAT("Swapchain", "Retrieved {} present modes", presentModeCount);
    for (uint32_t i = 0; i < presentModeCount; ++i) {
        LOG_DEBUG_CAT("Swapchain", "Present mode[{}]: {}", i, static_cast<uint32_t>(presentModes[i]));
    }

    // Choose surface format
    LOG_DEBUG_CAT("Swapchain", "Step 12: Selecting surface format");
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    LOG_DEBUG_CAT("Swapchain", "Default surface format: format={}, colorSpace={}", static_cast<uint32_t>(surfaceFormat.format), static_cast<uint32_t>(surfaceFormat.colorSpace));
    bool preferredFormatFound = false;
    for (const auto& availableFormat : formats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = availableFormat;
            preferredFormatFound = true;
            LOG_INFO_CAT("Swapchain", "Selected preferred surface format: format=VK_FORMAT_B8G8R8A8_SRGB, colorSpace=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR");
            break;
        }
    }
    if (!preferredFormatFound) {
        LOG_WARNING_CAT("Swapchain", "Preferred surface format (VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) not found; using default format={}",
                    static_cast<uint32_t>(surfaceFormat.format));
    }
    swapchainImageFormat_ = surfaceFormat.format;
    LOG_DEBUG_CAT("Swapchain", "Assigned swapchainImageFormat_: {}", static_cast<uint32_t>(swapchainImageFormat_));

    // Choose present mode
    LOG_DEBUG_CAT("Swapchain", "Step 13: Selecting present mode");
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    LOG_DEBUG_CAT("Swapchain", "Default present mode: VK_PRESENT_MODE_FIFO_KHR");
    bool mailboxModeFound = false;
    for (const auto& availablePresentMode : presentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = availablePresentMode;
            mailboxModeFound = true;
            LOG_INFO_CAT("Swapchain", "Selected preferred present mode: VK_PRESENT_MODE_MAILBOX_KHR");
            break;
        }
    }
    if (!mailboxModeFound) {
        LOG_WARNING_CAT("Swapchain", "Preferred present mode (VK_PRESENT_MODE_MAILBOX_KHR) not found; using VK_PRESENT_MODE_FIFO_KHR");
    }
    LOG_DEBUG_CAT("Swapchain", "Assigned presentMode: {}", static_cast<uint32_t>(presentMode));

    // Choose extent
    LOG_DEBUG_CAT("Swapchain", "Step 14: Selecting swapchain extent");
    swapchainExtent_ = capabilities.currentExtent;
    LOG_DEBUG_CAT("Swapchain", "Initial swapchain extent from capabilities: {}x{}", swapchainExtent_.width, swapchainExtent_.height);
    if (swapchainExtent_.width == 0 || swapchainExtent_.height == 0) {
        LOG_DEBUG_CAT("Swapchain", "Step 15: Adjusting extent due to invalid currentExtent (0x0)");
        swapchainExtent_.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        swapchainExtent_.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        LOG_DEBUG_CAT("Swapchain", "Adjusted swapchain extent to {}x{} based on requested width={} and height={}",
                  swapchainExtent_.width, swapchainExtent_.height, width, height);
    }
    if (swapchainExtent_.width == 0 || swapchainExtent_.height == 0) {
        LOG_ERROR_CAT("Swapchain", "Invalid swapchain extent after adjustment: {}x{}", swapchainExtent_.width, swapchainExtent_.height);
        throw std::runtime_error("Invalid swapchain extent (window not ready?)");
    }
    LOG_DEBUG_CAT("Swapchain", "Final swapchain extent: {}x{}", swapchainExtent_.width, swapchainExtent_.height);

    // Choose image count
    LOG_DEBUG_CAT("Swapchain", "Step 16: Selecting image count");
    imageCount_ = capabilities.minImageCount + 1;
    LOG_DEBUG_CAT("Swapchain", "Requested image count: {} (minImageCount={} + 1)", imageCount_, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0 && imageCount_ > capabilities.maxImageCount) {
        LOG_DEBUG_CAT("Swapchain", "Step 17: Clamping image count to maxImageCount");
        imageCount_ = capabilities.maxImageCount;
        LOG_DEBUG_CAT("Swapchain", "Clamped image count to maxImageCount: {}", imageCount_);
    }
    LOG_INFO_CAT("Swapchain", "Selected swapchain image count: {}", imageCount_);

    // Queue families
    LOG_DEBUG_CAT("Swapchain", "Step 18: Validating queue family indices");
    if (graphicsQueueFamilyIndex_ == UINT32_MAX || presentQueueFamilyIndex_ == UINT32_MAX) {
        LOG_ERROR_CAT("Swapchain", "Invalid queue family indices: graphicsQueueFamilyIndex_={}, presentQueueFamilyIndex_={}",
                  graphicsQueueFamilyIndex_, presentQueueFamilyIndex_);
        throw std::runtime_error("Invalid queue family indices");
    }
    LOG_DEBUG_CAT("Swapchain", "Step 19: Configuring queue families");
    std::vector<uint32_t> queueFamilies;
    VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (graphicsQueueFamilyIndex_ != presentQueueFamilyIndex_) {
        queueFamilies = {graphicsQueueFamilyIndex_, presentQueueFamilyIndex_};
        sharingMode = VK_SHARING_MODE_CONCURRENT;
        LOG_DEBUG_CAT("Swapchain", "Using concurrent sharing mode with queue families: graphics={}, present={}",
                  graphicsQueueFamilyIndex_, presentQueueFamilyIndex_);
    } else {
        LOG_DEBUG_CAT("Swapchain", "Using exclusive sharing mode (graphicsQueueFamilyIndex_ == presentQueueFamilyIndex_ = {})",
                  graphicsQueueFamilyIndex_);
    }

    // Create swapchain
    LOG_DEBUG_CAT("Swapchain", "Step 20: Preparing swapchain creation");
    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = context_.surface,
        .minImageCount = imageCount_,
        .imageFormat = swapchainImageFormat_,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = swapchainExtent_,
        .imageArrayLayers = 1,
        .imageUsage = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT ? VK_IMAGE_USAGE_STORAGE_BIT : 0)),
        .imageSharingMode = sharingMode,
        .queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size()),
        .pQueueFamilyIndices = queueFamilies.data(),
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };
    LOG_DEBUG_CAT("Swapchain", "Swapchain create info: minImageCount={}, imageFormat={}, imageColorSpace={}, extent={}x{}, imageArrayLayers={}, imageUsage={:x}, sharingMode={}, queueFamilyIndexCount={}, preTransform={:x}, compositeAlpha={:x}, presentMode={}, clipped={}",
              createInfo.minImageCount, static_cast<uint32_t>(createInfo.imageFormat), static_cast<uint32_t>(createInfo.imageColorSpace),
              createInfo.imageExtent.width, createInfo.imageExtent.height, createInfo.imageArrayLayers, createInfo.imageUsage,
              static_cast<uint32_t>(createInfo.imageSharingMode), createInfo.queueFamilyIndexCount,
              static_cast<uint32_t>(createInfo.preTransform), static_cast<uint32_t>(createInfo.compositeAlpha),
              static_cast<uint32_t>(createInfo.presentMode), createInfo.clipped);

    LOG_DEBUG_CAT("Swapchain", "Step 21: Creating swapchain");
    result = vkCreateSwapchainKHR(context_.device, &createInfo, nullptr, &swapchain_);
    if (result == VK_SUBOPTIMAL_KHR) {
        LOG_WARNING_CAT("Swapchain", "Swapchain created but is suboptimal: VkResult=VK_SUBOPTIMAL_KHR, may impact performance");
    } else if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Swapchain", "Failed to create swapchain: VkResult={}", static_cast<int>(result));
        throw std::runtime_error("Failed to create swapchain");
    }
    LOG_INFO_CAT("Swapchain", "Created swapchain at {:p} with {} images, format={}, extent={}x{}",
             static_cast<void*>(swapchain_), imageCount_, static_cast<uint32_t>(swapchainImageFormat_),
             swapchainExtent_.width, swapchainExtent_.height);

    // Retrieve swapchain images
    LOG_DEBUG_CAT("Swapchain", "Step 22: Querying swapchain image count");
    result = vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, nullptr);
    if (result != VK_SUCCESS || imageCount_ == 0) {
        LOG_ERROR_CAT("Swapchain", "Failed to get swapchain image count: VkResult={}, imageCount={}", static_cast<int>(result), imageCount_);
        throw std::runtime_error("Failed to get swapchain image count");
    }
    LOG_DEBUG_CAT("Swapchain", "Queried swapchain image count: {}", imageCount_);
    LOG_DEBUG_CAT("Swapchain", "Step 23: Resizing swapchainImages_ to size={}", imageCount_);
    swapchainImages_.resize(imageCount_);
    LOG_DEBUG_CAT("Swapchain", "Step 24: Retrieving swapchain images");
    result = vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, swapchainImages_.data());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Swapchain", "Failed to retrieve swapchain images: VkResult={}", static_cast<int>(result));
        throw std::runtime_error("Failed to retrieve swapchain images");
    }
    if (swapchainImages_.empty()) {
        LOG_ERROR_CAT("Swapchain", "Swapchain images vector is empty after retrieval");
        throw std::runtime_error("Empty swapchain images");
    }
    LOG_DEBUG_CAT("Swapchain", "Retrieved {} swapchain images", swapchainImages_.size());
    for (uint32_t i = 0; i < imageCount_; ++i) {
        LOG_DEBUG_CAT("Swapchain", "Swapchain image[{}] handle: {:p}", i, static_cast<void*>(swapchainImages_[i]));
        context_.resourceManager.addImage(swapchainImages_[i]);
        LOG_DEBUG_CAT("Swapchain", "Registered swapchain image[{}] with resource manager", i);
    }

    // Create image views
    LOG_DEBUG_CAT("Swapchain", "Step 25: Resizing swapchainImageViews_ to size={}", imageCount_);
    swapchainImageViews_.resize(imageCount_);
    LOG_DEBUG_CAT("Swapchain", "Resized swapchainImageViews_ to size={}", swapchainImageViews_.size());
    for (uint32_t i = 0; i < imageCount_; ++i) {
        LOG_DEBUG_CAT("Swapchain", "Step 26.{}: Creating image view for swapchain image[{}] at {:p}", i + 1, i, static_cast<void*>(swapchainImages_[i]));
        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = swapchainImages_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainImageFormat_,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        VkResult result = vkCreateImageView(context_.device, &viewInfo, nullptr, &swapchainImageViews_[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("Swapchain", "Failed to create image view for swapchain image[{}]: VkResult={}", i, static_cast<int>(result));
            throw std::runtime_error("Failed to create image view for swapchain image " + std::to_string(i));
        }
        context_.resourceManager.addImageView(swapchainImageViews_[i]);
        LOG_DEBUG_CAT("Swapchain", "Created and registered swapchain image view[{}] at {:p}", i, static_cast<void*>(swapchainImageViews_[i]));
    }
    if (swapchainImageViews_.empty()) {
        LOG_ERROR_CAT("Swapchain", "Swapchain image views vector is empty after creation");
        throw std::runtime_error("Empty swapchain image views after creation");
    }

    // Set max frames
    LOG_DEBUG_CAT("Swapchain", "Step 27: Setting maxFramesInFlight_");
    maxFramesInFlight_ = std::min(MAX_FRAMES_IN_FLIGHT, imageCount_);
    LOG_INFO_CAT("Swapchain", "Set maxFramesInFlight_ to {} (constrained by MAX_FRAMES_IN_FLIGHT={} and imageCount={})",
             maxFramesInFlight_, MAX_FRAMES_IN_FLIGHT, imageCount_);

    // Update context
    LOG_DEBUG_CAT("Swapchain", "Step 28: Updating Vulkan context");
    context_.swapchain = swapchain_;
    context_.swapchainImageFormat = swapchainImageFormat_;
    context_.swapchainExtent = swapchainExtent_;
    context_.swapchainImages = swapchainImages_;
    context_.swapchainImageViews = swapchainImageViews_;
    LOG_DEBUG_CAT("Swapchain", "Updated Vulkan context: swapchain={:p}, swapchainImageFormat={}, swapchainExtent={}x{}, swapchainImages.size={}, swapchainImageViews.size={}",
              static_cast<void*>(context_.swapchain), static_cast<uint32_t>(context_.swapchainImageFormat),
              context_.swapchainExtent.width, context_.swapchainExtent.height,
              context_.swapchainImages.size(), context_.swapchainImageViews.size());

    LOG_INFO_CAT("Swapchain", "Swapchain initialization completed: {} images, format={}, extent={}x{}, maxFramesInFlight={}, sharingMode={}",
             imageCount_, static_cast<uint32_t>(swapchainImageFormat_), swapchainExtent_.width, swapchainExtent_.height,
             maxFramesInFlight_, static_cast<uint32_t>(sharingMode));
}

void VulkanSwapchainManager::handleResize(int width, int height) {
    LOG_INFO_CAT("Swapchain", "Entering handleResize with new dimensions: width={}, height={}", width, height);
    LOG_DEBUG_CAT("Swapchain", "Calling vkDeviceWaitIdle before resize");
    vkDeviceWaitIdle(context_.device);
    LOG_DEBUG_CAT("Swapchain", "Cleaning up existing swapchain before resize");
    cleanupSwapchain();
    LOG_DEBUG_CAT("Swapchain", "Reinitializing swapchain with new dimensions");
    initializeSwapchain(width, height);
    LOG_INFO_CAT("Swapchain", "Swapchain resized successfully to {}x{}", width, height);
}

void VulkanSwapchainManager::cleanupSwapchain() {
    LOG_INFO_CAT("Swapchain", "Entering cleanupSwapchain");
    LOG_DEBUG_CAT("Swapchain", "Calling vkDeviceWaitIdle before cleanup");
    vkDeviceWaitIdle(context_.device);

    // Destroy image views
    LOG_DEBUG_CAT("Swapchain", "Destroying {} swapchain image views", swapchainImageViews_.size());
    Dispose::destroyImageViews(context_.device, swapchainImageViews_);
    context_.swapchainImageViews.clear();

    // Swapchain images are owned by the swapchain, just clear the vector
    LOG_DEBUG_CAT("Swapchain", "Clearing {} swapchain images", swapchainImages_.size());
    for (uint32_t i = 0; i < swapchainImages_.size(); ++i) {
        LOG_DEBUG_CAT("Swapchain", "Unregistering swapchain image[{}] at {:p}", i, static_cast<void*>(swapchainImages_[i]));
        context_.resourceManager.removeImage(swapchainImages_[i]);
    }
    swapchainImages_.clear();
    context_.swapchainImages.clear();

    // Destroy swapchain
    LOG_DEBUG_CAT("Swapchain", "Destroying swapchain at {:p}", static_cast<void*>(swapchain_));
    Dispose::destroySingleSwapchain(context_.device, swapchain_);
    context_.swapchain = VK_NULL_HANDLE;

    // Reset local state
    maxFramesInFlight_ = 0;
    imageCount_ = 0;
    swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    swapchainExtent_ = {0, 0};
    LOG_DEBUG_CAT("Swapchain", "Reset local state: maxFramesInFlight_={}, imageCount_={}, swapchainImageFormat_={}, swapchainExtent_={}x{}",
              maxFramesInFlight_, imageCount_, static_cast<uint32_t>(swapchainImageFormat_),
              swapchainExtent_.width, swapchainExtent_.height);

    LOG_INFO_CAT("Swapchain", "Swapchain cleanup completed");
}

VkSemaphore VulkanSwapchainManager::getRenderFinishedSemaphore(uint32_t currentFrame) const {
    LOG_DEBUG_CAT("Swapchain", "Requesting render finished semaphore for frame index {}", currentFrame);
    if (currentFrame >= renderFinishedSemaphores_.size()) {
        LOG_ERROR_CAT("Swapchain", "Invalid frame index {} (renderFinishedSemaphores_.size={})", currentFrame, renderFinishedSemaphores_.size());
        throw std::out_of_range("Invalid current frame for render finished semaphore: " + std::to_string(currentFrame));
    }
    VkSemaphore semaphore = renderFinishedSemaphores_[currentFrame];
    LOG_DEBUG_CAT("Swapchain", "Returning render finished semaphore[{}] at {:p}", currentFrame, static_cast<void*>(semaphore));
    return semaphore;
}

} // namespace VulkanRTX