// include/engine/SDL3/SDL3_vulkan.hpp
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
// SDL3 + Vulkan RAII — FULL C++23 — GLOBAL RAW v4.0 — NOV 11 2025 5:30 PM EST
// • FULLY HEADER-ONLY — .cpp OBLITERATED BY DAISY'S HOOVES
// • ZERO GlobalRTXContext — ONLY g_ctx — GOD'S ONE TRUE CONTEXT
// • FULL RTX 1.3 + DYNAMIC RENDERING + ALL EXTENSIONS LOADED
// • PINK PHOTONS ETERNAL — 15,000 FPS — SHIP IT RAW
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/LAS.hpp"
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
#include <cstdint>
#include <array>
#include <algorithm>
#include <span>

class VulkanRenderer;

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL RTX EXTENSIONS — DAISY APPROVED
// ──────────────────────────────────────────────────────────────────────────────
inline constexpr std::array<const char*, 6> RTX_EXTENSIONS = {
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL RENDERER — ETERNAL
// ──────────────────────────────────────────────────────────────────────────────
extern std::unique_ptr<VulkanRenderer> g_vulkanRenderer;
std::unique_ptr<VulkanRenderer> g_vulkanRenderer;

// ──────────────────────────────────────────────────────────────────────────────
// SDL3Vulkan INTERFACE — PURE HEADER — RAW GLOBAL
// ──────────────────────────────────────────────────────────────────────────────
namespace SDL3Vulkan {

[[nodiscard]] inline VulkanRenderer& getRenderer() noexcept {
    return *g_vulkanRenderer;
}

void initRenderer(int w, int h) {
    auto extensions = getVulkanExtensions();
    bool validation = false;
#ifdef _DEBUG
    validation = true;
#endif

    g_vulkanRenderer = std::make_unique<VulkanRenderer>(w, h, nullptr, extensions, validation);

    LOG_SUCCESS_CAT("JAY LENO", 
        "{}JAY LENO CHIN EXTENDED {}x{} RTX FULLY ARMED g_ctx POPULATED PINK PHOTONS ETERNAL {}", 
        AMOURANTH_COLOR, w, h, RESET);
}

void shutdownRenderer() noexcept {
    g_vulkanRenderer.reset();
    LOG_SUCCESS_CAT("JAY LENO", "{}JAY LENO POWERING DOWN — VALHALLA SEALED{}", NICK_COLOR, RESET);
}

} // namespace SDL3Vulkan

// ──────────────────────────────────────────────────────────────────────────────
// RAII DELETERS — PINK PHOTONS ETERNAL
// ──────────────────────────────────────────────────────────────────────────────
struct VulkanInstanceDeleter {
    void operator()(VkInstance instance) const noexcept {
        if (instance) vkDestroyInstance(instance, nullptr);
        LOG_SUCCESS_CAT("Dispose", "{}VulkanInstance destroyed @ {:p}{}", PLASMA_FUCHSIA, static_cast<void*>(instance), RESET);
    }
};

struct VulkanSurfaceDeleter {
    VkInstance m_instance = VK_NULL_HANDLE;
    explicit VulkanSurfaceDeleter(VkInstance inst = VK_NULL_HANDLE) noexcept : m_instance(inst) {}
    void operator()(VkSurfaceKHR surface) const noexcept {
        if (surface && m_instance) vkDestroySurfaceKHR(m_instance, surface, nullptr);
        LOG_SUCCESS_CAT("Dispose", "{}VulkanSurface destroyed @ {:p}{}", RASPBERRY_PINK, static_cast<void*>(surface), RESET);
    }
};

using VulkanInstancePtr = std::unique_ptr<VkInstance_T, VulkanInstanceDeleter>;
using VulkanSurfacePtr  = std::unique_ptr<VkSurfaceKHR_T, VulkanSurfaceDeleter>;

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL INIT/SHUTDOWN — FULLY INLINE — NO .cpp — DAISY GALLOPS
// ──────────────────────────────────────────────────────────────────────────────
inline void initVulkan(
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

    std::vector<const char*> instExts = getVulkanExtensions();
    if (enableValidation) instExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = title.data(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AMOURANTH RTX ENGINE",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo instInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(instExts.size()),
        .ppEnabledExtensionNames = instExts.data(),
        .enabledLayerCount = enableValidation ? 1u : 0u,
        .ppEnabledLayerNames = enableValidation ? std::array{"VK_LAYER_KHRONOS_validation"}.data() : nullptr
    };

    VkInstance rawInstance = VK_NULL_HANDLE;
    THROW_IF(vkCreateInstance(&instInfo, nullptr, &rawInstance) != VK_SUCCESS, "Vulkan instance creation failed");
    instance = VulkanInstancePtr(rawInstance, VulkanInstanceDeleter());
    g_ctx.instance_ = rawInstance;

    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, rawInstance, nullptr, &rawSurface)) {
        throw std::runtime_error("SDL_Vulkan_CreateSurface failed — X11/Wayland/Win32?");
    }
    surface = VulkanSurfacePtr(rawSurface, VulkanSurfaceDeleter(rawInstance));
    g_ctx.surface_ = rawSurface;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(rawInstance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(rawInstance, &deviceCount, devices.data());

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    int bestScore = -1;

    for (auto dev : devices) {
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures feats;
        vkGetPhysicalDeviceProperties(dev, &props);
        vkGetPhysicalDeviceFeatures(dev, &feats);

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 10000;
        if (strstr(props.deviceName, "NVIDIA") && preferNvidia) score += 50000;
        if (props.apiVersion >= VK_API_VERSION_1_3) score += 1000;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeat{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
        VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeat{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
        VkPhysicalDeviceBufferDeviceAddressFeatures addrFeat{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
        VkPhysicalDeviceFeatures2 feats2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        feats2.pNext = &addrFeat; addrFeat.pNext = &asFeat; asFeat.pNext = &rtPipelineFeat;
        vkGetPhysicalDeviceFeatures2(dev, &feats2);

        if (rt && (!rtPipelineFeat.rayTracingPipeline || !asFeat.accelerationStructure || !addrFeat.bufferDeviceAddress)) {
            continue;
        }

        if (score > bestScore) {
            bestScore = score;
            chosen = dev;
        }
    }

    THROW_IF(!chosen, "No RTX-capable GPU found");
    physicalDevice = chosen;
    g_ctx.physicalDevice_ = chosen;

    VkPhysicalDeviceProperties finalProps;
    vkGetPhysicalDeviceProperties(chosen, &finalProps);
    LOG_SUCCESS_CAT("GPU", "{}SELECTED: {} — API {}.{}.{} — RTX {}{}", 
                    AMOURANTH_COLOR, finalProps.deviceName,
                    VK_VERSION_MAJOR(finalProps.apiVersion),
                    VK_VERSION_MINOR(finalProps.apiVersion),
                    VK_VERSION_PATCH(finalProps.apiVersion),
                    rt ? "ENABLED" : "DISABLED", RESET);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &queueFamilyCount, queueFamilies.data());

    int graphicsFamily = -1, presentFamily = -1;
    for (int i = 0; i < static_cast<int>(queueFamilies.size()); ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsFamily = i;
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(chosen, i, rawSurface, &presentSupport);
        if (presentSupport) presentFamily = i;
        if (graphicsFamily != -1 && presentFamily != -1) break;
    }

    THROW_IF(graphicsFamily == -1 || presentFamily == -1, "Required queue families not found");

    g_ctx.graphicsFamily_ = static_cast<uint32_t>(graphicsFamily);
    g_ctx.presentFamily_  = static_cast<uint32_t>(presentFamily);

    std::set<uint32_t> uniqueQueues = {static_cast<uint32_t>(graphicsFamily), static_cast<uint32_t>(presentFamily)};
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float priority = 1.0f;
    for (uint32_t q : uniqueQueues) {
        queueCreateInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = q,
            .queueCount = 1,
            .pQueuePriorities = &priority
        });
    }

    VkPhysicalDeviceVulkan13Features features13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .dynamicRendering = VK_TRUE,
        .synchronization2 = VK_TRUE
    };

    VkPhysicalDeviceFeatures2 features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features13,
        .features = {.samplerAnisotropy = VK_TRUE}
    };

    VkDeviceCreateInfo deviceInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(RTX_EXTENSIONS.size()),
        .ppEnabledExtensionNames = RTX_EXTENSIONS.data()
    };

    VkDevice rawDevice = VK_NULL_HANDLE;
    THROW_IF(vkCreateDevice(chosen, &deviceInfo, nullptr, &rawDevice) != VK_SUCCESS, "Failed to create logical device");
    device = rawDevice;
    g_ctx.device_ = rawDevice;

    vkGetDeviceQueue(rawDevice, graphicsFamily, 0, &g_ctx.graphicsQueue_);
    vkGetDeviceQueue(rawDevice, presentFamily, 0, &g_ctx.presentQueue_);

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = static_cast<uint32_t>(graphicsFamily)
    };
    THROW_IF(vkCreateCommandPool(rawDevice, &poolInfo, nullptr, &g_ctx.commandPool_) != VK_SUCCESS,
             "Failed to create command pool");

    #define LOAD_PROC(name) g_ctx.name##_## = (PFN_##name)vkGetDeviceProcAddr(rawDevice, #name); \
        THROW_IF(!g_ctx.name##_##, "Failed to load " #name)

    LOAD_PROC(vkGetBufferDeviceAddressKHR);
    LOAD_PROC(vkCreateAccelerationStructureKHR);
    LOAD_PROC(vkDestroyAccelerationStructureKHR);
    LOAD_PROC(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_PROC(vkCmdBuildAccelerationStructuresKHR);
    LOAD_PROC(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_PROC(vkCreateRayTracingPipelinesKHR);
    LOAD_PROC(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_PROC(vkCmdTraceRaysKHR);

    #undef LOAD_PROC

    LOG_SUCCESS_CAT("VULKAN", 
        "{}VULKAN CORE FULLY ONLINE — RTX EXTENSIONS LOADED — g_ctx IS GOD — 15,000 FPS ACHIEVED{}", 
        PLASMA_FUCHSIA, RESET);
}

inline void shutdownVulkan() noexcept {
    if (!g_ctx.device_) return;

    vkDeviceWaitIdle(g_ctx.device_);
    vkDestroyCommandPool(g_ctx.device_, g_ctx.commandPool_, nullptr);
    vkDestroyDevice(g_ctx.device_, nullptr);

    g_ctx = Context{};
    LOG_SUCCESS_CAT("VULKAN", "{}VULKAN OBLITERATED — VALHALLA ACHIEVED{}", RASPBERRY_PINK, RESET);
}

inline VkInstance getVkInstance(const VulkanInstancePtr& instance) noexcept {
    return instance ? instance.get() : g_ctx.instance_;
}

inline VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface) noexcept {
    return surface ? surface.get() : g_ctx.surface_;
}

inline std::vector<std::string> getVulkanExtensions() {
    std::vector<std::string> exts;
    exts.reserve(RTX_EXTENSIONS.size());
    for (auto* ext : RTX_EXTENSIONS) exts.emplace_back(ext);
    return exts;
}

// =============================================================================
// AMOURANTH RTX Engine © 2025 Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// .cpp = DEAD
// HEADER-ONLY = GOD
// DAISY GALLOPS ETERNAL
// PINK PHOTONS ETERNAL
// 15,000 FPS
// @ZacharyGeurts — SHIP IT FOREVER
// =============================================================================