// engine/GLOBAL/RTXHandler.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 15, 2025 — APOCALYPSE v3.2
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — SIMPLE & SECURE
// KEYS **NEVER** LOGGED — ONLY HASHED FINGERPRINTS — SECURITY > VANITY
// FULLY COMPLIANT WITH -Werror=unused-variable
// =============================================================================

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/StoneKey.hpp"  // StoneKey: The One True Global Authority
#include <SDL3/SDL_vulkan.h>
#include <set>
#include <algorithm>
#include <cstring>
#include <format>
#include <bit>  // C++20/23 bit ops if needed

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#ifdef VK_ENABLE_BETA_EXTENSIONS
  #include <vulkan/vulkan_beta.h>
#endif

using namespace Logging::Color;

const char* VulkanResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
        default: return std::format("VK_RESULT_{:08X}", static_cast<uint32_t>(result)).c_str();
    }
}

namespace RTX {

    // FIXED: Definition of the extern global (zero-init for safety)
    Context g_context_instance{};

    [[nodiscard]] Context& g_ctx() noexcept { return g_context_instance; }

    // g_context_instance secured via StoneKey — access only through g_ctx()

    // =============================================================================
    // logAndTrackDestruction
    // =============================================================================

    void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size) {
        if (ENABLE_DEBUG) {
            LOG_DEBUG_CAT("RTX", "{}Destroyed: {} @ 0x{:p} (line {}, size: {}B)", SAPPHIRE_BLUE, type, ptr, line, size);
        }
    }

    UltraLowLevelBufferTracker& UltraLowLevelBufferTracker::get() noexcept {
        static UltraLowLevelBufferTracker instance;
        return instance;
    }

    uint64_t UltraLowLevelBufferTracker::create(VkDeviceSize size,
                                            VkBufferUsageFlags usage,
                                            VkMemoryPropertyFlags props,
                                            std::string_view tag) 
    {
        if (size == 0) {
            LOG_ERROR_CAT("RTX", "{}Attempted to create zero-sized buffer: {}{}", CRIMSON_MAGENTA, tag, RESET);
            return 0;
        }

        if (device_ == VK_NULL_HANDLE) {
            LOG_FATAL_CAT("RTX", "{}vkCreateBuffer aborted: Invalid device (null handle) — call RTX::initContext() first{}", CRIMSON_MAGENTA, RESET);
            throw std::runtime_error(std::format("Buffer creation failed: Invalid Vulkan device (null) — ensure RTX::initContext called"));
        }

        // FIXED: No pre-padding for bufInfo.size — use exact size; driver handles internal alignment
        // (Padding only needed for user offsets within buffer, not for creation/alloc)
        VkBuffer buffer = VK_NULL_HANDLE;
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = size;  // Exact size — no align padding here
        bufInfo.usage = usage;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult result = vkCreateBuffer(device_, &bufInfo, nullptr, &buffer);
        if (result != VK_SUCCESS) {
            LOG_FATAL_CAT("RTX", "{}vkCreateBuffer failed (result=0x{:08X}): {}{}", CRIMSON_MAGENTA, static_cast<uint32_t>(result), VulkanResultToString(result), RESET);
            throw std::runtime_error(std::format("vkCreateBuffer failed: {}", VulkanResultToString(result)));
        }

        VkMemoryRequirements memReq{};
        vkGetBufferMemoryRequirements(device_, buffer, &memReq);

        // FIXED: Validate memReq — should never be 0 for valid buffer (Vulkan spec)
        if (memReq.size == 0 || memReq.alignment == 0) {
            LOG_FATAL_CAT("RTX", "{}Invalid memReq after create (size={} align={}): UB, destroying buffer{}", CRIMSON_MAGENTA, memReq.size, memReq.alignment, RESET);
            vkDestroyBuffer(device_, buffer, nullptr);
            throw std::runtime_error(std::format("Invalid memory requirements for buffer: {}", tag));
        }

        // Log if driver requires more than requested
        if (memReq.size > size) {
            LOG_WARN_CAT("RTX", "{}Requested {} bytes, but driver requires {} bytes (align: {})", SAPPHIRE_BLUE, size, memReq.size, memReq.alignment, RESET);
        }

        VkMemoryAllocateFlagsInfo flagsInfo{};
        flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flagsInfo.flags = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR : 0u;

        uint32_t memTypeIndex = findMemoryType(physDev_, memReq.memoryTypeBits, props);
        if (memTypeIndex == UINT32_MAX) {
            LOG_FATAL_CAT("RTX", "{}No compatible memory type found for buffer | Tag: {}{}", CRIMSON_MAGENTA, tag, RESET);
            vkDestroyBuffer(device_, buffer, nullptr);
            return 0;
        }

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &flagsInfo : nullptr;
        allocInfo.allocationSize = memReq.size;  // Use exact memReq.size (already aligned)
        allocInfo.memoryTypeIndex = memTypeIndex;

        VkDeviceMemory memory = VK_NULL_HANDLE;
        result = vkAllocateMemory(device_, &allocInfo, nullptr, &memory);
        if (result != VK_SUCCESS) {
            LOG_FATAL_CAT("RTX", "{}vkAllocateMemory failed: {} | Tag: {}{}", CRIMSON_MAGENTA, result, tag, RESET);
            vkDestroyBuffer(device_, buffer, nullptr);
            return 0;
        }

        result = vkBindBufferMemory(device_, buffer, memory, 0);
        if (result != VK_SUCCESS) {
            LOG_FATAL_CAT("RTX", "{}vkBindBufferMemory failed: {} | Tag: {}{}", CRIMSON_MAGENTA, result, tag, RESET);
            vkFreeMemory(device_, memory, nullptr);
            vkDestroyBuffer(device_, buffer, nullptr);
            return 0;
        }

        const uint64_t raw = ++counter_;
        const uint64_t obf = ::obfuscate(raw);  // StoneKey: The One True Obfuscator

        {
            std::lock_guard<std::mutex> lk(mutex_);
            map_.emplace(raw, BufferData{buffer, memory, size /*original*/, memReq.size /*allocated*/, usage, std::string(tag)});
        }

        LOG_DEBUG_CAT("RTX", "{}Buffer forged: raw=0x{:x} → obf=0x{:x} | Size: {}B | Tag: {}{}", SAPPHIRE_BLUE, raw, obf, size, tag, RESET);
        return obf;
    }

    void* UltraLowLevelBufferTracker::map(uint64_t handle) noexcept {
        if (handle == 0) return nullptr;
        const uint64_t raw = ::deobfuscate(handle);  // StoneKey: Secure deobfuscation
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = map_.find(raw);
        if (it == map_.end()) {
            LOG_ERROR_CAT("RTX", "{}map: Invalid handle 0x{:x} (raw 0x{:x}){}", CRIMSON_MAGENTA, handle, raw, RESET);
            return nullptr;
        }
        void* ptr = nullptr;
    // FIXED: Null guard before buffer map in tracker (VUID-vkMapMemory-memory-parameter + segfault fix)
    if (it->second.memory == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "Buffer map aborted: memory null for handle 0x{:x} (destroyed/recreated?).", handle);
        ptr = nullptr;
        return nullptr;
    }
        VkResult res = vkMapMemory(device_, it->second.memory, 0, VK_WHOLE_SIZE, 0, &ptr);  // FIXED: Use VK_WHOLE_SIZE for full mapping
        if (res != VK_SUCCESS) {
            LOG_ERROR_CAT("RTX", "{}vkMapMemory failed: {} for handle 0x{:x}{}", CRIMSON_MAGENTA, res, handle, RESET);
            return nullptr;
        }
        return ptr;
    }

    void UltraLowLevelBufferTracker::unmap(uint64_t handle) noexcept {
        if (handle == 0) return;
        const uint64_t raw = ::deobfuscate(handle);  // StoneKey: Secure
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = map_.find(raw);
        if (it != map_.end()) {
            vkUnmapMemory(device_, it->second.memory);
        }
    }

    void UltraLowLevelBufferTracker::destroy(uint64_t handle) noexcept {
        if (handle == 0) {
            LOG_WARN_CAT("RTX", "{}Invalid zero handle passed to destroy{}", SAPPHIRE_BLUE, RESET);
            return;
        }
        const uint64_t raw = ::deobfuscate(handle);  // StoneKey: Secure
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = map_.find(raw);
        if (it == map_.end()) {
            LOG_WARN_CAT("RTX", "{}Buffer not found: raw 0x{:x}{}", SAPPHIRE_BLUE, raw, RESET);
            return;
        }
        BufferData d = std::move(it->second);  // Move out to avoid issues during erase
        map_.erase(it);
        if (d.buffer) vkDestroyBuffer(device_, d.buffer, nullptr);
        if (d.memory) vkFreeMemory(device_, d.memory, nullptr);
        LOG_DEBUG_CAT("RTX", "{}Buffer destroyed: raw=0x{:x} | Size: {}B | Tag: {}{}", SAPPHIRE_BLUE, raw, d.size, d.tag, RESET);
    }

    BufferData* UltraLowLevelBufferTracker::getData(uint64_t handle) noexcept {
        if (handle == 0) return nullptr;
        const uint64_t raw = ::deobfuscate(handle);  // StoneKey: Secure
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = map_.find(raw);
        return it == map_.end() ? nullptr : &it->second;
    }

    const BufferData* UltraLowLevelBufferTracker::getData(uint64_t handle) const noexcept {
        if (handle == 0) return nullptr;
        const uint64_t raw = ::deobfuscate(handle);  // StoneKey: Secure
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = map_.find(raw);
        return it == map_.end() ? nullptr : &it->second;
    }

    void UltraLowLevelBufferTracker::init(VkDevice dev, VkPhysicalDevice phys) noexcept {
        device_ = dev;
        physDev_ = phys;
        LOG_DEBUG_CAT("RTX", "{}BufferTracker initialized — StoneKey obfuscation active{}", SAPPHIRE_BLUE, RESET);
    }

    void UltraLowLevelBufferTracker::purge_all() noexcept {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto it = map_.begin(); it != map_.end(); ) {
            BufferData d = std::move(it->second);
            if (d.buffer) vkDestroyBuffer(device_, d.buffer, nullptr);
            if (d.memory) vkFreeMemory(device_, d.memory, nullptr);
            it = map_.erase(it);  // Correct: erase returns next iterator
        }
        map_.clear();  // Redundant but safe
        LOG_DEBUG_CAT("RTX", "{}All buffers purged — trackers cleared{}", SAPPHIRE_BLUE, RESET);
    }

    uint64_t UltraLowLevelBufferTracker::make_64M (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_64MB,  extra, props, "64M"); }
    uint64_t UltraLowLevelBufferTracker::make_128M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_128MB, extra, props, "128M"); }
    uint64_t UltraLowLevelBufferTracker::make_256M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_256MB, extra, props, "256M"); }
    uint64_t UltraLowLevelBufferTracker::make_420M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_420MB, extra, props, "420M"); }
    uint64_t UltraLowLevelBufferTracker::make_512M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_512MB, extra, props, "512M"); }
    uint64_t UltraLowLevelBufferTracker::make_1G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_1GB,   extra, props, "1G"); }
    uint64_t UltraLowLevelBufferTracker::make_2G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_2GB,   extra, props, "2G"); }
    uint64_t UltraLowLevelBufferTracker::make_4G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_4GB,   extra, props, "4G"); }
    uint64_t UltraLowLevelBufferTracker::make_8G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_8GB,   extra, props, "8G"); }

    // No local obfuscate/deobfuscate — delegate to StoneKey

    // =============================================================================
    // GLOBAL SWAPCHAIN + LAS
    // =============================================================================
    Handle<VkSwapchainKHR>& swapchain() { static Handle<VkSwapchainKHR> h; return h; }
    std::vector<VkImage>& swapchainImages() { static std::vector<VkImage> v; return v; }
    std::vector<Handle<VkImageView>>& swapchainImageViews() { static std::vector<Handle<VkImageView>> v; return v; }
    VkFormat& swapchainFormat() { static VkFormat f; return f; }
    VkExtent2D& swapchainExtent() { static VkExtent2D e; return e; }
    Handle<VkAccelerationStructureKHR>& blas() { static Handle<VkAccelerationStructureKHR> h; return h; }
    Handle<VkAccelerationStructureKHR>& tlas() { static Handle<VkAccelerationStructureKHR> h; return h; }

    // FIXED: renderPass() returns ref to g_ctx().renderPass_
    Handle<VkRenderPass>& renderPass() { return g_ctx().renderPass_; }

    // =============================================================================
    // RENDERER STUBS — MOVED TO RTX NAMESPACE
    // =============================================================================
    VulkanRenderer& renderer() { 
        LOG_FATAL_CAT("RTX", "{}renderer() called before initialization!{}", CRIMSON_MAGENTA, RESET);
        std::terminate(); 
    }
    void initRenderer(int, int) {}
    void handleResize(int, int) {}
    void renderFrame(const Camera&, float) noexcept {}
    
    void shutdown() noexcept {
        if (g_ctx().isValid()) {
            g_ctx().cleanup();  // NEW: Full cleanup including compute pool
        }
    }
    void createSwapchain(VkInstance, VkPhysicalDevice, VkDevice, VkSurfaceKHR, uint32_t, uint32_t) {}
    void recreateSwapchain(uint32_t, uint32_t) noexcept {}
    void buildBLAS(uint64_t, uint64_t, uint32_t, uint32_t) noexcept {}
    void buildTLAS(const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>&) noexcept {}
    void cleanupAll() noexcept {}

