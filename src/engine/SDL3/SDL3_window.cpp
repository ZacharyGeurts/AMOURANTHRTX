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
#include <SDL3/SDL_properties.h>
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
    if (w) {
        SDL_PropertiesID props = SDL_GetWindowProperties(w);
        if (props != 0) {
            void* data = SDL_GetPointerProperty(props, "vulkan_extensions", nullptr);
            delete static_cast<std::vector<std::string>*>(data);
        }
        Dispose::destroyWindow(w);
    }
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
    std::vector<std::string> extensions;
    for (uint32_t i = 0; i < extensionCount; ++i) {
        extensions.emplace_back(sdlExtensions[i]);
    }

    // Add extensions for ray tracing and debugging
    extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    LOG_DEBUG("Window", "Vulkan instance extensions retrieved: count={}", extensionCount, std::source_location::current());

    SDL_PropertiesID props = SDL_GetWindowProperties(window.get());
    if (props == 0) {
        throw std::runtime_error("Failed to get window properties");
    }
    SDL_SetPointerProperty(props, "vulkan_extensions", new std::vector<std::string>(std::move(extensions)));

    const char* videoDriver = SDL_GetCurrentVideoDriver();
    LOG_INFO("Window", "SDL window created with video driver: {}, flags=0x{:x}", 
             videoDriver ? videoDriver : "none", SDL_GetWindowFlags(window.get()), std::source_location::current());

    LOG_INFO("Window", "SDL window created with Vulkan extensions prepared", std::source_location::current());

    return window;
}

std::vector<std::string> getWindowExtensions(const SDLWindowPtr& window) {
    if (!window || !window.get()) {
        throw std::runtime_error("Invalid window pointer");
    }
    SDL_PropertiesID props = SDL_GetWindowProperties(window.get());
    if (props == 0) {
        throw std::runtime_error("Failed to get window properties");
    }
    void* ptr = SDL_GetPointerProperty(props, "vulkan_extensions", nullptr);
    if (!ptr) {
        throw std::runtime_error("No extensions data in window");
    }
    return *static_cast<std::vector<std::string>*>(ptr);
}

SDL_Window* getWindow(const SDLWindowPtr& window) {
    LOG_DEBUG("Window", "Retrieving SDL window", std::source_location::current());
    return window.get();
}

bool pollEventsForResize(const SDLWindowPtr& window, int& newWidth, int& newHeight, bool& shouldQuit, bool& toggleFullscreenKey) {
    SDL_Window* win = window.get();
    LOG_DEBUG("Window", "Polling SDL events for resize", std::source_location::current());
    SDL_Event event;
    bool resizeDetected = false;
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
                resizeDetected = true;
                LOG_DEBUG("Window", "Window resize event detected", std::source_location::current());
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

    if (resizeDetected) {
        if (!SDL_GetWindowSizeInPixels(win, &newWidth, &newHeight)) {
            LOG_ERROR("Window", "Failed to get window size in pixels after resize: {}", SDL_GetError(), std::source_location::current());
            newWidth = 0;
            newHeight = 0;
        } else {
            LOG_DEBUG("Window", "Window size in pixels after resize: {}x{}", newWidth, newHeight, std::source_location::current());
        }
        return true;
    }

    return false;
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

    // Get new drawable size and resize renderer
    int newW, newH;
    if (!SDL_GetWindowSizeInPixels(win, &newW, &newH)) {
        LOG_ERROR("Window", "Failed to get window size in pixels after fullscreen toggle: {}", SDL_GetError(), std::source_location::current());
        return;
    }
    renderer.handleResize(newW, newH);
    LOG_INFO("Window", "Fullscreen toggled to size {}x{}", newW, newH, std::source_location::current());
}

} // namespace SDL3Initializer