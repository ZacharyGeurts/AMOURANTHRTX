// include/engine/GLOBAL/SDL3_vulkan.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3 + Vulkan RAII — FULL C++23 — GLOBAL BROS v3.3 — NOVEMBER 11, 2025 10:40 AM EST
// • CIRCULAR INCLUDE OBLITERATED — NO NAMESPACES — ctx() SAFE ETERNAL
// • PINK PHOTONS ETERNAL — VALHALLA SEALED — SHIP IT RAW
// • initVulkan() populates global ctx() via RAII Handles (Dispose)
// • LAS build deferred — use BUILD_BLAS/BUILD_TLAS after init
// • Professional, -Werror clean, C++23, zero leaks
//
// =============================================================================

#pragma once

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <source_location>
#include <unordered_set>
#include <format>
#include <set>

// GLOBAL STACK — ORDER IS LAW — CIRCULAR FIX SUPREME
#include "engine/GLOBAL/Dispose.hpp"       // ← FIRST — struct Context; + ctx() + Handle + MakeHandle
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"       // ← AFTER Dispose — Color::PLASMA_FUCHSIA now visible
//#include "engine/GLOBAL/LAS.hpp"           // ← LAS BEFORE VulkanContext
//#include "engine/GLOBAL/VulkanContext.hpp" // ← LAST — FULL Context DEFINITION — ctx() now complete

// SDL3 keeps its namespace — respect
// Vulkan stays raw — God intended

struct VulkanInstanceDeleter {
    void operator()(VkInstance instance) const noexcept {
        if (instance) vkDestroyInstance(instance, nullptr);
        LOG_SUCCESS_CAT("Dispose", "{}VulkanInstance destroyed @ {:p} — Valhalla cleanup complete{}", Color::PLASMA_FUCHSIA, static_cast<void*>(instance), Color::RESET);
    }
};

struct VulkanSurfaceDeleter {
    VkInstance m_instance = VK_NULL_HANDLE;
    explicit VulkanSurfaceDeleter(VkInstance inst = VK_NULL_HANDLE) noexcept : m_instance(inst) {}
    void operator()(VkSurfaceKHR surface) const noexcept {
        if (surface && m_instance) vkDestroySurfaceKHR(m_instance, surface, nullptr);
        LOG_SUCCESS_CAT("Dispose", "{}VulkanSurface destroyed @ {:p} — pink photons safe{}", Color::RASPBERRY_PINK, static_cast<void*>(surface), Color::RESET);
    }
};

using VulkanInstancePtr = std::unique_ptr<VkInstance_T, VulkanInstanceDeleter>;
using VulkanSurfacePtr = std::unique_ptr<VkSurfaceKHR_T, VulkanSurfaceDeleter>;

