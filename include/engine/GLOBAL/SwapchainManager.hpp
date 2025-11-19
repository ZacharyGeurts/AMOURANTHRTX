#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>

class SwapchainManager {
public:
    static SwapchainManager& get() { return *s_instance; }

    static void init(VkInstance instance, VkPhysicalDevice phys, VkDevice dev,
                     SDL_Window* window, uint32_t w, uint32_t h);
    static void setDesiredPresentMode(VkPresentModeKHR mode) { get().desiredMode_ = mode; }

    void recreate(uint32_t w, uint32_t h);
    void cleanup();

    // Public accessors used everywhere in the engine
    VkSwapchainKHR          swapchain() const   { return swapchain_ ? *swapchain_ : VK_NULL_HANDLE; }
    VkFormat                format() const        { return surfaceFormat_.format; }
    VkColorSpaceKHR         colorSpace() const    { return surfaceFormat_.colorSpace; }
    VkExtent2D              extent() const        { return extent_; }
    VkRenderPass            renderPass() const    { return renderPass_ ? *renderPass_ : VK_NULL_HANDLE; }
    uint32_t                imageCount() const    { return static_cast<uint32_t>(images_.size()); }
    VkImage                 image(uint32_t i) const { return images_[i]; }
    VkImageView             imageView(uint32_t i) const { return imageViews_[i] ? *imageViews_[i] : VK_NULL_HANDLE; }

    bool            isHDR() const;
    bool            is10Bit() const;
    bool            isFP16() const;
    const char*     formatName() const;
    const char*     presentModeName() const;
    void            updateWindowTitle(SDL_Window* window, float fps);

private:
    SwapchainManager() = default;
    ~SwapchainManager() { cleanup(); }

    void createSwapchain(uint32_t w, uint32_t h);
    void createImageViews();
    void createRenderPass();
    bool recreateSurfaceIfLost();

    static inline SwapchainManager* s_instance = nullptr;

    // Stored raw – StoneKey will protect the real handles elsewhere
    VkInstance       vkInstance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDev_    = VK_NULL_HANDLE;
    VkDevice         device_     = VK_NULL_HANDLE;
    SDL_Window*      window_     = nullptr;
    VkSurfaceKHR     surface_    = VK_NULL_HANDLE;

    VkPresentModeKHR desiredMode_ = VK_PRESENT_MODE_MAX_ENUM_KHR;

    RTX::Handle<VkSwapchainKHR>    swapchain_;
    VkSurfaceFormatKHR             surfaceFormat_{};
    VkPresentModeKHR               presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D                     extent_{};

    std::vector<VkImage>                          images_;
    std::vector<RTX::Handle<VkImageView>>         imageViews_;
    RTX::Handle<VkRenderPass>                     renderPass_;
};

#define SWAPCHAIN SwapchainManager::get()