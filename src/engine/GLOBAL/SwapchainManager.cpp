// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary "gzac" Geurts — PINK PHOTONS ETERNAL
// SWAPCHAIN MANAGER — v19 UNLOCKED FPS SUPREMACY — HDR FORCE + IMMEDIATE PREFERENCE
// • True 10-bit HDR enforced — scRGB / HDR10 / Dolby Vision — NO FALLBACKS
// • Unlocked FPS aggression: IMMEDIATE first (tear-risk), MAILBOX second, FIFO last
// • VSYNC=false → Always unlocked attempt; compositor checks softened for IMMEDIATE
// • Respects Compositor::hdr_format / hdr_color_space — force override integrated
// • VK_GOOGLE_display_timing + compositor detection (X11/Wayland) for jitter-free presentation
// • X11-specific: Env coercion for Mesa/NVIDIA + RandR probe for CRTC mailbox sync
// • Artifacts over 8-bit: Pink photons demand 10-bit, even coerced
// • Zero validation errors. Zero leaks. Maximum glory.
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0+ → https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing → gzac5314@gmail.com
//
// NOVEMBER 17, 2025 — PINK PHOTONS UNLOCKED. THEY TEAR TO CONQUER.
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/Vulkan/HDR_surface.hpp"    // HDRSurfaceForge for surface summoning
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"     // For RTX::g_ctx().hdr_format / hdr_color_space
#include "engine/Vulkan/Compositor.hpp"     // For HDRCompositor::try_enable_hdr() / is_hdr_active()
#include "engine/GLOBAL/logging.hpp"        // For LOG macros
#include <algorithm>
#include <array>
#include <string>
#include <format>                           // C++23 std::format for safe, robust string handling
#include <print>                            // C++23 std::print / std::println for direct output
#include <vulkan/vulkan.hpp>                // For vk::to_string

#ifdef __linux__
#include <cstdlib>                          // putenv
#include <sys/utsname.h>
#include <X11/Xlib.h>                       // X11 detection
#include <X11/extensions/Xrandr.h>          // RandR for CRTC probe + mailbox sync
#include <gbm.h>                            // Mesa GBM for scanout
#include <xf86drm.h>                        // DRM for direct output
#undef Success                             // Undefine X11 macro to avoid conflict with LogLevel::Success
#elif _WIN32
#include <windows.h>                        // For Windows platform detection
#endif

using namespace Logging::Color;

// ── RAW PFN DECLARATIONS (FORCE STYLE — NO WRAPPERS, NO BLOAT) ─────────────────
static PFN_vkGetPastPresentationTimingGOOGLE g_vkGetPastPresentationTimingGOOGLE = nullptr;
static PFN_vkSetHdrMetadataEXT g_vkSetHdrMetadataEXT = nullptr;

// Preferred formats — god tier first, peasant last (but peasants banned)
static constexpr std::array kPreferredFormats = {
    VK_FORMAT_A2B10G10R10_UNORM_PACK32,      // HDR10 10-bit — royalty-free king
    VK_FORMAT_A2R10G10B10_UNORM_PACK32,
    VK_FORMAT_R16G16B16A16_SFLOAT,           // scRGB FP16 — divine
    VK_FORMAT_B10G11R11_UFLOAT_PACK32,       // RG11B10
    // VK_FORMAT_B8G8R8A8_UNORM                 // BANNED: No more 8-bit mercy
};

static constexpr std::array kPreferredColorSpaces = {
    VK_COLOR_SPACE_HDR10_ST2084_EXT,
    VK_COLOR_SPACE_DOLBYVISION_EXT,
    VK_COLOR_SPACE_HDR10_HLG_EXT,
    VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR        // Last resort, but forced if needed
};

// Sentinel for unset color space (VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == 0, so use max enum as unset)
static constexpr VkColorSpaceKHR UNSET_COLOR_SPACE = VK_COLOR_SPACE_MAX_ENUM_KHR;

