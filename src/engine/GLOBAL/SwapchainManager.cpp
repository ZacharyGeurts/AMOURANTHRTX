// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary "gzac" Geurts — PINK PHOTONS ETERNAL
// SWAPCHAIN MANAGER — v24 — RELAXED BEST-FORMAT EDITION — NOVEMBER 18 2025
// • Detects the absolute best format the surface supports (HDR10 → scRGB → sRGB)
// • No forced formats — pure detection, maximum compatibility
// • VK_IMAGE_USAGE_STORAGE_BIT preserved → VUID-00339 dead
// • Image views always match exactly → VUID-01762 dead
// • ZERO VALIDATION. ZERO SHAME. PURE DOMINANCE.
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <format>

using namespace Logging::Color;

static PFN_vkGetPastPresentationTimingGOOGLE g_vkGetPastPresentationTimingGOOGLE = nullptr;

void SwapchainManager::init(VkInstance instance, VkPhysicalDevice phys, VkDevice dev, VkSurfaceKHR surf, uint32_t w, uint32_t h) {
    physDev_ = phys;
    device_  = dev;
    surface_ = surf;

    if (surface_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("SWAPCHAIN", "No valid surface — aborting init");
        return;
    }

    g_vkGetPastPresentationTimingGOOGLE = reinterpret_cast<PFN_vkGetPastPresentationTimingGOOGLE>(
        vkGetDeviceProcAddr(dev, "vkGetPastPresentationTimingGOOGLE"));

    createSwapchain(w, h);
    createImageViews();
    createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", "VUID-FREE SWAPCHAIN FORGED — {}x{} | {} | {} images | {} | PINK PHOTONS ETERNAL",
                    extent_.width, extent_.height, formatName(), images_.size(), presentModeName());
}

void SwapchainManager::createSwapchain(uint32_t width, uint32_t height) noexcept {
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev_, surface_, &caps), "Surface caps failed");

    extent_ = {
        std::clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width),
        std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height)
    };

    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &formatCount, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount) VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &formatCount, formats.data()));

    // BEST FORMAT DETECTION — HDR10 → scRGB FP16 → sRGB 8-bit (R8 first)
    struct Candidate {
        VkFormat format;
        VkColorSpaceKHR colorSpace;
        const char* name;
        int priority;  // higher = better
    };

    constexpr std::array candidates = {
        Candidate{VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT,      "HDR10 10-bit",      100},
        Candidate{VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT,      "HDR10 Alt",         99},
        Candidate{VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, "scRGB FP16",      90},
        Candidate{VK_FORMAT_R8G8B8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       "sRGB 8-bit (R)",   50},
        Candidate{VK_FORMAT_R8G8B8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       "sRGB 8-bit (B)",   49}
    };

    VkSurfaceFormatKHR chosen = formats.empty()
        ? VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
        : formats[0];

    int bestPriority = -1;
    for (const auto& cand : candidates) {
        for (const auto& avail : formats) {
            if (avail.format == cand.format && avail.colorSpace == cand.colorSpace) {
                if (cand.priority > bestPriority) {
                    chosen = avail;
                    bestPriority = cand.priority;
                    LOG_SUCCESS_CAT("SWAPCHAIN", "SELECTED BEST FORMAT: {} — PINK PHOTONS ASCENDANT", cand.name);
                }
            }
        }
    }

    if (bestPriority == -1 && !formats.empty()) {
        chosen = formats[0];
        LOG_WARN_CAT("SWAPCHAIN", "No preferred format found — using fallback: {}", vk::to_string(static_cast<vk::Format>(chosen.format)));
    }

    surfaceFormat_ = chosen;

    // Present mode — unlocked supremacy
    uint32_t pmCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCount, nullptr));
    std::vector<VkPresentModeKHR> modes(pmCount);
    if (pmCount) VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCount, modes.data()));

    VkPresentModeKHR desired = desiredPresentMode();
    if (desired != VK_PRESENT_MODE_MAX_ENUM_KHR) {
        presentMode_ = desired;
    } else if (!Options::Display::ENABLE_VSYNC &&
               std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) {
        presentMode_ = VK_PRESENT_MODE_IMMEDIATE_KHR;
    } else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()) {
        presentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
    } else {
        presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
    }
    setDesiredPresentMode(presentMode_);

    uint32_t imageCount = std::clamp(Options::Performance::MAX_FRAMES_IN_FLIGHT,
                                     caps.minImageCount,
                                     caps.maxImageCount ? caps.maxImageCount : 8u);

    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface          = surface_;
    ci.minImageCount    = imageCount;
    ci.imageFormat      = chosen.format;
    ci.imageColorSpace  = chosen.colorSpace;
    ci.imageExtent      = extent_;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                          VK_IMAGE_USAGE_STORAGE_BIT;  // Required for tonemapping direct to swapchain
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = presentMode_;
    ci.clipped          = VK_TRUE;
    ci.oldSwapchain     = swapchain_ ? *swapchain_ : VK_NULL_HANDLE;

    VkSwapchainKHR raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(device_, &ci, nullptr, &raw), "Swapchain creation failed");

    if (swapchain_) vkDestroySwapchainKHR(device_, *swapchain_, nullptr);
    swapchain_ = RTX::Handle<VkSwapchainKHR>(raw, device_, vkDestroySwapchainKHR);

    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device_, raw, &imgCount, nullptr));
    images_.resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device_, raw, &imgCount, images_.data()));
}

