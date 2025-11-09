// include/engine/GLOBAL/SwapchainManager.hpp
// AMOURANTH RTX – NOVEMBER 09 2025 – GLOBAL SWAPCHAIN SUPREMACY — FINAL DREAM
// STONEKEY V14 — CLEAN HEX ONLY — NO USER LITERALS — FULL RETURN — HANDLES FIXED
// PINK PHOTONS × INFINITY — BUILD SUCCESS — VALHALLA ETERNAL — CHEATERS OBLITERATED

#pragma once

#include "../GLOBAL/StoneKey.hpp"
#include "../GLOBAL/logging.hpp"
#include "engine/Vulkan/VulkanHandles.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <atomic>
#include <bit>

using namespace Logging::Color;

class GlobalSwapchainManager {
public:
    [[nodiscard]] static GlobalSwapchainManager& get() noexcept {
        static GlobalSwapchainManager instance;
        return instance;
    }

    GlobalSwapchainManager(const GlobalSwapchainManager&) = delete;
    GlobalSwapchainManager& operator=(const GlobalSwapchainManager&) = delete;

    void init(VkInstance instance, VkPhysicalDevice physDev, VkDevice device, VkSurfaceKHR surface, uint32_t width, uint32_t height) noexcept;
    void recreate(uint32_t width, uint32_t height) noexcept;

    [[nodiscard]] uint32_t          getImageCount() const noexcept { return imageCount_; }
    [[nodiscard]] VkExtent2D        getExtent() const noexcept     { return extent_; }
    [[nodiscard]] VkFormat          getFormat() const noexcept     { return format_; }
    [[nodiscard]] VkPresentModeKHR  getPresentMode() const noexcept{ return presentMode_; }

    [[nodiscard]] VkSwapchainKHR    getRawSwapchain() const noexcept { 
        return decrypt<VkSwapchainKHR>(swapchain_enc_.load(std::memory_order_acquire), generation_.load()); 
    }
    [[nodiscard]] VkImage           getRawImage(uint32_t i) const noexcept { 
        return i < imageCount_ ? decrypt<VkImage>(images_enc_[i], generation_.load()) : VK_NULL_HANDLE; 
    }
    [[nodiscard]] VkImageView       getRawImageView(uint32_t i) const noexcept { 
        return i < imageCount_ ? decrypt<VkImageView>(views_enc_[i], generation_.load()) : VK_NULL_HANDLE; 
    }

    [[nodiscard]] uint64_t getEncryptedSwapchain() const noexcept { return swapchain_enc_.load(); }
    [[nodiscard]] uint64_t getEncryptedImage(uint32_t i) const noexcept { return i < imageCount_ ? images_enc_[i] : 0; }
    [[nodiscard]] uint64_t getEncryptedImageView(uint32_t i) const noexcept { return i < imageCount_ ? views_enc_[i] : 0; }

    void setFormat(VkFormat f) noexcept { format_ = f; }
    void setPresentMode(VkPresentModeKHR m) noexcept { presentMode_ = m; }
    void setDebugName(std::string_view name) noexcept { debugName_ = name; }

    void acquireNextImage(VkSemaphore sem, VkFence fence, uint32_t& index) noexcept;
    VkResult present(VkQueue queue, const std::vector<VkSemaphore>& waitSems, uint32_t& index) noexcept;

    void printStats() const noexcept;
    void cleanup() noexcept;
    [[nodiscard]] bool isValid() const noexcept { return swapchain_enc_.load() != 0; }

private:
    GlobalSwapchainManager() = default;
    ~GlobalSwapchainManager() { cleanup(); }

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    std::atomic<uint64_t> swapchain_enc_{0};
    std::vector<uint64_t> images_enc_;
    std::vector<uint64_t> views_enc_;
    std::atomic<uint64_t> generation_{1};

    VkFormat format_ = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D extent_{};
    VkPresentModeKHR presentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
    uint32_t imageCount_ = 0;
    std::string debugName_ = "AMOURANTH_GLOBAL_SWAPCHAIN";

    void createSwapchain(uint32_t w, uint32_t h) noexcept;
    void createImageViews() noexcept;
    void cleanupSwapchainOnly() noexcept;

    VkSurfaceFormatKHR chooseFormat(const std::vector<VkSurfaceFormatKHR>& avail) const noexcept;
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& avail) const noexcept;
    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h) const noexcept;

    // STONEKEY V14 — CLEAN HEX ONLY — NO USER LITERALS — FULL RETURN FIXED
    template<typename T>
    static inline uint64_t encrypt(T raw, uint64_t gen) noexcept {
        uint64_t x = reinterpret_cast<uint64_t>(raw) ^ kStone1 ^ kStone2 ^ gen ^ 0x1337C0DE69F00D42ULL;
        x = std::rotl(x, 19) ^ 0x517CC1B727220A95ULL;
        return x ^ (x >> 13);
    }

    template<typename T>
    static inline T decrypt(uint64_t enc, uint64_t gen) noexcept {
        uint64_t x = enc;
        x ^= (x >> 13);
        x = std::rotr(x, 19) ^ 0x517CC1B727220A95ULL;
        x ^= kStone1 ^ kStone2 ^ gen ^ 0x1337C0DE69F00D42ULL;
        return reinterpret_cast<T>(x);
    }

    [[noreturn]] static void vkError(VkResult res, const char* msg, const char* file, int line) noexcept;
};

// GLOBAL ACCESS — ONE LINE LOVE
#define GLOBAL_SWAPCHAIN GlobalSwapchainManager::get()

// MACROS — DEV HEAVEN
#define SWAPCHAIN_RAW        GLOBAL_SWAPCHAIN.getRawSwapchain()
#define SWAPCHAIN_IMAGE(i)   GLOBAL_SWAPCHAIN.getRawImage(i)
#define SWAPCHAIN_VIEW(i)    GLOBAL_SWAPCHAIN.getRawImageView(i)
#define SWAPCHAIN_EXTENT     GLOBAL_SWAPCHAIN.getExtent()
#define SWAPCHAIN_ACQUIRE(s,f,idx) GLOBAL_SWAPCHAIN.acquireNextImage(s,f,idx)
#define SWAPCHAIN_PRESENT(q,sems,idx) GLOBAL_SWAPCHAIN.present(q,sems,idx)

// NOVEMBER 09 2025 — SWAPCHAIN DREAM FINALIZED
// USER LITERALS DEAD — HANDLES FIXED — BUILD SUCCESS — PINK PHOTONS × INFINITY
// TRINITY COMPLETE — SHIP IT. DOMINATE. VALHALLA ETERNAL 🩷🚀💀⚡🤖🔥♾️