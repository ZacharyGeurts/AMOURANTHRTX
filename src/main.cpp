// AMOURANTH RTX Engine, October 2025 - Entry point for the application.
// Initializes the Application class with window dimensions and runs the event loop.
// Handles errors with timestamped logging.
// Displays assets/textures/ammo.png and plays assets/audio/ammo.wav on startup, ensuring audio playback completes before cleanup and main loop.

#include "main.hpp"  // Application class header (includes handle_app.hpp and ue_init.hpp)
#include "engine/SDL3/SDL3_audio.hpp"  // Audio management
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>  // For SDL_Vulkan_CreateSurface and SDL_Vulkan_GetInstanceExtensions
#include <SDL3_image/SDL_image.h>  // Direct SDL3_image include
#include <vulkan/vulkan.h>  // For VkSurfaceKHR
#include <iostream>  // For error logging
#include <ctime>     // For timestamp generation
#include <stdexcept> // For std::runtime_error
#include <sstream>   // For string formatting
#include <vector>    // For Vulkan extensions

// Cleanup function to ensure SDL and Vulkan resources are properly disposed
void cleanupSDL(SDL_Window*& window, SDL_Renderer*& renderer, SDL_Texture*& texture, VkInstance instance, VkSurfaceKHR surface) {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
        std::cerr << "[SDLDebug] Destroyed texture\n";
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
        std::cerr << "[SDLDebug] Destroyed renderer\n";
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
        std::cerr << "[SDLDebug] Destroyed window\n";
    }
    if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        std::cerr << "[VulkanDebug] Destroyed surface: " << surface << "\n";
    }
}

