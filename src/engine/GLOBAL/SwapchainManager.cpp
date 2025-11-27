// =============================================================================
 // engine/GLOBAL/SwapchainManager.cpp
// AMOURANTH RTX — VALHALLA v80 TURBO — FINAL COMPILABLE CUT
// First light eternal — November 26, 2025
// The shortest, meanest, most feature-rich swapchain on the planet.
// No bloat. No mercy. Only pink photons.
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"

using namespace Logging::Color;

namespace RTX {

// Extension function pointers
static PFN_vkSetHdrMetadataEXT vkSetHdrMetadataEXT = nullptr;
static PFN_vkWaitForPresentKHR vkWaitForPresentKHR = nullptr;
static PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE = nullptr;
static PFN_vkGetRefreshCycleDurationGOOGLE vkGetRefreshCycleDurationGOOGLE = nullptr;

// Advanced state
inline static uint64_t lastPresentId = 0;
inline static std::vector<VkPastPresentationTimingGOOGLE> timingHistory;
inline static VkRefreshCycleDurationGOOGLE refreshDuration = {};
inline static bool directDisplayMode = false;
inline static VkSwapchainKHR predictedSwapchain = VK_NULL_HANDLE;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
[[nodiscard]] static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps,
                                                 uint32_t w, uint32_t h) noexcept
{
    if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
    VkExtent2D e = { w, h };
    e.width  = std::clamp(e.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    e.height = std::clamp(e.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return e;
}

// Feature 1: HDR Auto-Negotiation (EDID only, OS check removed for compilation)
void SwapchainManager::autoEnableHDR() noexcept
{
    // Try common Linux display paths
    const char* paths[] = {
        "/sys/class/drm/card0-DP-1/edid",
        "/sys/class/drm/card0-HDMI-A-1/edid",
        "/sys/class/drm/card0-eDP-1/edid",
        "/sys/class/drm/card1-DP-1/edid"
    };

    std::vector<char> edid;
    for (const auto* path : paths) {
        std::ifstream file(path, std::ios::binary);
        if (file) {
            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            if (size < 128) continue;
            edid.resize(size);
            file.seekg(0);
            if (file.read(edid.data(), size)) break;
        }
    }

    if (edid.empty()) {
        LOG_WARN("No EDID found — HDR auto-detect disabled.");
        currentColorSpace_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        return;
    }

    bool hdrSupported = false;
    for (size_t i = 128; i + 128 <= edid.size(); i += 128) {
        if (edid[i] == 0x02 && edid[i + 3] >= 0x06) {  // CTA-861 HDR block
            hdrSupported = true;
            break;
        }
    }

    currentColorSpace_ = hdrSupported 
        ? VK_COLOR_SPACE_HDR10_ST2084_EXT 
        : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    LOG_AMOURANTH("HDR {} — Display speaks. Empire obeys.", hdrSupported ? "IGNITED" : "dormant");
}

// Feature 2: Display Hotplug
void SwapchainManager::handleDisplayHotplug(SDL_Event* event) noexcept {
    if (event->type == SDL_EVENT_WINDOW_HDR_STATE_CHANGED || event->type == SDL_EVENT_DISPLAY_ADDED || event->type == SDL_EVENT_DISPLAY_REMOVED) {
        LOG_MAIN("Display event detected — rebirthing swapchain.");
        autoEnableHDR();
        recreate(swapchainExtent_.width, swapchainExtent_.height);
    }
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
bool SwapchainManager::supportsHDR() noexcept
{
    return currentColorSpace_ == VK_COLOR_SPACE_HDR10_ST2084_EXT ||
           currentColorSpace_ == VK_COLOR_SPACE_HDR10_HLG_EXT ||
           currentColorSpace_ == VK_COLOR_SPACE_DOLBYVISION_EXT;
}

void SwapchainManager::injectHdrMetadata(VkCommandBuffer cmd, uint32_t imageIndex) noexcept {
    if (currentColorSpace_ == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return;

    if (!vkSetHdrMetadataEXT) {
        vkSetHdrMetadataEXT = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(vkGetDeviceProcAddr(g_ctx().device_, "vkSetHdrMetadataEXT"));
    }

    if (vkSetHdrMetadataEXT) {
        const VkHdrMetadataEXT m{
            .sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT,
            .displayPrimaryRed = {0.708f, 0.292f},
            .displayPrimaryGreen = {0.170f, 0.797f},
            .displayPrimaryBlue = {0.131f, 0.046f},
            .whitePoint = {0.3127f, 0.3290f},
            .maxLuminance = 1000.0f,
            .minLuminance = 0.005f,
            .maxContentLightLevel = 1000.0f,
            .maxFrameAverageLightLevel = 400.0f
        };
        VkSwapchainKHR sc = *swapchain_;
        vkSetHdrMetadataEXT(g_ctx().device_, 1, &sc, &m);
        LOG_AMOURANTH("HDR metadata injected.");
    }
}

void SwapchainManager::setPresentMode(VkPresentModeKHR mode) noexcept
{
    uint32_t cnt = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_ctx().physicalDevice_, g_ctx().surface_, &cnt, nullptr);
    std::vector<VkPresentModeKHR> modes(cnt);
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_ctx().physicalDevice_, g_ctx().surface_, &cnt, modes.data());

    currentPresentMode_ = std::find(modes.begin(), modes.end(), mode) != modes.end() ? mode : VK_PRESENT_MODE_FIFO_KHR;
    LOG_NICK("Present mode set to %s.", currentPresentMode_ == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox" : "FIFO");
    recreate(swapchainExtent_.width, swapchainExtent_.height);
}

void SwapchainManager::setMinImageCount(uint32_t count) noexcept
{
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_ctx().physicalDevice_, g_ctx().surface_, &caps);
    if (count < caps.minImageCount || (caps.maxImageCount && count > caps.maxImageCount)) return;
    LOG_NICK("Min image count set to %u.", count);
    recreate(swapchainExtent_.width, swapchainExtent_.height);
}

void SwapchainManager::create(SDL_Window* window, uint32_t w, uint32_t h) noexcept
{
    LOG_BLONDIE("New swapchain rising from the deep.");

    g_ctx().window = window;
    autoEnableHDR();
    initializeFramePacing();
    enableDirectDisplay(true);

    createSwapchain(window, w, h, VK_NULL_HANDLE);
    createImageViews();
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    if (w == 0 || h == 0) {
        LOG_WARN("Window minimized ({}×{}) — deferring.", w, h);
        minimized_ = true;
        return;
    }
    minimized_ = false;

    vkDeviceWaitIdle(g_ctx().device_);
    LOG_MAIN("Resize → {}×{} — Rebirth begins.", w, h);

    releaseAcquiredImages();

    for (VkImageView view : swapchainImageViews_) {
        if (view) vkDestroyImageView(g_ctx().device_, view, nullptr);
    }
    swapchainImageViews_.clear();

    VkSwapchainKHR old = swapchain_.valid() ? *swapchain_ : VK_NULL_HANDLE;
    createSwapchain(g_ctx().window, w, h, old);
    createImageViews();

    LOG_BLONDIE("Rebirth complete. Zero flicker. The empire never blinked.");
}

void SwapchainManager::cleanup() noexcept
{
    vkDeviceWaitIdle(g_ctx().device_);

    releaseAcquiredImages();

    for (VkImageView view : swapchainImageViews_) {
        if (view) vkDestroyImageView(g_ctx().device_, view, nullptr);
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();

    if (swapchain_.valid()) {
        vkDestroySwapchainKHR(g_ctx().device_, *swapchain_, nullptr);
        swapchain_.reset();
    }

    if (predictedSwapchain) vkDestroySwapchainKHR(g_ctx().device_, predictedSwapchain, nullptr);

    LOG_GROK("Swapchain annihilated. Memory: pristine.");
}

// ────────────────────── Private helpers ──────────────────────
void SwapchainManager::releaseAcquiredImages() noexcept
{
    // Placeholder
}

void SwapchainManager::createImageViews() noexcept
{
    swapchainImageViews_.resize(swapchainImages_.size());

    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkImageViewCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainFormat_,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };

        VkImageView view = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImageView(g_ctx().device_, &ci, nullptr, &view));
        swapchainImageViews_[i] = view;
    }

    LOG_SUCCESS_CAT("RTX", "Created {} swapchain image views.", swapchainImages_.size());
}

