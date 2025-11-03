// src/engine/Vulkan/VulkanSwapchainManager.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts
// FINAL: cleanup() added, RAII safe, matches main.cpp call
// LOVE: Every lifecycle event logged in neon teal glory
// UPDATE: Uncapped FPS - Prefer IMMEDIATE present mode, fallback MAILBOX/FIFO
//         Detailed logging for every step in initializeSwapchain
//         Enhanced error logging with file/line via VK_CHECK

#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/logging.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <algorithm>
#include <numeric>

using VulkanRTX::VulkanRTXException;

namespace VulkanRTX {

// ---------------------------------------------------------------------
//  SEXY LOGGING HELPER – OCEAN TEAL, FULL DETAIL
// ---------------------------------------------------------------------
void VulkanSwapchainManager::logSwapchainInfo(const char* prefix) const
{
    LOG_INFO_CAT("Swapchain",
        "{} | swapchain: {} | extent: {}x{} | format: {} | images: {} | views: {}",
        prefix,
        ptr_to_hex(swapchain_),
        swapchainExtent_.width, swapchainExtent_.height,
        swapchainImageFormat_,
        swapchainImages_.size(),
        swapchainImageViews_.size());
}

// ---------------------------------------------------------------------
//  CONSTRUCTOR – CREATE SURFACE + SYNC OBJECTS
// ---------------------------------------------------------------------
VulkanSwapchainManager::VulkanSwapchainManager(std::shared_ptr<Vulkan::Context> context,
                                               SDL_Window* window,
                                               int width,
                                               int height)
    : context_(std::move(context)), window_(window), width_(width), height_(height)
{
    if (!context_ || !context_->device) {
        throw std::runtime_error("VulkanSwapchainManager: null context/device");
    }

    surface_ = createSurface(window_);

    maxFramesInFlight_ = 2;
    imageAvailableSemaphores_.resize(maxFramesInFlight_);
    renderFinishedSemaphores_.resize(maxFramesInFlight_);
    inFlightFences_.resize(maxFramesInFlight_);

    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                               nullptr,
                               VK_FENCE_CREATE_SIGNALED_BIT};

    for (uint32_t i = 0; i < maxFramesInFlight_; ++i) {
        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr,
                                   &imageAvailableSemaphores_[i]),
                 "image-available semaphore");
        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr,
                                   &renderFinishedSemaphores_[i]),
                 "render-finished semaphore");
        VK_CHECK(vkCreateFence(context_->device, &fenceInfo, nullptr,
                               &inFlightFences_[i]),
                 "in-flight fence");
    }

    LOG_INFO_CAT("Swapchain", "Sync objects created: {} frames in flight", maxFramesInFlight_);
}

// ---------------------------------------------------------------------
//  DESTRUCTOR – FULL CLEANUP
// ---------------------------------------------------------------------
VulkanSwapchainManager::~VulkanSwapchainManager()
{
    cleanup();  // ← RAII: full cleanup
}

