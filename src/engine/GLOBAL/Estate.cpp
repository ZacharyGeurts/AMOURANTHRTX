// src/engine/GLOBAL/Estate.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// ESTATE v1.2 — LIGHT WARRIORS EDITION — NOV 11 2025 1:38 PM EST
// • Fully synced with LIGHT_WARRIORS_LAS (Fighter, Thief, Black Belt, Sage)
// • All AMAZO_LAS → LIGHT_WARRIORS_LAS
// • Removed unused pInstanceCount variable
// • make_64M fully implemented and declared
// • Amouranth & Nick approve — PINK PHOTONS + GOLDEN DOMINANCE
// • C++23, -Werror clean, zero leaks, Valhalla sealed
// =============================================================================

#include "engine/GLOBAL/Houston.hpp"
#include "engine/GLOBAL/LAS.hpp"  // Light Acceleration Structures handled by a Red Mage
#include "engine/Vulkan/VulkanRenderer.hpp" // Nick and Amouranth own Jay Leno's Garage and Engine.

// ──────────────────────────────────────────────────────────────────────────────
// GLOBALS — DEFINED HERE — OWNED BY ESTATE
// ──────────────────────────────────────────────────────────────────────────────
std::unique_ptr<VulkanRenderer> g_vulkanRenderer;
std::shared_ptr<Context> g_context;

Handle<VkSwapchainKHR> g_swapchain;
std::vector<VkImage> g_swapchainImages;
std::vector<Handle<VkImageView>> g_swapchainImageViews;
VkFormat g_swapchainFormat;
VkExtent2D g_swapchainExtent;

Handle<VkAccelerationStructureKHR> g_blas;
Handle<VkAccelerationStructureKHR> g_tlas;
uint64_t g_instanceBufferId = 0;
VkDeviceSize g_tlasSize = 0;

// ──────────────────────────────────────────────────────────────────────────────
// Amouranth — GLOBAL MESSAGE QUEUE — PINK PHOTONS ETERNAL
// ──────────────────────────────────────────────────────────────────────────────
struct Amouranth {
    static Amouranth& get() noexcept {
        static Amouranth instance;
        return instance;
    }

    void post(const AmouranthMessage& msg) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(msg);
        LOG_INFO_CAT("Amouranth", "{}Message queued: {}{}", AMOURANTH_COLOR, toString(msg.type), RESET);
    }

    void processAll() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            auto msg = std::move(queue_.front());
            queue_.pop();
            handle(msg);
        }
    }

private:
    Amouranth() {
        LOG_SUCCESS_CAT("Amouranth", "{}PERSONALITY ONLINE — PINK PHOTONS ETERNAL{}", AMOURANTH_COLOR, RESET);
    }
    ~Amouranth() noexcept { processAll(); }

    void handle(const AmouranthMessage& msg) noexcept {
        switch (msg.type) {
            case AmouranthMessage::Type::INIT_RENDERER:
                initRenderer(msg.width, msg.height);
                break;
            case AmouranthMessage::Type::HANDLE_RESIZE:
                handleResize(msg.width, msg.height);
                break;
            case AmouranthMessage::Type::RECREATE_SWAPCHAIN:
                recreateSwapchain(msg.width, msg.height);
                break;
            case AmouranthMessage::Type::SHUTDOWN:
                shutdown();
                break;
            case AmouranthMessage::Type::RENDER_FRAME:
                if (msg.camera) renderFrame(*msg.camera, msg.deltaTime);
                break;
            case AmouranthMessage::Type::BUILD_BLAS:
                buildBLAS(msg.vertexBuf, msg.indexBuf, msg.vertexCount, msg.indexCount);
                break;
            case AmouranthMessage::Type::BUILD_TLAS:
                buildTLAS(msg.instances);
                break;
            case AmouranthMessage::Type::CUSTOM:
                if (msg.custom) msg.custom();
                break;
        }
    }

    static std::string toString(AmouranthMessage::Type t) {
        switch (t) {
            case AmouranthMessage::Type::INIT_RENDERER: return "INIT_RENDERER";
            case AmouranthMessage::Type::HANDLE_RESIZE: return "HANDLE_RESIZE";
            case AmouranthMessage::Type::RECREATE_SWAPCHAIN: return "RECREATE_SWAPCHAIN";
            case AmouranthMessage::Type::SHUTDOWN: return "SHUTDOWN";
            case AmouranthMessage::Type::RENDER_FRAME: return "RENDER_FRAME";
            case AmouranthMessage::Type::BUILD_BLAS: return "BUILD_BLAS";
            case AmouranthMessage::Type::BUILD_TLAS: return "BUILD_TLAS";
            case AmouranthMessage::Type::CUSTOM: return "CUSTOM";
            default: return "UNKNOWN";
        }
    }

    mutable std::mutex mutex_;
    std::queue<AmouranthMessage> queue_;
};

