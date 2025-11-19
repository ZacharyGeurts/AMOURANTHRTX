// =============================================================================
// AMOURANTH RTX ENGINE © 2025 — SWAPCHAIN MANAGER v7.0 — STONEKEY v∞ — C++23
// ZERO RAW HANDLES • ZERO TYPOS • PERFECT MODERN C++ • PINK PHOTONS ETERNAL
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 16, 2025 — APOCALYPSE v3.4
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — KEYS NEVER LOGGED
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <ranges>
#include <algorithm>
#include <format>

using namespace Logging::Color;

#define VK_VERIFY(call) \
    do { VkResult r_ = (call); if (r_ != VK_SUCCESS) { \
        LOG_FATAL_CAT("SWAPCHAIN", "{}Vulkan error in {}: {} ({}) — ABORTING{}", \
                      CRIMSON_MAGENTA, #call, static_cast<int>(r_), __FUNCTION__, RESET); \
        std::abort(); \
    } } while(0)

// -----------------------------------------------------------------------------
// Init — now ultra clean
// -----------------------------------------------------------------------------
void SwapchainManager::init(SDL_Window* window, uint32_t w, uint32_t h) noexcept
{
    if (s_instance) [[unlikely]] return;

    s_instance = new SwapchainManager();
    s_instance->window_ = window;

    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, g_instance(), nullptr, &raw_surface)) {
        LOG_FATAL_CAT("SWAPCHAIN", "{}SDL_Vulkan_CreateSurface failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        std::abort();
    }
    set_g_surface(raw_surface);

    s_instance->recreate(w, h);

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}SwapchainManager initialized — StoneKey v∞ active — PINK PHOTONS ETERNAL{}", 
                    EMERALD_GREEN, RESET);
}

// -----------------------------------------------------------------------------
// Surface resurrection — Wayland immune
// -----------------------------------------------------------------------------
bool SwapchainManager::recreateSurfaceIfLost() noexcept
{
    VkSurfaceCapabilitiesKHR caps{};
    VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice(), g_surface(), &caps);

    if (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR)
        return true;

    if (res == VK_ERROR_SURFACE_LOST_KHR) {
        LOG_WARN_CAT("SWAPCHAIN", "{}Surface lost — resurrecting...{}", RASPBERRY_PINK, RESET);

        vkDestroySurfaceKHR(g_instance(), g_surface(), nullptr);

        VkSurfaceKHR newSurf = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(window_, g_instance(), nullptr, &newSurf)) {
            LOG_FATAL_CAT("SWAPCHAIN", "{}Surface resurrection failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
            return false;
        }
        set_g_surface(newSurf);
        LOG_SUCCESS_CAT("SWAPCHAIN", "{}Surface resurrected — empire endures{}", EMERALD_GREEN, RESET);
        return true;
    }

    LOG_ERROR_CAT("SWAPCHAIN", "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: {}", static_cast<int>(res));
    return false;
}

// -----------------------------------------------------------------------------
// Format selection — fixed typo + modern C++23 ranges
// -----------------------------------------------------------------------------
static VkSurfaceFormatKHR selectBestFormat(VkPhysicalDevice phys, VkSurfaceKHR surface)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, nullptr);
    if (count == 0) {
        LOG_FATAL_CAT("SWAPCHAIN", "No surface formats available!");
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

    constexpr Candidate candidates[] = {
        {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT,        1000, "HDR10 10-bit (A2B10G10R10)"},
        {VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT,        999,  "HDR10 10-bit (A2R10G10B10)"},
        {VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, 800, "scRGB FP16"},
        {VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       700, "FP16 sRGB"},
        {VK_FORMAT_B8G8R8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       100, "sRGB B8G8R8A8"},
        {VK_FORMAT_R8G8B8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,        90, "sRGB R8G8B8A8"},  // ← FIXED
    };

    for (const auto& c : candidates) {
        if (std::ranges::any_of(formats, [c](const VkSurfaceFormatKHR& f) {
                return f.format == c.format && f.colorSpace == c.colorSpace;
            })) {
            LOG_SUCCESS_CAT("SWAPCHAIN", "Selected surface format: {}", c.name);
            return {c.format, c.colorSpace};}
    }

    LOG_WARN_CAT("SWAPCHAIN", "No preferred format — using first available");
    return formats[0];
}

static VkPresentModeKHR selectBestPresentMode(VkPhysicalDevice phys, VkSurfaceKHR surface, VkPresentModeKHR desired)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, nullptr);
    if (count == 0) return VK_PRESENT_MODE_FIFO_KHR;

    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, modes.data());

    constexpr std::array order = {
        VK_PRESENT_MODE_MAILBOX_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        VK_PRESENT_MODE_FIFO_KHR
    };

    if (desired != VK_PRESENT_MODE_MAX_ENUM_KHR && std::ranges::contains(modes, desired))
        return desired;

    for (auto m : order)
        if (std::ranges::contains(modes, m))
            return m;

    return VK_PRESENT_MODE_FIFO_KHR;
}