// ---------------------------------------------------------------------
//  PUBLIC: FULL CLEANUP (called from main.cpp)
// ---------------------------------------------------------------------
void VulkanSwapchainManager::cleanup()
{
    LOG_INFO_CAT("Swapchain", "Manager cleanup initiated");

    cleanupSwapchain();

    for (uint32_t i = 0; i < maxFramesInFlight_; ++i) {
        if (renderFinishedSemaphores_[i]) vkDestroySemaphore(context_->device, renderFinishedSemaphores_[i], nullptr);
        if (imageAvailableSemaphores_[i]) vkDestroySemaphore(context_->device, imageAvailableSemaphores_[i], nullptr);
        if (inFlightFences_[i]) vkDestroyFence(context_->device, inFlightFences_[i], nullptr);
    }
    imageAvailableSemaphores_.clear();
    renderFinishedSemaphores_.clear();
    inFlightFences_.clear();

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(context_->instance, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    LOG_INFO_CAT("Swapchain", "Manager destroyed – surface + sync objects released");
}

// ---------------------------------------------------------------------
//  INITIALIZE SWAPCHAIN – FULLY LOGGED
// ---------------------------------------------------------------------
void VulkanSwapchainManager::initializeSwapchain(int width, int height)
{
    LOG_INFO_CAT("Swapchain", "initializeSwapchain START: {}x{}", width, height);

    width_  = width;
    height_ = height;

    LOG_INFO_CAT("Swapchain", "STEP 1: Querying surface capabilities");

    // --- Surface capabilities ---
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_->physicalDevice,
                                                       surface_, &caps),
             "surface capabilities");

    LOG_INFO_CAT("Swapchain", "Capabilities: minExtent={}x{}, maxExtent={}x{}, currentTransform={}",
                 caps.minImageExtent.width, caps.minImageExtent.height,
                 caps.maxImageExtent.width, caps.maxImageExtent.height,
                 static_cast<uint32_t>(caps.currentTransform));

    LOG_INFO_CAT("Swapchain", "STEP 2: Querying surface formats");

    // --- Choose format (prefer sRGB) ---
    uint32_t fmtCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physicalDevice,
                                                  surface_, &fmtCount, nullptr),
             "surface format count");
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physicalDevice,
                                                  surface_, &fmtCount, formats.data()),
             "surface formats");

    LOG_INFO_CAT("Swapchain", "Available formats: {}", formats.size());

    VkSurfaceFormatKHR chosenFmt = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFmt = f;
            break;
        }
    }

    LOG_INFO_CAT("Swapchain", "Selected format: {} (colorSpace: {})", 
                 static_cast<uint32_t>(chosenFmt.format), static_cast<uint32_t>(chosenFmt.colorSpace));

    LOG_INFO_CAT("Swapchain", "STEP 3: Querying present modes for uncapped FPS");

    // --- Choose present mode (prefer IMMEDIATE for uncapped, fallback MAILBOX/FIFO) ---
    uint32_t pmCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physicalDevice,
                                                       surface_, &pmCount, nullptr),
             "present mode count");
    std::vector<VkPresentModeKHR> presentModes(pmCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physicalDevice,
                                                       surface_, &pmCount,
                                                       presentModes.data()),
             "present modes");

    LOG_INFO_CAT("Swapchain", "Available present modes: {}", presentModes.size());

    VkPresentModeKHR chosenPM = VK_PRESENT_MODE_FIFO_KHR;  // Safe fallback
    bool immediateAvailable = false;
    bool mailboxAvailable = false;

    for (const auto& pm : presentModes) {
        LOG_DEBUG_CAT("Swapchain", "  Mode available: {}", static_cast<uint32_t>(pm));
        if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            immediateAvailable = true;
            chosenPM = pm;  // Prefer IMMEDIATE: uncapped FPS, allows tearing
        } else if (pm == VK_PRESENT_MODE_MAILBOX_KHR && chosenPM == VK_PRESENT_MODE_FIFO_KHR) {
            mailboxAvailable = true;
            chosenPM = pm;  // Fallback to MAILBOX: uncapped, tear-free triple-buffer
        }
    }

    std::string pmDesc;
    if (chosenPM == VK_PRESENT_MODE_IMMEDIATE_KHR) {
        pmDesc = "IMMEDIATE (uncapped FPS, may tear)";
    } else if (chosenPM == VK_PRESENT_MODE_MAILBOX_KHR) {
        pmDesc = "MAILBOX (uncapped FPS, smooth triple-buffer)";
    } else {
        pmDesc = "FIFO (VSync capped at display Hz)";
    }

    LOG_INFO_CAT("Swapchain", "Selected present mode: {} | IMMEDIATE avail: {} | MAILBOX avail: {}", 
                 pmDesc, immediateAvailable, mailboxAvailable);

    LOG_INFO_CAT("Swapchain", "STEP 4: Computing extent");

    // --- Extent ---
    VkExtent2D extent{
        std::clamp(static_cast<uint32_t>(width_),  caps.minImageExtent.width,
                                          caps.maxImageExtent.width),
        std::clamp(static_cast<uint32_t>(height_), caps.minImageExtent.height,
                                          caps.maxImageExtent.height)
    };

    LOG_INFO_CAT("Swapchain", "Computed extent: {}x{}", extent.width, extent.height);

    LOG_INFO_CAT("Swapchain", "STEP 5: Computing image count");

    // --- Image count ---
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    LOG_INFO_CAT("Swapchain", "Image count: {} (min: {}, max: {})", imageCount, caps.minImageCount, caps.maxImageCount);

    LOG_INFO_CAT("Swapchain", "STEP 6: Creating swapchain");

    // --- Create swapchain ---
    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface          = surface_;
    sci.minImageCount    = imageCount;
    sci.imageFormat      = chosenFmt.format;
    sci.imageColorSpace  = chosenFmt.colorSpace;
    sci.imageExtent      = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.preTransform     = caps.currentTransform;
    sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode      = chosenPM;
    sci.clipped          = VK_TRUE;
    sci.oldSwapchain     = VK_NULL_HANDLE;

    uint32_t qfi[] = { context_->graphicsQueueFamilyIndex,
                       context_->presentQueueFamilyIndex };
    if (context_->graphicsQueueFamilyIndex != context_->presentQueueFamilyIndex) {
        sci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices   = qfi;
        LOG_INFO_CAT("Swapchain", "Queue sharing: CONCURRENT (graphics QFI: {}, present QFI: {})",
                     context_->graphicsQueueFamilyIndex, context_->presentQueueFamilyIndex);
    } else {
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        LOG_INFO_CAT("Swapchain", "Queue sharing: EXCLUSIVE (same QFI: {})", context_->graphicsQueueFamilyIndex);
    }

    VK_CHECK(vkCreateSwapchainKHR(context_->device, &sci, nullptr, &swapchain_),
             "vkCreateSwapchainKHR");

    LOG_INFO_CAT("Swapchain", "Swapchain created successfully: {}", ptr_to_hex(swapchain_));

    LOG_INFO_CAT("Swapchain", "STEP 7: Retrieving swapchain images");

    // --- Retrieve images ---
    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(context_->device, swapchain_, &imgCount, nullptr),
             "swapchain image count");
    swapchainImages_.resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(context_->device, swapchain_, &imgCount,
                                    swapchainImages_.data()),
             "swapchain images");

    swapchainImageFormat_ = chosenFmt.format;
    swapchainExtent_      = extent;

    LOG_INFO_CAT("Swapchain", "Retrieved {} images", swapchainImages_.size());

    LOG_INFO_CAT("Swapchain", "STEP 8: Creating image views");

    // --- Create image views ---
    swapchainImageViews_.resize(swapchainImages_.size());
    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image            = swapchainImages_[i];
        vci.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vci.format           = swapchainImageFormat_;
        vci.components       = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                 VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr,
                                   &swapchainImageViews_[i]),
                 "swapchain image view");
    }

    LOG_INFO_CAT("Swapchain", "Created {} image views", swapchainImageViews_.size());

    // SEXY LOG
    logSwapchainInfo("CREATED");

    LOG_INFO_CAT("Swapchain", "initializeSwapchain COMPLETE: Uncapped FPS enabled via {}", pmDesc);
}

