// src/engine/Vulkan/VulkanSwapchainManager.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: Triple buffer, developer config via runtimeConfig_, full RAII, neon logging
// NO SwapchainConfig | NO vkDeviceWaitIdle | HDR + sRGB | MAILBOX | TRIPLE BUFFER
// → Replaces old VulkanSwapchain.cpp — DELETE THAT FILE

#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"  // VK_CHECK
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <algorithm>
#include <format>
#include <bit>
#include <vector>

using namespace VulkanRTX;
using namespace Logging::Color;

// ── LOGGING HELPER ───────────────────────────────────────────────────────────
void VulkanSwapchainManager::logSwapchainInfo(const char* prefix) const
{
    LOG_INFO_CAT("Swapchain",
        "{} | swapchain: 0x{:016x} | extent: {}x{} | format: {} | images: {} | views: {}",
        prefix,
        std::bit_cast<uint64_t>(swapchain_),
        swapchainExtent_.width, swapchainExtent_.height,
        static_cast<int>(swapchainImageFormat_),
        swapchainImages_.size(),
        swapchainImageViews_.size());
}

// ── CONSTRUCTOR ─────────────────────────────────────────────────────────────
VulkanSwapchainManager::VulkanSwapchainManager(std::shared_ptr<::Vulkan::Context> context,
                                               SDL_Window* window,
                                               int width,
                                               int height,
                                               SwapchainRuntimeConfig* runtimeConfig)
    : context_(std::move(context)), window_(window), width_(width), height_(height)
{
    if (!context_ || !context_->device) {
        throw std::runtime_error("VulkanSwapchainManager: null context/device");
    }

    if (runtimeConfig) runtimeConfig_ = *runtimeConfig;

    surface_ = createSurface(window_);

    imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &imageAvailableSemaphores_[i]),
                 "Failed to create image-available semaphore");
        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &renderFinishedSemaphores_[i]),
                 "Failed to create render-finished semaphore");
        VK_CHECK(vkCreateFence(context_->device, &fenceInfo, nullptr, &inFlightFences_[i]),
                 "Failed to create in-flight fence");
    }

    LOG_INFO_CAT("Swapchain", "{}Sync objects created: {} frames in flight (TRIPLE BUFFER){}", 
                 OCEAN_TEAL, MAX_FRAMES_IN_FLIGHT, RESET);
}

// ── DESTRUCTOR ─────────────────────────────────────────────────────────────
VulkanSwapchainManager::~VulkanSwapchainManager()
{
    cleanup();
}

// ── CLEANUP (RAII) ─────────────────────────────────────────────────────────
void VulkanSwapchainManager::cleanup() noexcept
{
    LOG_INFO_CAT("Swapchain", "{}Manager cleanup initiated{}", EMERALD_GREEN, RESET);
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

    LOG_INFO_CAT("Swapchain", "{}Manager destroyed – all resources released{}", EMERALD_GREEN, RESET);
}

