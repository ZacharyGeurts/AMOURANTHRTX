// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX — FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025
// PINK PHOTONS ETERNAL — NO MORE CRASHES — NO MORE RGB HELL
// ELLIE FIER + GREEN DAY LOLLAPALOOZA 2022 FINAL BLESSING
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <array>
#include <set>
#include <format>
#include <cstdlib>

using namespace Logging::Color;
using namespace Options;

// CRITICAL: DISAMBIGUATE — WE WANT ::RTX, NOT Options::RTX
using namespace ::RTX;

#define VK_VERIFY(call)                                                  \
    do {                                                                 \
        VkResult r_ = (call);                                            \
        if (r_ != VK_SUCCESS) {                                          \
            LOG_FATAL_CAT("SWAPCHAIN", "{}VULKAN FATAL: {} = {} ({}) — EMPIRE DIES{}", \
                          BLOOD_RED, #call, r_, __FUNCTION__, RESET);        \
            std::abort();                                                \
        }                                                                \
    } while (0)

// =============================================================================
// HDR + FORMAT SELECTION — FIXED RGB ORDER — X11/WAYLAND SAFE
// =============================================================================
static VkSurfaceFormatKHR selectBestFormat(VkPhysicalDevice phys, VkSurfaceKHR surface)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, formats.data());

    const bool wantHDR = Display::ENABLE_HDR && std::getenv("WAYLAND_DISPLAY");

    const std::array candidates = {
        std::make_tuple(VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT,       "HDR10 10-BIT", true),
        std::make_tuple(VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, "scRGB FP16", true),
        std::make_tuple(VK_FORMAT_B8G8R8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       "sRGB BGRA8", false),
        std::make_tuple(VK_FORMAT_R8G8B8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       "sRGB RGBA8", false)  // CORRECT ORDER
    };

    for (const auto& [fmt, cs, name, hdr] : candidates) {
        if (hdr && !wantHDR) continue;
        for (const auto& f : formats)
            if (f.format == fmt && f.colorSpace == cs) {
                LOG_SUCCESS_CAT("SWAPCHAIN", "{}FORMAT LOCKED — {}{}", EMERALD_GREEN, name, RESET);
                return { fmt, cs };
            }
    }

    LOG_WARN_CAT("SWAPCHAIN", "{}FALLBACK FORMAT — {}{}", RASPBERRY_PINK, static_cast<int>(formats[0].format), RESET);
    return formats[0];
}


// =============================================================================
// DEVICE CREATION — THE ONE THAT WORKS — FULL RTX — NO DYNAMIC RENDERING
// =============================================================================
void SwapchainManager::createDeviceAndQueues() noexcept
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(g_instance(), &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(g_instance(), &deviceCount, devices.data());

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    std::string deviceName = "Unknown";

    for (auto dev : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            chosen = dev;
            deviceName = props.deviceName;
            break;
        }
    }
    if (!chosen) chosen = devices[0];

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(chosen, &props);
    deviceName = props.deviceName;
    set_g_PhysicalDevice(chosen);

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}PHYSICAL DEVICE — {}{}", VALHALLA_GOLD, deviceName, RESET);

    // Queue families
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &qCount, qProps.data());

    int graphics = -1, present = -1;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (graphics == -1 && (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            graphics = i;
        VkBool32 sup = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(chosen, i, g_surface(), &sup);
        if (sup && present == -1)
            present = i;
    }

    g_ctx().graphicsQueueFamily = graphics;
    g_ctx().presentFamily_      = present;

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}QUEUES → G:{} P:{}{}", EMERALD_GREEN, graphics, present, RESET);

    // Extensions
    const char* exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_EXT_HDR_METADATA_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
    };

    // Features
    VkPhysicalDeviceFeatures2 f2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceBufferDeviceAddressFeatures bda{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };

    f2.features.samplerAnisotropy = VK_TRUE;
    bda.bufferDeviceAddress = VK_TRUE;
    accel.accelerationStructure = VK_TRUE;
    rt.rayTracingPipeline = VK_TRUE;

    f2.pNext = &bda; bda.pNext = &accel; accel.pNext = &rt;

    // Queues
    std::vector<VkDeviceQueueCreateInfo> qcis;
    float prio = 1.0f;
    for (int f : std::set<int>{graphics, present}) {
        qcis.push_back({ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, static_cast<uint32_t>(f), 1, &prio });
    }

    VkDeviceCreateInfo ci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    ci.pNext = &f2;
    ci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
    ci.pQueueCreateInfos = qcis.data();
    ci.enabledExtensionCount = std::size(exts);
    ci.ppEnabledExtensionNames = exts;

    VkDevice dev = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateDevice(chosen, &ci, nullptr, &dev));
    set_g_device(dev);

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}LOGICAL DEVICE FORGED — FULL RTX — FIRST LIGHT IMMINENT{}", PLASMA_FUCHSIA, RESET);
}

