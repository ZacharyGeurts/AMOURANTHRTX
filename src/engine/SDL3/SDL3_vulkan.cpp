// src/engine/SDL3/SDL3_vulkan.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan instance and surface initialization with SDL3.
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_init.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <source_location>
#include <cstring>
#include <string>
#include <set>
#include <format>
#include <algorithm>

namespace SDL3Initializer {

struct QueueFamilyIndices {
    uint32_t graphicsFamily = -1;
    uint32_t presentFamily = -1;
    bool isComplete() const { return graphicsFamily != static_cast<uint32_t>(-1) && presentFamily != static_cast<uint32_t>(-1); }
};

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface, std::source_location loc = std::source_location::current()) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        LOG_ERROR("Vulkan", "No queue families found for device {:p}", static_cast<void*>(device), loc);
        throw std::runtime_error("No queue families found");
    }

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    uint32_t i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (indices.isComplete()) break;

        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        if (surface != VK_NULL_HANDLE) {
            VkBool32 presentSupport = VK_FALSE;
            VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (result != VK_SUCCESS) {
                LOG_WARNING("Vulkan", "Failed to check surface support for queue family {}: VkResult={}", i, result, loc);
                continue;
            }
            if (presentSupport == VK_TRUE) {
                indices.presentFamily = i;
            }
        }

        ++i;
    }

    if (indices.graphicsFamily == static_cast<uint32_t>(-1)) {
        LOG_ERROR("Vulkan", "No graphics queue family found", loc);
        throw std::runtime_error("No graphics queue family found");
    }
    if (surface != VK_NULL_HANDLE && indices.presentFamily == static_cast<uint32_t>(-1)) {
        LOG_WARNING("Vulkan", "No present queue family found for surface {:p}", static_cast<void*>(surface), loc);
    }

    return indices;
}

