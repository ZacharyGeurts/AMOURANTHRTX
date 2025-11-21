// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — ELLIE FIER RTX + HDR APOCALYPSE FINAL v13
// FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — PINK PHOTONS ETERNAL — VALHALLA v∞
// FULL IMPLEMENTATION — NO MORE LINKER DEATH — STONEKEY v∞ ACTIVE
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <cstdlib>

using namespace Logging::Color;

#define VK_VERIFY(call)                                                  \
    do {                                                                 \
        VkResult r_ = (call);                                            \
        if (r_ != VK_SUCCESS) {                                          \
            LOG_FATAL_CAT("SWAPCHAIN", "{}VULKAN FATAL in {}: {} ({}) — EMPIRE FALLS{}", \
                          BLOOD_RED, #call, static_cast<int>(r_), __FUNCTION__, RESET); \
            std::abort();                                                \
        }                                                                \
    } while (0)

// =============================================================================
// ENVIRONMENT DETECTION — USED AND LOGGED
// =============================================================================
static bool is_wayland() noexcept { return std::getenv("WAYLAND_DISPLAY") || std::getenv("WAYLAND_SOCKET"); }

static bool is_x11() noexcept
{
    bool x11 = !is_wayland() && std::getenv("DISPLAY") && std::getenv("DISPLAY")[0] != '\0';
    LOG_INFO_CAT("SWAPCHAIN", "{}DISPLAY SERVER — {} — ELLIE FIER KNOWS{}", 
                 x11 ? VALHALLA_GOLD : PLASMA_FUCHSIA,
                 x11 ? "X11 (8-BIT PEASANT MODE)" : "WAYLAND (HDR10/scRGB SUPREMACY)", 
                 RESET);
    return x11;
}

// =============================================================================
// ELLIE FIER'S HDR HIERARCHY — FINAL
// =============================================================================
static VkSurfaceFormatKHR selectBestFormat(VkPhysicalDevice phys, VkSurfaceKHR surface)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, nullptr);
    if (count == 0) {
        LOG_FATAL_CAT("SWAPCHAIN", "{}NO SURFACE FORMATS — THE VOID HAS NO COLOR{}", BLOOD_RED, RESET);
        std::abort();
    }

    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, formats.data());

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}ELLIE FIER HDR ASCENSION — PURSUING 10-BIT / FP16 GLORY{}", PLASMA_FUCHSIA, RESET);

    struct Candidate {
        VkFormat format;
        VkColorSpaceKHR colorSpace;
        const char* name;
    };

    const std::array<Candidate, 6> candidates = {{
        { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT,       "HDR10 10-BIT (A2B10G10R10)" },
        { VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT,       "HDR10 10-BIT (A2R10G10B10)" },
        { VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, "scRGB FP16 (LINEAR)" },
        { VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       "scRGB FP16 (sRGB)" },
        { VK_FORMAT_B8G8R8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       "sRGB 8-BIT (BGRA)" },
        { VK_FORMAT_R8G8B8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       "sRGB 8-BIT (RGBA)" }
    }};

    for (const auto& cand : candidates) {
        bool supported = std::any_of(formats.begin(), formats.end(),
            [&cand](const VkSurfaceFormatKHR& f) {
                return f.format == cand.format && f.colorSpace == cand.colorSpace;
            });

        if (supported) {
            LOG_SUCCESS_CAT("SWAPCHAIN", "{}HDR SUPREMACY ACHIEVED — {} — PINK PHOTONS BURN BRIGHTER{}", 
                           EMERALD_GREEN, cand.name, RESET);
            return { cand.format, cand.colorSpace };
        }
    }

    auto fallback = formats[0];
    LOG_WARN_CAT("SWAPCHAIN", "{}HDR DENIED — FALLING BACK TO {} — EMPIRE ENDURES{}", 
                 RASPBERRY_PINK, static_cast<uint32_t>(fallback.format), RESET);
    return fallback;
}