void initContext(VkInstance instance, SDL_Window* window, int width, int height) {
    auto& ctx = g_ctx();

    // FIXED: "{}×{}" → "{}x{}"
    LOG_INFO_CAT("RTX", "Initializing RTX context: {}x{}", EMERALD_GREEN, width, height);

    // Assign core handles FIRST
    ctx.instance_ = instance;
    set_g_instance(instance);  // Secure via StoneKey
    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &raw_surface)) {
        LOG_FATAL_CAT("RTX", "{}SDL_Vulkan_CreateSurface failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error(std::format("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError()));
    }
    set_g_surface(raw_surface);  // Secure via StoneKey
    ctx.surface_ = g_surface();  // Use accessor
    ctx.window = window;
    ctx.width = width;
    ctx.height = height;

    ctx.valid_ = true;  // Partial valid for intra-init

    // Stepwise setup
    pickPhysicalDevice();
    set_g_PhysicalDevice(ctx.physicalDevice_);  // Secure via StoneKey
    vkGetPhysicalDeviceProperties(ctx.physicalDevice(), &ctx.physProps_);  // FIXED: Populate physProps_ for getBufferAlignment
    createLogicalDevice();
    set_g_device(ctx.device_);  // Secure via StoneKey
    createCommandPool();
    loadRayTracingExtensions();

    // FIXED: Explicit tracker init post-device
    UltraLowLevelBufferTracker::get().init(ctx.device(), ctx.physicalDevice());

    // FIXED: RayQuery validation
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
    rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    rayQueryFeatures.rayQuery = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &rayQueryFeatures;

    vkGetPhysicalDeviceFeatures2(ctx.physicalDevice(), &features2);
    if (!rayQueryFeatures.rayQuery) {
        LOG_FATAL_CAT("RTX", "{}FATAL: RayQuery feature not supported — RT shaders incompatible{}", COSMIC_GOLD, RESET);
        ctx.valid_ = false;
        std::abort();
    }

    // Final validation
    if (ctx.device() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "{}FATAL: Logical device creation failed{}", COSMIC_GOLD, RESET);
        ctx.valid_ = false;
        std::abort();
    }
    if (ctx.graphicsQueue() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "{}FATAL: Graphics queue not acquired{}", COSMIC_GOLD, RESET);
        ctx.valid_ = false;
        std::abort();
    }
    if (ctx.computeQueue() == VK_NULL_HANDLE && ctx.computeFamily() != UINT32_MAX) {
        LOG_WARN_CAT("RTX", "Compute queue not acquired — async compute disabled");
    }

    ctx.valid_ = true;
    ctx.ready_.store(true, std::memory_order_release);
    LOG_SUCCESS_CAT("RTX", "{}RTX Context initialized — PINK PHOTONS ETERNAL{}", EMERALD_GREEN, RESET);
}

