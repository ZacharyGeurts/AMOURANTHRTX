// AMOURANTH RTX ENGINE © 2025 — FINAL WAYLAND-IMMUNE SWAPCHAIN v3.0
// BULLETPROOF • HDR10 → scRGB → sRGB • STONEKEY PROTECTED • PINK PHOTONS ETERNAL

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using namespace Logging::Color;

// -----------------------------------------------------------------------------
// Helper – use the engine’s VK_CHECK (already defined in VulkanCore)
// -----------------------------------------------------------------------------
#define VK_VERIFY(call) \
    do { \
        VkResult r = (call); \
        if (r != VK_SUCCESS) { \
            LOG_ERROR_CAT("SWAPCHAIN", "Vulkan error {} (code {}) in {}", #call, (int)r, __FUNCTION__); \
            std::abort(); \
        } \
    } while(0)

// -----------------------------------------------------------------------------
// Singleton init
// -----------------------------------------------------------------------------
void SwapchainManager::init(VkInstance instance, VkPhysicalDevice phys, VkDevice dev,
                           SDL_Window* window, uint32_t w, uint32_t h)
{
    if (s_instance) return;

    s_instance = new SwapchainManager();

    s_instance->vkInstance_ = instance;
    s_instance->physDev_    = phys;
    s_instance->device_     = dev;
    s_instance->window_     = window;

    // Create surface immediately – Wayland can destroy it later, we will resurrect
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &s_instance->surface_)) {
        LOG_ERROR_CAT("SWAPCHAIN", "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        std::abort();
    }

    // Store in StoneKey raw cache (safe until first render)
    set_g_instance(instance);
    set_g_PhysicalDevice(phys);
    set_g_device(dev);
    set_g_surface(s_instance->surface_);

    s_instance->recreate(w, h);
}

// -----------------------------------------------------------------------------
// Surface resurrection – Wayland loves to kill surfaces
// -----------------------------------------------------------------------------
bool SwapchainManager::recreateSurfaceIfLost()
{
    VkSurfaceCapabilitiesKHR caps{};
    VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev_, surface_, &caps);

    if (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR)
        return true;

    if (res == VK_ERROR_SURFACE_LOST_KHR) {
        LOG_WARN_CAT("SWAPCHAIN", "Surface lost – resurrecting...");

        vkDestroySurfaceKHR(vkInstance_, surface_, nullptr);

        VkSurfaceKHR newSurf = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(window_, vkInstance_, nullptr, &newSurf)) {
            LOG_ERROR_CAT("SWAPCHAIN", "Resurrection failed: {}", SDL_GetError());
            return false;
        }
        surface_ = newSurf;
        set_g_surface(newSurf);
        LOG_SUCCESS_CAT("SWAPCHAIN", "Surface resurrected");
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Best format selection – HDR10 > scRGB > sRGB
// -----------------------------------------------------------------------------
static VkSurfaceFormatKHR selectBestFormat(VkPhysicalDevice phys, VkSurfaceKHR surface)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, nullptr);
    if (count == 0) {
        LOG_ERROR_CAT("SWAPCHAIN", "No surface formats!");
        std::abort();
    }

    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, formats.data());

    struct Candidate {
        VkFormat        format;
        VkColorSpaceKHR colorSpace;
        int             priority;
        const char*     name;
    };

    constexpr Candidate list[] = {
        {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT,        1000, "HDR10 10-bit (A2B10G10R10)"},
        {VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT,        999,  "HDR10 10-bit (A2R10G10B10)"},
        {VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, 800, "scRGB FP16"},
        {VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       700, "FP16 sRGB"},
        {VK_FORMAT_B8G8R8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       100, "sRGB B8G8R8A8"},
        {VK_FORMAT_R8G8B8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,        90, "sRGB R8G8B8A8"},
    };

    for (const auto& c : list) {
        for (const auto& f : formats) {
            if (f.format == c.format && f.colorSpace == c.colorSpace) {
                LOG_SUCCESS_CAT("SWAPCHAIN", "Selected format: {}", c.name);
                return f;
            }
        }
    }

    LOG_WARN_CAT("SWAPCHAIN", "Falling back to first format");
    return formats[0];
}