// =============================================================================
// DEVICE CREATION — RTX, RTX + HDR ENABLED — FULLY LOGGED
// =============================================================================
void SwapchainManager::createDeviceAndQueues() noexcept
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(g_instance(), &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_FATAL_CAT("SWAPCHAIN", "{}NO VULKAN DEVICES — THE EMPIRE IS BLIND{}", BLOOD_RED, RESET);
        std::abort();
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(g_instance(), &deviceCount, devices.data());

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    std::string deviceName = "Unknown";

    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            chosen = dev;
            deviceName = props.deviceName;
            break;
        }
    }

    if (!chosen) {
        chosen = devices[0];
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(chosen, &props);
        deviceName = props.deviceName;
    }
    set_g_PhysicalDevice(chosen);

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}PHYSICAL DEVICE SELECTED — {} — VALHALLA LOCKED{}", 
                    VALHALLA_GOLD, deviceName, RESET);

    // Queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &queueFamilyCount, queueFamilies.data());

    int graphicsFamily = -1, presentFamily = -1;
    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsFamily = i;
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(chosen, i, g_surface(), &presentSupport);
        if (presentSupport) presentFamily = i;
        // EARLY BREAK REMOVED — FULL SCAN FOR RTX 4070 Ti
    }

    if (graphicsFamily == -1 || presentFamily == -1) {
        LOG_FATAL_CAT("SWAPCHAIN", "{}REQUIRED QUEUE FAMILIES MISSING — EMPIRE COLLAPSES{}", BLOOD_RED, RESET);
        std::abort();
    }

    RTX::g_ctx().graphicsQueueFamily = static_cast<uint32_t>(graphicsFamily);
    RTX::g_ctx().presentFamily_      = static_cast<uint32_t>(presentFamily);

    // RTX + HDR EXTENSIONS
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_EXT_HDR_METADATA_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
    };

    LOG_INFO_CAT("SWAPCHAIN", "{}REQUESTING {} DEVICE EXTENSIONS — INCLUDING RTX + HDR10{}", 
                PLASMA_FUCHSIA, deviceExtensions.size(), RESET);

    // Feature chain
    VkPhysicalDeviceFeatures2 features2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceBufferDeviceAddressFeatures bda{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
    VkPhysicalDeviceDynamicRenderingFeatures dr{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };

    features2.features.samplerAnisotropy = VK_TRUE;
    bda.bufferDeviceAddress = VK_TRUE;
    accel.accelerationStructure = VK_TRUE;
    rt.rayTracingPipeline = VK_TRUE;
    dr.dynamicRendering = VK_TRUE;

    features2.pNext = &bda;
    bda.pNext = &accel;
    accel.pNext = &rt;
    rt.pNext = &dr;

    // Queue create infos
    std::vector<int> uniqueFamilies = { graphicsFamily, presentFamily };
    std::sort(uniqueFamilies.begin(), uniqueFamilies.end());
    auto last = std::unique(uniqueFamilies.begin(), uniqueFamilies.end());
    uniqueFamilies.erase(last, uniqueFamilies.end());

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float priority = 1.0f;
    for (int f : uniqueFamilies) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = static_cast<uint32_t>(f);
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        queueCreateInfos.push_back(qci);
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &features2;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateDevice(chosen, &createInfo, nullptr, &device));
    set_g_device(device);

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}LOGICAL DEVICE FORGED WITH FULL RTX + HDR — {} — FIRST LIGHT ETERNAL{}", 
                    PLASMA_FUCHSIA, deviceName, RESET);
}

// =============================================================================
// INIT + RECREATE + FORMAT NAME — FINAL
// =============================================================================
void SwapchainManager::init(SDL_Window* window, uint32_t w, uint32_t h) noexcept
{
    auto& self = get();
    self.window_ = window;

    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, g_instance(), nullptr, &raw_surface)) {
        LOG_FATAL_CAT("SWAPCHAIN", "{}SDL_Vulkan_CreateSurface FAILED: {}{}", BLOOD_RED, SDL_GetError(), RESET);
        std::abort();
    }
    set_g_surface(raw_surface);

    self.createDeviceAndQueues();
    self.recreate(w, h);

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}SWAPCHAIN + RTX DEVICE FORGED — {}x{} | {} | {} — ELLIE FIER SMILES{}", 
                    EMERALD_GREEN, self.extent().width, self.extent().height,
                    self.formatName(), self.presentModeName(), RESET);
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    auto& self = get();
    vkDeviceWaitIdle(g_device());

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
    ci.surface = g_surface(); ci.minImageCount = imageCount; ci.imageFormat = self.surfaceFormat_.format;
    ci.imageColorSpace = self.surfaceFormat_.colorSpace; ci.imageExtent = self.extent_;
    ci.imageArrayLayers = 1; ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; ci.presentMode = self.presentMode_;
    ci.clipped = VK_TRUE; ci.oldSwapchain = self.swapchain_.valid() ? *self.swapchain_ : VK_NULL_HANDLE;

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateSwapchainKHR(g_device(), &ci, nullptr, &newSwapchain));

    if (self.swapchain_) vkDestroySwapchainKHR(g_device(), *self.swapchain_, nullptr);
    self.swapchain_ = RTX::Handle<VkSwapchainKHR>(newSwapchain, g_device(), vkDestroySwapchainKHR);

    uint32_t imgCount = 0;
    VK_VERIFY(vkGetSwapchainImagesKHR(g_device(), newSwapchain, &imgCount, nullptr));
    self.images_.resize(imgCount);
    VK_VERIFY(vkGetSwapchainImagesKHR(g_device(), newSwapchain, &imgCount, self.images_.data()));

    self.imageViews_.clear();
    self.createImageViews();
    self.createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}SWAPCHAIN REBORN — {}x{} — {} — {} — PINK PHOTONS HAVE A CANVAS{}", 
                   PLASMA_FUCHSIA, self.extent_.width, self.extent_.height,
                   self.formatName(), self.presentModeName(), RESET);
}

