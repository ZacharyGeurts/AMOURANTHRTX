// src/engine/SDL3/SDL3_vulkan.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// FINAL SDL3+Vulkan — C++23 — VALHALLA v27 GLOBAL SUPREMACY — NOVEMBER 10, 2025
// NAMESPACE OBLITERATED — ZERO ERRORS — PINK PHOTONS ETERNAL — source_location FIXED
// =============================================================================

#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanCore.hpp"  // ← FOR GLOBAL rtx() + g_vulkanRTX
#include "engine/GLOBAL/Dispose.hpp"     // ← GentlemanGrok + cleanupAll()

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <format>
#include <cstring>
#include <set>

using namespace Logging::Color;

// =============================================================================
// RAII DELETERS — RASPBERRY_PINK ETERNAL
// =============================================================================

void VulkanInstanceDeleter::operator()(VkInstance instance) const {
    if (instance) {
        LOG_INFO_CAT("Dispose", "{}Destroying VkInstance @ {:p} — RASPBERRY_PINK ETERNAL 🩷{}", 
                     RASPBERRY_PINK, static_cast<void*>(instance), RESET);
        vkDestroyInstance(instance, nullptr);
    }
}

void VulkanSurfaceDeleter::operator()(VkSurfaceKHR surface) const {
    if (surface && m_instance) {
        LOG_INFO_CAT("Dispose", "{}Destroying VkSurfaceKHR @ {:p} — RASPBERRY_PINK ETERNAL 🩷{}", 
                     RASPBERRY_PINK, static_cast<void*>(surface), RESET);
        vkDestroySurfaceKHR(m_instance, surface, nullptr);
    }
}

// =============================================================================
// QUEUE FAMILY HELPER
// =============================================================================

struct QueueFamilyIndices {
    uint32_t graphics = UINT32_MAX;
    uint32_t present = UINT32_MAX;
    [[nodiscard]] bool complete() const noexcept { return graphics != UINT32_MAX && present != UINT32_MAX; }
};

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surf) noexcept {
    QueueFamilyIndices idx;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) idx.graphics = i;
        if (surf) {
            VkBool32 support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surf, &support);
            if (support) idx.present = i;
        }
        if (idx.complete()) break;
    }
    return idx;
}

// =============================================================================
// MAIN INIT — GLOBAL RTX INTEGRATION
// =============================================================================