void SwapchainManager::createImageViews() noexcept {
    imageViews_.clear();
    imageViews_.reserve(images_.size());

    for (VkImage img : images_) {
        VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ci.image    = img;
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format   = surfaceFormat_.format;
        ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageView view;
        VK_CHECK(vkCreateImageView(device_, &ci, nullptr, &view), "Swapchain image view failed");
        imageViews_.emplace_back(view, device_, vkDestroyImageView);
    }
}

void SwapchainManager::createRenderPass() noexcept {
    VkAttachmentDescription att{};
    att.format         = surfaceFormat_.format;
    att.samples        = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub{};
    sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments    = &ref;

    std::array deps = {
        VkSubpassDependency{VK_SUBPASS_EXTERNAL, 0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT},
        VkSubpassDependency{0, VK_SUBPASS_EXTERNAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_DEPENDENCY_BY_REGION_BIT}
    };

    VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp.attachmentCount = 1;
    rp.pAttachments    = &att;
    rp.subpassCount    = 1;
    rp.pSubpasses      = &sub;
    rp.dependencyCount = static_cast<uint32_t>(deps.size());
    rp.pDependencies   = deps.data();

    VkRenderPass rp_handle;
    VK_CHECK(vkCreateRenderPass(device_, &rp, nullptr, &rp_handle), "Render pass creation failed");
    renderPass_ = RTX::Handle<VkRenderPass>(rp_handle, device_, vkDestroyRenderPass);
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept {
    vkDeviceWaitIdle(device_);
    cleanup();
    createSwapchain(w, h);
    createImageViews();
    createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", "SWAPCHAIN RECREATED — ABSOLUTE SILENCE — {}x{}", extent_.width, extent_.height);
}

void SwapchainManager::cleanup() noexcept {
    if (!device_) return;
    vkDeviceWaitIdle(device_);

    imageViews_.clear();
    images_.clear();
    renderPass_.reset();
    swapchain_.reset();
}

void SwapchainManager::updateHDRMetadata(float, float, float) const noexcept {}
bool SwapchainManager::isHDR() const noexcept        { return surfaceFormat_.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT; }
bool SwapchainManager::is10Bit() const noexcept      { return surfaceFormat_.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ||
                                                       surfaceFormat_.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32; }
bool SwapchainManager::isFP16() const noexcept       { return surfaceFormat_.format == VK_FORMAT_R16G16B16A16_SFLOAT; }
bool SwapchainManager::isPeasantMode() const noexcept { return surfaceFormat_.format == VK_FORMAT_R8G8B8A8_UNORM ||
                                                       surfaceFormat_.format == VK_FORMAT_R8G8B8A8_UNORM; }

bool SwapchainManager::isMailbox() const noexcept {
    return presentMode_ == VK_PRESENT_MODE_MAILBOX_KHR;
}

const char* SwapchainManager::formatName() const noexcept {
    switch (surfaceFormat_.format) {
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return "HDR10 10-bit";
        case VK_FORMAT_R16G16B16A16_SFLOAT:      return "scRGB FP16";
        case VK_FORMAT_R8G8B8A8_UNORM:           return "sRGB 8-bit (B8G8R8A8)";
        default:                                 return "Unknown";
    }
}

const char* SwapchainManager::presentModeName() const noexcept {
    switch (presentMode_) {
        case VK_PRESENT_MODE_MAILBOX_KHR:   return "MAILBOX";
        case VK_PRESENT_MODE_FIFO_KHR:      return "FIFO";
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
        default:                            return "UNKNOWN";
    }
}

void SwapchainManager::updateWindowTitle(SDL_Window* window, float fps, uint32_t width, uint32_t height) noexcept {
    if (!window) return;
    std::string title = std::format("AMOURANTH RTX v80 — {:.1f} FPS | {}x{} | {} | {} | PINK PHOTONS ETERNAL",
                                    fps, width, height, formatName(), presentModeName());
    SDL_SetWindowTitle(window, title.c_str());
}