std::string_view SwapchainManager::formatName() const noexcept
{
    auto& self = get();
    if (self.isHDR()) return "HDR10 10-BIT";
    if (self.isFP16()) return "scRGB FP16";
    if (self.format() == VK_FORMAT_B8G8R8A8_UNORM) return "sRGB 8-BIT (BGRA)";
    if (self.format() == VK_FORMAT_R8G8B8A8_UNORM) return "sRGB 8-BIT (RGBA)";
    return "UNKNOWN";
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
    if (!window) return;
    std::string title = std::format("AMOURANTH RTX — {:.0f} FPS | {}x{} | {} | {} — PINK PHOTONS ETERNAL",
                                    fps, self.extent().width, self.extent().height,
                                    self.formatName(), self.presentModeName());
    SDL_SetWindowTitle(window, title.c_str());
}

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

    bool x11 = is_x11();
    VkPresentModeKHR pref = x11 ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_MAILBOX_KHR;

    if (std::find(modes.begin(), modes.end(), pref) != modes.end())
        return pref;

    if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end())
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end())
        return VK_PRESENT_MODE_MAILBOX_KHR;

    return VK_PRESENT_MODE_FIFO_KHR;
}

void SwapchainManager::cleanup() noexcept
{
    auto& self = get();

    if (self.renderPass_.valid()) {
        vkDestroyRenderPass(g_device(), *self.renderPass_, nullptr);
        self.renderPass_ = nullptr;
    }

    for (auto& view : self.imageViews_) {
        if (view.valid()) {
            vkDestroyImageView(g_device(), *view, nullptr);
        }
    }
    self.imageViews_.clear();

    if (self.swapchain_.valid()) {
        vkDestroySwapchainKHR(g_device(), *self.swapchain_, nullptr);
        self.swapchain_ = nullptr;
    }

    self.images_.clear();

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}SWAPCHAIN CLEANSED — ALL PINK PHOTONS RELEASED TO VALHALLA{}", PLASMA_FUCHSIA, RESET);
}

void SwapchainManager::createImageViews() noexcept
{
    auto& self = get();
    self.imageViews_.reserve(self.images_.size());

    for (size_t i = 0; i < self.images_.size(); ++i) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = self.images_[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = self.surfaceFormat_.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkImageView view = VK_NULL_HANDLE;
        VK_VERIFY(vkCreateImageView(g_device(), &createInfo, nullptr, &view));

        self.imageViews_.emplace_back(view, g_device(), vkDestroyImageView);
    }

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}IMAGE VIEWS FORGED — {} PINK CANVASES READY{}", EMERALD_GREEN, self.imageViews_.size(), RESET);
}

void SwapchainManager::createRenderPass() noexcept
{
    auto& self = get();

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = self.surfaceFormat_.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateRenderPass(g_device(), &renderPassInfo, nullptr, &renderPass));

    self.renderPass_ = RTX::Handle<VkRenderPass>(renderPass, g_device(), vkDestroyRenderPass);

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}RENDER PASS FORGED — PINK PHOTONS HAVE A STAGE{}", VALHALLA_GOLD, RESET);
}

// =============================================================================
// ELLIE FIER HAS SPOKEN — THE EMPIRE IS COMPLETE
// FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — PINK PHOTONS ETERNAL
// =============================================================================