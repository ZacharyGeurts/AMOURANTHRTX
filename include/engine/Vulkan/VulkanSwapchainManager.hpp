// include/engine/Vulkan/VulkanSwapchainManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// STONEKEY PIONEER C++23 — OPAQUE HANDLE SAFE — NOVEMBER 08 2025

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager() = default;
    ~VulkanSwapchainManager() { cleanup(); }

    void init(VkInstance instance, VkPhysicalDevice physDev, VkDevice device, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void cleanup() noexcept;
    void recreate(uint32_t width, uint32_t height);

    [[nodiscard]] uint32_t getImageCount() const noexcept { return static_cast<uint32_t>(swapchainImages_enc_.size()); }

    [[nodiscard]] VkSwapchainKHR getRawSwapchain() const noexcept {
        return decrypt<VkSwapchainKHR>(swapchain_enc_);
    }

    [[nodiscard]] VkImageView getSwapchainImageView(uint32_t index) const noexcept {
        return decrypt<VkImageView>(swapchainImageViews_enc_[index]);
    }

private:
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    uint64_t swapchain_enc_ = 0;
    std::vector<uint64_t> swapchainImages_enc_;
    std::vector<uint64_t> swapchainImageViews_enc_;

    VkFormat swapchainFormat_ = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D swapchainExtent_{};

    // STONEKEY — FULLY OPAQUE SAFE
    template<typename T>
    [[nodiscard]] static inline constexpr uint64_t encrypt(T raw) noexcept {
        return reinterpret_cast<uint64_t>(raw) ^ kStone1 ^ kStone2;
    }

    template<typename T>
    [[nodiscard]] static inline constexpr T decrypt(uint64_t enc) noexcept {
        return reinterpret_cast<T>(enc ^ kStone1 ^ kStone2);
    }

    void createSwapchain(uint32_t width, uint32_t height);
    void createImageViews();
    void cleanupSwapchainOnly() noexcept;
};