// ──────────────────────────────────────────────────────────────────────────────
// NICK — GOLDEN YELLOW BOLD — OWNS THE REALM
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] inline VulkanRenderer& getRenderer() {
    if (!g_vulkanRenderer) {
        LOG_ERROR_CAT("Nick", "{}Renderer not initialized — Valhalla breach{}", NICK_COLOR, RESET);
        throw std::runtime_error("VulkanRenderer not ready");
    }
    return *g_vulkanRenderer;
}

inline void initRenderer(int w, int h) {
    if (g_vulkanRenderer) return;
    g_vulkanRenderer = std::make_unique<VulkanRenderer>(w, h, nullptr, std::vector<std::string>{}, false);
    LOG_SUCCESS_CAT("Nick", "{}Renderer initialized — GOLDEN DOMINANCE ENGAGED{}", NICK_COLOR, RESET);
}

inline void handleResize(int w, int h) {
    if (g_vulkanRenderer) {
        g_vulkanRenderer->handleResize(w, h);
        LOG_INFO_CAT("Nick", "{}Resize {}×{} → forwarded to Renderer{}", NICK_COLOR, w, h, RESET);
        Amouranth::get().post(AmouranthMessage(AmouranthMessage::Type::RECREATE_SWAPCHAIN, w, h));
    }
}

inline void renderFrame(const Camera& camera, float deltaTime) noexcept {
    if (g_vulkanRenderer) {
        g_vulkanRenderer->renderFrame(camera, deltaTime);
    }
}

inline void shutdown() noexcept {
    g_vulkanRenderer.reset();
    LOG_SUCCESS_CAT("Nick", "{}Renderer shutdown — GOLDEN HUSBAND RESTS{}", NICK_COLOR, RESET);
}

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL SWAPCHAIN MANAGEMENT — IMPLEMENTATION
// ──────────────────────────────────────────────────────────────────────────────
inline void createSwapchain(VkInstance inst, VkPhysicalDevice phys, VkDevice dev,
                            VkSurfaceKHR surf, uint32_t w, uint32_t h) {
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surf, &caps);

    uint32_t width = std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width);
    uint32_t height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);

    uint32_t fmtCnt = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &fmtCnt, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(fmtCnt);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &fmtCnt, fmts.data());

    VkSurfaceFormatKHR chosen = fmts[0];
    for (const auto& f : fmts)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = f;
            break;
        }

    uint32_t pmCnt = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &pmCnt, nullptr);
    std::vector<VkPresentModeKHR> pms(pmCnt);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &pmCnt, pms.data());

    VkPresentModeKHR present = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : pms)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            present = m;
            break;
        }

    uint32_t imgCnt = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imgCnt > caps.maxImageCount)
        imgCnt = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = surf;
    ci.minImageCount = imgCnt;
    ci.imageFormat = chosen.format;
    ci.imageColorSpace = chosen.colorSpace;
    ci.imageExtent = {width, height};
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = present;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR raw = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(dev, &ci, nullptr, &raw) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swapchain");

    g_swapchain = MakeHandle(raw, dev, vkDestroySwapchainKHR);
    g_swapchainFormat = chosen.format;
    g_swapchainExtent = {width, height};

    uint32_t cnt = 0;
    vkGetSwapchainImagesKHR(dev, *g_swapchain, &cnt, nullptr);
    g_swapchainImages.resize(cnt);
    vkGetSwapchainImagesKHR(dev, *g_swapchain, &cnt, g_swapchainImages.data());

    g_swapchainImageViews.clear();
    g_swapchainImageViews.reserve(g_swapchainImages.size());
    for (auto img : g_swapchainImages) {
        VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ci.image = img;
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = g_swapchainFormat;
        ci.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageView view = VK_NULL_HANDLE;
        if (vkCreateImageView(dev, &ci, nullptr, &view) != VK_SUCCESS)
            throw std::runtime_error("Failed to create image view");

        g_swapchainImageViews.emplace_back(MakeHandle(view, dev, vkDestroyImageView));
    }

    LOG_SUCCESS_CAT("Swapchain", "{}Swapchain created: {}×{} | {} images{}", NICK_COLOR, width, height, cnt, RESET);
}

