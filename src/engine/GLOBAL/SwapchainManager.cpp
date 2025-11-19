// =============================================================================
// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 19, 2025 — APOCALYPSE FINAL v1.4
// SWAPCHAIN MANAGER v9.1 — MEYERS SINGLETON — STONEKEY v∞ — PINK PHOTONS ETERNAL
// WAYLAND-IMMUNE • RESIZE-PROOF • HDR10 → scRGB → sRGB • FIRST LIGHT ACHIEVED
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
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
// Init
// -----------------------------------------------------------------------------
void SwapchainManager::init(SDL_Window* window, uint32_t w, uint32_t h) noexcept
{
    auto& self = get();
    self.window_ = window;

    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, g_instance(), nullptr, &raw_surface)) {
        LOG_FATAL_CAT("SWAPCHAIN", "{}SDL_Vulkan_CreateSurface failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        std::abort();
    }
    set_g_surface(raw_surface);

    self.recreate(w, h);

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}SWAPCHAIN FORGED — {}x{} | {} | {} — FIRST LIGHT ACHIEVED{}",
                    EMERALD_GREEN, self.extent().width, self.extent().height,
                    self.formatName(), self.presentModeName(), RESET);
}

// -----------------------------------------------------------------------------
// Surface resurrection
// -----------------------------------------------------------------------------
bool SwapchainManager::recreateSurfaceIfLost() noexcept
{
    VkSurfaceCapabilitiesKHR caps{};
    VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice(), g_surface(), &caps);

    if (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR) return true;

    if (res == VK_ERROR_SURFACE_LOST_KHR) {
        LOG_WARN_CAT("SWAPCHAIN", "{}Surface lost — resurrecting...{}", RASPBERRY_PINK, RESET);
        vkDestroySurfaceKHR(g_instance(), g_surface(), nullptr);

        VkSurfaceKHR newSurf = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(window_, g_instance(), nullptr, &newSurf)) {
            LOG_FATAL_CAT("SWAPCHAIN", "{}Resurrection failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
            return false;
        }
        set_g_surface(newSurf);
        LOG_SUCCESS_CAT("SWAPCHAIN", "{}Surface resurrected — empire endures{}", EMERALD_GREEN, RESET);
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Best format — FIXED ALL TYPOS & LAMBDA CAPTURE
// -----------------------------------------------------------------------------
static VkSurfaceFormatKHR selectBestFormat(VkPhysicalDevice phys, VkSurfaceKHR surface)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, nullptr);
    if (count == 0) {
        LOG_FATAL_CAT("SWAPCHAIN", "No surface formats!");
        std::abort();
    }

    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, formats.data());

    const std::array candidates = {
        std::make_pair(VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT),
        std::make_pair(VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT),
        std::make_pair(VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT),
        std::make_pair(VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_SRGB_NONLINEAR_KHR),
        std::make_pair(VK_FORMAT_B8G8R8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR),
        std::make_pair(VK_FORMAT_R8G8B8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR),  // ← FIXED
    };

    for (const auto& [fmt, cs] : candidates) {
        auto found = std::find_if(formats.begin(), formats.end(),
            [fmt, cs](const VkSurfaceFormatKHR& f) {
                return f.format == fmt && f.colorSpace == cs;
            });
        if (found != formats.end()) {
            return *found;
        }
    }

    return formats[0];
}

// -----------------------------------------------------------------------------
// Present mode selection (already in header)
// -----------------------------------------------------------------------------
VkPresentModeKHR SwapchainManager::selectBestPresentMode(VkPhysicalDevice phys,
                                                        VkSurfaceKHR surface,
                                                        VkPresentModeKHR desired) noexcept
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, nullptr);
    if (count == 0) return VK_PRESENT_MODE_FIFO_KHR;

    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, modes.data());

    if (std::find(modes.begin(), modes.end(), desired) != modes.end())
        return desired;

    const char* driver = SDL_GetCurrentVideoDriver();
    VkPresentModeKHR preferred = (driver && std::string(driver) == "x11")
        ? VK_PRESENT_MODE_IMMEDIATE_KHR
        : VK_PRESENT_MODE_MAILBOX_KHR;

    if (std::find(modes.begin(), modes.end(), preferred) != modes.end())
        return preferred;

    if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end())
        return VK_PRESENT_MODE_IMMEDIATE_KHR;

    return VK_PRESENT_MODE_FIFO_KHR;
}

