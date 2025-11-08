// include/engine/SDL3/SDL3_init.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// SDL3 + Vulkan bootstrap — FULL C++23 — NOVEMBER 08 2025
// RASPBERRY_PINK FOR DISPOSE ONLY

#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

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
using VulkanSurfacePtr = std::unique_ptr<VkSurfaceKHR_T, VulkanSurfaceDeleter>;

class SDL3Initializer {
public:
    SDL3Initializer(const std::string& title, int width, int height);
    SDL3Initializer(const std::string& title, int width, int height, Uint32 flags);
    ~SDL3Initializer() = default;

    SDL3Initializer(const SDL3Initializer&) = delete;
    SDL3Initializer& operator=(const SDL3Initializer&) = delete;

    [[nodiscard]] VkInstance   getInstance() const noexcept { return instance_.get(); }
    [[nodiscard]] VkSurfaceKHR getSurface()  const noexcept { return surface_.get(); }
    [[nodiscard]] SDL_Window*  getWindow()   const noexcept { return window_; }

    bool shouldQuit() const;
    void pollEvents();

private:
    SDL_Window* window_ = nullptr;
    VulkanInstancePtr instance_;
    VulkanSurfacePtr surface_;

    static std::string vkResultToString(VkResult result);
    static std::string formatExtensions(const std::vector<const char*>& exts);
};

} // namespace SDL3Initializer