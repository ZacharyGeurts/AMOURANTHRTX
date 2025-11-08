// VulkanSwapchainManager.hpp
#pragma once
#include "engine/StoneKey.hpp"
#include <vulkan/vulkan.h>
#include <vector>

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager() = default;
    ~VulkanSwapchainManager() { cleanup(); }

    void init(VkDevice device, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void cleanup();
    void recreate(uint32_t width, uint32_t height);

    uint32_t getImageCount() const { return static_cast<uint32_t>(swapchainImages_enc_.size()); }
    uint64_t getSwapchainImageView(uint32_t index) const { return swapchainImageViews_enc_[index]; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_raw_ = VK_NULL_HANDLE;

    uint64_t swapchain_enc_ = 0;
    std::vector<uint64_t> swapchainImages_enc_;
    std::vector<uint64_t> swapchainImageViews_enc_;

    template<typename T>
    [[nodiscard]] static inline constexpr uint64_t encrypt(T raw) noexcept {
        return static_cast<uint64_t>(raw) ^ kStone1 ^ kStone2;
    }

    template<typename T>
    [[nodiscard]] static inline constexpr T decrypt(uint64_t enc) noexcept {
        return static_cast<T>(enc ^ kStone1 ^ kStone2);
    }

    void createSwapchain(uint32_t width, uint32_t height, VkSurfaceKHR surface);
    void createImageViews();
};