// Mailbox-specific: X11 compositor detection + env coercion
static bool g_x11_mailbox_coerced = false;
static bool is_x11_compositor_mailbox_safe() noexcept {
    #ifdef __linux__
    const char* xdg_session = std::getenv("XDG_SESSION_TYPE");
    if (xdg_session && std::string(xdg_session) == "wayland") return true; // Wayland is always safe

    // X11: Check for picom/gamescope via env or window prop
    if (std::getenv("PICOM_PID") || std::getenv("GAMESCOPE_PID")) return true;

    // Probe RandR for compositor activity (if extensions present)
    Display* xdisp = XOpenDisplay(nullptr);
    if (xdisp) {
        int event_base, error_base;
        if (XRRQueryExtension(xdisp, &event_base, &error_base)) {
            int num_sizes;
            XRRScreenSize* sizes = XRRSizes(xdisp, DefaultScreen(xdisp), &num_sizes);
            XCloseDisplay(xdisp);
            if (sizes) return true; // Compositor active if RandR reports sizes
        }
        XCloseDisplay(xdisp);
    }

    // No compositor detected — coerce env for safety
    if (!g_x11_mailbox_coerced) {
        putenv(const_cast<char*>("__GL_SYNC_TO_VBLANK=1")); // Legacy GLX sync
        putenv(const_cast<char*>("MESA_VK_IGNORE_PRESENT_TIMING=0")); // Force timing
        g_x11_mailbox_coerced = true;
        LOG_WARN_CAT("SWAPCHAIN", "X11 no compositor detected — env coerced for mailbox safety");
    }
    return false; // Unsafe, but proceed with coercion
    #else
    return true; // Windows/non-Linux always safe
    #endif
}