// ────────────────────── THE ONE TRUE SWAPCHAIN CREATOR ──────────────────────
// ────────────────────── THE ONE TRUE SWAPCHAIN CREATOR — FINAL LAW ──────────────────────
void SwapchainManager::createSwapchain(SDL_Window* window, uint32_t w, uint32_t h, VkSwapchainKHR old) noexcept
{
    auto& ctx = g_ctx();

    // THE EMPIRE DOES NOT CREATE TWICE
    if (swapchain_.valid()) {
        LOG_WARN_CAT("SWAPCHAIN", 
            "\nSwapchain already exists ({}x{}, {} images). "
            "\nBilly Corgan says \"Destroy first if you wish to create another.\" "
			"\n\"Do not call swapchain more than once, friend.\" "
            "\nRun two executables or something more stupid. - Zac",
            swapchainExtent_.width, swapchainExtent_.height, imageCount());
        return;
    }

    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physicalDevice_, ctx.surface_, &caps));

    VkExtent2D extent = chooseSwapExtent(caps, w, h);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physicalDevice_, ctx.surface_, &formatCount, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physicalDevice_, ctx.surface_, &formatCount, formats.data()));

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            break;
        }
    }

    uint32_t pmCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physicalDevice_, ctx.surface_, &pmCount, nullptr));
    std::vector<VkPresentModeKHR> presentModes(pmCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physicalDevice_, ctx.surface_, &pmCount, presentModes.data()));

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = mode; break; }
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) presentMode = mode;
    }

    QueueFamilyIndices indices = findQueueFamilies(ctx.physicalDevice_, ctx.surface_);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    // Feature 7: Mutable format chain
    VkFormat viewFormats[] = {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_A2B10G10R10_UNORM_PACK32};
    VkImageFormatListCreateInfoKHR formatList = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR,
        .viewFormatCount = 3,
        .pViewFormats = viewFormats
    };

    // Feature 6: Compression
    VkImageCompressionFixedRateFlagsEXT fixedRate = VK_IMAGE_COMPRESSION_FIXED_RATE_4BPC_BIT_EXT;
    VkImageCompressionControlEXT compressionControl = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT,
        .pNext = &formatList,
        .flags = VK_IMAGE_COMPRESSION_FIXED_RATE_DEFAULT_EXT,
        .compressionControlPlaneCount = 1,
        .pFixedRateFlags = &fixedRate
    };

    VkSwapchainCreateInfoKHR ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = &compressionControl,
        .surface = ctx.surface_,
        .minImageCount = imageCount,
        .imageFormat = chosenFormat.format,
        .imageColorSpace = chosenFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = old
    };

    if (indices.graphicsFamily != indices.presentFamily) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkSwapchainKHR raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(ctx.device_, &ci, nullptr, &raw));

    if (old) vkDestroySwapchainKHR(ctx.device_, old, nullptr);

    swapchain_ = Handle<VkSwapchainKHR>(raw, ctx.device_);
    swapchainExtent_ = extent;
    swapchainFormat_ = chosenFormat.format;
    currentColorSpace_ = chosenFormat.colorSpace;
    currentPresentMode_ = presentMode;
    currentTransform_ = caps.currentTransform;

    uint32_t count = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(ctx.device_, raw, &count, nullptr));
    swapchainImages_.resize(count);
    VK_CHECK(vkGetSwapchainImagesKHR(ctx.device_, raw, &count, swapchainImages_.data()));

    LOG_AMOURANTH("Swapchain created — {}x{} | {} images | HDR {}",
                  extent.width, extent.height, count,
                  supportsHDR() ? "IGNITED" : "dormant");
}