inline void recreateSwapchain(uint32_t w, uint32_t h) noexcept {
    if (!g_swapchain) return;

    VkDevice dev = g_swapchain.device;
    vkDeviceWaitIdle(dev);

    for (auto& v : g_swapchainImageViews) v.reset();
    g_swapchainImageViews.clear();
    g_swapchainImages.clear();
    g_swapchain.reset();

    try {
        createSwapchain(VK_NULL_HANDLE, VK_NULL_HANDLE, dev, VK_NULL_HANDLE, w, h);
    } catch (...) {
        LOG_ERROR_CAT("Swapchain", "{}Failed to recreate swapchain{}", NICK_COLOR, RESET);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL LAS MANAGEMENT — LIGHT WARRIORS EDITION
// ──────────────────────────────────────────────────────────────────────────────
void buildBLAS(uint64_t vertexBuf, uint64_t indexBuf, uint32_t vertexCount, uint32_t indexCount) noexcept {
    auto& c = ctx();
    if (!c.vkDevice()) return;

    LIGHT_WARRIORS_LAS::get().buildBLAS(
        c.commandPool(),
        c.graphicsQueue(),
        vertexBuf, indexBuf, vertexCount, indexCount
    );
}

void buildTLAS(const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances) noexcept {
    auto& c = ctx();
    if (!c.vkDevice()) return;

    LIGHT_WARRIORS_LAS::get().buildTLAS(
        c.commandPool(),
        c.graphicsQueue(),
        std::span(instances)
    );
}

// ──────────────────────────────────────────────────────────────────────────────
// UltraLowLevelBufferTracker — FULL IMPLEMENTATION
// ──────────────────────────────────────────────────────────────────────────────
static inline uint32_t findMemoryType(VkPhysicalDevice physDev, uint32_t typeFilter, VkMemoryPropertyFlags props) noexcept {
    if (physDev == VK_NULL_HANDLE) return ~0u;
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    LOG_ERROR_CAT("Buffer", "No suitable memory type found for props 0x%X", static_cast<uint32_t>(props));
    return ~0u;
}

inline UltraLowLevelBufferTracker& UltraLowLevelBufferTracker::get() noexcept {
    static UltraLowLevelBufferTracker instance;
    return instance;
}

inline void UltraLowLevelBufferTracker::init(VkDevice dev, VkPhysicalDevice phys) noexcept {
    if (device_ != VK_NULL_HANDLE) return;
    device_ = dev;
    physDev_ = phys;
    LOG_SUCCESS_CAT("Buffer", "{}UltraLowLevelBufferTracker initialized{}", NICK_COLOR, RESET);
}

inline VkDevice UltraLowLevelBufferTracker::device() const noexcept { return device_; }
inline VkPhysicalDevice UltraLowLevelBufferTracker::physicalDevice() const noexcept { return physDev_; }

inline uint64_t UltraLowLevelBufferTracker::create(VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags props,
                    std::string_view tag = "UnnamedBuffer") noexcept {
    if (size == 0 || device_ == VK_NULL_HANDLE || size > SIZE_8GB) return 0;

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buf = VK_NULL_HANDLE;
    if (vkCreateBuffer(device_, &bci, nullptr, &buf) != VK_SUCCESS) return 0;

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device_, buf, &req);

    VkDeviceMemory mem = [&]() -> VkDeviceMemory {
        uint32_t idx = findMemoryType(physDev_, req.memoryTypeBits, props);
        if (idx == ~0u) return VK_NULL_HANDLE;
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, req.size, idx};
        VkDeviceMemory m{};
        if (vkAllocateMemory(device_, &ai, nullptr, &m) != VK_SUCCESS) return VK_NULL_HANDLE;
        char buf[256]{}; std::snprintf(buf, sizeof(buf), "Allocated %zu bytes [%s]", req.size, tag.data());
        LOG_SUCCESS_CAT("Buffer", "%s", buf);
        logAndTrackDestruction("VkDeviceMemory", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(m)), __LINE__, req.size);
        return m;
    }();
    if (mem == VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buf, nullptr);
        return 0;
    }

    if (vkBindBufferMemory(device_, buf, mem, 0) != VK_SUCCESS) {
        INLINE_FREE(device_, mem, req.size, std::string(tag).c_str());
        vkDestroyBuffer(device_, buf, nullptr);
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t raw;
    do {
        if (++counter_ == 0) counter_ = 1;
        raw = counter_;
    } while (map_.find(raw) != map_.end());
    map_[raw] = {buf, mem, size, usage, std::string(tag)};

    logAndTrackDestruction("VkBuffer", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(buf)), __LINE__, size);
    return obfuscate(raw);
}

