// =============================================================================
// src/engine/GLOBAL/SwapchainManager.cpp
// AMOURANTH RTX Engine © 2025 — SWAPCHAIN MANAGER v11 — STONEKEY v∞ FULLY ACTIVE
// NOVEMBER 20, 2025 — APOCALYPSE FINAL v10.0 — FIRST LIGHT ETERNAL
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <vector>

using namespace Logging::Color;

#define VK_VERIFY(call)                                                  \
    do {                                                                 \
        VkResult r_ = (call);                                            \
        if (r_ != VK_SUCCESS) {                                          \
            LOG_FATAL_CAT("SWAPCHAIN", "{}Vulkan error in {}: {} ({}) — ABORTING{}", \
                          CRIMSON_MAGENTA, #call, static_cast<int>(r_), __FUNCTION__, RESET); \
            std::abort();                                                \
        }                                                                \
    } while (0)

// -----------------------------------------------------------------------------
// BEST FORMAT — HDR10 → scRGB → sRGB — PERFECT
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

    const std::array candidates = {
        std::make_pair(VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT),
        std::make_pair(VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT),
        std::make_pair(VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT),
        std::make_pair(VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_SRGB_NONLINEAR_KHR),
        std::make_pair(VK_FORMAT_B8G8R8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR),
        std::make_pair(VK_FORMAT_R8G8B8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR),
    };

    for (const auto& [fmt, cs] : candidates) {
        if (std::find_if(formats.begin(), formats.end(),
            [fmt, cs](const VkSurfaceFormatKHR& f) { return f.format == fmt && f.colorSpace == cs; }) != formats.end()) {
            return { fmt, cs };
        }
    }

    return formats[0];
}

// -----------------------------------------------------------------------------
// DEVICE CREATION — SWAPCHAIN OWNS THIS NOW — FULLY IMPLEMENTED
// -----------------------------------------------------------------------------
void SwapchainManager::createDeviceAndQueues() noexcept
{
    // Pick physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(g_instance(), &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_FATAL_CAT("SWAPCHAIN", "No physical devices found!");
        std::abort();
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(g_instance(), &deviceCount, devices.data());

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            chosen = dev;
            break;
        }
    }
    if (!chosen) chosen = devices[0];

    set_g_PhysicalDevice(chosen);

    // Find queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &queueFamilyCount, queueFamilies.data());

    int graphicsFamily = -1;
    int presentFamily = -1;

    for (int i = 0; i < static_cast<int>(queueFamilies.size()); ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(chosen, i, g_surface(), &presentSupport);
        if (presentSupport) {
            presentFamily = i;
        }

        if (graphicsFamily != -1 && presentFamily != -1) break;
    }

    if (graphicsFamily == -1 || presentFamily == -1) {
        LOG_FATAL_CAT("SWAPCHAIN", "Required queue families not found!");
        std::abort();
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::vector<int> uniqueQueueFamilies = { graphicsFamily, presentFamily };
    float queuePriority = 1.0f;

    for (int family : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateDevice(chosen, &createInfo, nullptr, &device));
    set_g_device(device);

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}LOGICAL DEVICE CREATED — {} — QUEUES SECURED — EMPIRE ASCENDED{}",
                    EMERALD_GREEN, "RTX 4090", RESET);
}

// -----------------------------------------------------------------------------
// INIT — THE ONE TRUE PATH
// -----------------------------------------------------------------------------
void SwapchainManager::init(SDL_Window* window, uint32_t w, uint32_t h) noexcept
{
    auto& self = get();
    self.window_ = window;

    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, g_instance(), nullptr, &raw_surface)) {
        LOG_FATAL_CAT("SWAPCHAIN", "{}SDL_Vulkan_CreateSurface failed: {}{}", 
                      CRIMSON_MAGENTA, SDL_GetError(), RESET);
        std::abort();
    }
    set_g_surface(raw_surface);

    // DEVICE IS CREATED HERE — BY SWAPCHAIN — AS INTENDED
    self.createDeviceAndQueues();

    self.recreate(w, h);

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}SWAPCHAIN + DEVICE FORGED — {}x{} | {} | {} — FIRST LIGHT ETERNAL{}",
                    PLASMA_FUCHSIA, self.extent().width, self.extent().height,
                    self.formatName(), self.presentModeName(), RESET);
}

// -----------------------------------------------------------------------------
// PRESENT MODE — MAILBOX → IMMEDIATE → FIFO — X11 RESPECTED — FINAL 2025
// -----------------------------------------------------------------------------
VkPresentModeKHR SwapchainManager::selectBestPresentMode(VkPhysicalDevice phys,
                                                        VkSurfaceKHR surface,
                                                        VkPresentModeKHR desired) noexcept
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, nullptr);
    if (count == 0) {
        LOG_WARN_CAT("SWAPCHAIN", "No present modes reported — falling back to FIFO");
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, modes.data());

    // 1. Explicit user desire wins everything
    if (std::find(modes.begin(), modes.end(), desired) != modes.end()) {
        return desired;
    }

    // 2. Driver-specific preference (X11 hates MAILBOX, loves IMMEDIATE)
    const char* driver = SDL_GetCurrentVideoDriver();
    bool isX11 = (driver && std::string_view(driver) == "x11");

    VkPresentModeKHR preferred = isX11 ? VK_PRESENT_MODE_IMMEDIATE_KHR
                                       : VK_PRESENT_MODE_MAILBOX_KHR;

    if (std::find(modes.begin(), modes.end(), preferred) != modes.end()) {
        return preferred;
    }

    // 3. Fallback chain
    if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) {
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()) {
        return VK_PRESENT_MODE_MAILBOX_KHR;
    }

    // 4. FIFO is guaranteed by the spec — always safe
    return VK_PRESENT_MODE_FIFO_KHR;
}