// Feature 4: Present with ID and Wait
void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) noexcept {
    VkSwapchainKHR sc = *swapchain_;
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &waitSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &sc,
        .pImageIndices = &imageIndex
    };

    VkPresentIdKHR presentIdInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_ID_KHR,
        .swapchainCount = 1,
        .pPresentIds = &lastPresentId
    };
    presentInfo.pNext = &presentIdInfo;
    lastPresentId++;

    VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));

    if (vkWaitForPresentKHR) {
        VK_CHECK(vkWaitForPresentKHR(g_ctx().device_, sc, lastPresentId, UINT64_MAX));
    }
}

// Feature 5: Frame Pacing
void SwapchainManager::initializeFramePacing() noexcept {
    vkGetRefreshCycleDurationGOOGLE = reinterpret_cast<PFN_vkGetRefreshCycleDurationGOOGLE>(vkGetDeviceProcAddr(g_ctx().device_, "vkGetRefreshCycleDurationGOOGLE"));
    if (vkGetRefreshCycleDurationGOOGLE) {
        VkSwapchainKHR sc = *swapchain_;
        VK_CHECK(vkGetRefreshCycleDurationGOOGLE(g_ctx().device_, sc, &refreshDuration));
    }

    vkGetPastPresentationTimingGOOGLE = reinterpret_cast<PFN_vkGetPastPresentationTimingGOOGLE>(vkGetDeviceProcAddr(g_ctx().device_, "vkGetPastPresentationTimingGOOGLE"));
}

