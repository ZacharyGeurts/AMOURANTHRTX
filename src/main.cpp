// AMOURANTH RTX Engine, October 2025 - Entry point for the application.
// This file serves as the program's main entry point, responsible for:
// - Validating minimum resolution requirements (320x200) to ensure Vulkan swapchain compatibility.
// - Instantiating the Application class with user-specified (or default) window dimensions.
// - Running the application's event loop (via Application::run()).
// - Catching and logging any exceptions with a timestamp for debugging.
// Key features:
// - Error handling: Catches std::exception and logs with local timestamp to stderr.
// - Resolution clamping: Enforces minimum size to prevent Vulkan validation errors.
// - Simplicity: Minimal main function; all logic delegated to Application class.
// - Extensibility: Easy to modify resolution or add command-line parsing (e.g., for variable sizes).
// - Platform support: Designed for Linux and Windows; enforces 1280x720 default resolution.
// Requirements:
// - Application class from main.hpp (integrates SDL3, Vulkan, and engine logic).
// - Compiled with -O3 -std=c++20 -Wall -Wextra -g -fopenmp -Iinclude (for OpenMP and debugging).
// Usage: Compile and run; the app launches a Vulkan window with the Dimensional Navigator renderer.
// Notes:
// - On error: Logs "[YYYY-MM-DD HH:MM:SS] Error: <message>" and exits with code 1.
// - For debugging: Use Vulkan validation layers to catch GPU issues; enable SDL3 logging for input/window events.
// - X11 BadMatch: Ensure SDL3 window attributes match Vulkan surface requirements (e.g., SDL_WINDOW_VULKAN).

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

        // Instantiate the Application class, passing title and resolution.
        // This triggers SDL3 window creation, Vulkan initialization, and engine setup.
        // Application constructor handles OpenMP configuration, simulator/renderer creation, and Vulkan resources.
        Application app("AMOURANTH RTX", width, height);

        // Start the main event loop: Handles rendering, input, resize, and quitting.
        // Runs until user closes window (e.g., Alt+F4, SDL_QUIT event).
        // Delegates to SDL3Initializer::eventLoop for cross-platform compatibility.
        app.run();
    } catch (const std::exception& e) {
        // Catch any std::exception thrown during initialization or runtime (e.g., Vulkan failures, SDL3 errors).
        // Generate a timestamp for logging to aid debugging (e.g., correlate with Vulkan validation output).
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