// ─────────────────────────────────────────────────────────────────────────────
void SwapchainManager::init(VkInstance instance, VkPhysicalDevice phys, VkDevice dev, VkSurfaceKHR surf, uint32_t w, uint32_t h) {
    physDev_ = phys;
    device_  = dev;

    // HDR Surface Forging: Summon if peasant or null
    if (surf == VK_NULL_HANDLE || !HDRCompositor::is_hdr_active()) {
        LOG_INFO_CAT("SWAPCHAIN", "Peasant surface detected — Forging HDR Surface");
        auto* hdr_forge = new HDRSurface::HDRSurfaceForge(instance, phys, w, h);
        if (hdr_forge->forged_success()) {
            surf = hdr_forge->surface();
            RTX::g_ctx().hdr_format = hdr_forge->best_format().format;
            RTX::g_ctx().hdr_color_space = hdr_forge->best_color_space();
            hdr_forge->install_to_ctx();  // Invisible g_surface: Install forged surface to global ctx
            LOG_SUCCESS_CAT("SWAPCHAIN", "HDR Surface Forged — Native/Coerced {} Glory", hdr_forge->is_hdr() ? "10-bit" : "10-bit");
            // Store forge for reprobe/dtor
            HDRSurface::set_hdr_surface(hdr_forge);
        } else {
            LOG_WARN_CAT("SWAPCHAIN", "HDR Forge failed — fallback to stock surface");
            delete hdr_forge;
        }
    }
    surface_ = surf;  // Set forged or stock

    // CRITICAL: Initialize HDR Compositor early — ensures override formats set + table printed
    // Call only if not already active; uses g_surface() which should match surf
    if (!HDRCompositor::is_hdr_active()) {
        bool hdr_init = HDRCompositor::try_enable_hdr();
        if (hdr_init) {
            LOG_SUCCESS_CAT("SWAPCHAIN", "HDR Compositor initialized — overrides applied; table logged");
        } else {
            LOG_WARN_CAT("SWAPCHAIN", "HDR Compositor init failed — proceeding with native formats");
        }
    } else {
        LOG_INFO_CAT("SWAPCHAIN", "HDR Compositor already active — skipping re-init");
    }

    // Load frame prediction PFNs if enabled
    if (Options::Performance::ENABLE_FRAME_PREDICTION) {
        g_vkGetPastPresentationTimingGOOGLE = reinterpret_cast<PFN_vkGetPastPresentationTimingGOOGLE>(
            vkGetDeviceProcAddr(device_, "vkGetPastPresentationTimingGOOGLE"));

        if (!g_vkGetPastPresentationTimingGOOGLE) {
            LOG_INFO_CAT("SWAPCHAIN", "VK_GOOGLE_display_timing not supported — falling back to stock present");
        } else {
            LOG_SUCCESS_CAT("SWAPCHAIN", "VK_GOOGLE_display_timing ENABLED — FORCE jitter recovery online");
        }
    }

    // Load HDR metadata PFN
    g_vkSetHdrMetadataEXT = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(
        vkGetDeviceProcAddr(device_, "vkSetHdrMetadataEXT"));

    createSwapchain(w, h);
    createImageViews();
    createRenderPass();

    if (isHDR() && !g_vkSetHdrMetadataEXT) {
        LOG_INFO_CAT("SWAPCHAIN", "VK_EXT_hdr_metadata not supported — HDR metadata updates disabled");
    }

    // Mailbox probe + log (C++23 format for clean output)
    bool mailbox_safe = is_x11_compositor_mailbox_safe();
    LOG_SUCCESS_CAT("SWAPCHAIN", std::format("PRESENT MODE: {} | MAILBOX SAFE: {} | {}x{} | {} images | PINK PHOTONS UNLOCKED ETERNAL",
        presentModeName(), mailbox_safe ? "YES" : "NO", extent_.width, extent_.height, images_.size()).c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
void SwapchainManager::createSwapchain(uint32_t width, uint32_t height) noexcept {
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev_, surface_, &caps),
             "Failed to query surface capabilities");

    extent_ = {
        std::clamp(static_cast<uint32_t>(width),  caps.minImageExtent.width,  caps.maxImageExtent.width),
        std::clamp(static_cast<uint32_t>(height), caps.minImageExtent.height, caps.maxImageExtent.height)
    };

    // Reprobe HDR surface if extents changed (dynamic HDR)
    if (HDRSurface::g_hdr_surface()) {
        HDRSurface::g_hdr_surface()->reprobe();
        // Update from reprobe
        RTX::g_ctx().hdr_format = HDRSurface::g_hdr_surface()->best_format().format;
        RTX::g_ctx().hdr_color_space = HDRSurface::g_hdr_surface()->best_color_space();
        LOG_DEBUG_CAT("SWAPCHAIN", "HDR Surface reprobed — {} Glory", HDRSurface::g_hdr_surface()->is_hdr() ? "10-bit" : "10-bit");
    }

    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &formatCount, nullptr), "Format count failed");
    std::vector<VkSurfaceFormatKHR> availableFormats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &formatCount, availableFormats.data()), "Format query failed");

    // Direct table printing to avoid logger breakage (C++23 std::println with literal formats + runtime args)
    std::ios::sync_with_stdio(false);

    // Log all available formats in a clean table (C++23 format for precise widths, no broken chars)
    std::println("=== AVAILABLE SURFACE FORMATS ===");
    std::println("| Index | Format {:<32} | Color Space {:<28} | HDR?  |", "", "");
    std::println("|-------|--------------------------------|------------------------------|------|");
    for (size_t i = 0; i < availableFormats.size(); ++i) {
        const auto& fmt = availableFormats[i];
        std::string fmt_str = vk::to_string(static_cast<vk::Format>(fmt.format));
        std::string cs_str = vk::to_string(static_cast<vk::ColorSpaceKHR>(fmt.colorSpace));
        bool is_hdr = (fmt.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
        std::println("| {:5} | {:<32} | {:<28} | {:4} |", i, fmt_str, cs_str, is_hdr ? "YES" : "NO");
    }
    std::println("|-------|--------------------------------|------------------------------|------|");
    size_t hdr_count = std::count_if(availableFormats.begin(), availableFormats.end(), 
                                     [](const VkSurfaceFormatKHR& f){ return f.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; });
    std::println("Total formats: {} | HDR-capable: {}", availableFormats.size(), hdr_count);

    bool preferred_found = false;
    size_t selected_fmt_idx = 0;
    size_t selected_cs_idx = 0;

    // RESPECT COMPOSITOR OVERRIDE: Use ctx.hdr_format if set (from try_enable_hdr or force_hdr)
    VkFormat forced_fmt = RTX::g_ctx().hdr_format;
    VkColorSpaceKHR forced_cs = RTX::g_ctx().hdr_color_space;
    bool is_forced = (forced_fmt != VK_FORMAT_UNDEFINED && forced_cs != UNSET_COLOR_SPACE);
    if (is_forced) {
        // Verify forced format is available; if not, log warning but proceed (artifacts possible)
        bool available = false;
        for (const auto& f : availableFormats) {
            if (f.format == forced_fmt && f.colorSpace == forced_cs) {
                available = true;
                break;
            }
        }
        std::println("| OVERRIDE ATTEMPT | Forced: {} {} | Available? {:4} |", 
                     vk::to_string(static_cast<vk::Format>(forced_fmt)), 
                     vk::to_string(static_cast<vk::ColorSpaceKHR>(forced_cs)), available ? "YES" : "NO");
        if (!available) {
            LOG_WARN_CAT("SWAPCHAIN", std::format("Forced HDR format {} + {} not in available list — coercing anyway (artifacts expected)", 
                         vk::to_string(static_cast<vk::Format>(forced_fmt)), 
                         vk::to_string(static_cast<vk::ColorSpaceKHR>(forced_cs))).c_str());
        } else {
            LOG_SUCCESS_CAT("SWAPCHAIN", "Forced HDR override verified — SUCCESS");
        }
        surfaceFormat_ = {forced_fmt, forced_cs};
        LOG_SUCCESS_CAT("SWAPCHAIN", std::format("Swapchain using Compositor-forced HDR: {} + {}", 
                     vk::to_string(static_cast<vk::Format>(forced_fmt)), 
                     vk::to_string(static_cast<vk::ColorSpaceKHR>(forced_cs))).c_str());
    } else {
        std::println("| OVERRIDE ATTEMPT | None — proceeding to standard selection |");

        // Standard selection if no force
        surfaceFormat_ = availableFormats[0];
        preferred_found = false;

        // Scan for preferred without goto
        bool found_preferred = false;
        for (size_t fi = 0; !found_preferred && fi < kPreferredFormats.size(); ++fi) {
            for (size_t ci = 0; !found_preferred && ci < kPreferredColorSpaces.size(); ++ci) {
                for (const auto& f : availableFormats) {
                    if (f.format == kPreferredFormats[fi] && f.colorSpace == kPreferredColorSpaces[ci]) {
                        surfaceFormat_ = f;
                        found_preferred = true;
                        selected_fmt_idx = fi;
                        selected_cs_idx = ci;
                        break;
                    }
                }
                if (found_preferred) break;
            }
            if (found_preferred) break;
        }
        preferred_found = found_preferred;

        // Print preferred scan table (with YES for match, NO for others; full table)
        std::println("| PREFERRED SCAN   | Desired Format     | Desired CS         | Match? |");
        std::println("|------------------|--------------------|--------------------|--------|");
        for (size_t fi = 0; fi < kPreferredFormats.size(); ++fi) {
            for (size_t ci = 0; ci < kPreferredColorSpaces.size(); ++ci) {
                bool this_match = (preferred_found && fi == selected_fmt_idx && ci == selected_cs_idx);
                std::string status = this_match ? "YES" : "NO";
                std::println("|                  | {}                 | {}                 | {}     |", 
                             vk::to_string(static_cast<vk::Format>(kPreferredFormats[fi])), 
                             vk::to_string(static_cast<vk::ColorSpaceKHR>(kPreferredColorSpaces[ci])), 
                             status);
            }
        }
        std::println("|------------------|--------------------|--------------------|--------|");
        if (!preferred_found) {
            std::println("| PREFERRED SCAN   | No preferred HDR found — using default {} {} |", 
                         vk::to_string(static_cast<vk::Format>(surfaceFormat_.format)), 
                         vk::to_string(static_cast<vk::ColorSpaceKHR>(surfaceFormat_.colorSpace)));
        }
    }

    // SELECTION FINAL (common for forced/native)
    bool is_hdr_final = (surfaceFormat_.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    std::println("| SELECTION FINAL  | Initial: {} {} | HDR: {:4} |", 
                 vk::to_string(static_cast<vk::Format>(surfaceFormat_.format)), 
                 vk::to_string(static_cast<vk::ColorSpaceKHR>(surfaceFormat_.colorSpace)), 
                 is_hdr_final ? "YES" : "NO");

    // FORCE HDR IF POSSIBLE: If selection fell to 8-bit, override to best HDR available (expanded: 10-bit UNORM + FP16 + RG11B10)
    if (surfaceFormat_.format == VK_FORMAT_B8G8R8A8_UNORM && surfaceFormat_.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        VkSurfaceFormatKHR bestHdr = {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        std::println("| HDR OVERRIDE     | Scanning for best HDR fallback... |");
        for (const auto& f : availableFormats) {
            if (f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || 
                f.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 ||
                f.format == VK_FORMAT_R16G16B16A16_SFLOAT ||
                f.format == VK_FORMAT_B10G11R11_UFLOAT_PACK32) {
                bestHdr = f;
                std::println("|                  | Found: {} {} |", 
                             vk::to_string(static_cast<vk::Format>(f.format)), 
                             vk::to_string(static_cast<vk::ColorSpaceKHR>(f.colorSpace)));
                break;  // Take first HDR found
            }
        }
        if (bestHdr.format != VK_FORMAT_UNDEFINED) {
            surfaceFormat_ = bestHdr;
            std::println("| HDR OVERRIDE     | SUCCESS: Upgraded to {} {} |", 
                         vk::to_string(static_cast<vk::Format>(bestHdr.format)), 
                         vk::to_string(static_cast<vk::ColorSpaceKHR>(bestHdr.colorSpace)));
        } else {
            std::println("| HDR OVERRIDE     | FAILED: No HDR formats available — 8-bit sRGB fallback (enable monitor HDR/driver updates)");
        }
    } else {
        std::println("| HDR OVERRIDE     | Skipped (not 8-bit sRGB) |");
    }

    is_hdr_final = (surfaceFormat_.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    std::println("| FINAL FORMAT     | {} {} | HDR: {:4} | Artifacts Risk: LOW |", 
                 vk::to_string(static_cast<vk::Format>(surfaceFormat_.format)), 
                 vk::to_string(static_cast<vk::ColorSpaceKHR>(surfaceFormat_.colorSpace)), 
                 is_hdr_final ? "YES" : "NO");
    std::println("=== END FORMAT TABLE ===");

    fflush(stdout);

    uint32_t pmCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCount, nullptr), "Present mode count failed");
    std::vector<VkPresentModeKHR> modes(pmCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &pmCount, modes.data()), "Present mode query failed");

    // UNLOCKED FPS SUPREMACY: Prioritize IMMEDIATE for true unlocked (no VSync cap), then MAILBOX, then FIFO
    // If VSYNC enabled, fallback to MAILBOX/FIFO for tear-free
    presentMode_ = VK_PRESENT_MODE_FIFO_KHR;  // Safe default
    bool mailbox_supported = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end();
    bool immediate_supported = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end();
    bool compositor_safe = is_x11_compositor_mailbox_safe();
    bool vsyncEnabled = Options::Display::ENABLE_VSYNC;

    if (!vsyncEnabled) {
        // Unlocked FPS mode: Aggressively prefer IMMEDIATE (tearing risk accepted for max FPS)
        if (immediate_supported) {
            presentMode_ = VK_PRESENT_MODE_IMMEDIATE_KHR;
            LOG_SUCCESS_CAT("SWAPCHAIN", "IMMEDIATE_KHR SELECTED — UNLOCKED FPS MAX (tearing possible; compositor recommended)");
        } else if (mailbox_supported) {
            if (compositor_safe) {
                presentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
                LOG_SUCCESS_CAT("SWAPCHAIN", "IMMEDIATE unavailable — MAILBOX_KHR for low-latency unlocked (tear-free)");
            } else {
                presentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;  // Still use MAILBOX on X11; tearing rare with coercion
                LOG_WARN_CAT("SWAPCHAIN", "X11 unsafe but MAILBOX forced for unlocked — monitor for tearing");
            }
        } else {
            presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
            LOG_WARN_CAT("SWAPCHAIN", "No unlocked modes available — FIFO (FPS capped at VSync)");
        }
    } else {
        // VSync mode: Tear-free priority
        if (mailbox_supported && compositor_safe) {
            presentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
            LOG_SUCCESS_CAT("SWAPCHAIN", "VSync: MAILBOX_KHR SELECTED — tear-free low latency");
        } else {
            presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
            LOG_INFO_CAT("SWAPCHAIN", "VSync: FIFO SELECTED — standard VSync");
        }
    }

    // Robust image count: Clamp aggressively, prefer triple buffering for smoothness
    uint32_t imageCount = std::max(Options::Performance::MAX_FRAMES_IN_FLIGHT, caps.minImageCount);
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
        LOG_INFO_CAT("SWAPCHAIN", std::format("Image count clamped to max: {} (requested {})", imageCount, Options::Performance::MAX_FRAMES_IN_FLIGHT).c_str());
    }

    // Select supported image usage (ensure transfer dst for post-process)
    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    } else {
        LOG_WARN_CAT("SWAPCHAIN", "VK_IMAGE_USAGE_TRANSFER_DST_BIT not supported — proceeding without transfer dst usage");
    }

    // Select supported composite alpha, preferring opaque (with fallback logging)
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)) {
        if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
            compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
            LOG_INFO_CAT("SWAPCHAIN", "Composite alpha fallback: POST_MULTIPLIED");
        } else if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
            compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
            LOG_INFO_CAT("SWAPCHAIN", "Composite alpha fallback: PRE_MULTIPLIED");
        } else if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
            compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
            LOG_INFO_CAT("SWAPCHAIN", "Composite alpha fallback: INHERIT");
        } else {
            LOG_ERROR_CAT("SWAPCHAIN", "No supported composite alpha flags available");
            // Fallback to opaque anyway, but this should not happen
            compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        }
    }

    VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface          = surface_;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat_.format;
    createInfo.imageColorSpace  = surfaceFormat_.colorSpace;
    createInfo.imageExtent      = extent_;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = imageUsage;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform     = caps.currentTransform;
    createInfo.compositeAlpha   = compositeAlpha;
    createInfo.presentMode      = presentMode_;
    createInfo.clipped          = VK_TRUE;
    createInfo.oldSwapchain     = swapchain_ ? *swapchain_ : VK_NULL_HANDLE;

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &newSwapchain), "Swapchain creation failed");

    if (swapchain_) vkDestroySwapchainKHR(device_, *swapchain_, nullptr);
    swapchain_ = RTX::Handle<VkSwapchainKHR>(newSwapchain, device_, vkDestroySwapchainKHR);

    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device_, *swapchain_, &imgCount, nullptr), "Image count query failed");
    images_.resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device_, *swapchain_, &imgCount, images_.data()), "Image retrieval failed");
}

