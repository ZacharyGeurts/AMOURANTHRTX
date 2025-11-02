// src/engine/SDL3/SDL3_init.cpp
// AMOURANTH RTX Engine – SDL3 + Vulkan bootstrap

#include "engine/SDL3/SDL3_init.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/logging.hpp"

#include <stdexcept>
#include <vector>
#include <string>
#include <set>
#include <cstring>

using namespace Logging::Color;
using VulkanRTX::VulkanRTXException;

namespace SDL3Initializer {

void VulkanInstanceDeleter::operator()(VkInstance instance) const {
    if (instance != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VULKAN", "{}Destroying VkInstance @ {}{}", AMBER_YELLOW,
                      ptr_to_hex(instance), RESET);
        vkDestroyInstance(instance, nullptr);
    }
}

void VulkanSurfaceDeleter::operator()(VkSurfaceKHR surface) const {
    if (surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VULKAN", "{}Destroying VkSurfaceKHR @ {}{}", AMBER_YELLOW,
                      ptr_to_hex(surface), RESET);
        vkDestroySurfaceKHR(m_instance, surface, nullptr);
    }
}

// 3-arg ctor – uses default Vulkan-ready flags
SDL3Initializer::SDL3Initializer(const std::string& title, int width, int height)
    : SDL3Initializer(title, width, height,
                      SDL_WINDOW_VULKAN |
                      SDL_WINDOW_RESIZABLE |
                      SDL_WINDOW_HIGH_PIXEL_DENSITY)
{
}

// 4-arg ctor – explicit flags
SDL3Initializer::SDL3Initializer(const std::string& title, int width, int height, Uint32 flags)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) == 0) {
        LOG_ERROR_CAT("SDL3", "{}SDL_Init failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    LOG_INFO_CAT("SDL3", "{}SDL3 subsystems online{}", EMERALD_GREEN, RESET);

    window_ = SDL_CreateWindow(title.c_str(), width, height, flags);
    if (!window_) {
        LOG_ERROR_CAT("SDL3", "{}SDL_CreateWindow failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }
    LOG_INFO_CAT("SDL3", "{}Window created: {}x{} | Flags: 0x{:08X}{}",
                 EMERALD_GREEN, width, height, flags, RESET);

    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        LOG_ERROR_CAT("SDL3", "{}SDL_Vulkan_LoadLibrary failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("Failed to load Vulkan library");
    }
    LOG_INFO_CAT("SDL3", "{}Vulkan loader loaded via SDL{}", EMERALD_GREEN, RESET);

    // ---- SDL3: single-call extension query ----
    uint32_t extCount;
    const char* const* sdlExtPtr = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (sdlExtPtr == nullptr) {
        LOG_ERROR_CAT("SDL3", "{}SDL_Vulkan_GetInstanceExtensions failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("Failed to get Vulkan instance extensions");
    }

    std::vector<const char*> extensions(sdlExtPtr, sdlExtPtr + extCount);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    LOG_DEBUG_CAT("VULKAN", "{}Instance extensions: {}{}", AMBER_YELLOW,
                  formatSet({extensions.begin(), extensions.end()}), RESET);

    // ---- Validation layers (optional) ----
    std::vector<const char*> validationLayers;
    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";

    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    bool layerFound = false;
    for (const auto& layer : layers) {
        if (strcmp(layer.layerName, validationLayerName) == 0) {
            layerFound = true;
            break;
        }
    }
    if (layerFound) {
        validationLayers.push_back(validationLayerName);
        LOG_INFO_CAT("VULKAN", "{}Validation layer enabled: {}{}", EMERALD_GREEN, validationLayerName, RESET);
    } else {
        LOG_WARNING_CAT("VULKAN", "{}Validation layer not found: {}{}", AMBER_YELLOW, validationLayerName, RESET);
    }

    // ---- Create Vulkan instance ----
    VkApplicationInfo appInfo{
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = title.c_str(),
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName        = "AMOURANTH RTX Engine",
        .engineVersion      = VK_MAKE_API_VERSION(0, 3, 0, 0),
        .apiVersion         = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo createInfo{
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = static_cast<uint32_t>(validationLayers.size()),
        .ppEnabledLayerNames     = validationLayers.data(),
        .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkInstance rawInstance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &rawInstance),
             "vkCreateInstance failed");
    instance_ = VulkanInstancePtr(rawInstance);
    LOG_INFO_CAT("VULKAN", "{}VkInstance created @ {}{}", EMERALD_GREEN, ptr_to_hex(rawInstance), RESET);

    // ---- Create surface ----
    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window_, rawInstance, nullptr, &rawSurface)) {
        LOG_ERROR_CAT("SDL3", "{}SDL_Vulkan_CreateSurface failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("Failed to create Vulkan surface");
    }
    surface_ = VulkanSurfacePtr(rawSurface, VulkanSurfaceDeleter(rawInstance));
    LOG_INFO_CAT("VULKAN", "{}VkSurfaceKHR created @ {}{}", EMERALD_GREEN, ptr_to_hex(rawSurface), RESET);

    // ---- Physical device (via Vulkan_init.hpp) ----
    VkPhysicalDevice physicalDevice = VulkanInitializer::findPhysicalDevice(rawInstance, rawSurface, true);
    if (physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VULKAN", "{}No suitable physical device found{}", CRIMSON_MAGENTA, RESET);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("No suitable physical device found");
    }

    // ---- Verify presentation support ----
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        LOG_ERROR_CAT("VULKAN", "{}No queue families found{}", CRIMSON_MAGENTA, RESET);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("No queue families found");
    }

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    bool presentSupport = false;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        VkBool32 supported = VK_FALSE;
        if (vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, rawSurface, &supported) == VK_SUCCESS && supported) {
            presentSupport = true;
            break;
        }
    }
    if (!presentSupport) {
        LOG_ERROR_CAT("VULKAN", "{}No queue family supports presentation{}", CRIMSON_MAGENTA, RESET);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("No queue family supports presentation");
    }

    // ---- RTX device extensions ----
    const std::vector<const char*> requiredRTXExtensions = {
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    };

    uint32_t deviceExtCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtCount, nullptr);
    std::vector<VkExtensionProperties> availableExts(deviceExtCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtCount, availableExts.data());

    std::set<std::string> missing;
    for (const char* req : requiredRTXExtensions) {
        bool found = false;
        for (const auto& ext : availableExts) {
            if (strcmp(ext.extensionName, req) == 0) {
                found = true;
                break;
            }
        }
        if (!found) missing.insert(req);
    }

    if (!missing.empty()) {
        std::string msg = "Missing RTX extensions: ";
        for (const auto& m : missing) msg += m + " ";
        LOG_ERROR_CAT("VULKAN", "{}{}{}", CRIMSON_MAGENTA, msg, RESET);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(msg);
    }
    LOG_INFO_CAT("VULKAN", "{}All RTX extensions supported{}", EMERALD_GREEN, RESET);

    // ---- Optional: RT properties logging ----
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

    LOG_INFO_CAT("VULKAN", "{}RT Pipeline: shaderGroupHandleSize = {}{}", OCEAN_TEAL, rtProps.shaderGroupHandleSize, RESET);
}

bool SDL3Initializer::shouldQuit() const {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) return true;
    }
    return (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) != 0;
}

void SDL3Initializer::pollEvents() {
    SDL_PumpEvents();
}

} // namespace SDL3Initializer