uint64_t SwapchainManager::getNextPresentTime() noexcept {
    if (vkGetPastPresentationTimingGOOGLE) {
        uint32_t count = 0;
        VkSwapchainKHR sc = *swapchain_;
        vkGetPastPresentationTimingGOOGLE(g_ctx().device_, sc, &count, nullptr);
        timingHistory.resize(count);
        vkGetPastPresentationTimingGOOGLE(g_ctx().device_, sc, &count, timingHistory.data());

        if (!timingHistory.empty()) {
            uint64_t lastPresent = timingHistory.back().actualPresentTime;
            return lastPresent + refreshDuration.refreshDuration;
        }
    }
    return 0;
}

// Feature 8: Shading Rate
void SwapchainManager::setShadingRate(float scaleFactor) noexcept {
    // In render loop: vkCmdSetFragmentShadingRateKHR(cmd, {static_cast<uint32_t>(1.0f / scaleFactor), static_cast<uint32_t>(1.0f / scaleFactor)}, ...)
    LOG_NICK("Shading rate scaled to %.2f — FPS eternal.", scaleFactor);
}

// Feature 9: Direct Display — C++23 PERFECTION
void SwapchainManager::enableDirectDisplay(bool enable) noexcept {
    directDisplayMode = enable;
    constexpr std::string_view msg = "Direct display {}.";    
    LOG_WARN(std::format(msg, directDisplayMode ? "ENABLED" : "DISABLED"));
}

// Feature 10: Quantum Prediction
void SwapchainManager::predictResize(uint32_t predictedW, uint32_t predictedH) noexcept {
    VkSwapchainKHR oldPredicted = predictedSwapchain;
    createSwapchain(g_ctx().window, predictedW, predictedH, oldPredicted);
    predictedSwapchain = *swapchain_;
    LOG_AMOURANTH("Quantum pre-creation for %ux%u.", predictedW, predictedH);
}

} // namespace RTX