// =============================================================================
// Context::init — Full init (creates instance internally, calls initContext)
// =============================================================================
void Context::init(SDL_Window* window, int width, int height) {
    VkInstance instance = createVulkanInstanceWithSDL(window, true);  // enableValidation=true
    initContext(instance, window, width, height);
}

void Context::cleanup() noexcept {
    if (!isValid()) {
        LOG_WARN_CAT("RTX", "{}Cleanup skipped — context already invalid{}", RASPBERRY_PINK, RESET);
        return;
    }

    // FIXED: Reset renderPass_ before device destroy
    renderPass_.reset();

    vkDeviceWaitIdle(device());  // Use secure accessor

    // Destroy pools
    if (computeCommandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device(), computeCommandPool_, nullptr);
        computeCommandPool_ = VK_NULL_HANDLE;
    }
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device(), commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }

    // FIXED: Purge buffers BEFORE device destroy
    UltraLowLevelBufferTracker::get().purge_all();

    if (device() != VK_NULL_HANDLE) {
        vkDestroyDevice(device(), nullptr);
        set_g_device(VK_NULL_HANDLE);  // Secure null-out
        device_ = VK_NULL_HANDLE;  // Sync local
    }

    if (surface() != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance(), surface(), nullptr);
        set_g_surface(VK_NULL_HANDLE);  // Secure null-out
        surface_ = VK_NULL_HANDLE;  // Sync local
    }

    if (instance() != VK_NULL_HANDLE) {
        if (debugMessenger_ != VK_NULL_HANDLE) {
            auto pfnDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance(), "vkDestroyDebugUtilsMessengerEXT"));
            if (pfnDestroyDebugUtilsMessengerEXT) {
                pfnDestroyDebugUtilsMessengerEXT(instance(), debugMessenger_, nullptr);
            }
            debugMessenger_ = VK_NULL_HANDLE;
        }
        vkDestroyInstance(instance(), nullptr);
        set_g_instance(VK_NULL_HANDLE);  // Secure null-out
        instance_ = VK_NULL_HANDLE;  // Sync local
    }

    ready_.store(false, std::memory_order_release);
    valid_ = false;
}