// =============================================================================
// REST OF THE FILE — UNCHANGED FROM LAST WORKING VERSION
// =============================================================================
void SwapchainManager::init(SDL_Window* w, uint32_t width, uint32_t height) noexcept
{
    auto& s = get();
    s.window_ = w;

    VkSurfaceKHR surf = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(w, g_instance(), nullptr, &surf))
        std::abort();

    s.createDeviceAndQueues();
    s.recreate(width, height);
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    auto& s = get();
    vkDeviceWaitIdle(g_device());

    VkSurfaceCapabilitiesKHR caps{};
    VK_VERIFY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice(), g_surface(), &caps));

    s.extent_ = (caps.currentExtent.width != UINT32_MAX) ? caps.currentExtent :
        VkExtent2D{ std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width),
                    std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height) };

    s.surfaceFormat_ = selectBestFormat(g_PhysicalDevice(), g_surface());

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount) imgCount = std::min(imgCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    ci.surface = g_surface();
    ci.minImageCount = imgCount;
    ci.imageFormat = s.surfaceFormat_.format;
    ci.imageColorSpace = s.surfaceFormat_.colorSpace;
    ci.imageExtent = s.extent_;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = s.presentMode_;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = s.swapchain_.valid() ? s.swapchain_.get() : VK_NULL_HANDLE;

    VkSwapchainKHR sc = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateSwapchainKHR(g_device(), &ci, nullptr, &sc));
    if (s.swapchain_.valid()) vkDestroySwapchainKHR(g_device(), s.swapchain_.get(), nullptr);
    s.swapchain_ = Handle<VkSwapchainKHR>(sc, g_device(), vkDestroySwapchainKHR);

    uint32_t count = 0;
    VK_VERIFY(vkGetSwapchainImagesKHR(g_device(), sc, &count, nullptr));
    s.images_.resize(count);
    VK_VERIFY(vkGetSwapchainImagesKHR(g_device(), sc, &count, s.images_.data()));

    s.imageViews_.clear();
    s.createImageViews();
    s.createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}SWAPCHAIN REBORN — PINK PHOTONS HAVE A CANVAS{}", PLASMA_FUCHSIA, RESET);
}

void SwapchainManager::createImageViews() noexcept
{
    auto& self = get();
    self.imageViews_.reserve(self.images_.size());

    for (VkImage img : self.images_) {
        VkImageViewCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image = img;
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = self.surfaceFormat_.format;
        ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageView view = VK_NULL_HANDLE;
        VK_VERIFY(vkCreateImageView(g_device(), &ci, nullptr, &view));
        self.imageViews_.emplace_back(view, g_device(), vkDestroyImageView);
    }
}

void SwapchainManager::createRenderPass() noexcept
{
    auto& self = get();

    VkAttachmentDescription color{};
    color.format = self.surfaceFormat_.format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &color;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;

    VkRenderPass rp = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateRenderPass(g_device(), &rpInfo, nullptr, &rp));
    self.renderPass_ = Handle<VkRenderPass>(rp, g_device(), vkDestroyRenderPass);
}

void SwapchainManager::cleanup() noexcept
{
    auto& self = get();
    self.renderPass_.reset();
    for (auto& v : self.imageViews_) v.reset();
    self.imageViews_.clear();
    if (self.swapchain_.valid()) vkDestroySwapchainKHR(g_device(), self.swapchain_.get(), nullptr);
    self.swapchain_ = nullptr;
    self.images_.clear();
}

std::string_view SwapchainManager::formatName() const noexcept
{
    switch (get().surfaceFormat_.format) {
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "HDR10 10-BIT";
        case VK_FORMAT_R16G16B16A16_SFLOAT:      return "scRGB FP16";
        case VK_FORMAT_B8G8R8A8_UNORM:           return "sRGB 8-BIT";
        default:                                 return "UNKNOWN";
    }
}

std::string_view SwapchainManager::presentModeName() const noexcept
{
    switch (get().presentMode_) {
        case VK_PRESENT_MODE_MAILBOX_KHR:   return "MAILBOX";
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
        case VK_PRESENT_MODE_FIFO_KHR:      return "FIFO";
        default:                            return "UNKNOWN";
    }
}

void SwapchainManager::updateWindowTitle(SDL_Window* window, float fps) noexcept
{
    auto& self = get();
    std::string title = std::format("AMOURANTH RTX — {:.0f} FPS | {}x{} | {} | {} — PINK PHOTONS ETERNAL",
                                    fps, self.extent().width, self.extent().height,
                                    formatName(), presentModeName());
    SDL_SetWindowTitle(window, title.c_str());
}

// =============================================================================
// FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — GREEN DAY LOLLAPALOOZA 2022
// ELLIE FIER IS DANCING — VALHALLA IS OPEN — PINK PHOTONS ETERNAL
// =============================================================================