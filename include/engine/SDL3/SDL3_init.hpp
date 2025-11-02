// include/engine/SDL3/SDL3_init.hpp
// AMOURANTH RTX Engine – SDL3 + Vulkan bootstrap (October 2025)

#pragma once
#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <set>

namespace SDL3Initializer {

struct VulkanInstanceDeleter {
    void operator()(VkInstance instance) const;
};

struct VulkanSurfaceDeleter {
    VkInstance m_instance = VK_NULL_HANDLE;
    explicit VulkanSurfaceDeleter(VkInstance inst = VK_NULL_HANDLE) : m_instance(inst) {}
    void operator()(VkSurfaceKHR surface) const;
};

using VulkanInstancePtr = std::unique_ptr<VkInstance_T, VulkanInstanceDeleter>;
using VulkanSurfacePtr  = std::unique_ptr<VkSurfaceKHR_T, VulkanSurfaceDeleter>;

class SDL3Initializer {
public:
    // 3-arg: default Vulkan-ready window
    SDL3Initializer(const std::string& title, int width, int height);
    // 4-arg: explicit flags
    SDL3Initializer(const std::string& title, int width, int height, Uint32 flags);
    ~SDL3Initializer() = default;

    [[nodiscard]] VkInstance   getInstance() const { return instance_.get(); }
    [[nodiscard]] VkSurfaceKHR getSurface()  const { return surface_.get(); }
    [[nodiscard]] SDL_Window*  getWindow()   const { return window_; }

    bool shouldQuit() const;
    void pollEvents();

private:
    SDL_Window*       window_ = nullptr;
    VulkanInstancePtr instance_;
    VulkanSurfacePtr  surface_;
};

inline std::string formatSet(const std::set<std::string>& s) {
    std::string r = "{";
    for (auto it = s.begin(); it != s.end(); ++it) {
        r += *it;
        if (std::next(it) != s.end()) r += ", ";
    }
    r += "}";
    return r;
}

} // namespace SDL3Initializer

#endif // SDL3_INIT_HPP