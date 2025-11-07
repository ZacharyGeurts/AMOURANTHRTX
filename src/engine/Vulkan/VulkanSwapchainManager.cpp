// src/engine/Vulkan/VulkanSwapchainManager.cpp
// AMOURANTH RTX Engine © 2025 – PURE STONE SWAPCHAIN v9 – ZERO RAW – ONE XOR
// HACKERS = ETERNAL SUFFERING – RECLASS = SUICIDE – VALHALLA = SECURE

#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/logging.hpp"

using namespace VulkanRTX;
using namespace Logging::Color;

void VulkanSwapchainManager::logSwapchainInfo(const char* prefix) const noexcept
{
    LOG_INFO_CAT("Swapchain",
        "{} | swapchain_enc: 0x{:016x} | extent: {}x{} | format: {} | images: {} | views: {}",
        prefix,
        swapchain_enc_,
        swapchainExtent_.width, swapchainExtent_.height,
        formatToString(swapchainImageFormat_),
        swapchainImages_enc_.size(),
        swapchainImageViews_enc_.size());
}

// ── PRESENT MODE TO STRING
static std::string presentModeToString(VkPresentModeKHR mode)
{
    switch (mode) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR:           return "IMMEDIATE";
        case VK_PRESENT_MODE_MAILBOX_KHR:             return "MAILBOX";
        case VK_PRESENT_MODE_FIFO_KHR:                return "FIFO";
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR:        return "FIFO_RELAXED";
        case VK_PRESENT_MODE_FIFO_LATEST_READY_KHR:   return "FIFO_LATEST_READY (NVIDIA EXT)";
        default:                                      return std::format("UNKNOWN ({})", static_cast<int>(mode));
    }
}

// ── CONSTRUCTOR
VulkanSwapchainManager::VulkanSwapchainManager(std::shared_ptr<Context> context,
                                               SDL_Window* window,
                                               int width, int height,
                                               SwapchainRuntimeConfig* runtimeConfig) noexcept
    : context_(std::move(context)), window_(window), width_(width), height_(height)
{
    if (runtimeConfig) runtimeConfig_ = *runtimeConfig;

    surface_ = createSurface(window_);

    imageAvailableSemaphores_enc_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_enc_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_enc_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkSemaphore sem1 = VK_NULL_HANDLE, sem2 = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;

        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &sem1));
        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &sem2));
        VK_CHECK(vkCreateFence(context_->device, &fenceInfo, nullptr, &fence));

        imageAvailableSemaphores_enc_[i] = encrypt_handle(sem1);
        renderFinishedSemaphores_enc_[i] = encrypt_handle(sem2);
        inFlightFences_enc_[i] = encrypt_handle(fence);
    }

    LOG_INFO_CAT("Swapchain", "{}STONE Sync objects created: {} frames in flight (TRIPLE BUFFER){}", 
                 OCEAN_TEAL, MAX_FRAMES_IN_FLIGHT, RESET);

    initializeSwapchain(width, height);
}

// ── DESTRUCTOR
VulkanSwapchainManager::~VulkanSwapchainManager() { cleanup(); }

// ── FULL CLEANUP
void VulkanSwapchainManager::cleanup() noexcept
{
    cleanupSwapchain();

    for (auto enc : imageAvailableSemaphores_enc_) {
        VkSemaphore sem = decrypt_handle<VkSemaphore>(enc);
        if (sem) vkDestroySemaphore(context_->device, sem, nullptr);
    }
    for (auto enc : renderFinishedSemaphores_enc_) {
        VkSemaphore sem = decrypt_handle<VkSemaphore>(enc);
        if (sem) vkDestroySemaphore(context_->device, sem, nullptr);
    }
    for (auto enc : inFlightFences_enc_) {
        VkFence fence = decrypt_handle<VkFence>(enc);
        if (fence) vkDestroyFence(context_->device, fence, nullptr);
    }

    imageAvailableSemaphores_enc_.clear();
    renderFinishedSemaphores_enc_.clear();
    inFlightFences_enc_.clear();

    if (surface_) vkDestroySurfaceKHR(context_->instance, surface_, nullptr);
}