int main() {
    SDL_Window* splashWindow = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkSurfaceKHR splashSurface = VK_NULL_HANDLE;
    bool sdlInitialized = false;

    try {
        // Define and validate window resolution (minimum 320x200)
        int width = 1280;
        int height = 720;
        if (width < 320 || height < 200) {
            std::stringstream ss;
            ss << "Resolution must be at least 320x200, got " << width << "x" << height;
            throw std::runtime_error(ss.str());
        }

        // Initialize SDL with video and audio subsystems
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0) {
            throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
        }
        sdlInitialized = true;
        std::cerr << "[SDLDebug] Initialized SDL video and audio subsystems\n";

        // Scope for splash screen and audio to ensure disposal before main loop
        {
            // Create temporary window for splash screen
            splashWindow = SDL_CreateWindow("AMOURANTH RTX Splash", width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
            if (!splashWindow) {
                throw std::runtime_error(std::string("SDL_CreateWindow failed for splash: ") + SDL_GetError());
            }
            std::cerr << "[SDLDebug] Created splash window: " << splashWindow << "\n";

            // Get required Vulkan extensions for SDL
            Uint32 extensionCount = 0;
            const char* const* extensionsRaw = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
            if (!extensionsRaw) {
                cleanupSDL(splashWindow, renderer, texture, vkInstance, splashSurface);
                throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
            }
            std::vector<const char*> extensions(extensionsRaw, extensionsRaw + extensionCount);
            std::cerr << "[VulkanDebug] Retrieved " << extensionCount << " Vulkan extensions for SDL\n";

            // Enable Vulkan validation layers for debugging
            const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
            uint32_t layerCount = 1;

            // Create Vulkan instance for splash screen surface
            VkApplicationInfo appInfo = {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pNext = nullptr,
                .pApplicationName = "AMOURANTH RTX Splash",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName = "AMOURANTH RTX",
                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion = VK_API_VERSION_1_3
            };
            VkInstanceCreateInfo instanceInfo = {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .pApplicationInfo = &appInfo,
                .enabledLayerCount = layerCount,
                .ppEnabledLayerNames = validationLayers,
                .enabledExtensionCount = extensionCount,
                .ppEnabledExtensionNames = extensions.data()
            };
            VkResult instanceResult = vkCreateInstance(&instanceInfo, nullptr, &vkInstance);
            if (instanceResult != VK_SUCCESS) {
                cleanupSDL(splashWindow, renderer, texture, VK_NULL_HANDLE, VK_NULL_HANDLE);
                throw std::runtime_error(std::string("Failed to create Vulkan instance for splash screen: VkResult ") + std::to_string(instanceResult));
            }
            std::cerr << "[VulkanDebug] Created Vulkan instance: " << vkInstance << "\n";

            // Create Vulkan surface for splash window
            if (!SDL_Vulkan_CreateSurface(splashWindow, vkInstance, nullptr, &splashSurface)) {
                cleanupSDL(splashWindow, renderer, texture, vkInstance, VK_NULL_HANDLE);
                throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
            }
            std::cerr << "[VulkanDebug] Created Vulkan surface: " << splashSurface << "\n";

            // Create renderer for splash screen
            renderer = SDL_CreateRenderer(splashWindow, nullptr);
            if (!renderer) {
                cleanupSDL(splashWindow, renderer, texture, vkInstance, splashSurface);
                throw std::runtime_error(std::string("SDL_CreateRenderer failed for splash: ") + SDL_GetError());
            }
            std::cerr << "[SDLDebug] Created renderer: " << renderer << "\n";

            // Show the window
            SDL_ShowWindow(splashWindow);
            std::cerr << "[SDLDebug] Showed splash window\n";

            // Load texture directly with SDL3_image
            texture = IMG_LoadTexture(renderer, "assets/textures/ammo.png");
            if (!texture) {
                cleanupSDL(splashWindow, renderer, texture, vkInstance, splashSurface);
                throw std::runtime_error(std::string("IMG_LoadTexture failed for assets/textures/ammo.png: ") + SDL_GetError());
            }
            std::cerr << "[SDLDebug] Loaded texture: " << texture << "\n";

            // Get texture dimensions
            float texWidth, texHeight;
            SDL_GetTextureSize(texture, &texWidth, &texHeight);

            // Clear screen
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            // Center the texture on screen
            SDL_FRect dstRect = {
                static_cast<float>(width - texWidth) / 2.0f,
                static_cast<float>(height - texHeight) / 2.0f,
                texWidth,
                texHeight
            };

            // Render texture
            SDL_RenderTexture(renderer, texture, nullptr, &dstRect);
            SDL_RenderPresent(renderer);
            std::cerr << "[SDLDebug] Rendered splash texture\n";

            // Initialize audio and play startup sound
            {
                SDL3Audio::AudioConfig audioConfig;
                audioConfig.frequency = 44100;
                audioConfig.format = SDL_AUDIO_S16LE;
                audioConfig.channels = 8; // 7.1
                audioConfig.callback = nullptr;
                SDL3Audio::AudioManager audioManager(audioConfig);
                audioManager.playAmmoSound();
                std::cerr << "[AudioDebug] Played startup sound: assets/audio/ammo.wav\n";

                // Wait for audio playback to complete (assume 2 seconds for ammo.wav)
                // TODO: Replace with callback-based detection if audio duration is variable
                SDL_Delay(3000); // Adjust based on actual audio duration if known

                // AudioManager destroyed here after playback completes
                std::cerr << "[AudioDebug] Audio resources cleaned up\n";
            }

            // Ensure splash screen is displayed for at least 3 seconds total
            SDL_Delay(100); // Small additional delay to ensure rendering stability

            // Clear screen again to clean up
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);
            std::cerr << "[SDLDebug] Cleared splash screen\n";

            // Clean up texture, renderer, window, and Vulkan surface
            cleanupSDL(splashWindow, renderer, texture, vkInstance, splashSurface);
            splashSurface = VK_NULL_HANDLE;

            // Destroy Vulkan instance
            if (vkInstance != VK_NULL_HANDLE) {
                vkDestroyInstance(vkInstance, nullptr);
                std::cerr << "[VulkanDebug] Destroyed Vulkan instance: " << vkInstance << "\n";
                vkInstance = VK_NULL_HANDLE;
            }

            // Extended delay to ensure X11/Vulkan resources are released
            SDL_Delay(100);
            std::cerr << "[SDLDebug] Completed splash screen and audio cleanup\n";
        }

        // Instantiate Application with title and resolution
        Application app("AMOURANTH RTX", width, height);

        // Run the main event loop
        app.run();

        // Cleanup SDL (after Application's destructor)
        if (sdlInitialized) {
            SDL_Quit();
            sdlInitialized = false;
            std::cerr << "[SDLDebug] Quit SDL\n";
        }
    } catch (const std::exception& e) {
        // Clean up any remaining SDL and Vulkan resources
        cleanupSDL(splashWindow, renderer, texture, vkInstance, splashSurface);
        if (vkInstance != VK_NULL_HANDLE) {
            vkDestroyInstance(vkInstance, nullptr);
            std::cerr << "[VulkanDebug] Destroyed Vulkan instance due to exception: " << vkInstance << "\n";
        }
        if (sdlInitialized) {
            SDL_Quit();
            sdlInitialized = false;
            std::cerr << "[SDLDebug] Quit SDL due to exception\n";
        }

        // Log error with timestamp to stderr
        std::time_t now = std::time(nullptr);
        char timeStr[64];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        std::stringstream ss;
        ss << "[" << timeStr << "] Error on " << SDL_GetPlatform() << ": " << e.what();
        std::cerr << ss.str() << std::endl;
        return 1;
    }
    return 0;
}