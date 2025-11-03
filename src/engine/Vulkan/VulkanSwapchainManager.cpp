// src/engine/Vulkan/VulkanSwapchainManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// FINAL: Triple buffer, developer config, full RAII, neon logging

#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <algorithm>
#include <format>
#include <bit>

using VulkanRTX::VulkanRTXException;

namespace VulkanRTX {

// ── LOGGING HELPER ───────────────────────────────────────────────────────────
void VulkanSwapchainManager::logSwapchainInfo(const char* prefix) const
{
    LOG_INFO_CAT("Swapchain",
        "{} | swapchain: {} | extent: {}x{} | format: {} | images: {} | views: {}",
        prefix,
        ptr_to_hex(swapchain_),
        swapchainExtent_.width, swapchainExtent_.height,
        static_cast<int>(swapchainImageFormat_),
        swapchainImages_.size(),
        swapchainImageViews_.size());
}

// ── CONSTRUCTOR ─────────────────────────────────────────────────────────────
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

    imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &imageAvailableSemaphores_[i]),
                 "image-available semaphore");
        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &renderFinishedSemaphores_[i]),
                 "render-finished semaphore");
        VK_CHECK(vkCreateFence(context_->device, &fenceInfo, nullptr, &inFlightFences_[i]),
                 "in-flight fence");
    }

    LOG_INFO_CAT("Swapchain", "Sync objects created: {} frames in flight (TRIPLE BUFFER)", MAX_FRAMES_IN_FLIGHT);
}

// ── DESTRUCTOR ─────────────────────────────────────────────────────────────
VulkanSwapchainManager::~VulkanSwapchainManager()
{
    cleanup();
}

// ── CLEANUP (RAII) ─────────────────────────────────────────────────────────
void VulkanSwapchainManager::cleanup() noexcept
{
    LOG_INFO_CAT("Swapchain", "Manager cleanup initiated");
    cleanupSwapchain();

    for (auto& sem : imageAvailableSemaphores_) if (sem) vkDestroySemaphore(context_->device, sem, nullptr);
    for (auto& sem : renderFinishedSemaphores_) if (sem) vkDestroySemaphore(context_->device, sem, nullptr);
    for (auto& fence : inFlightFences_) if (fence) vkDestroyFence(context_->device, fence, nullptr);

    imageAvailableSemaphores_.clear();
    renderFinishedSemaphores_.clear();
    inFlightFences_.clear();

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(context_->instance, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    LOG_INFO_CAT("Swapchain", "Manager destroyed – all resources released");
}

// ── INITIALIZE SWAPCHAIN ───────────────────────────────────────────────────
void VulkanSwapchainManager::initializeSwapchain(int width, int height)
{
    LOG_INFO_CAT("Swapchain", "initializeSwapchain START: {}x{}", width, height);
    width_ = width; height_ = height;

    // Step 1: Capabilities
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_->physicalDevice, surface_, &caps),
             "surface capabilities");

    // Step 2: Format
    uint32_t fmtCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physicalDevice, surface_, &fmtCount, nullptr),
             "format count");
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physicalDevice, surface_, &fmtCount, formats.data()),
             "formats");

    VkSurfaceFormatKHR chosenFmt = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFmt = f;
            break;
        }
    }

    // Step 3: Present Mode (Developer Config)
    uint32_t pmCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physicalDevice, surface_, &pmCount, nullptr),
             "present mode count");
    std::vector<VkPresentModeKHR> presentModes(pmCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physicalDevice, surface_, &pmCount, presentModes.data()),
             "present modes");

    bool mailboxAvailable = false, immediateAvailable = false;
    for (const auto& pm : presentModes) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR)   mailboxAvailable = true;
        if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR) immediateAvailable = true;
    }

    VkPresentModeKHR chosenPM = VK_PRESENT_MODE_FIFO_KHR;
    std::string pmDesc = "FIFO (fallback)";

    if (SwapchainConfig::FORCE_VSYNC) {
        chosenPM = VK_PRESENT_MODE_FIFO_KHR;
        pmDesc = "FIFO (VSync, 60 FPS)";
    } else if (SwapchainConfig::DESIRED_PRESENT_MODE == VK_PRESENT_MODE_MAILBOX_KHR && mailboxAvailable) {
        chosenPM = VK_PRESENT_MODE_MAILBOX_KHR;
        pmDesc = "MAILBOX (uncapped, tear-free)";
    } else if (SwapchainConfig::DESIRED_PRESENT_MODE == VK_PRESENT_MODE_IMMEDIATE_KHR && immediateAvailable) {
        chosenPM = VK_PRESENT_MODE_IMMEDIATE_KHR;
        pmDesc = "IMMEDIATE (uncapped, may tear)";
    } else if (SwapchainConfig::DESIRED_PRESENT_MODE == VK_PRESENT_MODE_FIFO_KHR) {
        chosenPM = VK_PRESENT_MODE_FIFO_KHR;
        pmDesc = "FIFO (VSync)";
    } else if (mailboxAvailable) {
        chosenPM = VK_PRESENT_MODE_MAILBOX_KHR;
        pmDesc = "MAILBOX (fallback)";
    } else if (immediateAvailable) {
        chosenPM = VK_PRESENT_MODE_IMMEDIATE_KHR;
        pmDesc = "IMMEDIATE (fallback)";
    }

    LOG_INFO_CAT("Swapchain", "Selected present mode: {}", pmDesc);

    // Step 4: Extent
    VkExtent2D extent = {
        .width  = std::clamp(static_cast<uint32_t>(width_),  caps.minImageExtent.width,  caps.maxImageExtent.width),
        .height = std::clamp(static_cast<uint32_t>(height_), caps.minImageExtent.height, caps.maxImageExtent.height)
    };

    // Step 5: Image Count
    uint32_t imageCount = std::max(caps.minImageCount, 3u);
    if (SwapchainConfig::FORCE_TRIPLE_BUFFER && imageCount < 3) imageCount = 3;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

    // Step 6: Create Swapchain
    VkSwapchainCreateInfoKHR sci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface_,
        .minImageCount = imageCount,
        .imageFormat = chosenFmt.format,
        .imageColorSpace = chosenFmt.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = chosenPM,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    uint32_t qfi[] = { context_->graphicsQueueFamilyIndex, context_->presentQueueFamilyIndex };
    if (context_->graphicsQueueFamilyIndex != context_->presentQueueFamilyIndex) {
        sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices = qfi;
    } else {
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VK_CHECK(vkCreateSwapchainKHR(context_->device, &sci, nullptr, &swapchain_),
             "vkCreateSwapchainKHR");

    // Step 7: Retrieve Images
    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(context_->device, swapchain_, &imgCount, nullptr),
             "image count");
    swapchainImages_.resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(context_->device, swapchain_, &imgCount, swapchainImages_.data()),
             "images");

    swapchainImageFormat_ = chosenFmt.format;
    swapchainExtent_ = extent;

    // Step 8: Image Views
    swapchainImageViews_.resize(swapchainImages_.size());
    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkImageViewCreateInfo vci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainImageFormat_,
            .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &swapchainImageViews_[i]),
                 std::format("image view #{}", i));
    }

    logSwapchainInfo("CREATED");

    if (SwapchainConfig::LOG_FINAL_CONFIG) {
        LOG_INFO_CAT("Swapchain", "DEVELOPER CONFIG:");
        LOG_INFO_CAT("Swapchain", "  • Desired Mode : {}", [mode = SwapchainConfig::DESIRED_PRESENT_MODE]() -> std::string {
            switch (mode) {
                case VK_PRESENT_MODE_MAILBOX_KHR:   return "MAILBOX";
                case VK_PRESENT_MODE_IMMEDIATE_KHR:return "IMMEDIATE";
                case VK_PRESENT_MODE_FIFO_KHR:     return "FIFO";
                default:                           return "UNKNOWN";
            }
        }());
        LOG_INFO_CAT("Swapchain", "  • Force VSync  : {}", SwapchainConfig::FORCE_VSYNC ? "YES" : "NO");
        LOG_INFO_CAT("Swapchain", "  • Force Triple : {}", SwapchainConfig::FORCE_TRIPLE_BUFFER ? "YES" : "NO");
        LOG_INFO_CAT("Swapchain", "  • Final Mode   : {}", pmDesc);
        LOG_INFO_CAT("Swapchain", "  • Images       : {} {}", swapchainImages_.size(),
                     (swapchainImages_.size() >= 3 ? "(TRIPLE)" : "(DOUBLE)"));
    }
}

