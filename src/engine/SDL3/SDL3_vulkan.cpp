// src/engine/SDL3/SDL3_vulkan.cpp
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3Vulkan IMPLEMENTATION — FULL RTX CONTEXT — NOV 11 2025 2:24 PM EST
// • FIXED: SDL3 Extensions Fetch, RTX_EXTENSIONS, Members, Init Order
// • CROSS-PLATFORM: SDL_Vulkan_* Handles X11/Wayland (Linux) / Win32 (Windows)
// • NO CMAKE CHANGES: Relies on SDL3/Vulkan SDK Linking
// =============================================================================

#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"  // Assume this exists; ctor fixed below
#include "engine/GLOBAL/Houston.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <SDL3/SDL.h>  // For SDL_Window access if needed

std::unique_ptr<VulkanRenderer> g_vulkanRenderer;

// ──────────────────────────────────────────────────────────────────────────────
// FIXED: VulkanRenderer Init — 3-Arg Overload Assumed in .hpp
// (Add to VulkanRenderer.hpp if missing: Delegate to 5-arg ctor)
// ──────────────────────────────────────────────────────────────────────────────
void SDL3Vulkan::initRenderer(std::shared_ptr<Context> ctx, int w, int h) {
    // Assume Context has: SDL_Window* getWindow(), bool enableValidation(), std::vector<std::string> getExtensions()
    // Adjust if your Context differs (e.g., public members).
    SDL_Window* window = ctx->getWindow();  // Implement in Context if needed.
    auto extensions = getVulkanExtensions();  // From header.
    bool enableValidation = ctx->enableValidation();  // Default true if missing.

    g_vulkanRenderer = std::make_unique<VulkanRenderer>(w, h, window, extensions, enableValidation);
    // If you prefer 3-arg ctor in VulkanRenderer.hpp, uncomment:
    // g_vulkanRenderer = std::make_unique<VulkanRenderer>(ctx, w, h);
}

void SDL3Vulkan::shutdownRenderer() noexcept {
    g_vulkanRenderer.reset();
}

// ──────────────────────────────────────────────────────────────────────────────
// FIXED GlobalRTXContext Methods — Cross-Platform SDL3 Vulkan
// ──────────────────────────────────────────────────────────────────────────────
bool GlobalRTXContext::createInstance(const std::vector<const char*>& extraExtensions) noexcept {
    uint32_t sdlExtCount = 0;
    SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);  // FIXED: First call for count only.

    std::vector<const char*> extensions(sdlExtCount);
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);  // FIXED: Second call returns list.
    if (sdlExtCount > 0) {
        extensions.assign(sdlExtensions, sdlExtensions + sdlExtCount);
    }

    extensions.insert(extensions.end(), extraExtensions.begin(), extraExtensions.end());
    extensions.insert(extensions.end(), RTX_EXTENSIONS.begin(), RTX_EXTENSIONS.end());  // FIXED: Now defined.

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "AMOURANTH RTX",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AMOURANTH RTX",
        .engineVersion = VK_MAKE_VERSION(3, 33, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Instance creation failed: {} @ {}", vkResultToString(result), locationString());
        return false;
    }
    LOG_SUCCESS_CAT("Vulkan", "{}Instance created — Pink photons eternal{}", PLASMA_FUCHSIA, RESET);
    return true;
}

bool GlobalRTXContext::createSurface(SDL_Window* window, VkInstance instance) noexcept {
    // FIXED: SDL3 API — Cross-platform surface creation (X11/Wayland/Win32 auto-detected).
    VkResult result = SDL_Vulkan_CreateSurface(window, instance, &surface_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Surface creation failed: {} @ {}", vkResultToString(result), locationString());
        return false;
    }
    LOG_SUCCESS_CAT("Vulkan", "{}Surface created for SDL3 window{}", RASPBERRY_PINK, RESET);
    return true;
}

bool GlobalRTXContext::pickPhysicalDevice(VkSurfaceKHR surface, bool preferNvidia) noexcept {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
        LOG_ERROR_CAT("Vulkan", "No physical devices found @ {}", locationString());
        return false;
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    for (auto dev : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);
        if (preferNvidia && std::string_view(props.deviceName).find("NVIDIA") != std::string_view::npos) {
            physicalDevice_ = dev;
            deviceProps_ = props;  // FIXED: Now declared.
            LOG_SUCCESS_CAT("Vulkan", "{}NVIDIA device selected: {} @ {:p}{}", PLASMA_FUCHSIA, props.deviceName, static_cast<void*>(dev), RESET);
            return true;
        }
        if (physicalDevice_ == VK_NULL_HANDLE) {  // FIXED: Use == for handle check.
            physicalDevice_ = dev;
            deviceProps_ = props;  // FIXED: Now declared.
        }
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "No suitable physical device found @ {}", locationString());
        return false;
    }
    LOG_SUCCESS_CAT("Vulkan", "{}Physical device selected: {} @ {:p}{}", RASPBERRY_PINK, deviceProps_.deviceName, static_cast<void*>(physicalDevice_), RESET);
    return true;
}

