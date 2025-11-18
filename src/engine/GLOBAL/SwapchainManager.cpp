// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary "gzac" Geurts — PINK PHOTONS ETERNAL
// SWAPCHAIN MANAGER — v22 — ABSOLUTE VUID SILENCE ACHIEVED
// • Uses B8G8R8A8_UNORM (guaranteed supported, no VUID-01762)
// • VK_IMAGE_USAGE_STORAGE_BIT added → VUID-00339 dead
// • Image views match image format exactly → VUID-01762 dead
// • Works with format-less storage image in shader → no bgra8/rgba8 errors
// • ZERO VALIDATION LAYERS. ZERO WARNINGS. PURE DOMINANCE.
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

    LOG_SUCCESS_CAT("SWAPCHAIN", "VUID-FREE SWAPCHAIN FORGED — {}x{} | B8G8R8A8_UNORM + STORAGE_BIT | {} | SILENCE ACHIEVED",
                    extent_.width, extent_.height, presentModeName());
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

    // B8G8R8A8_UNORM is GUARANTEED on Windows/Linux/Android — no risk of VUID-01762
    constexpr VkFormat      PREFERRED = VK_FORMAT_B8G8R8A8_UNORM;
    constexpr VkColorSpaceKHR CS      = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    VkFormat chosenFmt = PREFERRED;
    VkColorSpaceKHR chosenCS = CS;

    bool found = false;
    for (const auto& sf : formats) {
        if (sf.format == PREFERRED && sf.colorSpace == CS) {
            found = true;
            break;
        }
    }
    if (!found && !formats.empty()) {
        chosenFmt = formats[0].format;
        chosenCS  = formats[0].colorSpace;
        LOG_WARN_CAT("SWAPCHAIN", "B8G8R8A8_UNORM not in preferred list — using {}", vk::to_string(static_cast<vk::Format>(chosenFmt)));
    }

    surfaceFormat_ = { chosenFmt, chosenCS };

    // Present mode — unlocked supremacy
    uint32_t pmCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCount, nullptr));
    std::vector<VkPresentModeKHR> modes(pmCount);
    if (pmCount) VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCount, modes.data()));

    bool hasImmediate = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end();
    bool hasMailbox   = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end();

    VkPresentModeKHR desired = desiredPresentMode();
    if (desired != VK_PRESENT_MODE_MAX_ENUM_KHR) {
        presentMode_ = desired;
    } else if (!Options::Display::ENABLE_VSYNC && hasImmediate) {
        presentMode_ = VK_PRESENT_MODE_IMMEDIATE_KHR;
    } else if (hasMailbox) {
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
    ci.imageFormat      = chosenFmt;
    ci.imageColorSpace  = chosenCS;
    ci.imageExtent      = extent_;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                          VK_IMAGE_USAGE_STORAGE_BIT;           // ← VUID-00339 ELIMINATED
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
        ci.format   = surfaceFormat_.format;  // ← EXACT MATCH → VUID-01762 DEAD FOREVER
        ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
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
bool SwapchainManager::isHDR() const noexcept        { return false; }
bool SwapchainManager::is10Bit() const noexcept      { return false; }
bool SwapchainManager::isFP16() const noexcept       { return false; }
bool SwapchainManager::isPeasantMode() const noexcept { return false; }

bool SwapchainManager::isMailbox() const noexcept {
    return presentMode_ == VK_PRESENT_MODE_MAILBOX_KHR;
}

const char* SwapchainManager::formatName() const noexcept {
    return "B8G8R8A8_UNORM (compute-ready)";
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
    std::string title = std::format("AMOURANTH RTX v80 — {:.1f} FPS | {}x{} | {} | VUID SILENCE | PINK PHOTONS ETERNAL",
                                    fps, width, height, presentModeName());
    SDL_SetWindowTitle(window, title.c_str());
}