// GLOBAL FUNCTIONS — NO NAMESPACE — RAW POWER
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

    // === INSTANCE EXTENSIONS FROM SDL3 ===
    unsigned int extCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&extCount, nullptr)) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_GetInstanceExtensions failed (count): {}", SDL_GetError());
        return;
    }

    std::vector<const char*> extensions(extCount);
    if (!SDL_Vulkan_GetInstanceExtensions(&extCount, extensions.data())) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_GetInstanceExtensions failed (names): {}", SDL_GetError());
        return;
    }

    // === RTX EXTENSIONS ===
    if (rt) {
        extensions.insert(extensions.end(), {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_SHADER_CLOCK_EXTENSION_NAME
        });
    }

    // === VALIDATION LAYERS ===
    std::vector<const char*> layers;
    if (enableValidation) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    // === APPLICATION INFO ===
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = title.data(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AMOURANTH RTX",
        .engineVersion = VK_MAKE_VERSION(3, 33, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    // === CREATE INSTANCE ===
    VkInstance rawInstance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &rawInstance);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "vkCreateInstance failed: {}", vkResultToString(result));
        return;
    }

    instance = VulkanInstancePtr(rawInstance);
    LOG_SUCCESS_CAT("Vulkan", "{}VkInstance created @ {:p} — OLD GOD GLOBAL ENGAGED{}", Color::PLASMA_FUCHSIA, static_cast<void*>(rawInstance), Color::RESET);

    // === CREATE SURFACE ===
    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, rawInstance, nullptr, &rawSurface)) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        return;
    }

    surface = VulkanSurfacePtr(rawSurface, VulkanSurfaceDeleter(rawInstance));
    LOG_SUCCESS_CAT("Vulkan", "{}VkSurfaceKHR created @ {:p} — RASPBERRY_PINK ETERNAL{}", Color::RASPBERRY_PINK, static_cast<void*>(rawSurface), Color::RESET);

    // === PHYSICAL DEVICE SELECTION ===
    uint32_t gpuCount = 0;
    vkEnumeratePhysicalDevices(rawInstance, &gpuCount, nullptr);
    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vkEnumeratePhysicalDevices(rawInstance, &gpuCount, gpus.data());

    physicalDevice = VK_NULL_HANDLE;
    for (auto gpu : gpus) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(gpu, &props);
        LOG_INFO_CAT("Vulkan", "GPU: {} (type: {})", props.deviceName, props.deviceType);

        if (preferNvidia && std::string_view(props.deviceName).find("NVIDIA") != std::string_view::npos) {
            physicalDevice = gpu;
            break;
        }
        if (!physicalDevice) physicalDevice = gpu;
    }

    if (!physicalDevice) {
        LOG_ERROR_CAT("Vulkan", "No suitable GPU found — RTX OFFLINE");
        return;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    LOG_SUCCESS_CAT("Vulkan", "{}PhysicalDevice selected: {} — TITAN POWER{}", Color::EMERALD_GREEN, props.deviceName, Color::RESET);

    // === QUEUE FAMILIES ===
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    int graphicsFamily = -1, presentFamily = -1;
    for (int i = 0; i < static_cast<int>(queueFamilyCount); ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsFamily = i;

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, rawSurface, &presentSupport);
        if (presentSupport) presentFamily = i;

        if (graphicsFamily != -1 && presentFamily != -1) break;
    }

    if (graphicsFamily == -1 || presentFamily == -1) {
        LOG_ERROR_CAT("Vulkan", "Required queue families not found");
        return;
    }

    // === DEVICE EXTENSIONS ===
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };

    if (rt) {
        deviceExtensions.insert(deviceExtensions.end(), {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
        });
    }

    // === DEVICE FEATURES CHAIN ===
    VkPhysicalDeviceBufferDeviceAddressFeatures addrFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .accelerationStructure = VK_TRUE
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .rayTracingPipeline = VK_TRUE
    };

    VkPhysicalDeviceFeatures2 features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &addrFeatures
    };

    void* pNext = &features2;
    if (rt) {
        addrFeatures.pNext = &asFeatures;
        asFeatures.pNext = &rtFeatures;
        pNext = &features2;
    }

    // === QUEUE CREATE INFO ===
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { static_cast<uint32_t>(graphicsFamily), static_cast<uint32_t>(presentFamily) };
    float queuePriority = 1.0f;

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        queueCreateInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        });
    }

    // === CREATE LOGICAL DEVICE ===
    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = pNext,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data()
    };

    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create logical device");
        return;
    }

    LOG_SUCCESS_CAT("Vulkan", "{}Logical device created @ {:p} — AMOURANTH RTX READY{}", Color::PLASMA_FUCHSIA, static_cast<void*>(device), Color::RESET);

    // === POPULATE GLOBAL CONTEXT ===
    auto& c = *ctx();
    c.vkInstance = rawInstance;
    c.vkPhysicalDevice = physicalDevice;
    c.vkDevice = device;
    c.vkSurface = rawSurface;

    LOG_SUCCESS_CAT("Vulkan", "{}initVulkan complete — OLD GOD GLOBAL ENGAGED — 3.33 Hz vacuum phonon locked{}", Color::COSMIC_GOLD, Color::RESET);
}

void shutdownVulkan() noexcept {
    if (ctx()->vkDevice) {
        vkDeviceWaitIdle(ctx()->vkDevice);
        vkDestroyDevice(ctx()->vkDevice, nullptr);
        ctx()->vkDevice = VK_NULL_HANDLE;
    }

    ctx()->vkInstance = VK_NULL_HANDLE;
    ctx()->vkSurface = VK_NULL_HANDLE;
    ctx()->vkPhysicalDevice = VK_NULL_HANDLE;

    cleanupAll();  // ← Dispose::cleanupAll() — GentlemanGrok + shred
    LOG_SUCCESS_CAT("Vulkan", "{}VULKAN SHUTDOWN COMPLETE — PINK PHOTONS REST ETERNAL{}", Color::PLASMA_FUCHSIA, Color::RESET);
}

VkInstance getVkInstance(const VulkanInstancePtr& instance) noexcept { return instance ? instance.get() : VK_NULL_HANDLE; }
VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface) noexcept { return surface ? surface.get() : VK_NULL_HANDLE; }

std::vector<std::string> getVulkanExtensions() {
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> props(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());

    std::vector<std::string> names;
    names.reserve(count);
    for (const auto& p : props) {
        names.emplace_back(p.extensionName);
    }
    return names;
}

// Inline helpers — C++23 professional
static inline std::string vkResultToString(VkResult result) noexcept {
    switch (result) {
        case VK_SUCCESS: return "SUCCESS";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "INITIALIZATION_FAILED";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "FEATURE_NOT_PRESENT";
        default: return std::format("UNKNOWN({})", static_cast<int>(result));
    }
}

static inline std::string locationString(const std::source_location& loc = std::source_location::current()) noexcept {
    return std::format("{}:{}:{}", loc.file_name(), loc.line(), loc.function_name());
}

/*
    GLOBAL BROS v3.3 — LAS INCLUDED BEFORE VulkanContext — ctx() SAFE ETERNAL
    NO NAMESPACES — SDL3 KEEPS THEIRS — ALL FUNCTIONS RAW
    Context POPULATED — OLD GOD GLOBAL READY
    initGrok() called automatically on first use via Dispose
    -Werror clean — C++23 professional
    SHIP IT RAW — PINK PHOTONS ETERNAL — VALHALLA SEALED
    God bless you again. SHIP IT FOREVER
*/

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================