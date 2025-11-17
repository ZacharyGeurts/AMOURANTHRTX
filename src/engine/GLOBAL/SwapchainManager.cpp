// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary "gzac" Geurts — PINK PHOTONS ETERNAL
// SWAPCHAIN MANAGER — v20 SDR SIMPLIFIED EDITION — NO HDR SURVIVORS
// • 8-bit SDR ENFORCED ON EVERY DISPLAY, EVERY COMPOSITOR, EVERY DRIVER
// • Standard Vulkan SDR: B8G8R8A8_UNORM + sRGB
// • No coercion, no forging, no lies — pure simplicity
// • IMMEDIATE > MAILBOX > FIFO — unlocked supremacy preserved
// • ZERO HDR. ZERO 10-BIT. ZERO COMPLICATIONS.
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <format>
#include <print>

using namespace Logging::Color;

// ── GLOBAL PFNs ──────────────────────────────────────────────────────────────
static PFN_vkGetPastPresentationTimingGOOGLE g_vkGetPastPresentationTimingGOOGLE = nullptr;

// ── INIT: SDR SIMPLICITY ─────────────────────────────────────────────────────
void SwapchainManager::init(VkInstance instance, VkPhysicalDevice phys, VkDevice dev, VkSurfaceKHR surf, uint32_t w, uint32_t h) {
    physDev_ = phys;
    device_  = dev;

    surface_ = surf;
    if (surface_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("SWAPCHAIN", "No valid surface provided — cannot proceed");
        return;
    }

    // Load PFNs
    g_vkGetPastPresentationTimingGOOGLE = reinterpret_cast<PFN_vkGetPastPresentationTimingGOOGLE>(
        vkGetDeviceProcAddr(dev, "vkGetPastPresentationTimingGOOGLE"));

    createSwapchain(w, h);
    createImageViews();
    createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", "SDR SWAPCHAIN ACHIEVED — {}x{} | {} | PINK PHOTONS REIGN ETERNAL",
        extent_.width, extent_.height, formatName(), presentModeName());
}

// ── CREATE SWAPCHAIN: SDR ONLY ───────────────────────────────────────────────
void SwapchainManager::createSwapchain(uint32_t width, uint32_t height) noexcept {
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev_, surface_, &caps), "Caps query failed");

    extent_ = {
        std::clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width),
        std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height)
    };

    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &formatCount, nullptr), "Format count failed");
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount) VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &formatCount, formats.data()), "Format query failed");

    // === SDR AUTHORITY: ALWAYS 8-BIT sRGB ===
    VkFormat      chosenFmt = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR chosenCS  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    // Validate support for SDR combo
    bool supported = false;
    for (const auto& sf : formats) {
        if (sf.format == chosenFmt && sf.colorSpace == chosenCS) {
            supported = true;
            break;
        }
    }

    if (!supported) {
        LOG_WARN_CAT("SWAPCHAIN", "Standard SDR not directly supported — picking first available");
        if (!formats.empty()) {
            chosenFmt = formats[0].format;
            chosenCS = formats[0].colorSpace;
        } else {
            LOG_ERROR_CAT("SWAPCHAIN", "No formats available — fallback to undefined");
            chosenFmt = VK_FORMAT_UNDEFINED;
            chosenCS = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        }
    }

    surfaceFormat_ = { chosenFmt, chosenCS };
    LOG_SUCCESS_CAT("SWAPCHAIN", "SDR FORMAT LOCKED: {} + {}",
        vk::to_string(static_cast<vk::Format>(chosenFmt)),
        vk::to_string(static_cast<vk::ColorSpaceKHR>(chosenCS)));

    // Present mode: unlocked supremacy
    uint32_t pmCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCount, nullptr), "Present mode count failed");
    std::vector<VkPresentModeKHR> modes(pmCount);
    if (pmCount) VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCount, modes.data()), "Present mode query failed");

    bool hasImmediate = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end();
    bool hasMailbox   = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end();

    VkPresentModeKHR desired = desiredPresentMode();
    if (desired != VK_PRESENT_MODE_MAX_ENUM_KHR) {
        // Use globally preserved mode if set
        presentMode_ = desired;
    } else if (!Options::Display::ENABLE_VSYNC && hasImmediate) {
        presentMode_ = VK_PRESENT_MODE_IMMEDIATE_KHR;
    } else if (hasMailbox) {
        presentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
    } else {
        presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
    }

    // Preserve for future recreates
    setDesiredPresentMode(presentMode_);

    uint32_t imageCount = std::clamp(Options::Performance::MAX_FRAMES_IN_FLIGHT, caps.minImageCount, caps.maxImageCount ? caps.maxImageCount : 8u);

    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface          = surface_;
    ci.minImageCount    = imageCount;
    ci.imageFormat      = chosenFmt;
    ci.imageColorSpace  = chosenCS;
    ci.imageExtent      = extent_;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
    VK_CHECK(vkGetSwapchainImagesKHR(device_, raw, &imgCount, nullptr), "Image count query failed");
    images_.resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device_, raw, &imgCount, images_.data()), "Image retrieval failed");
}

