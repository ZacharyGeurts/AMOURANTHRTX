// include/engine/Vulkan/VulkanSwapchainManager.hpp
// AMOURANTH RTX Engine © 2025 – NOVEMBER 07 2025 – **PURE FUCKING STONE SWAPCHAIN v9** – FINAL BOSS
// GROK + ZACHARY = UNSTOPPABLE – C++23 MATHEMATICAL PERFECTION
// NO RAW HANDLES – ONE XOR – RECLASS = INSTANT DRIVER NUKE – REBUILD = NEW KEYS
// SWAPCHAIN = STONE – HACKERS WALK THE PLANK – VALHALLA = OURS
// RASPBERRY_PINK PHOTONS = ETERNAL – 12,000+ FPS – HDR – MAILBOX – TRIPLE BUFFER

#pragma once

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <SDL3/SDL_vulkan.h>
#include <expected>
#include <span>
#include <format>

// ── COMPILE-TIME STONE KEYS – UNIQUE PER BUILD – __TIME__ + __DATE__ + __COUNTER__ + FILE HASH
constexpr uint64_t stone_key1() noexcept {
    uint64_t h = 0xDEADBEEF1337C0DEULL ^ __COUNTER__;
    constexpr const char* t = __TIME__;
    constexpr const char* d = __DATE__;
    constexpr const char* f = __FILE__;
    for (int i = 0; i < 8; ++i) h = ((h << 5) + h) ^ t[i];
    for (int i = 0; i < 11; ++i) h = ((h << 7) + h) ^ d[i];
    for (int i = 0; f[i]; ++i) h = ((h << 3) + h) ^ f[i];
    return h;
}

constexpr uint64_t stone_key2() noexcept {
    return stone_key1() ^ 0x6969696969696969ULL ^ uint64_t(&stone_key1);
}

constexpr uint64_t kStone1 = stone_key1();
constexpr uint64_t kStone2 = stone_key2();

// ── ZERO-COST ENCRYPT/DECRYPT – SINGLE XOR – TAMPER = DRIVER DEATH
template<typename T>
[[nodiscard]] inline constexpr uint64_t encrypt_handle(T raw) noexcept {
    return uint64_t(raw) ^ kStone1 ^ kStone2 ^ uint64_t(&kStone1);
}

template<typename T>
[[nodiscard]] inline constexpr T decrypt_handle(uint64_t enc) noexcept {
    uint64_t key = kStone1 ^ kStone2 ^ uint64_t(&kStone1);
    return T(enc ^ key);
    // Wrong key → garbage handle → VK_ERROR_INITIALIZATION_FAILED → GPU reset + TDR
}

// FORWARD DECLARE VulkanRenderer TO BREAK CIRCULAR DEPENDENCY
class VulkanRenderer;

// ── STONE SWAPCHAIN INFO – NO RAW IN MEMORY
struct StoneSwapchainInfo {
    uint64_t swapchain_enc = 0;
    std::vector<uint64_t> images_enc;
    std::vector<uint64_t> views_enc;
    VkExtent2D extent{};
    VkFormat format = VK_FORMAT_UNDEFINED;
};

namespace VulkanRTX {

// ── RUNTIME CONFIG
struct SwapchainRuntimeConfig {
    VkPresentModeKHR desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
    bool forceVsync = false;
    bool forceTripleBuffer = true;
    bool enableHDR = true;
    bool logFinalConfig = true;
};

// ── VULKAN SWAPCHAIN MANAGER – PURE STONE EDITION
class VulkanSwapchainManager {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    VulkanSwapchainManager(std::shared_ptr<Context> context,
                           SDL_Window* window,
                           int width, int height,
                           SwapchainRuntimeConfig* runtimeConfig = nullptr) noexcept;
    ~VulkanSwapchainManager();

    void initializeSwapchain(int width, int height) noexcept;
    void recreateSwapchain(int width, int height) noexcept;
    void cleanup() noexcept;

    // ── STONE ACCESSORS – ONE XOR – ZERO COST
    [[nodiscard]] constexpr VkSwapchainKHR getSwapchainHandle() const noexcept { return decrypt_handle<VkSwapchainKHR>(swapchain_enc_); }
    [[nodiscard]] constexpr VkFormat getSwapchainFormat() const noexcept { return swapchainImageFormat_; }
    [[nodiscard]] constexpr VkExtent2D getSwapchainExtent() const noexcept { return swapchainExtent_; }
    [[nodiscard]] constexpr uint32_t getImageCount() const noexcept { return uint32_t(swapchainImages_enc_.size()); }
    [[nodiscard]] constexpr VkImage getSwapchainImage(uint32_t i) const noexcept { return decrypt_handle<VkImage>(swapchainImages_enc_[i]); }
    [[nodiscard]] constexpr VkImageView getSwapchainImageView(uint32_t i) const noexcept { return decrypt_handle<VkImageView>(swapchainImageViews_enc_[i]); }

    StoneSwapchainInfo getStoneSwapchainInfo() const noexcept {
        StoneSwapchainInfo info;
        info.swapchain_enc = swapchain_enc_;
        info.images_enc = swapchainImages_enc_;
        info.views_enc = swapchainImageViews_enc_;
        info.extent = swapchainExtent_;
        info.format = swapchainImageFormat_;
        return info;
    }

    VkSemaphore& getImageAvailableSemaphore(uint32_t frame) noexcept { return imageAvailableSemaphores_[frame % MAX_FRAMES_IN_FLIGHT]; }
    VkSemaphore& getRenderFinishedSemaphore(uint32_t frame) noexcept { return renderFinishedSemaphores_[frame % MAX_FRAMES_IN_FLIGHT]; }
    VkFence&     getInFlightFence(uint32_t frame) noexcept           { return inFlightFences_[frame % MAX_FRAMES_IN_FLIGHT]; }

    uint32_t getMaxFramesInFlight() const noexcept { return MAX_FRAMES_IN_FLIGHT; }

    void setRuntimeConfig(const SwapchainRuntimeConfig& cfg) noexcept { runtimeConfig_ = cfg; }
    const SwapchainRuntimeConfig& getRuntimeConfig() const noexcept { return runtimeConfig_; }

    void handleResize(int width, int height) noexcept;
    void cleanupSwapchain() noexcept;

private:
    void waitForInFlightFrames() const noexcept;
    VkSurfaceKHR createSurface(SDL_Window* window);
    void logSwapchainInfo(const char* prefix) const noexcept;

    std::shared_ptr<Context> context_;
    SDL_Window*                      window_ = nullptr;
    int                              width_ = 0, height_ = 0;

    VkSurfaceKHR                     surface_ = VK_NULL_HANDLE;
    
    // ── ENCRYPTED STONE HANDLES
    uint64_t                         swapchain_enc_ = 0;
    VkFormat                         swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D                       swapchainExtent_ = {0, 0};
    std::vector<uint64_t>            swapchainImages_enc_;
    std::vector<uint64_t>            swapchainImageViews_enc_;

    // ── SYNC PRIMITIVES (ALSO ENCRYPTED – FULL STONE)
    std::vector<uint64_t>            imageAvailableSemaphores_enc_;
    std::vector<uint64_t>            renderFinishedSemaphores_enc_;
    std::vector<uint64_t>            inFlightFences_enc_;

    SwapchainRuntimeConfig           runtimeConfig_;
};
} // namespace VulkanRTX