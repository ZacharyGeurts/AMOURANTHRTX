// AMOURANTH RTX Engine, October 2025 - Entry point for the application.
// Initializes the Application class with window dimensions and runs the event loop.
// Handles errors with timestamped logging.

#include "main.hpp"  // Application class header
#include "engine/SDL3/SDL3_audio.hpp"  // Audio management
#include <iostream>  // For error logging
#include <ctime>     // For timestamp generation
#include <stdexcept> // For std::runtime_error
#include <sstream>   // For string formatting

int main() {
    try {
        // Define and validate window resolution (minimum 320x200)
        int width = 1280;
        int height = 720;
        if (width < 320 || height < 200) {
            std::stringstream ss;
            ss << "Resolution must be at least 320x200, got " << width << "x" << height;
            throw std::runtime_error(ss.str());
        }

        // Initialize audio and play startup sound
        SDL3Audio::AudioConfig audioConfig;
        audioConfig.frequency = 44100;
        audioConfig.format = SDL_AUDIO_S16LE;
        audioConfig.channels = 8; // 7.1
        audioConfig.callback = nullptr;
        SDL3Audio::AudioManager audioManager(audioConfig);
        audioManager.playAmmoSound();
        SDL_Delay(1000);  // Brief delay to allow sound to play (adjust as needed)

        // Instantiate Application with title and resolution
        Application app("AMOURANTH RTX", width, height);

        // Run the main event loop (handles rendering, input, and quitting)
        app.run();
    } catch (const std::exception& e) {
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