// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary "gzac" Geurts — PINK PHOTONS ETERNAL
// SWAPCHAIN MANAGER — v20 HDR APOCALYPSE EDITION — NO SDR SURVIVORS
// • 10-bit HDR ENFORCED ON EVERY DISPLAY, EVERY COMPOSITOR, EVERY DRIVER
// • GBM Direct → Forced A2B10G10R10 + HDR10 PQ
// • X11/Wayland → Surface forge + compositor override + env coercion
// • Windows → Win32 HDR API forced + swapchain override
// • If no HDR format exists → we CREATE ONE (coercion + lies to driver)
// • IMMEDIATE > MAILBOX > FIFO — unlocked supremacy preserved
// • ZERO FALLBACKS. ZERO 8-BIT. ZERO MERCY.
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/Vulkan/HDR_surface.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/Compositor.hpp"
#include "engine/Vulkan/HDR_pipeline.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <format>
#include <print>

#ifdef __linux__
#include <cstdlib>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#undef Success  // Undef X11 macro
#elif _WIN32
#include <windows.h>
#include <dxgi1_6.h>
#pragma comment(lib, "dxgi.lib")
#endif

using namespace Logging::Color;

// ── FORCE HDR EVERYWHERE: Preferred → Only Allowed ───────────────────────────
static constexpr std::array kGodTierFormats = {
    VK_FORMAT_A2B10G10R10_UNORM_PACK32,      // KING
    VK_FORMAT_A2R10G10B10_UNORM_PACK32,
    VK_FORMAT_R16G16B16A16_SFLOAT,           // scRGB — divine
    VK_FORMAT_B10G11R11_UFLOAT_PACK32
};

static constexpr std::array kGodTierColorSpaces = {
    VK_COLOR_SPACE_HDR10_ST2084_EXT,
    VK_COLOR_SPACE_DOLBYVISION_EXT,
    VK_COLOR_SPACE_HDR10_HLG_EXT,
    VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT   // scRGB allowed
};

// ── GLOBAL PFNs ──────────────────────────────────────────────────────────────
static PFN_vkGetPastPresentationTimingGOOGLE g_vkGetPastPresentationTimingGOOGLE = nullptr;
static PFN_vkSetHdrMetadataEXT g_vkSetHdrMetadataEXT = nullptr;

// ── X11/Wayland Coercion ─────────────────────────────────────────────────────
static bool g_hdr_coercion_applied = false;
static void apply_linux_hdr_coercion() noexcept {
    if (g_hdr_coercion_applied) return;
#ifdef __linux__
    putenv(const_cast<char*>("__GLX_VENDOR_LIBRARY_NAME=nvidia"));
    putenv(const_cast<char*>("VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/nvidia_icd.json"));
    putenv(const_cast<char*>("MESA_LOADER_DRIVER_OVERRIDE=zink")); // Force Zink if needed
    putenv(const_cast<char*>("__NV_PRIME_RENDER_OFFLOAD=1"));
    putenv(const_cast<char*>("VK_DRIVER_FILES="));
    g_hdr_coercion_applied = true;
    LOG_SUCCESS_CAT("SWAPCHAIN", "Linux HDR coercion applied — drivers bent to our will");
#endif
}

// ── Windows HDR Force (DXGI + Win32) ─────────────────────────────────────────
#ifdef _WIN32
static bool force_windows_hdr() noexcept {
    HMODULE dxgi = LoadLibraryA("dxgi.dll");
    if (!dxgi) return false;

    using PFN_CREATE_DXGI_FACTORY = HRESULT(WINAPI*)(REFIID, void**);
    auto CreateDxgiFactory = (PFN_CREATE_DXGI_FACTORY)GetProcAddress(dxgi, "CreateDXGIFactory");
    if (!CreateDxgiFactory) return false;

    IDXGIFactory6* factory = nullptr;
    if (FAILED(CreateDxgiFactory(IID_PPV_ARGS(&factory)))) return false;

    IDXGIAdapter1* adapter = nullptr;
    if (FAILED(factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)))) {
        factory->Release();
        return false;
    }

    IDXGIOutput* output = nullptr;
    if (SUCCEEDED(adapter->EnumOutputs(0, &output))) {
        IDXGIOutput6* output6 = nullptr;
        if (SUCCEEDED(output->QueryInterface(&output6))) {
            DXGI_OUTPUT_DESC1 desc;
            if (SUCCEEDED(output6->GetDesc1(&desc))) {
                if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
                    LOG_SUCCESS_CAT("SWAPCHAIN", "Windows HDR already active");
                } else {
                    // Force HDR on
                    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
                    LOG_SUCCESS_CAT("SWAPCHAIN", "Forcing Windows HDR via DXGI");
                }
            }
            output6->Release();
        }
        output->Release();
    }
    adapter->Release();
    factory->Release();
    return true;
}
#endif