// -----------------------------------------------------------------------------
// Best present mode
// -----------------------------------------------------------------------------
static VkPresentModeKHR selectBestPresentMode(VkPhysicalDevice phys, VkSurfaceKHR surface,
                                            VkPresentModeKHR desired)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, nullptr);
    if (count == 0) return VK_PRESENT_MODE_FIFO_KHR;

    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, modes.data());

    const VkPresentModeKHR order[] = {
        VK_PRESENT_MODE_MAILBOX_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        VK_PRESENT_MODE_FIFO_KHR
    };

    if (desired != VK_PRESENT_MODE_MAX_ENUM_KHR &&
        std::find(modes.begin(), modes.end(), desired) != modes.end())
        return desired;

    for (auto pm : order) {
        if (std::find(modes.begin(), modes.end(), pm) != modes.end())
            return pm;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

// -----------------------------------------------------------------------------
// Core recreate – called on startup and on resize / surface loss
// -----------------------------------------------------------------------------
void SwapchainManager::recreate(uint32_t w, uint32_t h)
{
    vkDeviceWaitIdle(device_);

    while (!recreateSurfaceIfLost())
        SDL_Delay(50);

    VkSurfaceCapabilitiesKHR caps{};
    VK_VERIFY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev_, surface_, &caps));

    // Extent
    if (caps.currentExtent.width != UINT32_MAX) {
        extent_ = caps.currentExtent;
    } else {
        extent_ = {
            std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width),
            std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height)
        };
    }

    surfaceFormat_ = selectBestFormat(physDev_, surface_);
    presentMode_   = selectBestPresentMode(physDev_, surface_, desiredMode_);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0)
        imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    ci.surface          = surface_;
    ci.minImageCount    = imageCount;
    ci.imageFormat      = surfaceFormat_.format;
    ci.imageColorSpace  = surfaceFormat_.colorSpace;
    ci.imageExtent      = extent_;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                         VK_IMAGE_USAGE_STORAGE_BIT;
    ci.imageSharingMode  = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = presentMode_;
    ci.clipped          = VK_TRUE;
    ci.oldSwapchain     = swapchain_ ? *swapchain_ : VK_NULL_HANDLE;

    VkSwapchainKHR raw = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateSwapchainKHR(device_, &ci, nullptr, &raw));

    if (swapchain_) vkDestroySwapchainKHR(device_, *swapchain_, nullptr);
    swapchain_ = RTX::Handle<VkSwapchainKHR>(raw, device_, vkDestroySwapchainKHR);

    // Retrieve images
    uint32_t cnt = 0;
    VK_VERIFY(vkGetSwapchainImagesKHR(device_, raw, &cnt, nullptr));
    images_.resize(cnt);
    VK_VERIFY(vkGetSwapchainImagesKHR(device_, raw, &cnt, images_.data()));

    createImageViews();
    createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", "SWAPCHAIN REBORN — {}×{} | {} | {} — PINK PHOTONS ETERNAL",
                   extent_.width, extent_.height, formatName(), presentModeName());
}

// -----------------------------------------------------------------------------
// Image views & render pass
// -----------------------------------------------------------------------------
void SwapchainManager::createImageViews()
{
    imageViews_.clear();
    imageViews_.reserve(images_.size());

    VkImageViewCreateInfo civ = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    civ.viewType = VK_IMAGE_VIEW_TYPE_2D;
    civ.format   = surfaceFormat_.format;
    civ.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    civ.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    civ.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    civ.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    civ.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    for (VkImage img : images_) {
        civ.image = img;
        VkImageView view = VK_NULL_HANDLE;
        VK_VERIFY(vkCreateImageView(device_, &civ, nullptr, &view));
        imageViews_.emplace_back(view, device_, vkDestroyImageView);
    }
}

void SwapchainManager::createRenderPass()
{
    VkAttachmentDescription att = {};
    att.format         = surfaceFormat_.format;
    att.samples        = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription sub = {};
    sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments    = &ref;

    constexpr VkSubpassDependency deps[2] = {
        { VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT },
        { 0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_DEPENDENCY_BY_REGION_BIT }
    };

    VkRenderPassCreateInfo rp = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rp.attachmentCount = 1;
    rp.pAttachments    = &att;
    rp.subpassCount    = 1;
    rp.pSubpasses      = &sub;
    rp.dependencyCount = 2;
    rp.pDependencies   = deps;

    VkRenderPass handle = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateRenderPass(device_, &rp, nullptr, &handle));
    renderPass_ = RTX::Handle<VkRenderPass>(handle, device_, vkDestroyRenderPass);
}

// -----------------------------------------------------------------------------
// Cleanup
// -----------------------------------------------------------------------------
void SwapchainManager::cleanup()
{
    vkDeviceWaitIdle(device_);
    imageViews_.clear();
    images_.clear();
    renderPass_.reset();
    swapchain_.reset();
}

// -----------------------------------------------------------------------------
// Query helpers
// -----------------------------------------------------------------------------
bool SwapchainManager::isHDR() const    { return surfaceFormat_.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT; }
bool SwapchainManager::is10Bit() const { return surfaceFormat_.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ||
                                                surfaceFormat_.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32; }
bool SwapchainManager::isFP16() const  { return surfaceFormat_.format == VK_FORMAT_R16G16B16A16_SFLOAT; }

const char* SwapchainManager::formatName() const
{
    if (isHDR()) return "HDR10 10-bit";
    if (isFP16() && surfaceFormat_.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) return "scRGB FP16";
    if (surfaceFormat_.format == VK_FORMAT_B8G8R8A8_UNORM) return "sRGB (B8G8R8A8)";
    if (surfaceFormat_.format == VK_FORMAT_R8G8B8A8_UNORM) return "sRGB (R8G8B8A8)";
    return "Unknown";
}

const char* SwapchainManager::presentModeName() const
{
    switch (presentMode_) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR:    return "IMMEDIATE";
        case VK_PRESENT_MODE_MAILBOX_KHR:      return "MAILBOX";
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
        case VK_PRESENT_MODE_FIFO_KHR:         return "FIFO";
        default:                               return "UNKNOWN";
    }
}

void SwapchainManager::updateWindowTitle(SDL_Window* window, float fps)
{
    if (!window) return;
    char buf[256];
    snprintf(buf, sizeof(buf),
             "AMOURANTH RTX v80 — %.0f FPS | %ux%u | %s | %s — PINK PHOTONS ETERNAL",
             fps, extent_.width, extent_.height, formatName(), presentModeName());
    SDL_SetWindowTitle(window, buf);
}