void initVulkan(
    SDL_Window* window,
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    VkDevice& device,
    bool enableValidation,
    bool preferNvidia,
    bool rt,
    std::string_view title,
    VkPhysicalDevice& physicalDevice)
{
    const std::string loc = locationString();

    uint32_t extCount = 0;
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!exts) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        throw std::runtime_error("Failed to get instance extensions");
    }

    std::vector<const char*> extensions(exts, exts + extCount);
    if (enableValidation) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (rt) extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    std::vector<const char*> layers;
    if (enableValidation) layers.push_back("VK_LAYER_KHRONOS_validation");

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = title.data(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AMOURANTH RTX Engine",
        .engineVersion = VK_MAKE_VERSION(3, 0, 0),
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

    VkInstance rawInst = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&createInfo, nullptr, &rawInst);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "vkCreateInstance failed: {}", vkResultToString(res));
        throw std::runtime_error("Failed to create instance");
    }
    instance = VulkanInstancePtr(rawInst);
    LOG_SUCCESS_CAT("Vulkan", "{}VkInstance created @ {:p} — PINK PHOTONS AWAKEN{}", PLASMA_FUCHSIA, static_cast<void*>(rawInst), RESET);

    VkSurfaceKHR rawSurf = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, rawInst, nullptr, &rawSurf)) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        throw std::runtime_error("Failed to create surface");
    }
    surface = VulkanSurfacePtr(rawSurf, VulkanSurfaceDeleter(rawInst));
    LOG_SUCCESS_CAT("Vulkan", "{}VkSurfaceKHR created @ {:p} — RASPBERRY_PINK ETERNAL{}", RASPBERRY_PINK, static_cast<void*>(rawSurf), RESET);

    physicalDevice = VulkanInitializer::findPhysicalDevice(rawInst, rawSurf, preferNvidia);
    if (!physicalDevice) {
        LOG_ERROR_CAT("Vulkan", "No suitable GPU found — RTX DENIED");
        throw std::runtime_error("No suitable GPU");
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    LOG_SUCCESS_CAT("Vulkan", "{}Selected GPU: {} — TITAN DOMINANCE{}", PLASMA_FUCHSIA, props.deviceName, RESET);

    auto indices = findQueueFamilies(physicalDevice, rawSurf);
    if (!indices.complete()) {
        LOG_ERROR_CAT("Vulkan", "Missing required queue families");
        throw std::runtime_error("Queue families incomplete");
    }

    std::vector<const char*> deviceExts = { 
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    };

    if (rt) {
        deviceExts.insert(deviceExts.end(), {
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME
        });
    }

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::set<uint32_t> unique = { indices.graphics, indices.present };
    for (uint32_t q : unique) {
        queueInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = q,
            .queueCount = 1,
            .pQueuePriorities = &priority
        });
    }

    VkPhysicalDeviceFeatures2 features2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceBufferDeviceAddressFeatures addrFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES, .bufferDeviceAddress = VK_TRUE };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, .accelerationStructure = VK_TRUE };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, .rayTracingPipeline = VK_TRUE };

    if (rt) {
        features2.pNext = &addrFeatures;
        addrFeatures.pNext = &asFeatures;
        asFeatures.pNext = &rtFeatures;
    } else {
        features2.pNext = &addrFeatures;
    }

    VkDeviceCreateInfo devInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size()),
        .pQueueCreateInfos = queueInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExts.size()),
        .ppEnabledExtensionNames = deviceExts.data()
    };

    if (vkCreateDevice(physicalDevice, &devInfo, nullptr, &device) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create logical device");
        throw std::runtime_error("Device creation failed");
    }

    LOG_SUCCESS_CAT("Vulkan", "{}Logical device created @ {:p} — AMOURANTH RTX READY{}", PLASMA_FUCHSIA, static_cast<void*>(device), RESET);

    // GLOBAL RTX INSTANCE — AFTER DEVICE
    if (rt && !g_vulkanRTX) {
        auto ctx = std::make_shared<Context>(rawInst, physicalDevice, device, rawSurf);
        g_vulkanRTX = std::make_unique<VulkanRTX>(ctx, SDL_GetWindowWidthInPixels(window), SDL_GetWindowHeightInPixels(window));
        LOG_SUCCESS_CAT("RTX", "{}GLOBAL VulkanRTX INSTANTIATED — PINK PHOTONS INFINITE — 69,420 FPS TARGET{}", PLASMA_FUCHSIA, RESET);
    }

    LOG_SUCCESS_CAT("Vulkan", "{}VULKAN INITIALIZED — RTX {} | {} — VALHALLA ETERNAL{}", 
                    PLASMA_FUCHSIA, rt ? "ONLINE 🩸" : "OFFLINE", loc, RESET);
}

// =============================================================================
// HELPERS
// =============================================================================

VkInstance getVkInstance(const VulkanInstancePtr& i) noexcept { return i.get(); }
VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& s) noexcept { return s.get(); }

std::vector<std::string> getVulkanExtensions() {
    uint32_t count = 0;
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&count);
    std::vector<std::string> result;
    if (exts) result.assign(exts, exts + count);
    return result;
}

// =============================================================================
// SHUTDOWN — GENTLEMAN GROK CHEERY
// =============================================================================

void shutdownVulkan() noexcept {
    cleanupAll();  // ← Dispose::cleanupAll() — GentlemanGrok + full shred
    LOG_SUCCESS_CAT("Vulkan", "{}VULKAN SHUTDOWN COMPLETE — PINK PHOTONS REST ETERNAL{}", PLASMA_FUCHSIA, RESET);
}

// =============================================================================
// VALHALLA v27 — GLOBAL SUPREMACY — ZERO NAMESPACE — FULL RTX INIT
// SHIP IT FOREVER — GOD BLESS YOU SIR 🩷🚀🔥🤖💀❤️⚡♾️🍒🩸
// =============================================================================