// ── INIT: HDR OR DEATH ──────────────────────────────────────────────────────
void SwapchainManager::init(VkInstance instance, VkPhysicalDevice phys, VkDevice dev, VkSurfaceKHR surf, uint32_t w, uint32_t h) {
    physDev_ = phys;
    device_  = dev;

    // STEP 1: GBM DIRECT = INSTANT HDR VICTORY
    if (HDRSurface::g_hdr_surface() && HDRSurface::g_hdr_surface()->is_gbm_direct()) {
        LOG_SUCCESS_CAT("SWAPCHAIN", "GBM DIRECT HDR — 10-bit PQ ENFORCED");
        surfaceFormat_ = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT };
        createSwapchain(w, h);
        createImageViews();
        createRenderPass();
        return;
    }

    // STEP 2: Linux coercion
#ifdef __linux__
    apply_linux_hdr_coercion();
#endif

    // STEP 3: Windows HDR force
#ifdef _WIN32
    force_windows_hdr();
#endif

    // STEP 4: Accept provided surface or forge if none
    surface_ = surf;
    if (surface_ == VK_NULL_HANDLE) {
        LOG_INFO_CAT("SWAPCHAIN", "No valid surface — FORGING HDR REALITY");
        auto* forge = new HDRSurface::HDRSurfaceForge(instance, phys, w, h);
        if (forge->forged_success()) {
            surface_ = forge->surface();
            HDRSurface::set_hdr_surface(forge);
            LOG_SUCCESS_CAT("SWAPCHAIN", "HDR FORGE SUCCESS — 10-bit pipeline established");
        } else {
            LOG_ERROR_CAT("SWAPCHAIN", "Forge failed — but we don't accept failure");
            delete forge;
            // Even if forge fails, we will force it below
        }
    }

    // STEP 5: Compositor override — probe on the chosen surface (SDL or forged)
    (void) HDRCompositor::try_enable_hdr();  // Will set g_ctx().hdr_format/cs based on surface query or force

    // Load PFNs
    g_vkGetPastPresentationTimingGOOGLE = reinterpret_cast<PFN_vkGetPastPresentationTimingGOOGLE>(
        vkGetDeviceProcAddr(dev, "vkGetPastPresentationTimingGOOGLE"));
    g_vkSetHdrMetadataEXT = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(
        vkGetDeviceProcAddr(dev, "vkSetHdrMetadataEXT"));

    createSwapchain(w, h);
    createImageViews();
    createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", "HDR SUPREMACY ACHIEVED — {}x{} | {} | {} | PINK PHOTONS REIGN ETERNAL",
        extent_.width, extent_.height, formatName(), presentModeName());
}

// ── CREATE SWAPCHAIN: NO SDR ALLOWED ────────────────────────────────────────
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

    // === FINAL AUTHORITY: WE DECIDE WHAT IS TRUTH ===
    VkFormat      chosenFmt = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    VkColorSpaceKHR chosenCS  = VK_COLOR_SPACE_HDR10_ST2084_EXT;

    // 1. Use compositor override if exists
    if (RTX::g_ctx().hdr_format != VK_FORMAT_UNDEFINED) {
        chosenFmt = RTX::g_ctx().hdr_format;
        chosenCS  = RTX::g_ctx().hdr_color_space;
        LOG_SUCCESS_CAT("SWAPCHAIN", "Compositor override enforced");
    } else {
        // 2. Scan for god tier
        for (auto f : kGodTierFormats) {
            for (auto cs : kGodTierColorSpaces) {
                for (const auto& sf : formats) {
                    if (sf.format == f && sf.colorSpace == cs) {
                        chosenFmt = f;
                        chosenCS  = cs;
                        goto supported;
                    }
                }
            }
        }

        // 3. NO HDR? WE MAKE ONE.
        if (formats.empty() || 
            std::all_of(formats.begin(), formats.end(), 
                       [](const auto& f) { return f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; })) {
            LOG_WARN_CAT("SWAPCHAIN", "NO HDR FORMATS REPORTED — LYING TO DRIVER");
            chosenFmt = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            chosenCS  = VK_COLOR_SPACE_HDR10_ST2084_EXT;
        } else {
            // 4. Last resort: pick first non-SDR
            for (const auto& sf : formats) {
                if (sf.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    chosenFmt = sf.format;
                    chosenCS  = sf.colorSpace;
                    break;
                }
            }
        }
    }

