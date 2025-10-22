// include/engine/SDL3/SDL3_init.hpp
// AMOURANTH RTX Engine, October 2025 - SDL3 and Vulkan initialization.
// Creates Vulkan instance and surface via SDL3; handles window creation.
// Thread-safe, no mutexes; designed for Windows/Linux (X11/Wayland).
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library.
// Usage: Create SDL3Initializer, call getInstance() and getSurface() for VulkanRenderer.
// Zachary Geurts 2025

#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <set>

namespace SDL3Initializer {

// Custom deleters for Vulkan resources (declarations only; definitions in .cpp)
struct VulkanInstanceDeleter {
    void operator()(VkInstance instance) const;
};

struct VulkanSurfaceDeleter {
    VkInstance m_instance;  // Note: Assumed member from cpp usage; adjust if needed
    explicit VulkanSurfaceDeleter(VkInstance inst = VK_NULL_HANDLE) : m_instance(inst) {}
    void operator()(VkSurfaceKHR surface) const;
};

// Smart pointer aliases
using VulkanInstancePtr = std::unique_ptr<VkInstance_T, VulkanInstanceDeleter>;
using VulkanSurfacePtr = std::unique_ptr<VkSurfaceKHR_T, VulkanSurfaceDeleter>;

class SDL3Initializer {
public:
    SDL3Initializer(const std::string& title, int width, int height);
    ~SDL3Initializer() = default;

    VkInstance getInstance() const { return instance_.get(); }
    VkSurfaceKHR getSurface() const { return surface_.get(); }
    SDL_Window* getWindow() const { return window_; }
    bool shouldQuit() const;
    void pollEvents();

private:
    SDL_Window* window_ = nullptr;
    VulkanInstancePtr instance_;
    VulkanSurfacePtr surface_;
};

// Helper function to format std::set<std::string> for logging
inline std::string formatSet(const std::set<std::string>& set) {
    std::string result = "{";
    for (auto it = set.begin(); it != set.end(); ++it) {
        result += *it;
        if (std::next(it) != set.end()) {
            result += ", ";
        }
    }
    result += "}";
    return result;
}

} // namespace SDL3Initializer

#endif // SDL3_INIT_HPP