// src/engine/SDL3/SDL3_window.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// SDL3 window creation and management.
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library, logging.hpp, Vulkan_init.hpp, Dispose.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <source_location>
#include <vector>
#include <string>
#include <cstring>
#include <format>
#include <set>

namespace SDL3Initializer {

void SDLWindowDeleter::operator()(SDL_Window* w) const {
    Dispose::destroyWindow(w);
}

SDLWindowPtr createWindow(const char* title, int w, int h, Uint32 flags) {
    LOG_INFO("Window", "Creating SDL window with title={}, width={}, height={}, flags=0x{:x}", 
             title, w, h, flags, std::source_location::current());

    flags |= SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        LOG_ERROR("Window", "SDL_Init failed: {}", SDL_GetError(), std::source_location::current());
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    SDLWindowPtr window(SDL_CreateWindow(title, w, h, flags));
    if (!window) {
        LOG_ERROR("Window", "SDL_CreateWindow failed: {}", SDL_GetError(), std::source_location::current());
        Dispose::quitSDL();
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    // Get required Vulkan extensions from SDL
    uint32_t extensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (sdlExtensions == nullptr || extensionCount == 0) {
        LOG_ERROR("Window", "SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError(), std::source_location::current());
        Dispose::quitSDL();
        throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }
    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);

    // Add extensions for ray tracing and debugging
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    LOG_DEBUG("Window", "Vulkan instance extensions retrieved: count={}", extensionCount, std::source_location::current());
    for (const auto* ext : extensions) {
        LOG_DEBUG("Window", "Extension: {}", ext, std::source_location::current());
    }

    // Validation layers
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
        LOG_DEBUG("Window", "Validation layer VK_LAYER_KHRONOS_validation enabled", std::source_location::current());
    } else {
        LOG_WARNING("Window", "Validation layer VK_LAYER_KHRONOS_validation not supported", std::source_location::current());
    }

    // Create temporary Vulkan instance
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = title,
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AMOURANTH RTX Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
        .ppEnabledLayerNames = validationLayers.empty() ? nullptr : validationLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Window", "vkCreateInstance failed with result={}", static_cast<int>(result), std::source_location::current());
        Dispose::quitSDL();
        throw std::runtime_error("vkCreateInstance failed: " + std::to_string(static_cast<int>(result)));
    }
    LOG_DEBUG("Window", "Temporary Vulkan instance created successfully: instance={:p}", static_cast<void*>(instance), std::source_location::current());

