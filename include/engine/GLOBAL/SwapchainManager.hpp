// include/engine/GLOBAL/SwapchainManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary "ST4CK" Geurts gzac5314@gmail.com
// GLOBAL STONEKEYED SWAPCHAIN — HYPER-SECURE | GETTER/SETTER GODMODE | LOGGING SUPREMACY
// PINK PHOTON ETERNAL — NOVEMBER 08 2025 — HACKERS OBLITERATED 🩷🚀🔥🤖💀❤️⚡♾️

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <string>
#include <optional>
#include <limits>

// ────── BULLETPROOF VK_CHECK — OPTIONAL MSG EDITION ──────
#define VK_CHECK_NOMSG(call) do {                    \
    VkResult __res = (call);                         \
    if (__res != VK_SUCCESS) {                       \
        VulkanSwapchainManager::vkError(__res, "Vulkan call failed", __FILE__, __LINE__); \
    }                                                \
} while (0)

#define VK_CHECK(call, msg) do {                     \
    VkResult __res = (call);                         \
    if (__res != VK_SUCCESS) {                       \
        VulkanSwapchainManager::vkError(__res, msg, __FILE__, __LINE__); \
    }                                                \
} while (0)

class VulkanSwapchainManager {
public:
    // ────── GLOBAL SINGLETON — IMMORTAL & THREAD-SAFE ──────
    static VulkanSwapchainManager& get() {
        static VulkanSwapchainManager instance;
        return instance;
    }

    VulkanSwapchainManager(const VulkanSwapchainManager&) = delete;
    VulkanSwapchainManager& operator=(const VulkanSwapchainManager&) = delete;
    ~VulkanSwapchainManager() { cleanup(); }

    // ────── CORE API ──────
    void init(VkInstance instance, VkPhysicalDevice physDev, VkDevice device, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void cleanup() noexcept;
    void recreate(uint32_t width, uint32_t height);

    // ────── GETTERS — RAW + ENCRYPTED — PERFECT NAMES ──────
    [[nodiscard]] uint32_t          getImageCount() const noexcept { return static_cast<uint32_t>(swapchainImages_enc_.size()); }
    [[nodiscard]] VkExtent2D        getExtent() const noexcept { return swapchainExtent_; }
    [[nodiscard]] VkFormat          getFormat() const noexcept { return swapchainFormat_; }
    [[nodiscard]] VkPresentModeKHR  getPresentMode() const noexcept { return presentMode_; }

    // RAW (DECRYPTED) — EXACT NAMES YOUR VulkanCommon.cpp EXPECTS
    [[nodiscard]] VkSwapchainKHR    getRawSwapchain() const noexcept { return decrypt<VkSwapchainKHR>(swapchain_enc_); }
    [[nodiscard]] VkImage           getSwapchainImage(uint32_t index) const noexcept { 
        return index < swapchainImages_enc_.size() ? decrypt<VkImage>(swapchainImages_enc_[index]) : VK_NULL_HANDLE; 
    }
    [[nodiscard]] VkImageView       getSwapchainImageView(uint32_t index) const noexcept { 
        return index < swapchainImageViews_enc_.size() ? decrypt<VkImageView>(swapchainImageViews_enc_[index]) : VK_NULL_HANDLE; 
    }

    // ENCRYPTED HANDLES — SAFE PUBLIC API
    [[nodiscard]] uint64_t          getEncryptedSwapchain() const noexcept { return swapchain_enc_; }
    [[nodiscard]] uint64_t          getEncryptedImage(uint32_t index) const noexcept { 
        return index < swapchainImages_enc_.size() ? swapchainImages_enc_[index] : 0; 
    }
    [[nodiscard]] uint64_t          getEncryptedImageView(uint32_t index) const noexcept { 
        return index < swapchainImageViews_enc_.size() ? swapchainImageViews_enc_[index] : 0; 
    }

    // ────── SETTERS WITH LOGGING ──────
    void setFormat(VkFormat format) noexcept { 
        swapchainFormat_ = format; 
        LOG_INFO_CAT("Swapchain", "Format set to {} (0x{:X})", static_cast<uint32_t>(format), static_cast<uint32_t>(format)); 
    }
    void setPresentMode(VkPresentModeKHR mode) noexcept { 
        presentMode_ = mode; 
        LOG_INFO_CAT("Swapchain", "Present mode set to {} (0x{:X})", static_cast<uint32_t>(mode), static_cast<uint32_t>(mode)); 
    }
    void setDebugName(const std::string& name) noexcept { 
        debugName_ = name; 
        LOG_INFO_CAT("Swapchain", "Debug name set to '{}'", name); 
    }

    // ────── ADVANCED ──────
    void acquireNextImage(VkSemaphore imageAvailableSemaphore, VkFence imageAvailableFence, uint32_t& imageIndex) noexcept;
    VkResult present(VkQueue presentQueue, const std::vector<VkSemaphore>& waitSemaphores, uint32_t& imageIndex) noexcept;
    void printStats() const noexcept;

private:
    // PRIVATE CTOR — ONLY SINGLETON CAN LIVE
    VulkanSwapchainManager() = default;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    uint64_t swapchain_enc_ = 0;
    std::vector<uint64_t> swapchainImages_enc_;
    std::vector<uint64_t> swapchainImageViews_enc_;

    VkFormat swapchainFormat_ = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D swapchainExtent_{};
    VkPresentModeKHR presentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
    std::string debugName_ = "AMOURANTH_GLOBAL_SWAPCHAIN";

    // ────── STONEKEY V6 — CONSTEXPR CHAOS — BUILD-UNIQUE ──────
    template<typename T>
    [[nodiscard]] static inline constexpr uint64_t encrypt(T raw) noexcept {
        constexpr uint64_t pinkChaos = 0x1337C0DEFA11BEEFULL;
        return reinterpret_cast<uint64_t>(raw) ^ kStone1 ^ kStone2 ^ pinkChaos;
    }

    template<typename T>
    [[nodiscard]] static inline constexpr T decrypt(uint64_t enc) noexcept {
        constexpr uint64_t pinkChaos = 0x1337C0DEFA11BEEFULL;
        return reinterpret_cast<T>(enc ^ kStone1 ^ kStone2 ^ pinkChaos);
    }

    // ────── ERROR SYSTEM — TOASTER-PROOF ──────
    [[noreturn]] static void vkError(VkResult res, const char* msg, const char* file, int line) noexcept {
        std::cerr << "\n🩷 [SWAPCHAIN FATAL] " << static_cast<int>(res)
                  << " | " << msg << " | " << file << ":" << line << " 🩷\n";
        std::cerr << "TOASTER DEFENSE ENGAGED — SWAPCHAIN PROTECTED 💀\n";
        std::terminate();
    }

    void createSwapchain(uint32_t width, uint32_t height);
    void createImageViews();
    void cleanupSwapchainOnly() noexcept;

    [[nodiscard]] VkSurfaceFormatKHR selectSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const noexcept;
    [[nodiscard]] VkPresentModeKHR selectSwapchainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const noexcept;
    [[nodiscard]] VkExtent2D selectSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) const noexcept;
};

#undef VK_CHECK_NOMSG
#undef VK_CHECK

#define SWAPCHAIN_MGR VulkanSwapchainManager::get()
#define SWAPCHAIN_RAW SWAPCHAIN_MGR.getRawSwapchain()
#define SWAPCHAIN_IMAGE(i) SWAPCHAIN_MGR.getSwapchainImage(i)
#define SWAPCHAIN_VIEW(i) SWAPCHAIN_MGR.getSwapchainImageView(i)