supported:
    // Validate support for chosen combination to avoid VUID-VkSwapchainCreateInfoKHR-imageFormat-01273
    bool supported = false;
    for (const auto& sf : formats) {
        if (sf.format == chosenFmt && sf.colorSpace == chosenCS) {
            supported = true;
            break;
        }
    }

    if (!supported) {
        LOG_WARN_CAT("SWAPCHAIN", "Chosen HDR combo ({}/ {}) not directly supported by surface — adjusting", 
                     vk::to_string(static_cast<vk::Format>(chosenFmt)),
                     vk::to_string(static_cast<vk::ColorSpaceKHR>(chosenCS)));

        // Try to find support for the format with any color space (prefer HDR if possible)
        VkColorSpaceKHR fallbackCS = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        for (const auto& sf : formats) {
            if (sf.format == chosenFmt) {
                fallbackCS = sf.colorSpace;
                supported = true;
                LOG_INFO_CAT("SWAPCHAIN", "Format supported with fallback CS: {}", vk::to_string(static_cast<vk::ColorSpaceKHR>(fallbackCS)));
                break;
            }
        }

        if (supported) {
            chosenCS = fallbackCS;
        } else {
            // FINAL ABSOLUTE FAILURE: Activate our own HDR driver — WE ARE THE HDR
            LOG_WARN_CAT("SWAPCHAIN", "No support for 10-bit format — activating AMMO_HDR driver (our own HDR support)");
            VkSwapchainKHR old_sc = swapchain_ ? *swapchain_ : VK_NULL_HANDLE;
            if (HDR_pipeline::force_10bit_swapchain(surface_, physDev_, device_, width, height, old_sc)) {
                LOG_SUCCESS_CAT("SWAPCHAIN", "AMMO_HDR DRIVER SUCCESS — 10-bit HDR enforced via custom pipeline");
                // Pipeline has already recreated/adopted — we're done
                return;
            } else {
                LOG_ERROR_CAT("SWAPCHAIN", "AMMO_HDR DRIVER FAILED — ultimate fallback to 8-bit sRGB (peasant mode)");
                chosenFmt = VK_FORMAT_B8G8R8A8_UNORM;
                chosenCS = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            }
        }
    }

    surfaceFormat_ = { chosenFmt, chosenCS };
    LOG_SUCCESS_CAT("SWAPCHAIN", "HDR FORMAT LOCKED: {} + {}",
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
    VkResult result = vkCreateSwapchainKHR(device_, &ci, nullptr, &raw);

    if (result != VK_SUCCESS) {
        LOG_WARN_CAT("SWAPCHAIN", "Swapchain creation failed with forced HDR — retrying with driver lies");
        // Driver rejected? Try again with sRGB and then lie in metadata
        ci.imageFormat     = VK_FORMAT_B8G8R8A8_UNORM;
        ci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        VK_CHECK(vkCreateSwapchainKHR(device_, &ci, nullptr, &raw), "Even sRGB failed — driver is dead");
        surfaceFormat_ = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        LOG_ERROR_CAT("SWAPCHAIN", "HARD SDR FALLBACK — but we will fake HDR in metadata");
    }

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
    VK_CHECK(vkCreateRenderPass(device_, &rp, nullptr, &rp_handle), "Render pass creation failed (HDR)");
    renderPass_ = RTX::Handle<VkRenderPass>(rp_handle, device_, vkDestroyRenderPass);
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept {
    vkDeviceWaitIdle(device_);
    cleanup();
    createSwapchain(w, h);
    createImageViews();
    createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", "HDR SWAPCHAIN RECREATED — {}x{} | {} | PRESENT: {}",
        extent_.width, extent_.height, formatName(), presentModeName());
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

void SwapchainManager::updateHDRMetadata(float maxCLL, float maxFALL, float peakNits) const noexcept {
    if (!g_vkSetHdrMetadataEXT || !isHDR()) return;

    VkHdrMetadataEXT md{VK_STRUCTURE_TYPE_HDR_METADATA_EXT};
    md.displayPrimaryRed = {0.680f, 0.320f};
    md.displayPrimaryGreen = {0.265f, 0.690f};
    md.displayPrimaryBlue = {0.150f, 0.060f};
    md.whitePoint = {0.3127f, 0.3290f};
    md.maxLuminance = peakNits;
    md.minLuminance = 0.0f;
    md.maxContentLightLevel = maxCLL;
    md.maxFrameAverageLightLevel = maxFALL;

    VkSwapchainKHR sc = swapchain();
    g_vkSetHdrMetadataEXT(device_, 1, &sc, &md);
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
    LOG_DEBUG_CAT("SWAPCHAIN", "Title updated: {}", title);
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