    // Create temporary Vulkan surface
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window.get(), instance, nullptr, &surface) || surface == VK_NULL_HANDLE) {
        LOG_ERROR("Window", "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError(), std::source_location::current());
        Dispose::destroyInstance(instance);
        Dispose::quitSDL();
        throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
    LOG_DEBUG("Window", "Temporary Vulkan surface created successfully: surface={:p}", static_cast<void*>(surface), std::source_location::current());

    // Select physical device (prefer NVIDIA)
    VkPhysicalDevice physicalDevice = VulkanInitializer::findPhysicalDevice(instance, surface, true);
    if (physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR("Window", "No suitable Vulkan physical device found", std::source_location::current());
        Dispose::destroySurfaceKHR(instance, surface);
        Dispose::destroyInstance(instance);
        Dispose::quitSDL();
        throw std::runtime_error("No suitable Vulkan physical device found");
    }

    // Verify queue family support for surface
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        LOG_ERROR("Window", "No queue families found for physical device", std::source_location::current());
        Dispose::destroySurfaceKHR(instance, surface);
        Dispose::destroyInstance(instance);
        Dispose::quitSDL();
        throw std::runtime_error("No queue families found for physical device");
    }

    bool surfaceSupported = false;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        VkBool32 presentSupport = VK_FALSE;
        VkResult surfaceResult = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if (surfaceResult != VK_SUCCESS) {
            LOG_ERROR("Window", "vkGetPhysicalDeviceSurfaceSupportKHR failed with result={}", static_cast<int>(surfaceResult), std::source_location::current());
            continue;
        }
        if (presentSupport) {
            surfaceSupported = true;
            break;
        }
    }
    if (!surfaceSupported) {
        LOG_ERROR("Window", "Vulkan surface not supported by any queue family", std::source_location::current());
        Dispose::destroySurfaceKHR(instance, surface);
        Dispose::destroyInstance(instance);
        Dispose::quitSDL();
        throw std::runtime_error("Vulkan surface not supported by any queue family");
    }

    // Verify ray tracing extensions
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
        std::string missingExts;
        for (const auto& ext : requiredExtSet) {
            missingExts += ext + ", ";
        }
        if (!missingExts.empty()) {
            missingExts.pop_back(); 
            missingExts.pop_back();
        }
        LOG_ERROR("Window", "Mandatory RTX extension(s) not supported: {}", missingExts, std::source_location::current());
        Dispose::destroySurfaceKHR(instance, surface);
        Dispose::destroyInstance(instance);
        Dispose::quitSDL();
        throw std::runtime_error("Mandatory RTX extension(s) not supported: " + missingExts);
    }
    LOG_DEBUG("Window", "Mandatory RTX extensions verified on physical device", std::source_location::current());

    // Check required features using feature chain
    VkPhysicalDeviceFeatures2 physicalDeviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = nullptr,
        .features = {}
    };

    VkPhysicalDeviceVulkan13Features vulkan13Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = nullptr,
        .robustImageAccess = VK_FALSE,
        .inlineUniformBlock = VK_FALSE,
        .descriptorBindingInlineUniformBlockUpdateAfterBind = VK_FALSE,
        .pipelineCreationCacheControl = VK_FALSE,
        .privateData = VK_FALSE,
        .shaderDemoteToHelperInvocation = VK_FALSE,
        .shaderTerminateInvocation = VK_FALSE,
        .subgroupSizeControl = VK_FALSE,
        .computeFullSubgroups = VK_FALSE,
        .synchronization2 = VK_FALSE,
        .textureCompressionASTC_HDR = VK_FALSE,
        .shaderZeroInitializeWorkgroupMemory = VK_FALSE,
        .dynamicRendering = VK_FALSE,
        .shaderIntegerDotProduct = VK_FALSE,
        .maintenance4 = VK_FALSE
    };

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .pNext = &vulkan13Features,
        .bufferDeviceAddress = VK_FALSE,
        .bufferDeviceAddressCaptureReplay = VK_FALSE,
        .bufferDeviceAddressMultiDevice = VK_FALSE
    };

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .pNext = &bufferDeviceAddressFeatures,
        .shaderInputAttachmentArrayDynamicIndexing = VK_FALSE,
        .shaderUniformTexelBufferArrayDynamicIndexing = VK_FALSE,
        .shaderStorageTexelBufferArrayDynamicIndexing = VK_FALSE,
        .shaderUniformBufferArrayNonUniformIndexing = VK_FALSE,
        .shaderSampledImageArrayNonUniformIndexing = VK_FALSE,
        .shaderStorageBufferArrayNonUniformIndexing = VK_FALSE,
        .shaderStorageImageArrayNonUniformIndexing = VK_FALSE,
        .shaderInputAttachmentArrayNonUniformIndexing = VK_FALSE,
        .shaderUniformTexelBufferArrayNonUniformIndexing = VK_FALSE,
        .shaderStorageTexelBufferArrayNonUniformIndexing = VK_FALSE,
        .descriptorBindingUniformBufferUpdateAfterBind = VK_FALSE,
        .descriptorBindingSampledImageUpdateAfterBind = VK_FALSE,
        .descriptorBindingStorageImageUpdateAfterBind = VK_FALSE,
        .descriptorBindingStorageBufferUpdateAfterBind = VK_FALSE,
        .descriptorBindingUniformTexelBufferUpdateAfterBind = VK_FALSE,
        .descriptorBindingStorageTexelBufferUpdateAfterBind = VK_FALSE,
        .descriptorBindingUpdateUnusedWhilePending = VK_FALSE,
        .descriptorBindingPartiallyBound = VK_FALSE,
        .descriptorBindingVariableDescriptorCount = VK_FALSE,
        .runtimeDescriptorArray = VK_FALSE
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &descriptorIndexingFeatures,
        .accelerationStructure = VK_FALSE,
        .accelerationStructureCaptureReplay = VK_FALSE,
        .accelerationStructureIndirectBuild = VK_FALSE,
        .accelerationStructureHostCommands = VK_FALSE,
        .descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &accelerationStructureFeatures,
        .rayTracingPipeline = VK_FALSE,
        .rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE,
        .rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE,
        .rayTracingPipelineTraceRaysIndirect = VK_FALSE,
        .rayTraversalPrimitiveCulling = VK_FALSE
    };

    physicalDeviceFeatures.pNext = &rayTracingPipelineFeatures;
    vkGetPhysicalDeviceFeatures2(physicalDevice, &physicalDeviceFeatures);

    // Verify key features
    if (!bufferDeviceAddressFeatures.bufferDeviceAddress) {
        LOG_ERROR("Window", "Buffer device address feature not supported for ray tracing", std::source_location::current());
        Dispose::destroySurfaceKHR(instance, surface);
        Dispose::destroyInstance(instance);
        Dispose::quitSDL();
        throw std::runtime_error("Physical device does not support bufferDeviceAddress for ray tracing");
    }
    if (!descriptorIndexingFeatures.descriptorBindingPartiallyBound) {
        LOG_ERROR("Window", "Descriptor binding partially bound feature not supported for ray tracing", std::source_location::current());
        Dispose::destroySurfaceKHR(instance, surface);
        Dispose::destroyInstance(instance);
        Dispose::quitSDL();
        throw std::runtime_error("Physical device does not support descriptorBindingPartiallyBound for ray tracing");
    }
    if (!accelerationStructureFeatures.accelerationStructure) {
        LOG_ERROR("Window", "Acceleration structure feature not supported for ray tracing", std::source_location::current());
        Dispose::destroySurfaceKHR(instance, surface);
        Dispose::destroyInstance(instance);
        Dispose::quitSDL();
        throw std::runtime_error("Physical device does not support accelerationStructure for ray tracing");
    }
    if (!rayTracingPipelineFeatures.rayTracingPipeline) {
        LOG_ERROR("Window", "Ray tracing pipeline feature not supported", std::source_location::current());
        Dispose::destroySurfaceKHR(instance, surface);
        Dispose::destroyInstance(instance);
        Dispose::quitSDL();
        throw std::runtime_error("Physical device does not support rayTracingPipeline");
    }

    LOG_DEBUG("Window", "RTX features verified on physical device", std::source_location::current());

    // Clean up temporary Vulkan resources
    Dispose::destroySurfaceKHR(instance, surface);
    Dispose::destroyInstance(instance);

    const char* videoDriver = SDL_GetCurrentVideoDriver();
    LOG_INFO("Window", "SDL window created with video driver: {}, flags=0x{:x}", 
             videoDriver ? videoDriver : "none", SDL_GetWindowFlags(window.get()), std::source_location::current());

    LOG_INFO("Window", "SDL window created with mandatory RTX Vulkan context verified", std::source_location::current());

    return window;
}