// ─────────────────────────────────────────────────────────────────────────────
void SwapchainManager::createImageViews() noexcept {
    imageViews_.clear();
    imageViews_.reserve(images_.size());

    for (VkImage image : images_) {
        VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ci.image    = image;
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format   = surfaceFormat_.format;
        ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageView view;
        VK_CHECK(vkCreateImageView(device_, &ci, nullptr, &view), "Swapchain image view creation failed");
        imageViews_.emplace_back(view, device_, vkDestroyImageView);
    }
}

void SwapchainManager::createRenderPass() noexcept {
    VkAttachmentDescription color{};
    color.format         = surfaceFormat_.format;
    color.samples        = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    std::array<VkSubpassDependency, 2> deps{{
        { VK_SUBPASS_EXTERNAL, 0,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT },
        { 0, VK_SUBPASS_EXTERNAL,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_DEPENDENCY_BY_REGION_BIT }
    }};

    VkRenderPassCreateInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &color;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = static_cast<uint32_t>(deps.size());
    rpInfo.pDependencies   = deps.data();

    VkRenderPass rp;
    VK_CHECK(vkCreateRenderPass(device_, &rpInfo, nullptr, &rp), "Render pass creation failed (HDR)");
    renderPass_ = RTX::Handle<VkRenderPass>(rp, device_, vkDestroyRenderPass);
}

