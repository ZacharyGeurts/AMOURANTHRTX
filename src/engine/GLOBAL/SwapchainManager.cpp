// AMOURANTH RTX ENGINE © 2025 — FINAL WAYLAND-IMMUNE SWAPCHAIN
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <algorithm>
#include <array>

using namespace Logging::Color;

#ifndef VK_CHECK
#define VK_CHECK(call) do { VkResult r = (call); if (r != VK_SUCCESS) { LOG_ERROR_CAT("VULKAN", "VK_CHECK failed: {} ({})", #call, (int)r); std::abort(); } } while(0)
#endif

void SwapchainManager::init(VkInstance instance, VkPhysicalDevice phys, VkDevice dev, SDL_Window* window, uint32_t w, uint32_t h)
{
    if (s_instance) return;
    s_instance = new SwapchainManager();

    s_instance->vkInstance_ = instance;
    s_instance->physDev_    = phys;
    s_instance->device_     = dev;
    s_instance->window_     = window;          // ← now valid
    s_instance->surface_    = VK_NULL_HANDLE;  // will be created in recreate()

    // Create surface from the window (standard SDL3 way)
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &s_instance->surface_)) {
        LOG_ERROR_CAT("SWAPCHAIN", "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        return;
    }

    s_instance->recreate(w, h);
}

bool SwapchainManager::recreateSurfaceIfLost()
{
    VkSurfaceCapabilitiesKHR caps{};
    VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev_, surface_, &caps);

    if (res == VK_ERROR_SURFACE_LOST_KHR || res == VK_ERROR_INITIALIZATION_FAILED) {
        LOG_WARN_CAT("SWAPCHAIN", "Surface lost — recreating from SDL window...");

        vkDestroySurfaceKHR(vkInstance_, surface_, nullptr);

        VkSurfaceKHR newSurf = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(window_, vkInstance_, nullptr, &newSurf)) {
            LOG_ERROR_CAT("SWAPCHAIN", "Failed to recreate surface: {}", SDL_GetError());
            return false;
        }
        surface_ = newSurf;
        LOG_SUCCESS_CAT("SWAPCHAIN", "Surface resurrected");
        return true;
    }
    return res == VK_SUCCESS;
}

void SwapchainManager::createSwapchain(uint32_t width, uint32_t height)
{
    while (!recreateSurfaceIfLost()) SDL_Delay(50);

    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev_, surface_, &caps));

    extent_ = (caps.currentExtent.width != UINT32_MAX)
        ? caps.currentExtent
        : VkExtent2D{ std::clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width),
                      std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height) };

    // === FORMAT SELECTION (HDR10 → scRGB → sRGB) ===
    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    if (fmtCount) vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &fmtCount, formats.data());

    struct Cand { VkFormat f; VkColorSpaceKHR cs; int p; const char* n; };
    const Cand cands[] = {
        {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT,        100, "HDR10"},
        {VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT,        99,  "HDR10"},
        {VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, 90, "scRGB"},
        {VK_FORMAT_B8G8R8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       60, "sRGB"},
        {VK_FORMAT_R8G8B8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       50, "sRGB"},
    };

    surfaceFormat_ = formats.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR} : formats[0];
    for (const auto& c : cands)
        for (const auto& f : formats)
            if (f.format == c.f && f.colorSpace == c.cs) {
                surfaceFormat_ = f;
                LOG_SUCCESS_CAT("SWAPCHAIN", "SELECTED: {}", c.n);
                goto fmt_done;
            }