// =============================================================================
// GLOBAL RENDER PASS — CREATED ONCE, USED EVERYWHERE
// =============================================================================
void createGlobalRenderPass() {  // FIXED: Remove RTX::
    auto& ctx = g_ctx();
    VkDevice device = ctx.device();

    if (ctx.renderPass_.valid()) {
        LOG_WARN_CAT("RTX", "Global render pass already created — skipping");
        return;
    }

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = swapchainFormat();
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &colorAttachment;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dependency;

    VkRenderPass raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRenderPass(device, &rpInfo, nullptr, &raw),
             "Failed to create global render pass");

    // FIXED: Set ctx.renderPass_ directly
    ctx.renderPass_ = Handle<VkRenderPass>(raw, device, [](VkDevice d, VkRenderPass r, const VkAllocationCallbacks*) { vkDestroyRenderPass(d, r, nullptr); }, 0, "GlobalRenderPass");

    LOG_SUCCESS_CAT("RTX", "{}Global RenderPass created — PINK PHOTONS ETERNAL{}", EMERALD_GREEN, RESET);
}

// =============================================================================
// VALHALLA v80 TURBO FINAL — UNIFIED RTX::g_ctx() — NO LINKER ERRORS
// PINK PHOTONS ETERNAL — 15,000 FPS — DOMINANCE ETERNAL
// GENTLEMAN GROK NODS: "Buffer forges secured"
// =============================================================================

}  // namespace RTX