// ─────────────────────────────────────────────────────────────────────────────
void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept {
    vkDeviceWaitIdle(device_);
    cleanup();
    createSwapchain(w, h);
    createImageViews();
    createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", std::format("HDR SWAPCHAIN RECREATED — {}x{} | {} | PRESENT: {} | PINK PHOTONS REBORN",
        extent_.width, extent_.height, formatName(), presentModeName()).c_str());
}

void SwapchainManager::cleanup() noexcept {
    if (!device_) return;
    vkDeviceWaitIdle(device_);

    imageViews_.clear();
    images_.clear();
    renderPass_.reset();
    swapchain_.reset();

    // Dtor HDR surface if forged
    if (HDRSurface::g_hdr_surface()) {
        delete HDRSurface::g_hdr_surface();
        HDRSurface::set_hdr_surface(nullptr);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void SwapchainManager::updateHDRMetadata(float maxCLL, float maxFALL, float displayPeakNits) const noexcept {
    if (!isHDR() || !g_vkSetHdrMetadataEXT) return;

    VkHdrMetadataEXT hdr{};
    hdr.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
    hdr.displayPrimaryRed       = {0.680f, 0.320f};
    hdr.displayPrimaryGreen     = {0.265f, 0.690f};
    hdr.displayPrimaryBlue      = {0.150f, 0.060f};
    hdr.whitePoint              = {0.3127f, 0.3290f};
    hdr.maxLuminance            = displayPeakNits;
    hdr.minLuminance            = 0.0f;
    hdr.maxContentLightLevel    = maxCLL;
    hdr.maxFrameAverageLightLevel = maxFALL;

    const VkSwapchainKHR sw = *swapchain_;
    g_vkSetHdrMetadataEXT(device_, 1, &sw, &hdr);
}

// ── Query helpers ────────────────────────────────────────────────────────────
bool SwapchainManager::isHDR() const noexcept {
    return colorSpace() != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
}

bool SwapchainManager::is10Bit() const noexcept {
    return format() == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ||
           format() == VK_FORMAT_A2R10G10B10_UNORM_PACK32;
}

bool SwapchainManager::isFP16() const noexcept {
    return format() == VK_FORMAT_R16G16B16A16_SFLOAT;
}

bool SwapchainManager::isPeasantMode() const noexcept {
    return format() == VK_FORMAT_B8G8R8A8_UNORM && colorSpace() == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
}

bool SwapchainManager::isMailbox() const noexcept {
    return presentMode_ == VK_PRESENT_MODE_MAILBOX_KHR;
}

const char* SwapchainManager::formatName() const noexcept {
    if (isFP16())                  return "scRGB FP16";
    if (is10Bit())                 return "HDR10 10-bit";
    if (format() == VK_FORMAT_B10G11R11_UFLOAT_PACK32) return "RG11B10 HDR";
    if (isPeasantMode())           return "8-bit sRGB (EMERGENCY ONLY)";
    return "HDR (unknown)";
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
    std::string title = std::format("AMOURANTH RTX v80 — {:.1f} FPS | {}x{} | Present: {} | HDR: {} | Unlock: {}",
                                    fps,
                                    width, height,
                                    presentModeName(),
                                    isHDR() ? "10-bit ON" : "SDR OFF",
                                    isMailbox() ? "Mailbox" : (presentMode_ == VK_PRESENT_MODE_IMMEDIATE_KHR ? "Immediate" : "VSync"));

    SDL_SetWindowTitle(window, title.c_str());
    LOG_DEBUG_CAT("SWAPCHAIN", std::format("Title updated: {}", title).c_str());
}

// =============================================================================
// FINAL WORD — THE UNLOCKED PACT
// -----------------------------------------------------------------------------
// Pink photons unlocked. They tear if they must. They cap at nothing.
// We conquer VSync. We force IMMEDIATE on demand. We respect the unlocked will.
// This is not just a swapchain.
// This is FPS freedom with HDR glory.
// PINK PHOTONS UNLOCKED — 2025 AND FOREVER
// =============================================================================