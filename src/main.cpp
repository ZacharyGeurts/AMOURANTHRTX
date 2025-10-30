// AMOURANTH RTX Engine, October 2025 - Entry point for the application.
// Initializes the Application class with window dimensions and runs the event loop.
// Handles errors with timestamped logging.
// Displays assets/textures/ammo.png and plays assets/audio/ammo.wav on startup, ensuring audio playback completes before cleanup and main loop.

#include "main.hpp"  // Application class header (includes handle_app.hpp and ue_init.hpp)
#include "engine/SDL3/SDL3_audio.hpp"  // Audio management
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>  // For IMG_LoadTexture
#include <iostream>  // For error logging
#include <ctime>     // For timestamp generation
#include <stdexcept> // For std::runtime_error
#include <sstream>   // For string formatting

// Cleanup function to ensure SDL resources are properly disposed
void cleanupSDL(SDL_Window*& window, SDL_Renderer*& renderer, SDL_Texture*& texture) {
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
}

int main() {
    SDL_Window* splashWindow = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
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
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
            throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
        }
        sdlInitialized = true;
        std::cerr << "[SDLDebug] Initialized SDL video and audio subsystems\n";

        // Scope for splash screen and audio to ensure disposal before main loop
        {
            // Create temporary window for splash screen (no VULKAN flag needed for software rendering)
            splashWindow = SDL_CreateWindow("AMOURANTH RTX", width, height, SDL_WINDOW_HIDDEN);
            if (!splashWindow) {
                throw std::runtime_error(std::string("SDL_CreateWindow failed for splash: ") + SDL_GetError());
            }
            std::cerr << "[SDLDebug] Created splash window: " << splashWindow << "\n";

            // Create renderer for splash screen
            renderer = SDL_CreateRenderer(splashWindow, nullptr);
            if (!renderer) {
                cleanupSDL(splashWindow, renderer, texture);
                throw std::runtime_error(std::string("SDL_CreateRenderer failed for splash: ") + SDL_GetError());
            }
            std::cerr << "[SDLDebug] Created renderer: " << renderer << "\n";

            // Show the window
            SDL_ShowWindow(splashWindow);
            std::cerr << "[SDLDebug] Showed splash window\n";

            // Load texture directly with SDL3_image
            texture = IMG_LoadTexture(renderer, "assets/textures/ammo.png");
            if (!texture) {
                cleanupSDL(splashWindow, renderer, texture);
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

            // Clean up texture, renderer, and window
            cleanupSDL(splashWindow, renderer, texture);

            // Extended delay to ensure resources are released
            SDL_Delay(100);
            std::cerr << "[SDLDebug] Completed splash screen and audio cleanup\n";
        }

        // Instantiate Application with title and resolution
        Application app("AMOURANTH RTX", width, height);

        // Run the main event loop
        app.run();

        // No explicit SDL_Quit here; handled in Application destructor
    } catch (const std::exception& e) {
        // Clean up any remaining SDL resources
        cleanupSDL(splashWindow, renderer, texture);
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