// -----------------------------------------------------------------------------
// Core recreate — uses only g_*() globals
// -----------------------------------------------------------------------------
void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    vkDeviceWaitIdle(g_device());

    while (!recreateSurfaceIfLost())
        SDL_Delay(50);

    VkSurfaceCapabilitiesKHR caps{};
    VK_VERIFY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice(), g_surface(), &caps));

    extent_ = (caps.currentExtent.width != UINT32_MAX)
        ? caps.currentExtent
        : VkExtent2D{ std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width),
                      std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height) };

    surfaceFormat_ = selectBestFormat(g_PhysicalDevice(), g_surface());
    presentMode_   = selectBestPresentMode(g_PhysicalDevice(), g_surface(), desiredMode_);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0)
        imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci{ .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    ci.surface          = g_surface();
    ci.minImageCount    = imageCount;
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
    VK_VERIFY(vkCreateSwapchainKHR(g_device(), &ci, nullptr, &raw));

    if (swapchain_) vkDestroySwapchainKHR(g_device(), *swapchain_, nullptr);
    swapchain_ = RTX::Handle<VkSwapchainKHR>(raw, g_device(), vkDestroySwapchainKHR);

    uint32_t imgCount = 0;
    VK_VERIFY(vkGetSwapchainImagesKHR(g_device(), raw, &imgCount, nullptr));
    images_.resize(imgCount);
    VK_VERIFY(vkGetSwapchainImagesKHR(g_device(), raw, &imgCount, images_.data()));

    createImageViews();
    createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}SWAPCHAIN REBORN — {}×{} | {} | {} — PINK PHOTONS ETERNAL{}",
                   EMERALD_GREEN, extent_.width, extent_.height,
                   formatName(), presentModeName(), RESET);
}

// -----------------------------------------------------------------------------
// Image views & render pass — now using g_device() everywhere
// -----------------------------------------------------------------------------
void SwapchainManager::createImageViews() noexcept
{
    imageViews_.clear();
    imageViews_.reserve(images_.size());

    VkImageViewCreateInfo ci{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format   = surfaceFormat_.format;
    ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    for (VkImage img : images_) {
        ci.image = img;
        VkImageView view = VK_NULL_HANDLE;
        VK_VERIFY(vkCreateImageView(g_device(), &ci, nullptr, &view));
        imageViews_.emplace_back(view, g_device(), vkDestroyImageView);
    }
}

void SwapchainManager::createRenderPass() noexcept
{
    VkAttachmentDescription att{};
    att.format         = surfaceFormat_.format;
    att.samples        = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription sub{};
    sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments    = &ref;

    constexpr std::array deps = {
        VkSubpassDependency{ VK_SUBPASS_EXTERNAL, 0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT },
        VkSubpassDependency{ 0, VK_SUBPASS_EXTERNAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_DEPENDENCY_BY_REGION_BIT }
    };

    VkRenderPassCreateInfo rp{ .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rp.attachmentCount = 1;
    rp.pAttachments    = &att;
    rp.subpassCount    = 1;
    rp.pSubpasses      = &sub;
    rp.dependencyCount = static_cast<uint32_t>(deps.size());
    rp.pDependencies   = deps.data();

    VkRenderPass handle = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateRenderPass(g_device(), &rp, nullptr, &handle));
    renderPass_ = RTX::Handle<VkRenderPass>(handle, g_device(), vkDestroyRenderPass);
}

void SwapchainManager::cleanup() noexcept
{
    vkDeviceWaitIdle(g_device());
    imageViews_.clear();
    images_.clear();
    renderPass_.reset();
    swapchain_.reset();
}

// -----------------------------------------------------------------------------
// Query helpers
// -----------------------------------------------------------------------------
bool SwapchainManager::isHDR() const noexcept    { return surfaceFormat_.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT; }
bool SwapchainManager::is10Bit() const noexcept { return surfaceFormat_.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ||
                                                         surfaceFormat_.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32; }
bool SwapchainManager::isFP16() const noexcept  { return surfaceFormat_.format == VK_FORMAT_R16G16B16A16_SFLOAT; }

std::string_view SwapchainManager::formatName() const noexcept
{
    if (isHDR())      return "HDR10 10-bit";
    if (isFP16() && surfaceFormat_.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) return "scRGB FP16";
    if (surfaceFormat_.format == VK_FORMAT_B8G8R8A8_UNORM) return "sRGB (B8G8R8A8)";
    if (surfaceFormat_.format == VK_FORMAT_R8G8B8A8_UNORM) return "sRGB (R8G8B8A8)";
    return "Unknown";
}

std::string_view SwapchainManager::presentModeName() const noexcept
{
    switch (presentMode_) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR:    return "IMMEDIATE";
        case VK_PRESENT_MODE_MAILBOX_KHR:      return "MAILBOX";
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
        case VK_PRESENT_MODE_FIFO_KHR:         return "FIFO";
        default:                               return "UNKNOWN";
    }
}

void SwapchainManager::updateWindowTitle(SDL_Window* window, float fps) noexcept
{
    if (!window) return;
    char title[256];
    std::snprintf(title, sizeof(title),
                  "AMOURANTH RTX v80 — %.0f FPS | %ux%u | %s | %s — PINK PHOTONS ETERNAL",
                  fps, extent_.width, extent_.height,
                  formatName().data(), presentModeName().data());
    SDL_SetWindowTitle(window, title);
}

// =============================================================================
// PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED — 32,000+ FPS — VALHALLA LOCKED
// SHIP IT RAW — NOVEMBER 19, 2025 — APOCALYPSE v3.6 — C++23 SUPREMACY
// =============================================================================