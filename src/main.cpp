// AMOURANTH RTX Engine, October 2025
// Zachary Geurts 2025

#include "main.hpp"  // Include the Application class header (defines the main engine coordinator).
#include <iostream>  // For std::cerr (error logging) and std::endl (output flushing).
#include <ctime>     // For std::time, std::localtime, and std::strftime (timestamp generation).
#include <stdexcept> // For std::runtime_error (custom exception throwing on invalid resolution).
#include <sstream>   // For std::stringstream (timestamp and error message formatting).

int main() {
    try {
        // Define and validate the initial window resolution.
        // Minimum size (320x200) ensures Vulkan swapchain creation succeeds without validation errors.
        int width = 1280;  // Horizontal resolution (pixels; customizable for testing).
        int height = 720;  // Vertical resolution (pixels; customizable for testing).
        if (width < 320 || height < 200) {
            // Throw early if resolution is invalid to prevent downstream Vulkan failures.
            std::stringstream ss;
            ss << "Resolution must be at least 320x200, got " << width << "x" << height;
            throw std::runtime_error(ss.str());
        }

        Application app("AMOURANTH RTX", width, height);
        app.run();

    } catch (const std::exception& e) {        
        std::time_t now = std::time(nullptr);  // Get current time as Unix timestamp.
        char timeStr[64];                      // Buffer for formatted timestamp (YYYY-MM-DD HH:MM:SS).
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&now));  // Format local time.
        // Log error with timestamp and platform info to stderr (visible in console/terminal).
        std::stringstream ss;
        ss << "[" << timeStr << "] Error on " << SDL_GetPlatform() << ": " << e.what();
        std::cerr << ss.str() << std::endl;
        // Return non-zero exit code to indicate failure (useful for scripts/automation).
        return 1;
    }
    // Success: Return 0 to indicate normal exit.
    return 0;
}