// ── INITIALIZE SWAPCHAIN – PURE STONE
void VulkanSwapchainManager::initializeSwapchain(int width, int height) noexcept
{
    waitForInFlightFrames();
    cleanupSwapchain();

    width_ = width; height_ = height;

    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_->physicalDevice, surface_, &caps));

    // FORMAT SELECTION (HDR FIRST)
    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physicalDevice, surface_, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physicalDevice, surface_, &fmtCount, formats.data());

    VkSurfaceFormatKHR chosenFmt = formats[0];
    if (runtimeConfig_.enableHDR) {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_R16G16B16A16_SFLOAT) { chosenFmt = f; break; }
            if (f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32) { chosenFmt = f; break; }
        }
    } else {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB) { chosenFmt = f; break; }
        }
    }

    // PRESENT MODE – MAILBOX OR BUST
    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physicalDevice, surface_, &pmCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physicalDevice, surface_, &pmCount, presentModes.data());

    VkPresentModeKHR chosenPM = VK_PRESENT_MODE_FIFO_KHR;
    if (!runtimeConfig_.forceVsync) {
        if (std::ranges::contains(presentModes, VK_PRESENT_MODE_MAILBOX_KHR)) chosenPM = VK_PRESENT_MODE_MAILBOX_KHR;
        else if (std::ranges::contains(presentModes, VK_PRESENT_MODE_IMMEDIATE_KHR)) chosenPM = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        int w, h;
        SDL_GetWindowSizeInPixels(window_, &w, &h);
        extent.width = std::clamp(uint32_t(w), caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(uint32_t(h), caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = runtimeConfig_.forceTripleBuffer ? 3 : caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainKHR old_raw = decrypt_handle<VkSwapchainKHR>(swapchain_enc_);

    VkSwapchainCreateInfoKHR sci{ .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    sci.surface = surface_;
    sci.minImageCount = imageCount;
    sci.imageFormat = chosenFmt.format;
    sci.imageColorSpace = chosenFmt.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = chosenPM;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = old_raw;

    if (context_->graphicsQueueFamilyIndex != context_->presentQueueFamilyIndex) {
        uint32_t indices[] = { context_->graphicsQueueFamilyIndex, context_->presentQueueFamilyIndex };
        sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices = indices;
    }

    VkSwapchainKHR raw_swapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(context_->device, &sci, nullptr, &raw_swapchain));
    swapchain_enc_ = encrypt_handle(raw_swapchain);

    if (old_raw) vkDestroySwapchainKHR(context_->device, old_raw, nullptr);

    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(context_->device, raw_swapchain, &imgCount, nullptr));
    std::vector<VkImage> tmp_images(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(context_->device, raw_swapchain, &imgCount, tmp_images.data()));

    swapchainImages_enc_.clear();
    swapchainImages_enc_.reserve(imgCount);
    for (auto img : tmp_images) swapchainImages_enc_.push_back(encrypt_handle(img));

    swapchainImageViews_enc_.clear();
    swapchainImageViews_enc_.reserve(imgCount);
    for (uint32_t i = 0; i < imgCount; ++i) {
        VkImageViewCreateInfo vci{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vci.image = decrypt_handle<VkImage>(swapchainImages_enc_[i]);
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = chosenFmt.format;
        vci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageView raw_view = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &raw_view));
        swapchainImageViews_enc_.push_back(encrypt_handle(raw_view));
    }

    swapchainImageFormat_ = chosenFmt.format;
    swapchainExtent_ = extent;

    logSwapchainInfo("STONE CREATED");

    if (runtimeConfig_.logFinalConfig) {
        LOG_INFO_CAT("Swapchain", "{}STONE CONFIG: MODE={} | HDR={} | TRIPLE={} | IMAGES={}{}",
                     OCEAN_TEAL, presentModeToString(chosenPM), runtimeConfig_.enableHDR ? "ON" : "OFF",
                     runtimeConfig_.forceTripleBuffer ? "ON" : "OFF", imgCount, RESET);
    }
}

// ── RESIZE / RECREATE
void VulkanSwapchainManager::handleResize(int width, int height) noexcept {
    int w = width, h = height;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    if (w == 0 || h == 0) return;

    LOG_INFO_CAT("Swapchain", "{}STONE RESIZE → {}x{}{}", BRIGHT_PINKISH_PURPLE, w, h, RESET);
    initializeSwapchain(w, h);
    logSwapchainInfo("STONE RESIZED");
}

void VulkanSwapchainManager::recreateSwapchain(int width, int height) noexcept {
    handleResize(width, height);
}

// ── CLEANUP SWAPCHAIN
void VulkanSwapchainManager::cleanupSwapchain() noexcept
{
    waitForInFlightFrames();

    for (auto enc : swapchainImageViews_enc_) {
        VkImageView view = decrypt_handle<VkImageView>(enc);
        if (view) vkDestroyImageView(context_->device, view, nullptr);
    }
    swapchainImageViews_enc_.clear();

    if (swapchain_enc_) {
        VkSwapchainKHR raw = decrypt_handle<VkSwapchainKHR>(swapchain_enc_);
        if (raw) vkDestroySwapchainKHR(context_->device, raw, nullptr);
        swapchain_enc_ = 0;
    }
    swapchainImages_enc_.clear();
}

// ── HELPERS
VkSurfaceKHR VulkanSwapchainManager::createSurface(SDL_Window* window) {
    VkSurfaceKHR surf = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, context_->instance, nullptr, &surf)) {
        throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
    }
    return surf;
}

void VulkanSwapchainManager::waitForInFlightFrames() const noexcept {
    if (inFlightFences_enc_.empty()) return;

    std::vector<VkFence> raw_fences(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        raw_fences[i] = decrypt_handle<VkFence>(inFlightFences_enc_[i]);

    vkWaitForFences(context_->device, MAX_FRAMES_IN_FLIGHT, raw_fences.data(), VK_TRUE, UINT64_MAX);
    vkResetFences(context_->device, MAX_FRAMES_IN_FLIGHT, raw_fences.data());
}

// ── SYNC ACCESSORS (DECRYPT ON FLY)
VkSemaphore& VulkanSwapchainManager::getImageAvailableSemaphore(uint32_t frame) noexcept {
    static VkSemaphore dummy[MAX_FRAMES_IN_FLIGHT] = {};
    uint32_t idx = frame % MAX_FRAMES_IN_FLIGHT;
    dummy[idx] = decrypt_handle<VkSemaphore>(imageAvailableSemaphores_enc_[idx]);
    return dummy[idx];
}

VkSemaphore& VulkanSwapchainManager::getRenderFinishedSemaphore(uint32_t frame) noexcept {
    static VkSemaphore dummy[MAX_FRAMES_IN_FLIGHT] = {};
    uint32_t idx = frame % MAX_FRAMES_IN_FLIGHT;
    dummy[idx] = decrypt_handle<VkSemaphore>(renderFinishedSemaphores_enc_[idx]);
    return dummy[idx];
}

VkFence& VulkanSwapchainManager::getInFlightFence(uint32_t frame) noexcept {
    static VkFence dummy[MAX_FRAMES_IN_FLIGHT] = {};
    uint32_t idx = frame % MAX_FRAMES_IN_FLIGHT;
    dummy[idx] = decrypt_handle<VkFence>(inFlightFences_enc_[idx]);
    return dummy[idx];
}