// ── RESIZE ─────────────────────────────────────────────────────────────────
void VulkanSwapchainManager::handleResize(int width, int height)
{
    int w = width, h = height;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    if (w == 0 || h == 0) return;

    LOG_INFO_CAT("Swapchain", "RESIZE → {}x{}", w, h);
    vkDeviceWaitIdle(context_->device);
    cleanupSwapchain();
    initializeSwapchain(w, h);
    logSwapchainInfo("RESIZED");
}

// ── CLEANUP SWAPCHAIN ─────────────────────────────────────────────────────
void VulkanSwapchainManager::cleanupSwapchain() noexcept
{
    LOG_INFO_CAT("Swapchain", "CLEANUP SWAPCHAIN");
    for (auto view : swapchainImageViews_) if (view) vkDestroyImageView(context_->device, view, nullptr);
    swapchainImageViews_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context_->device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    swapchainImages_.clear();
}

// ── CREATE SURFACE ────────────────────────────────────────────────────────
VkSurfaceKHR VulkanSwapchainManager::createSurface(SDL_Window* window)
{
    VkSurfaceKHR surf = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, context_->instance, nullptr, &surf)) {
        throw std::runtime_error(std::format("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError()));
    }
    LOG_INFO_CAT("Swapchain", "Surface created: 0x{:016x}", std::bit_cast<uint64_t>(surf));
    return surf;
}

// ── WAIT FOR FENCES ───────────────────────────────────────────────────────
void VulkanSwapchainManager::waitForInFlightFrames() const
{
    LOG_DEBUG_CAT("Swapchain", "waitForInFlightFrames()");
    VK_CHECK(vkWaitForFences(context_->device, MAX_FRAMES_IN_FLIGHT, inFlightFences_.data(), VK_TRUE, UINT64_MAX),
             "wait fences");
}

} // namespace VulkanRTX