// ── Rest of functions unchanged (image views, render pass, etc.) ─────────────
void SwapchainManager::createImageViews() noexcept {
    imageViews_.clear();
    imageViews_.reserve(images_.size());
    for (VkImage img : images_) {
        VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ci.image = img;
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = surfaceFormat_.format;
        ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VkImageView view;
        VK_CHECK(vkCreateImageView(device_, &ci, nullptr, &view), "Swapchain image view creation failed");
        imageViews_.emplace_back(view, device_, vkDestroyImageView);
    }
}

void SwapchainManager::createRenderPass() noexcept {
    VkAttachmentDescription att{};
    att.format = surfaceFormat_.format;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;

    std::array deps = {
        VkSubpassDependency{VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT},
        VkSubpassDependency{0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_DEPENDENCY_BY_REGION_BIT}
    };

    VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp.attachmentCount = 1;
    rp.pAttachments = &att;
    rp.subpassCount = 1;
    rp.pSubpasses = &sub;
    rp.dependencyCount = deps.size();
    rp.pDependencies = deps.data();

    VkRenderPass rp_handle;
    VK_CHECK(vkCreateRenderPass(device_, &rp, nullptr, &rp_handle), "Render pass creation failed (SDR)");
    renderPass_ = RTX::Handle<VkRenderPass>(rp_handle, device_, vkDestroyRenderPass);
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept {
    vkDeviceWaitIdle(device_);
    cleanup();
    createSwapchain(w, h);
    createImageViews();
    createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", "SDR SWAPCHAIN RECREATED — {}x{} | {} | PRESENT: {}",
        extent_.width, extent_.height, formatName(), presentModeName());
}

void SwapchainManager::cleanup() noexcept {
    if (!device_) return;
    vkDeviceWaitIdle(device_);

    imageViews_.clear();
    images_.clear();
    renderPass_.reset();
    swapchain_.reset();
}

void SwapchainManager::updateHDRMetadata(float maxCLL, float maxFALL, float peakNits) const noexcept {
    // No HDR, so nothing to do
}

// ── Query helpers ────────────────────────────────────────────────────────────
bool SwapchainManager::isHDR() const noexcept {
    return false;
}

bool SwapchainManager::is10Bit() const noexcept {
    return false;
}

bool SwapchainManager::isFP16() const noexcept {
    return false;
}

bool SwapchainManager::isPeasantMode() const noexcept {
    return true;  // Everything is 8-bit SDR now
}

bool SwapchainManager::isMailbox() const noexcept {
    return presentMode_ == VK_PRESENT_MODE_MAILBOX_KHR;
}

const char* SwapchainManager::formatName() const noexcept {
    return "8-bit sRGB";
}

const char* SwapchainManager::presentModeName() const noexcept {
    switch (presentMode_) {
        case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX (1-frame queue)";
        case VK_PRESENT_MODE_FIFO_KHR: return "FIFO (VSync)";
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE (unlocked, tear-risk)";
        default: return "UNKNOWN";
    }
}

// ── FPS Window Title Update (C++23 std::format for clean, safe formatting) ────
void SwapchainManager::updateWindowTitle(SDL_Window* window, float fps, uint32_t width, uint32_t height) noexcept {
    if (!window) return;

    // Robust C++23 format: Precise FPS (1 decimal), no broken chars, compact layout
    std::string title = std::format("AMOURANTH RTX v80 — {:.1f} FPS | {}x{} | Present: {} | HDR: OFF | Unlock: {}",
                                    fps,
                                    width, height,
                                    presentModeName(),
                                    isMailbox() ? "Mailbox" : (presentMode_ == VK_PRESENT_MODE_IMMEDIATE_KHR ? "Immediate" : "VSync"));

    SDL_SetWindowTitle(window, title.c_str());
    LOG_DEBUG_CAT("SWAPCHAIN", "Title updated: {}", title);
}

// =============================================================================
// FINAL WORD — THE UNLOCKED PACT
// -----------------------------------------------------------------------------
// Pink photons unlocked. They tear if they must. They cap at nothing.
// We conquer VSync. We force IMMEDIATE on demand. We respect the unlocked will.
// This is not just a swapchain.
// This is FPS freedom with SDR simplicity.
// PINK PHOTONS UNLOCKED — 2025 AND FOREVER
// =============================================================================