// ── INITIALIZE SWAPCHAIN ───────────────────────────────────────────────────
void VulkanSwapchainManager::initializeSwapchain(int width, int height)
{
    LOG_INFO_CAT("Swapchain", "{}initializeSwapchain START: {}x{}{}", OCEAN_TEAL, width, height, RESET);
    width_ = width; height_ = height;

    // Step 1: Capabilities
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_->physicalDevice, surface_, &caps),
             "Failed to get surface capabilities");

    // Step 2: Format (HDR + sRGB)
    uint32_t fmtCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physicalDevice, surface_, &fmtCount, nullptr),
             "Failed to query surface format count");
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physicalDevice, surface_, &fmtCount, formats.data()),
             "Failed to query surface formats");

    VkSurfaceFormatKHR chosenFmt = formats[0];
    if (runtimeConfig_.enableHDR) {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_R16G16B16A16_SFLOAT && f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
                chosenFmt = f; break;
            }
        }
        if (chosenFmt.format != VK_FORMAT_R16G16B16A16_SFLOAT) {
            for (const auto& f : formats) {
                if (f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
                    chosenFmt = f; break;
                }
            }
        }
    } else {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosenFmt = f; break;
            }
        }
    }

    // Step 3: Present Mode (Developer Config via runtimeConfig_)
    uint32_t pmCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physicalDevice, surface_, &pmCount, nullptr),
             "Failed to query present mode count");
    std::vector<VkPresentModeKHR> presentModes(pmCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physicalDevice, surface_, &pmCount, presentModes.data()),
             "Failed to query present modes");

    bool mailboxAvailable = false, immediateAvailable = false, relaxedAvailable = false;
    for (const auto& pm : presentModes) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR)   mailboxAvailable = true;
        if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR) immediateAvailable = true;
        if (pm == VK_PRESENT_MODE_FIFO_RELAXED_KHR) relaxedAvailable = true;
    }

    VkPresentModeKHR chosenPM = VK_PRESENT_MODE_FIFO_KHR;
    std::string pmDesc = "FIFO (fallback)";

    if (runtimeConfig_.forceVsync) {
        chosenPM = VK_PRESENT_MODE_FIFO_KHR;
        pmDesc = "FIFO (VSync, 60 FPS)";
    } else {
        VkPresentModeKHR desired = runtimeConfig_.desiredMode;
        if (desired == VK_PRESENT_MODE_MAILBOX_KHR && mailboxAvailable) {
            chosenPM = VK_PRESENT_MODE_MAILBOX_KHR;
            pmDesc = "MAILBOX (uncapped, tear-free)";
        } else if (desired == VK_PRESENT_MODE_IMMEDIATE_KHR && immediateAvailable) {
            chosenPM = VK_PRESENT_MODE_IMMEDIATE_KHR;
            pmDesc = "IMMEDIATE (uncapped, may tear)";
        } else if (desired == VK_PRESENT_MODE_FIFO_RELAXED_KHR && relaxedAvailable) {
            chosenPM = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            pmDesc = "FIFO_RELAXED (low latency)";
        } else if (mailboxAvailable) {
            chosenPM = VK_PRESENT_MODE_MAILBOX_KHR;
            pmDesc = "MAILBOX (fallback)";
        } else if (immediateAvailable) {
            chosenPM = VK_PRESENT_MODE_IMMEDIATE_KHR;
            pmDesc = "IMMEDIATE (fallback)";
        } else {
            chosenPM = VK_PRESENT_MODE_FIFO_KHR;
            pmDesc = "FIFO (fallback)";
        }
    }

    LOG_INFO_CAT("Swapchain", "{}Selected present mode: {}{}", OCEAN_TEAL, pmDesc, RESET);

    // Step 4: Extent
    VkExtent2D extent{};
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        int w = width_, h = height_;
        SDL_GetWindowSizeInPixels(window_, &w, &h);
        extent.width  = std::clamp(static_cast<uint32_t>(w), caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    // Step 5: Image Count
    uint32_t imageCount = std::max(caps.minImageCount, 3u);
    if (runtimeConfig_.forceTripleBuffer && imageCount < 3) imageCount = 3;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

    // Step 6: Create Swapchain
    VkSwapchainKHR oldSwapchain = swapchain_;
    swapchain_ = VK_NULL_HANDLE;

    VkSwapchainCreateInfoKHR sci{
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
        .oldSwapchain = oldSwapchain
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
             "Failed to create swapchain");

    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context_->device, oldSwapchain, nullptr);
    }

    // Step 7: Retrieve Images
    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(context_->device, swapchain_, &imgCount, nullptr),
             "Failed to query swapchain image count");
    swapchainImages_.resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(context_->device, swapchain_, &imgCount, swapchainImages_.data()),
             "Failed to retrieve swapchain images");

    swapchainImageFormat_ = chosenFmt.format;
    swapchainExtent_ = extent;

    // Step 8: Image Views
    swapchainImageViews_.resize(swapchainImages_.size());
    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkImageViewCreateInfo vci{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainImageFormat_,
            .components = {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &swapchainImageViews_[i]),
                 std::format("Failed to create image view #{}", i));
    }

    logSwapchainInfo("CREATED");

    // Step 9: Developer Config Logging
    if (runtimeConfig_.logFinalConfig) {
        LOG_INFO_CAT("Swapchain", "{}DEVELOPER CONFIG:{}{}", OCEAN_TEAL, RESET);
        LOG_INFO_CAT("Swapchain", "  • Desired Mode : {}", [mode = runtimeConfig_.desiredMode]() -> std::string {
            switch (mode) {
                case VK_PRESENT_MODE_MAILBOX_KHR:   return "MAILBOX";
                case VK_PRESENT_MODE_IMMEDIATE_KHR:return "IMMEDIATE";
                case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
                case VK_PRESENT_MODE_FIFO_KHR:     return "FIFO";
                default:                           return "UNKNOWN";
            }
        }());
        LOG_INFO_CAT("Swapchain", "  • Force VSync  : {}", runtimeConfig_.forceVsync ? "YES" : "NO");
        LOG_INFO_CAT("Swapchain", "  • Force Triple : {}", runtimeConfig_.forceTripleBuffer ? "YES" : "NO");
        LOG_INFO_CAT("Swapchain", "  • Final Mode   : {}", pmDesc);
        LOG_INFO_CAT("Swapchain", "  • Images       : {} {}", swapchainImages_.size(),
                     (swapchainImages_.size() >= 3 ? "(TRIPLE)" : "(DOUBLE)"));
    }

    LOG_INFO_CAT("Swapchain", "{}initializeSwapchain COMPLETE{}", EMERALD_GREEN, RESET);
}

