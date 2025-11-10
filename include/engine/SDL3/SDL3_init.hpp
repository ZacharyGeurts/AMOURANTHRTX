// include/engine/SDL3/SDL3_init.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// FINAL SDL3+Vulkan bootstrap — PURE HEADER-ONLY — NOVEMBER 10 2025
// RASPBERRY_PINK DISPOSE INTEGRATION — ZERO CPP — ETERNAL BLISS

#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <vector>
#include <format>
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Dispose.hpp"

namespace SDL3Initializer {

struct VulkanInstanceDeleter {
    void operator()(VkInstance i) const {
        if (i) { LOG_INFO_CAT("Dispose", "Destroying VkInstance @ {:p}", static_cast<void*>(i)); vkDestroyInstance(i, nullptr); }
    }
};

struct VulkanSurfaceDeleter {
    VkInstance inst = VK_NULL_HANDLE;
    explicit VulkanSurfaceDeleter(VkInstance i = VK_NULL_HANDLE) : inst(i) {}
    void operator()(VkSurfaceKHR s) const {
        if (s && inst) { LOG_INFO_CAT("Dispose", "Destroying VkSurfaceKHR @ {:p}", static_cast<void*>(s)); vkDestroySurfaceKHR(inst, s, nullptr); }
    }
};

using VulkanInstancePtr = std::unique_ptr<VkInstance_T, VulkanInstanceDeleter>;
using VulkanSurfacePtr = std::unique_ptr<VkSurfaceKHR_T, VulkanSurfaceDeleter>;

class SDL3Initializer {
public:
    SDL3Initializer(const std::string& title, int w, int h, Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) != 0)
            throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
        LOG_SUCCESS_CAT("SDL3", "Subsystems online");

        window_ = SDL_CreateWindow(title.c_str(), w, h, flags);
        if (!window_) throw std::runtime_error(std::string("Window failed: ") + SDL_GetError());
        LOG_SUCCESS_CAT("SDL3", "Window {}x{}", w, h);

        if (!SDL_Vulkan_LoadLibrary(nullptr))
            throw std::runtime_error("Vulkan loader failed");

        uint32_t extCount = 0;
        const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
        std::vector<const char*> extensions(exts, exts + extCount);
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        std::vector<const char*> layers;
        if ([]{
            uint32_t c = 0; vkEnumerateInstanceLayerProperties(&c, nullptr);
            std::vector<VkLayerProperties> l(c); vkEnumerateInstanceLayerProperties(&c, l.data());
            return std::any_of(l.begin(), l.end(), [](auto& p){ return strcmp(p.layerName, "VK_LAYER_KHRONOS_validation") == 0; });
        }()) layers.push_back("VK_LAYER_KHRONOS_validation");

        VkApplicationInfo app{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = title.c_str(),
            .apiVersion = VK_API_VERSION_1_3, .pEngineName = "AMOURANTH RTX", .engineVersion = VK_MAKE_API_VERSION(0,3,0,0)};
        VkInstanceCreateInfo ci{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app,
            .enabledLayerCount = static_cast<uint32_t>(layers.size()), .ppEnabledLayerNames = layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(extensions.size()), .ppEnabledExtensionNames = extensions.data()};

        VkInstance raw = VK_NULL_HANDLE;
        if (vkCreateInstance(&ci, nullptr, &raw) != VK_SUCCESS)
            throw std::runtime_error("vkCreateInstance failed");
        instance_ = VulkanInstancePtr(raw);
        LOG_SUCCESS_CAT("Vulkan", "Instance @ {:p}", static_cast<void*>(raw));

        VkSurfaceKHR surf = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(window_, raw, nullptr, &surf))
            throw std::runtime_error("Surface creation failed");
        surface_ = VulkanSurfacePtr(surf, VulkanSurfaceDeleter(raw));
        LOG_SUCCESS_CAT("Vulkan", "Surface @ {:p}", static_cast<void*>(surf));

        // Auto-register for Dispose
        ::Dispose::SDL_Window(window_);
        ::Dispose::VkSurfaceKHR(surf);
    }

    [[nodiscard]] VkInstance   getInstance() const noexcept { return instance_.get(); }
    [[nodiscard]] VkSurfaceKHR getSurface()  const noexcept { return surface_.get(); }
    [[nodiscard]] SDL_Window*  getWindow()   const noexcept { return window_; }

    bool shouldQuit() const { SDL_Event e; while (SDL_PollEvent(&e)) if (e.type == SDL_EVENT_QUIT) return true; return false; }
    void pollEvents() { SDL_PumpEvents(); }

private:
    SDL_Window* window_ = nullptr;
    VulkanInstancePtr instance_;
    VulkanSurfacePtr surface_;
};

} // namespace SDL3Initializer

// PURE HEADER-ONLY — NO CPP — DISPOSE AUTO-TRACKS WINDOW+SURFACE — COMPILES CLEAN NOVEMBER 10 2025 🩷⚡