fmt_done:

    // === PRESENT MODE ===
    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCount, nullptr);
    std::vector<VkPresentModeKHR> modes(pmCount);
    if (pmCount) vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCount, modes.data());

    presentMode_ = desiredMode_ != VK_PRESENT_MODE_MAX_ENUM_KHR ? desiredMode_ : VK_PRESENT_MODE_FIFO_KHR;
    if (presentMode_ == VK_PRESENT_MODE_MAX_ENUM_KHR || std::find(modes.begin(), modes.end(), presentMode_) == modes.end()) {
        if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end())
            presentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
        else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end())
            presentMode_ = VK_PRESENT_MODE_IMMEDIATE_KHR;
        else
            presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
    }

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount) imgCount = std::min(imgCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface          = surface_;
    ci.minImageCount    = imgCount;
    ci.imageFormat      = surfaceFormat_.format;
    ci.imageColorSpace  = surfaceFormat_.colorSpace;
    ci.imageExtent      = extent_;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = presentMode_;
    ci.clipped          = VK_TRUE;
    ci.oldSwapchain     = swapchain_ ? *swapchain_ : VK_NULL_HANDLE;

    VkSwapchainKHR raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(device_, &ci, nullptr, &raw));

    if (swapchain_) vkDestroySwapchainKHR(device_, *swapchain_, nullptr);
    swapchain_ = RTX::Handle<VkSwapchainKHR>(raw, device_, vkDestroySwapchainKHR);

    uint32_t count = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device_, raw, &count, nullptr));
    images_.resize(count);
    VK_CHECK(vkGetSwapchainImagesKHR(device_, raw, &count, images_.data()));
}

void SwapchainManager::createImageViews()
{
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

        VkImageView view = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImageView(device_, &ci, nullptr, &view));
        imageViews_.emplace_back(view, device_, vkDestroyImageView);
    }
}

void SwapchainManager::createRenderPass()
{
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

    VkRenderPass handle = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRenderPass(device_, &rp, nullptr, &handle));
    renderPass_ = RTX::Handle<VkRenderPass>(handle, device_, vkDestroyRenderPass);
}

void SwapchainManager::recreate(uint32_t w, uint32_t h)
{
    vkDeviceWaitIdle(device_);
    cleanup();
    createSwapchain(w, h);
    createImageViews();
    createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", "SWAPCHAIN REBORN — {}x{} | {} | {} — PINK PHOTONS ETERNAL",
                    extent_.width, extent_.height, formatName(), presentModeName());
}

void SwapchainManager::cleanup()
{
    vkDeviceWaitIdle(device_);
    imageViews_.clear();
    images_.clear();
    renderPass_.reset();
    swapchain_.reset();
}

bool SwapchainManager::isHDR() const    { return surfaceFormat_.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT; }
bool SwapchainManager::is10Bit() const { return surfaceFormat_.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || surfaceFormat_.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32; }
bool SwapchainManager::isFP16() const  { return surfaceFormat_.format == VK_FORMAT_R16G16B16A16_SFLOAT; }

const char* SwapchainManager::formatName() const
{
    switch (surfaceFormat_.format) {
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return "HDR10 10-bit";
        case VK_FORMAT_R16G16B16A16_SFLOAT:      return "scRGB FP16";
        case VK_FORMAT_B8G8R8A8_UNORM:           return "sRGB (B8G8R8A8)";
        case VK_FORMAT_R8G8B8A8_UNORM:           return "sRGB (R8G8B8A8)";
        default:                                 return "Unknown";
    }
}

const char* SwapchainManager::presentModeName() const
{
    switch (presentMode_) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE (UNLOCKED)";
        case VK_PRESENT_MODE_MAILBOX_KHR:   return "MAILBOX";
        case VK_PRESENT_MODE_FIFO_KHR:      return "FIFO";
        default:                            return "UNKNOWN";
    }
}

void SwapchainManager::updateWindowTitle(SDL_Window* window, float fps)
{
    if (!window) return;
    std::string title = std::format("AMOURANTH RTX v80 — {:.0f} FPS | {}x{} | {} | {} — PINK PHOTONS ETERNAL",
                                    fps, extent_.width, extent_.height, formatName(), presentModeName());
    SDL_SetWindowTitle(window, title.c_str());
}