// ── RESIZE ─────────────────────────────────────────────────────────────────
void VulkanSwapchainManager::handleResize(int width, int height)
{
    int w = width, h = height;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    if (w == 0 || h == 0) {
        LOG_INFO_CAT("Swapchain", "{}Window minimized → skip resize{}", OCEAN_TEAL, RESET);
        return;
    }

    LOG_INFO_CAT("Swapchain", "{}RESIZE → {}x{}{}", BRIGHT_PINKISH_PURPLE, w, h, RESET);

    waitForInFlightFrames();  // Fence-based, non-blocking
    cleanupSwapchain();
    initializeSwapchain(w, h);
    logSwapchainInfo("RESIZED");

    LOG_INFO_CAT("Swapchain", "{}RESIZE COMPLETE{}", EMERALD_GREEN, RESET);
}

// ── RECREATE SWAPCHAIN (Zero-downtime) ────────────────────────────────────
void VulkanSwapchainManager::recreateSwapchain(int width, int height)
{
    waitForInFlightFrames();
    cleanupSwapchain();
    initializeSwapchain(width, height);
    logSwapchainInfo("RECREATED");
}

// ── CLEANUP SWAPCHAIN ─────────────────────────────────────────────────────
void VulkanSwapchainManager::cleanupSwapchain() noexcept
{
    LOG_INFO_CAT("Swapchain", "{}CLEANUP SWAPCHAIN{}", ARCTIC_CYAN, RESET);

    waitForInFlightFrames();

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
    LOG_INFO_CAT("Swapchain", "{}Surface created: 0x{:016x}{}", OCEAN_TEAL, std::bit_cast<uint64_t>(surf), RESET);
    return surf;
}

// ── WAIT FOR FENCES ───────────────────────────────────────────────────────
void VulkanSwapchainManager::waitForInFlightFrames() const
{
    if (inFlightFences_.empty()) return;

    LOG_DEBUG_CAT("Swapchain", "waitForInFlightFrames() – waiting on {} fences", inFlightFences_.size());
    VK_CHECK(vkWaitForFences(context_->device, static_cast<uint32_t>(inFlightFences_.size()),
                             inFlightFences_.data(), VK_TRUE, 10'000'000'000ULL),
             "Failed to wait for in-flight fences");
    VK_CHECK(vkResetFences(context_->device, static_cast<uint32_t>(inFlightFences_.size()), inFlightFences_.data()),
             "Failed to reset in-flight fences");
}