// ---------------------------------------------------------------------
//  RESIZE – FULL RECREATION + LOGGING
// ---------------------------------------------------------------------
void VulkanSwapchainManager::handleResize(int width, int height)
{
    int pixelW = width, pixelH = height;
    SDL_GetWindowSizeInPixels(window_, &pixelW, &pixelH);
    if (pixelW == 0 || pixelH == 0) {
        LOG_DEBUG_CAT("Swapchain", "Resize ignored (window minimized)");
        return;
    }

    LOG_INFO_CAT("Swapchain", "RESIZE -> {}x{}", pixelW, pixelH);

    vkDeviceWaitIdle(context_->device);
    cleanupSwapchain();
    initializeSwapchain(pixelW, pixelH);

    logSwapchainInfo("RESIZED");
}

// ---------------------------------------------------------------------
//  INTERNAL: CLEANUP SWAPCHAIN ONLY
// ---------------------------------------------------------------------
void VulkanSwapchainManager::cleanupSwapchain()
{
    LOG_INFO_CAT("Swapchain", "CLEANUP START – destroying {} image views", swapchainImageViews_.size());

    for (auto view : swapchainImageViews_) {
        if (view) vkDestroyImageView(context_->device, view, nullptr);
    }
    swapchainImageViews_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context_->device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    swapchainImages_.clear();

    LOG_INFO_CAT("Swapchain", "CLEANUP DONE");
}

// ---------------------------------------------------------------------
//  PRIVATE: CREATE SURFACE
// ---------------------------------------------------------------------
VkSurfaceKHR VulkanSwapchainManager::createSurface(SDL_Window* window)
{
    VkSurfaceKHR surf = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, context_->instance, nullptr, &surf)) {
        throw std::runtime_error(
            std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
    LOG_INFO_CAT("Swapchain", "Surface created: {}", ptr_to_hex(surf));
    return surf;
}

// ---------------------------------------------------------------------
//  WAIT FOR IN-FLIGHT FRAMES
// ---------------------------------------------------------------------
void VulkanSwapchainManager::waitForInFlightFrames() const
{
    LOG_DEBUG_CAT("Swapchain", "waitForInFlightFrames() -> fence {}", ptr_to_hex(inFlightFences_[0]));
    VK_CHECK(vkWaitForFences(context_->device, 1, &inFlightFences_[0], VK_TRUE, UINT64_MAX),
             "wait in-flight fence");
}

} // namespace VulkanRTX