// src/engine/SDL3/SDL3_init.cpp
// AMOURANTH RTX Engine, October 2025 - SDL3 and Vulkan initialization implementation.
// Initializes SDL3 window and Vulkan instance/surface for rendering.
// Dependencies: SDL3, Vulkan 1.3, C++20 standard library, logging.hpp, Vulkan_init.hpp.
// Supported platforms: Windows, Linux.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_init.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <source_location>
#include <vector>
#include <string>
#include <set>
#include <cstdlib>
#include <cstring>
#include <format>

namespace SDL3Initializer {

SDL3Initializer::SDL3Initializer(const std::string& title, int width, int height) {
    LOG_INFO("General", "Initializing SDL3 with title={}, width={}, height={}", title, width, height, std::source_location::current());

    // Force NVIDIA GPU for Vulkan (hybrid GPU fix on Linux)
    putenv(const_cast<char*>("VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/nvidia_icd.json"));
    // Enable PRIME offload for hybrid GPU setups (uncomment for laptops)
    // putenv(const_cast<char*>("__NV_PRIME_RENDER_OFFLOAD=1"));
    // putenv(const_cast<char*>("__GLX_VENDOR_LIBRARY_NAME=nvidia"));

    // Prefer Wayland for better NVIDIA Vulkan support
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) == 0) {
        LOG_ERROR("General", "SDL_Init failed: {}", SDL_GetError(), std::source_location::current());
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    LOG_DEBUG("General", "SDL initialized successfully", std::source_location::current());

    window_ = SDL_CreateWindow(title.c_str(), width, height, 
                               SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window_) {
        LOG_ERROR("General", "SDL_CreateWindow failed: {}", SDL_GetError(), std::source_location::current());
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }
    LOG_DEBUG("General", "SDL window created successfully: window={:p}", static_cast<void*>(window_), std::source_location::current());

    const char* videoDriver = SDL_GetCurrentVideoDriver();
    LOG_INFO("General", "SDL video driver: {}", videoDriver ? videoDriver : "none", std::source_location::current());

    uint32_t extensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (sdlExtensions == nullptr || extensionCount == 0) {
        LOG_ERROR("General", "SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError(), std::source_location::current());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }
    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);
    LOG_DEBUG("General", "Vulkan instance extensions retrieved: count={}", extensionCount, std::source_location::current());
    for (const auto* ext : extensions) {
        LOG_DEBUG("General", "Extension: {}", ext, std::source_location::current());
    }

    // Add ray tracing and debug extensions
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    std::vector<const char*> validationLayers;
    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    uint32_t availableLayerCount = 0;
    vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(availableLayerCount);
    vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers.data());
    bool validationLayerSupported = false;
    for (const auto& layer : availableLayers) {
        if (strcmp(layer.layerName, validationLayerName) == 0) {
            validationLayerSupported = true;
            break;
        }
    }
    if (validationLayerSupported) {
        validationLayers.push_back(validationLayerName);
        LOG_DEBUG("General", "Validation layers and debug utils enabled", std::source_location::current());
    } else {
        LOG_WARNING("General", "Validation layer VK_LAYER_KHRONOS_validation not supported", std::source_location::current());
    }

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = title.c_str(),
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName = "AMOURANTH RTX Engine",
        .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
        .ppEnabledLayerNames = validationLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkInstance rawInstance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &rawInstance);
    if (result != VK_SUCCESS) {
        LOG_ERROR("General", "vkCreateInstance failed with result={}", static_cast<int>(result), std::source_location::current());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("vkCreateInstance failed: " + std::to_string(static_cast<int>(result)));
    }
    instance_ = VulkanInstancePtr(rawInstance, VulkanInstanceDeleter());
    LOG_DEBUG("General", "Vulkan instance created successfully: instance={:p}", static_cast<void*>(rawInstance), std::source_location::current());

    // Validate window before surface creation
    if (!window_) {
        LOG_ERROR("General", "Invalid window: window={:p}", static_cast<void*>(window_), std::source_location::current());
        SDL_Quit();
        throw std::runtime_error("Invalid window before surface creation");
    }

    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    bool surfaceResult = SDL_Vulkan_CreateSurface(window_, rawInstance, nullptr, &rawSurface);
    if (!surfaceResult || rawSurface == VK_NULL_HANDLE) {
        LOG_ERROR("General", "SDL_Vulkan_CreateSurface failed: {}, surface={:p}", SDL_GetError(), static_cast<void*>(rawSurface), std::source_location::current());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
    surface_ = VulkanSurfacePtr(rawSurface, VulkanSurfaceDeleter(rawInstance));
    LOG_DEBUG("General", "Vulkan surface created successfully: surface={:p}", static_cast<void*>(rawSurface), std::source_location::current());

    VkPhysicalDevice physicalDevice = VulkanInitializer::findPhysicalDevice(rawInstance, rawSurface, true);
    if (physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR("General", "No suitable physical device found", std::source_location::current());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("No suitable physical device found");
    }

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    LOG_INFO("General", "Selected physical device: {}", deviceProperties.deviceName, std::source_location::current());

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        LOG_ERROR("General", "No queue families found for physical device", std::source_location::current());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("No queue families found for physical device");
    }

    bool surfaceSupported = false;
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        VkBool32 presentSupport = VK_FALSE;
        VkResult queueResult = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, rawSurface, &presentSupport);
        if (queueResult != VK_SUCCESS) {
            LOG_ERROR("General", "vkGetPhysicalDeviceSurfaceSupportKHR failed for queue family {}: result={}", i, static_cast<int>(queueResult), std::source_location::current());
            continue;
        }
        if (presentSupport) {
            surfaceSupported = true;
            LOG_DEBUG("General", "Queue family {} supports surface presentation", i, std::source_location::current());
            break;
        }
    }
    if (!surfaceSupported) {
        LOG_ERROR("General", "Vulkan surface not supported by any queue family", std::source_location::current());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("Vulkan surface not supported by any queue family");
    }

    // RTX Extension Validation
    std::vector<const char*> requiredRTXExtensions = {
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    };

    uint32_t deviceExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr);
    std::vector<VkExtensionProperties> availableDeviceExtensions(deviceExtensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, availableDeviceExtensions.data());

    std::set<std::string> requiredExtSet(requiredRTXExtensions.begin(), requiredRTXExtensions.end());
    for (const auto& ext : availableDeviceExtensions) {
        requiredExtSet.erase(ext.extensionName);
    }
    if (!requiredExtSet.empty()) {
        LOG_ERROR("General", "Mandatory RTX extension(s) not supported: {}", formatSet(requiredExtSet), std::source_location::current());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("Mandatory RTX extension(s) not supported");
    }
    LOG_DEBUG("General", "Mandatory RTX extensions verified on physical device", std::source_location::current());

    // RTX Capability Check
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);
    if (rtProps.shaderGroupHandleSize == 0) {
        LOG_WARNING("General", "Device supports RTX extensions but may lack dedicated RT cores; fallback to software RT possible", std::source_location::current());
    } else {
        LOG_DEBUG("General", "RTX hardware acceleration confirmed (group handle size={})", rtProps.shaderGroupHandleSize, std::source_location::current());
    }

    LOG_INFO("General", "SDL3Initializer initialized successfully with mandatory RTX Vulkan context", std::source_location::current());
}

bool SDL3Initializer::shouldQuit() const {
    bool minimized = (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) != 0;
    bool hasQuitEvent = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            hasQuitEvent = true;
            break;
        }
    }
    LOG_DEBUG("General", "shouldQuit checked, minimized={}, hasQuitEvent={}", minimized, hasQuitEvent, std::source_location::current());
    return minimized || hasQuitEvent;
}

void SDL3Initializer::pollEvents() {
    LOG_DEBUG("General", "Polling SDL events", std::source_location::current());
    SDL_PumpEvents();
}

void VulkanInstanceDeleter::operator()(VkInstance instance) const {
    if (instance != VK_NULL_HANDLE) {
        LOG_INFO("Vulkan", "Destroying Vulkan instance: {:p}", static_cast<void*>(instance), std::source_location::current());
        vkDestroyInstance(instance, nullptr);
    }
}

void VulkanSurfaceDeleter::operator()(VkSurfaceKHR surface) const {
    if (surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        LOG_INFO("Vulkan", "Destroying Vulkan surface: {:p}", static_cast<void*>(surface), std::source_location::current());
        vkDestroySurfaceKHR(m_instance, surface, nullptr);
    }
}

} // namespace SDL3Initializer