SDL_Window* getWindow(const SDLWindowPtr& window) {
    LOG_DEBUG("Window", "Retrieving SDL window", std::source_location::current());
    return window.get();
}

bool pollEventsForResize(const SDLWindowPtr& window, int& newWidth, int& newHeight, bool& shouldQuit, bool& toggleFullscreenKey) {
    (void)window; // Suppress unused parameter warning
    LOG_DEBUG("Window", "Polling SDL events for resize", std::source_location::current());
    SDL_Event event;
    bool resized = false;
    shouldQuit = false;
    toggleFullscreenKey = false;

    SDL_PumpEvents(); // Flush pending events to prevent blocking

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                LOG_INFO("Window", "Quit event received", std::source_location::current());
                shouldQuit = true;
                return false;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_F11) {
                    toggleFullscreenKey = true;
                    LOG_INFO("Window", "F11 pressed: toggling fullscreen", std::source_location::current());
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                newWidth = event.window.data1;
                newHeight = event.window.data2;
                resized = true;
                LOG_DEBUG("Window", "Window resized to {}x{}", newWidth, newHeight, std::source_location::current());
                break;
            case SDL_EVENT_WINDOW_MOVED:
            case SDL_EVENT_WINDOW_MOUSE_ENTER:
            case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
            case SDL_EVENT_WINDOW_FOCUS_LOST:
            case SDL_EVENT_WINDOW_MINIMIZED:
            case SDL_EVENT_WINDOW_MAXIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
                // Handle other window events if needed
                break;
            default:
                // Ignore other events
                break;
        }
    }

    return resized;
}

void toggleFullscreen(SDLWindowPtr& window, VulkanRTX::VulkanRenderer& renderer) {
    SDL_Window* win = window.get();
    bool isFullscreen = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
    LOG_INFO("Window", "Toggling fullscreen: current={}", isFullscreen ? "yes" : "no", std::source_location::current());

    // Ensure Vulkan device is idle
    VkDevice device = renderer.getDevice();
    if (device != VK_NULL_HANDLE) {
        LOG_DEBUG("Window", "Calling vkDeviceWaitIdle before fullscreen toggle", std::source_location::current());
        vkDeviceWaitIdle(device);
    }

    // Toggle borderless fullscreen
    int fullscreenState = isFullscreen ? 0 : 1; // 0 for windowed, 1 for fullscreen
    if (SDL_SetWindowFullscreen(win, fullscreenState) != 0) {
        LOG_ERROR("Window", "SDL_SetWindowFullscreen failed: {}", SDL_GetError(), std::source_location::current());
        return;
    }

    // Wait a frame for SDL to process
    SDL_Delay(16); // ~60 FPS tick

    // Get new size and resize renderer
    int newW, newH;
    SDL_GetWindowSize(win, &newW, &newH);
    renderer.handleResize(newW, newH);
    LOG_INFO("Window", "Fullscreen toggled to {}x{}", newW, newH, std::source_location::current());
}

} // namespace SDL3Initializer