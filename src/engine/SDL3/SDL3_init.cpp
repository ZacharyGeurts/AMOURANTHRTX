// src/engine/SDL3/SDL3_init.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// FINAL SDL3+Vulkan bootstrap — C++23 — NOVEMBER 08 2025
// NO VkResult in format → ZERO compiler errors

#include "engine/SDL3/SDL3_init.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include <stdexcept>
#include <format>
#include <cstring>

using namespace Logging::Color;

namespace SDL3Initializer {

void VulkanInstanceDeleter::operator()(VkInstance instance) const {
    if (instance != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Dispose", "{}Destroying VkInstance @ {:p} — RASPBERRY_PINK ETERNAL 🩷{}", 
                     RASPBERRY_PINK, static_cast<void*>(instance), RESET);
        vkDestroyInstance(instance, nullptr);
    }
}

void VulkanSurfaceDeleter::operator()(VkSurfaceKHR surface) const {
    if (surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Dispose", "{}Destroying VkSurfaceKHR @ {:p} — RASPBERRY_PINK ETERNAL 🩷{}", 
                     RASPBERRY_PINK, static_cast<void*>(surface), RESET);
        vkDestroySurfaceKHR(m_instance, surface, nullptr);
    }
}

std::string SDL3Initializer::vkResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        default: return std::format("VK_UNKNOWN_ERROR({})", static_cast<int>(result));
    }
}

std::string SDL3Initializer::formatExtensions(const std::vector<const char*>& exts) {
    std::string s = "{";
    for (size_t i = 0; i < exts.size(); ++i) {
        s += exts[i];
        if (i + 1 < exts.size()) s += ", ";
    }
    s += "}";
    return s;
}

SDL3Initializer::SDL3Initializer(const std::string& title, int width, int height)
    : SDL3Initializer(title, width, height,
                      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY)
{}

SDL3Initializer::SDL3Initializer(const std::string& title, int width, int height, Uint32 flags) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) == 0) {
        LOG_ERROR_CAT("SDL3", "SDL_Init failed: {}", SDL_GetError());
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    LOG_SUCCESS_CAT("SDL3", "SDL3 subsystems online");

    window_ = SDL_CreateWindow(title.c_str(), width, height, flags);
    if (!window_) {
        LOG_ERROR_CAT("SDL3", "SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }
    LOG_SUCCESS_CAT("SDL3", "Window created: {}x{}", width, height);

    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        LOG_ERROR_CAT("SDL3", "SDL_Vulkan_LoadLibrary failed: {}", SDL_GetError());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("Failed to load Vulkan library");
    }
    LOG_SUCCESS_CAT("Vulkan", "Vulkan loader loaded via SDL3");

    uint32_t extCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!sdlExts) {
        LOG_ERROR_CAT("SDL3", "SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("Failed to get Vulkan instance extensions");
    }

    std::vector<const char*> extensions(sdlExts, sdlExts + extCount);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    LOG_INFO_CAT("Vulkan", "Instance extensions: {}", formatExtensions(extensions));

    std::vector<const char*> layers;
    const char* validation = "VK_LAYER_KHRONOS_validation";
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    bool found = false;
    for (const auto& layer : availableLayers) {
        if (strcmp(layer.layerName, validation) == 0) { found = true; break; }
    }
    if (found) {
        layers.push_back(validation);
        LOG_SUCCESS_CAT("Vulkan", "Validation layer enabled");
    } else {
        LOG_WARNING_CAT("Vulkan", "Validation layer '{}' not found", validation);
    }

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = title.c_str(),
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName = "AMOURANTH RTX Engine",
        .engineVersion = VK_MAKE_API_VERSION(0, 3, 0, 0),
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
        std::string msg = std::format("vkCreateInstance failed: {}", vkResultToString(result));
        LOG_ERROR_CAT("Vulkan", "{}", msg);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(msg);
    }

    instance_ = VulkanInstancePtr(rawInstance);
    LOG_SUCCESS_CAT("Vulkan", "VkInstance created @ {:p}", static_cast<void*>(rawInstance));

    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window_, rawInstance, nullptr, &rawSurface)) {
        LOG_ERROR_CAT("SDL3", "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("Failed to create Vulkan surface");
    }

    surface_ = VulkanSurfacePtr(rawSurface, VulkanSurfaceDeleter(rawInstance));
    LOG_SUCCESS_CAT("Vulkan", "VkSurfaceKHR created @ {:p}", static_cast<void*>(rawSurface));

    // Physical device selection deferred to VulkanInitializer
    LOG_INFO_CAT("Vulkan", "SDL3+Vulkan bootstrap complete — ready for RTX");
}

bool SDL3Initializer::shouldQuit() const {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) return true;
    }
    return false;
}

void SDL3Initializer::pollEvents() {
    SDL_PumpEvents();
}

} // namespace SDL3Initializer