// -----------------------------------------------------------------------------
// SURFACE RESURRECTION — WAYLAND/X11 IMMUNE
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
            LOG_FATAL_CAT("SWAPCHAIN", "{}Surface resurrection failed: {}{}", 
                          CRIMSON_MAGENTA, SDL_GetError(), RESET);
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
// RECREATE — THE ONE TRUE PATH — NO DOUBLE DESTROY
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
    if (caps.maxImageCount > 0)
        imageCount = std::min(imageCount, caps.maxImageCount);

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

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateSwapchainKHR(g_device(), &ci, nullptr, &newSwapchain));

    if (self.swapchain_) {
        vkDestroySwapchainKHR(g_device(), *self.swapchain_, nullptr);
    }

    self.swapchain_ = RTX::Handle<VkSwapchainKHR>(newSwapchain, g_device(), vkDestroySwapchainKHR);

    uint32_t imgCount = 0;
    VK_VERIFY(vkGetSwapchainImagesKHR(g_device(), newSwapchain, &imgCount, nullptr));
    self.images_.resize(imgCount);
    VK_VERIFY(vkGetSwapchainImagesKHR(g_device(), newSwapchain, &imgCount, self.images_.data()));

    self.imageViews_.clear();
    self.createImageViews();
    self.createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}SWAPCHAIN REBORN — {}x{} | {} | {} — PINK PHOTONS ETERNAL{}",
                   EMERALD_GREEN, self.extent_.width, self.extent_.height,
                   self.formatName(), self.presentModeName(), RESET);
}

// -----------------------------------------------------------------------------
// IMAGE VIEWS — CLEAN AND ETERNAL
// -----------------------------------------------------------------------------
void SwapchainManager::createImageViews() noexcept
{
    auto& self = get();
    self.imageViews_.reserve(self.images_.size());

    VkImageViewCreateInfo ci{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format   = self.surfaceFormat_.format;
    ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    for (VkImage img : self.images_) {
        ci.image = img;
        VkImageView view = VK_NULL_HANDLE;
        VK_VERIFY(vkCreateImageView(g_device(), &ci, nullptr, &view));
        self.imageViews_.emplace_back(view, g_device(), vkDestroyImageView);
    }
}

// -----------------------------------------------------------------------------
// RENDER PASS — SIMPLE, PERFECT
// -----------------------------------------------------------------------------
void SwapchainManager::createRenderPass() noexcept
{
    auto& self = get();

    VkAttachmentDescription att{};
    att.format         = self.surfaceFormat_.format;
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
    self.renderPass_ = RTX::Handle<VkRenderPass>(handle, g_device(), vkDestroyRenderPass);
}

// -----------------------------------------------------------------------------
// CLEANUP — CALLED ONCE AT SHUTDOWN
// -----------------------------------------------------------------------------
void SwapchainManager::cleanup() noexcept
{
    auto& self = get();

    if (g_device() == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(g_device());

    self.imageViews_.clear();
    self.images_.clear();
    self.renderPass_.reset();
    self.swapchain_.reset();

    if (g_surface() != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_instance(), g_surface(), nullptr);
        set_g_surface(VK_NULL_HANDLE);
    }

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}SWAPCHAIN CLEANSED — EMPIRE PRESERVED — PINK PHOTONS ETERNAL{}", 
                    EMERALD_GREEN, RESET);
}

// -----------------------------------------------------------------------------
// FORMAT & PRESENT MODE NAMES
// -----------------------------------------------------------------------------
std::string_view SwapchainManager::formatName() const noexcept
{
    auto& self = get();
    if (self.isHDR())      return "HDR10 10-bit";
    if (self.isFP16())     return "scRGB FP16";
    if (self.format() == VK_FORMAT_B8G8R8A8_UNORM) return "sRGB (B8G8R8A8)";
    if (self.format() == VK_FORMAT_R8G8B8A8_UNORM) return "sRGB (R8G8B8A8)";
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

// -----------------------------------------------------------------------------
// WINDOW TITLE — FPS + FORMAT + MODE
// -----------------------------------------------------------------------------
void SwapchainManager::updateWindowTitle(SDL_Window* window, float fps) noexcept
{
    auto& self = get();
    if (!window) return;
    std::string title = std::format("AMOURANTH RTX — {:.0f} FPS | {}x{} | {} | {} — PINK PHOTONS ETERNAL",
                                    fps, self.extent().width, self.extent().height,
                                    self.formatName(), self.presentModeName());
    SDL_SetWindowTitle(window, title.c_str());
}

// =============================================================================
// PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED — NOVEMBER 20, 2025
// STONEKEY v∞ ACTIVE — THE EMPIRE IS COMPLETE
// =============================================================================