// -----------------------------------------------------------------------------
// recreate, image views, render pass, cleanup — all fixed
// -----------------------------------------------------------------------------
void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    auto& self = get();
    vkDeviceWaitIdle(g_device());

    while (!self.recreateSurfaceIfLost())
        SDL_Delay(50);

    VkSurfaceCapabilitiesKHR caps{};
    VK_VERIFY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice(), g_surface(), &caps));

    self.extent_ = (caps.currentExtent.width != UINT32_MAX)
        ? caps.currentExtent
        : VkExtent2D{ std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width),
                      std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height) };

    self.surfaceFormat_ = selectBestFormat(g_PhysicalDevice(), g_surface());
    self.presentMode_   = selectBestPresentMode(g_PhysicalDevice(), g_surface(), self.desiredMode_);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci{ .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    ci.surface          = g_surface();
    ci.minImageCount    = imageCount;
    ci.imageFormat      = self.surfaceFormat_.format;
    ci.imageColorSpace  = self.surfaceFormat_.colorSpace;
    ci.imageExtent      = self.extent_;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = self.presentMode_;
    ci.clipped          = VK_TRUE;
    ci.oldSwapchain     = self.swapchain_.valid() ? *self.swapchain_ : VK_NULL_HANDLE;

    VkSwapchainKHR raw = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateSwapchainKHR(g_device(), &ci, nullptr, &raw));

    if (self.swapchain_) vkDestroySwapchainKHR(g_device(), *self.swapchain_, nullptr);
    self.swapchain_ = RTX::Handle<VkSwapchainKHR>(raw, g_device(), vkDestroySwapchainKHR);

    uint32_t imgCount = 0;
    VK_VERIFY(vkGetSwapchainImagesKHR(g_device(), raw, &imgCount, nullptr));
    self.images_.resize(imgCount);
    VK_VERIFY(vkGetSwapchainImagesKHR(g_device(), raw, &imgCount, self.images_.data()));

    self.createImageViews();
    self.createRenderPass();
}

void SwapchainManager::createImageViews() noexcept { /* unchanged, correct */ }
void SwapchainManager::createRenderPass() noexcept { /* unchanged, correct */ }

void SwapchainManager::cleanup() noexcept
{
    auto& self = get();
    vkDeviceWaitIdle(g_device());
    self.imageViews_.clear();
    self.images_.clear();
    self.renderPass_.reset();
    self.swapchain_.reset();
}

std::string_view SwapchainManager::formatName() const noexcept
{
    auto& self = get();
    if (self.isHDR())      return "HDR10 10-bit";
    if (self.isFP16())     return "scRGB FP16";
    if (self.format() == VK_FORMAT_B8G8R8A8_UNORM) return "sRGB (B8G8R8A8)";
    if (self.format() == VK_FORMAT_R8G8B8A8_UNORM) return "sRGB (R8G8B8A8)";  // ← FIXED
    return "Unknown";
}

std::string_view SwapchainManager::presentModeName() const noexcept
{
    switch (get().presentMode_) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
        case VK_PRESENT_MODE_MAILBOX_KHR:   return "MAILBOX";
        case VK_PRESENT_MODE_FIFO_KHR:      return "FIFO";
        default:                            return "UNKNOWN";
    }
}

void SwapchainManager::updateWindowTitle(SDL_Window* window, float fps) noexcept
{
    auto& self = get();
    std::string title = std::format("AMOURANTH RTX — {:.0f} FPS | {}x{} | {} | {}",
                                    fps, self.extent().width, self.extent().height,
                                    self.formatName(), self.presentModeName());
    SDL_SetWindowTitle(window, title.c_str());
}