inline void UltraLowLevelBufferTracker::destroy(uint64_t obf_id) noexcept {
    if (obf_id == 0) return;
    uint64_t raw = deobfuscate(obf_id);
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(raw);
    if (it == map_.end()) return;

    const auto& d = it->second;
    logAndTrackDestruction("VkBuffer", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(d.buffer)), __LINE__, 0);
    INLINE_FREE(device_, d.memory, d.size, d.tag.c_str());
    vkDestroyBuffer(device_, d.buffer, nullptr);
    map_.erase(it);
}

inline BufferData* UltraLowLevelBufferTracker::getData(uint64_t obf_id) noexcept {
    if (obf_id == 0) return nullptr;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(deobfuscate(obf_id));
    return (it != map_.end()) ? &it->second : nullptr;
}

inline const BufferData* UltraLowLevelBufferTracker::getData(uint64_t obf_id) const noexcept {
    if (obf_id == 0) return nullptr;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(deobfuscate(obf_id));
    return (it != map_.end()) ? &it->second : nullptr;
}

inline void UltraLowLevelBufferTracker::purge_all() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [_, d] : map_) {
        logAndTrackDestruction("VkBuffer", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(d.buffer)), __LINE__, 0);
        INLINE_FREE(device_, d.memory, d.size, ("PURGE_" + d.tag).c_str());
        vkDestroyBuffer(device_, d.buffer, nullptr);
    }
    map_.clear();
    counter_ = 0;
    scratch512M_ = scratch1G_ = scratch2G_ = 0;
}

inline uint64_t UltraLowLevelBufferTracker::obfuscate(uint64_t raw) const noexcept { return raw ^ kStone1; }
inline uint64_t UltraLowLevelBufferTracker::deobfuscate(uint64_t obf) const noexcept { return obf ^ kStone1; }


// ──────────────────────────────────────────────────────────────────────────────
// Global Cleanup — FINAL
// ──────────────────────────────────────────────────────────────────────────────
inline void cleanupAll() noexcept {
    UltraLowLevelBufferTracker::get().purge_all();
    g_blas.reset();
    g_tlas.reset();
    if (g_instanceBufferId) BUFFER_DESTROY(g_instanceBufferId);
    shutdown();
    std::thread([] { SDL_Quit(); }).detach();
    LOG_SUCCESS_CAT("Houston", "{}GLOBAL CLEANUP — VALHALLA SEALED{}", NICK_COLOR, RESET);
}

static const auto _houston_init = [] { atexit(cleanupAll); return 0; }();

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================