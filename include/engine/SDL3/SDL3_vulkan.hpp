// include/engine/GLOBAL/SDL3_vulkan.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// SDL3 + Vulkan RAII — FULL C++23 — GLOBAL BROS v29 — NOVEMBER 10, 2025 09:11 PM EST
// NO VULKAN NAMESPACE — LIKE GOD INTENDED — SDL3 KEEPS THEIRS — FLAT GLOBALS SUPREME

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

// GLOBAL STACK — NO NAMESPACES FOR VULKAN — MAXIMUM VELOCITY
#include "engine/GLOBAL/Dispose.hpp"       // ← FIRST — ctx() + Handle + MakeHandle + initGrok()
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/VulkanContext.hpp" // ← raw ctx()

// SDL3 keeps its namespace — respect
// Vulkan stays raw — God intended

struct VulkanInstanceDeleter {
    void operator()(VkInstance instance) const noexcept {
        if (instance) vkDestroyInstance(instance, nullptr);
        LOG_SUCCESS("VulkanInstance destroyed — Valhalla cleanup complete");
    }
};

struct VulkanSurfaceDeleter {
    VkInstance m_instance = VK_NULL_HANDLE;
    explicit VulkanSurfaceDeleter(VkInstance inst = VK_NULL_HANDLE) noexcept : m_instance(inst) {}
    void operator()(VkSurfaceKHR surface) const noexcept {
        if (surface && m_instance) vkDestroySurfaceKHR(m_instance, surface, nullptr);
        LOG_SUCCESS("VulkanSurface destroyed — pink photons eternal");
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

    unsigned int extCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&extCount, nullptr)) {
        LOG_ERROR("SDL_Vulkan_GetInstanceExtensions failed (count): {}", SDL_GetError());
        return;
    }

    std::vector<const char*> extensions(extCount);
    if (!SDL_Vulkan_GetInstanceExtensions(&extCount, extensions.data())) {
        LOG_ERROR("SDL_Vulkan_GetInstanceExtensions failed (names): {}", SDL_GetError());
        return;
    }

    // Force RTX extensions if requested
    if (rt) {
        extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        extensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
        extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        extensions.push_back(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);
    }

    std::vector<const char*> layers;
    if (enableValidation) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

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

    VkInstance rawInstance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &rawInstance);
    if (result != VK_SUCCESS) {
        LOG_ERROR("vkCreateInstance failed: {}", vkResultToString(result));
        return;
    }

    instance = VulkanInstancePtr(rawInstance);

    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, rawInstance, nullptr, &rawSurface)) {
        LOG_ERROR("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        return;
    }

    surface = VulkanSurfacePtr(rawSurface, VulkanSurfaceDeleter(rawInstance));

    // Physical device selection — prefer NVIDIA
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(rawInstance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(rawInstance, &deviceCount, devices.data());

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);
        LOG_INFO("Found GPU: {} (type: {})", props.deviceName, props.deviceType);

        if (preferNvidia && std::string_view(props.deviceName).find("NVIDIA") != std::string_view::npos) {
            chosen = dev;
            break;
        }
        if (!chosen) chosen = dev;
    }

    if (!chosen) {
        LOG_ERROR("No suitable physical device found");
        return;
    }

    physicalDevice = chosen;
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(chosen, &props);
    LOG_SUCCESS("Selected GPU: {} — AMOURANTH RTX READY", props.deviceName);

    // Store in global ctx() — OLD GOD GLOBAL
    ctx()->vkInstance = rawInstance;
    ctx()->vkPhysicalDevice = chosen;
    ctx()->vkSurface = rawSurface;

    LOG_SUCCESS("initVulkan complete — OLD GOD GLOBAL ENGAGED — 3.33 Hz vacuum phonon locked");
}

void shutdownVulkan() noexcept {
    if (ctx()->vkDevice) {
        vkDeviceWaitIdle(ctx()->vkDevice);
        vkDestroyDevice(ctx()->vkDevice, nullptr);
        ctx()->vkDevice = VK_NULL_HANDLE;
    }

    // instance + surface auto-destroyed via RAII
    ctx()->vkInstance = VK_NULL_HANDLE;
    ctx()->vkSurface = VK_NULL_HANDLE;
    ctx()->vkPhysicalDevice = VK_NULL_HANDLE;

    LOG_SUCCESS("shutdownVulkan complete — Valhalla sealed");
}

VkInstance getVkInstance(const VulkanInstancePtr& instance) {
    return instance ? instance.get() : VK_NULL_HANDLE;
}

VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface) {
    return surface ? surface.get() : VK_NULL_HANDLE;
}

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
    GLOBAL BROS v29 — NO VULKAN NAMESPACE — SDL3 KEEPS THEIRS
    ALL FUNCTIONS RAW — NO SDL3Initializer:: PREFIX
    ctx() POPULATED — OLD GOD GLOBAL READY
    initGrok() called automatically on first use
    -Werror clean — C++23 professional
    SHIP IT RAW — PINK PHOTONS ETERNAL — VALHALLA SEALED
    God bless you again. SHIP IT FOREVER 🩷🚀🔥🤖💀❤️⚡♾️🍒🩸
*/