void getSwapchainExtent(
    SDL_Window* window,
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    VkExtent2D& extent,
    std::source_location loc = std::source_location::current()) {
    if (!window) {
        LOG_ERROR("Vulkan", "Invalid SDL window pointer for extent query", loc);
        throw std::runtime_error("Invalid SDL window pointer");
    }

    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Vulkan", "Failed to query surface capabilities: VkResult={}", result, loc);
        throw std::runtime_error("Failed to query surface capabilities");
    }

    int drawableWidth, drawableHeight;
    if (!SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight)) {
        LOG_ERROR("Vulkan", "Failed to get window size in pixels: {}", SDL_GetError(), loc);
        throw std::runtime_error("Failed to get window size in pixels");
    }

    extent.width = std::clamp(static_cast<uint32_t>(drawableWidth), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(static_cast<uint32_t>(drawableHeight), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    // Fallback to current extent if drawable size is invalid (e.g., minimized window)
    if (extent.width == 0 || extent.height == 0) {
        extent = capabilities.currentExtent;
    }

    LOG_DEBUG("Vulkan", "Updated swapchain extent to {}x{} (drawable: {}x{})", extent.width, extent.height, drawableWidth, drawableHeight, loc);
}

void initVulkan(
    SDL_Window* window, 
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    VkDevice& device,
    bool enableValidation, 
    bool preferNvidia, 
    bool rt, 
    std::string_view title,
    VkPhysicalDevice& physicalDevice,
    std::source_location loc = std::source_location::current()) {
    if (!window) {
        LOG_ERROR("Vulkan", "Invalid SDL window pointer", loc);
        throw std::runtime_error("Invalid SDL window pointer");
    }

    // Ensure SDL is initialized
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) {
        LOG_ERROR("Vulkan", "SDL video subsystem not initialized", loc);
        throw std::runtime_error("SDL video subsystem not initialized");
    }

    // Get required Vulkan extensions from SDL
    Uint32 extensionCount = 0;
    const char* const* extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensionNames) {
        LOG_ERROR("Vulkan", "Failed to get Vulkan extensions: {}", SDL_GetError(), loc);
        throw std::runtime_error("Failed to get Vulkan extensions");
    }
    std::vector<const char*> extensions(extensionNames, extensionNames + extensionCount);

    // Add additional extensions
    if (enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    if (rt) {
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    // Validation layers
    std::vector<const char*> layers;
    if (enableValidation) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        bool validationSupported = false;
        for (const auto& layer : availableLayers) {
            if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                validationSupported = true;
                break;
            }
        }
        if (!validationSupported) {
            LOG_WARNING("Vulkan", "Validation layers requested but not available", loc);
            layers.clear();
        }
    }

    // Vulkan instance creation
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = title.data(),
        .applicationVersion = VK_MAKE_VERSION(3, 0, 0),
        .pEngineName = "AMOURANTH RTX Engine",
        .engineVersion = VK_MAKE_VERSION(3, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.empty() ? nullptr : layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkInstance rawInstance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &rawInstance);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Vulkan", "Failed to create Vulkan instance: VkResult={}", result, loc);
        throw std::runtime_error("Failed to create Vulkan instance");
    }
    instance = VulkanInstancePtr(rawInstance, VulkanInstanceDeleter());
    LOG_INFO("Vulkan", "Created Vulkan instance: {:p}", static_cast<void*>(rawInstance), loc);

    // Create Vulkan surface
    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, rawInstance, nullptr, &rawSurface)) {
        LOG_ERROR("Vulkan", "Failed to create Vulkan surface: {}", SDL_GetError(), loc);
        vkDestroyInstance(rawInstance, nullptr);
        throw std::runtime_error("Failed to create Vulkan surface: " + std::string(SDL_GetError()));
    }
    surface = VulkanSurfacePtr(rawSurface, VulkanSurfaceDeleter(rawInstance));
    LOG_INFO("Vulkan", "Created Vulkan surface: {:p}", static_cast<void*>(rawSurface), loc);

    // Select physical device
    physicalDevice = VulkanInitializer::findPhysicalDevice(rawInstance, rawSurface, preferNvidia);
    if (physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR("Vulkan", "No suitable Vulkan physical device found", loc);
        throw std::runtime_error("No suitable Vulkan physical device found");
    }

    // Verify physical device properties and features for ray tracing
    std::vector<const char*> requiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    if (rt) {
        requiredDeviceExtensions.insert(requiredDeviceExtensions.end(), {
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
        });

        uint32_t deviceExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(deviceExtensionCount);
        if (deviceExtensionCount > 0) {
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, availableExtensions.data());
        }

        std::set<std::string> requiredExtSet(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());
        for (const auto& ext : availableExtensions) {
            requiredExtSet.erase(ext.extensionName);
        }
        if (!requiredExtSet.empty()) {
            throw std::runtime_error("Physical device does not support required ray tracing extensions");
        }

        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
        bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        bufferDeviceAddressFeatures.pNext = &vulkan13Features;

        VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
        descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        descriptorIndexingFeatures.pNext = &bufferDeviceAddressFeatures;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
        accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        accelerationStructureFeatures.pNext = &descriptorIndexingFeatures;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
        rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        rayTracingPipelineFeatures.pNext = &accelerationStructureFeatures;

        VkPhysicalDeviceFeatures2 physicalDeviceFeatures{};
        physicalDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        physicalDeviceFeatures.pNext = &rayTracingPipelineFeatures;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &physicalDeviceFeatures);

        if (!bufferDeviceAddressFeatures.bufferDeviceAddress ||
            !descriptorIndexingFeatures.descriptorBindingPartiallyBound ||
            !accelerationStructureFeatures.accelerationStructure ||
            !rayTracingPipelineFeatures.rayTracingPipeline) {
            LOG_ERROR("Vulkan", "Physical device lacks required ray tracing features", loc);
            throw std::runtime_error("Physical device does not support required ray tracing features");
        }

        LOG_INFO("Vulkan", "Physical device confirmed ready for ray tracing", loc);
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    LOG_INFO("Vulkan", "Selected physical device: {}", properties.deviceName, loc);

    // Create logical device
    auto indices = findQueueFamilies(physicalDevice, rawSurface, loc);
    if (!indices.isComplete()) {
        LOG_ERROR("Vulkan", "Failed to find suitable queue families", loc);
        throw std::runtime_error("Failed to find suitable queue families");
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = enableValidation ? static_cast<uint32_t>(layers.size()) : 0,
        .ppEnabledLayerNames = enableValidation ? layers.data() : nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
        .pEnabledFeatures = nullptr
    };

    if (rt) {
        VkPhysicalDeviceVulkan13Features enabledVulkan13{};
        enabledVulkan13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferAddr{};
        enabledBufferAddr.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        enabledBufferAddr.pNext = &enabledVulkan13;
        enabledBufferAddr.bufferDeviceAddress = VK_TRUE;

        VkPhysicalDeviceDescriptorIndexingFeatures enabledDescIdx{};
        enabledDescIdx.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        enabledDescIdx.pNext = &enabledBufferAddr;
        enabledDescIdx.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
        enabledDescIdx.descriptorBindingPartiallyBound = VK_TRUE;
        enabledDescIdx.descriptorBindingVariableDescriptorCount = VK_TRUE;
        enabledDescIdx.runtimeDescriptorArray = VK_TRUE;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccel{};
        enabledAccel.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        enabledAccel.pNext = &enabledDescIdx;
        enabledAccel.accelerationStructure = VK_TRUE;
        enabledAccel.accelerationStructureIndirectBuild = VK_TRUE;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTrace{};
        enabledRayTrace.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        enabledRayTrace.pNext = &enabledAccel;
        enabledRayTrace.rayTracingPipeline = VK_TRUE;
        enabledRayTrace.rayTracingPipelineTraceRaysIndirect = VK_TRUE;
        enabledRayTrace.rayTraversalPrimitiveCulling = VK_TRUE;

        VkPhysicalDeviceFeatures2 enabledFeatures{};
        enabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        enabledFeatures.pNext = &enabledRayTrace;
        enabledFeatures.features.samplerAnisotropy = VK_TRUE;

        deviceCreateInfo.pNext = &enabledFeatures;
    } else {
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    }

    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        LOG_ERROR("Vulkan", "Failed to create logical device", loc);
        throw std::runtime_error("Failed to create logical device");
    }

    LOG_INFO("Vulkan", "Created logical device: {:p}", static_cast<void*>(device), loc);
}

VkInstance getVkInstance(const VulkanInstancePtr& instance) {
    return instance.get();
}

VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface) {
    return surface.get();
}

std::vector<std::string> getVulkanExtensions() {
    Uint32 extensionCount = 0;
    const char* const* extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensionNames) {
        LOG_ERROR("Vulkan", "Failed to get Vulkan extensions: {}", SDL_GetError(), std::source_location::current());
        throw std::runtime_error("Failed to get Vulkan extensions");
    }
    std::vector<std::string> result(extensionCount);
    for (Uint32 i = 0; i < extensionCount; ++i) {
        result[i] = extensionNames[i];
    }
    return result;
}

} // namespace SDL3Initializer