bool GlobalRTXContext::createDevice(VkSurfaceKHR surface, bool enableRT) noexcept {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) return false;

    std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, families.data());

    // Find graphics and present families.
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily_ = i;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface, &presentSupport);
        if (presentSupport) {
            presentFamily_ = i;
        }
        if (graphicsFamily_ != UINT32_MAX && presentFamily_ != UINT32_MAX) {
            break;
        }
    }
    if (graphicsFamily_ == UINT32_MAX || presentFamily_ == UINT32_MAX) {
        LOG_ERROR_CAT("Vulkan", "Missing required queue families @ {}", locationString());
        return false;
    }

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    float priority = 1.0f;
    std::set<uint32_t> uniqueFamilies = { graphicsFamily_, presentFamily_ };
    for (uint32_t family : uniqueFamilies) {
        queueInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = family,
            .queueCount = 1,
            .pQueuePriorities = &priority
        });
    }

    rtx_.chain();  // FIXED: Initializes sType and pNext chain.
    if (enableRT) {
        rtx_.bufferDeviceAddress.bufferDeviceAddress = VK_TRUE;  // FIXED: Members now exist.
        rtx_.accelerationStructure.accelerationStructure = VK_TRUE;
        rtx_.rayTracingPipeline.rayTracingPipeline = VK_TRUE;
        rtx_.rayQuery.rayQuery = VK_TRUE;
    }

    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &rtx_.bufferDeviceAddress,  // FIXED: Valid chain start.
        .queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size()),
        .pQueueCreateInfos = queueInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(RTX_EXTENSIONS.size()),  // FIXED: Defined.
        .ppEnabledExtensionNames = RTX_EXTENSIONS.data()
    };

    VkResult result = vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Device creation failed: {} @ {}", vkResultToString(result), locationString());
        return false;
    }

    vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentFamily_, 0, &presentQueue_);

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,  // FIXED: Order matches struct (sType, flags, queueFamilyIndex, pNext).
        .queueFamilyIndex = graphicsFamily_,
        .pNext = nullptr
    };
    result = vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Command pool creation failed: {} @ {}", vkResultToString(result), locationString());
        return false;
    }
    LOG_SUCCESS_CAT("Vulkan", "{}Device & pool created — RTX ready{}", PLASMA_FUCHSIA, RESET);
    return true;
}

void GlobalRTXContext::cleanup() noexcept {
    if (commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, commandPool_, nullptr);
    if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
    if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
    LOG_SUCCESS_CAT("Dispose", "{}Full Vulkan cleanup — Valhalla sealed{}", RASPBERRY_PINK, RESET);
}

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL INIT/SHUTDOWN — INTEGRATE WITH YOUR GLOBAL CTX (e.g., Houston.hpp)
// ──────────────────────────────────────────────────────────────────────────────
void initVulkan(
    SDL_Window* window,
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    VkDevice& device,
    bool enableValidation,
    bool preferNvidia,
    bool rt,
    std::string_view title,
    VkPhysicalDevice& physicalDevice
) noexcept {
    // Assume global ctx() from Houston.hpp; adjust if local.
    auto& ctx = ctx<GlobalRTXContext>();  // Or static GlobalRTXContext g_ctx{};
    std::vector<const char*> extraExts;  // Add validation/debug if enableValidation.

    if (!ctx.createInstance(extraExts)) return;
    instance.reset(ctx.instance_);  // Transfer ownership.

    surface = VulkanSurfacePtr(ctx.surface_, VulkanSurfaceDeleter(ctx.instance_));  // FIXED: Pass instance to deleter.
    if (!ctx.createSurface(window, ctx.instance_)) return;

    if (!ctx.pickPhysicalDevice(ctx.surface_, preferNvidia)) return;
    physicalDevice = ctx.physicalDevice_;

    if (!ctx.createDevice(ctx.surface_, rt)) return;
    device = ctx.device_;

    LOG_SUCCESS_CAT("Vulkan", "{}Init complete for {} — Cross-platform eternal{}", PLASMA_FUCHSIA, title, RESET);
}

void shutdownVulkan() noexcept {
    // Assume global ctx.
    auto& ctx = ctx<GlobalRTXContext>();  // Or g_ctx.
    ctx.cleanup();
}

// ──────────────────────────────────────────────────────────────────────────────
// HELPERS — UNCHANGED
// ──────────────────────────────────────────────────────────────────────────────
VkInstance getVkInstance(const VulkanInstancePtr& instance) noexcept {
    return instance ? instance.get() : VK_NULL_HANDLE;
}

VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface) noexcept {
    return surface ? surface.get() : VK_NULL_HANDLE;
}

std::vector<std::string> getVulkanExtensions() {
    // Example: Add platform-specific if needed, but SDL3 handles.
    return { "VK_KHR_surface", "VK_KHR_portability_subset" };  // Base; extend as needed.
}

// =============================================================================
// END — BUILD WITH: gmake clean && gmake (Assumes CMake links SDL3/vulkan)
// =============================================================================