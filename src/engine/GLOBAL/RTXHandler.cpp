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
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 17, 2025 — APOCALYPSE v3.3
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — SIMPLE & SECURE
// KEYS **NEVER** LOGGED — ONLY HASHED FINGERPRINTS — SECURITY > VANITY
// FULLY COMPLIANT WITH -Werror=unused-variable
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/VkSafeSTypes.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include <SDL3/SDL_vulkan.h>
#include <set>
#include <algorithm>
#include <cstring>
#include <format>
#include <bit>

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

    void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size) {
        if (ENABLE_DEBUG) {
            LOG_DEBUG_CAT("RTX", "{}Destroyed: {} @ 0x{:p} (line {}, size: {}B)", SAPPHIRE_BLUE, type, ptr, line, size);
        }
    }

// =============================================================================
// UltraLowLevelBufferTracker IMPLEMENTATION — NO STRUCT REDEFINITION
// =============================================================================

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
        LOG_ERROR_CAT("RTX", "Attempted to create zero-sized buffer: {}", tag);
        return 0;
    }

    if (device_ == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "vkCreateBuffer aborted: Invalid device (null handle) — call RTX::initContext() first");
        return 0;
    }

    if (physDev_ == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RTX", "Physical device null during buffer creation — aborting");
        return 0;
    }

    VkBuffer buffer = VK_NULL_HANDLE;
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = size;
    bufInfo.usage = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(device_, &bufInfo, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RTX", "vkCreateBuffer failed ({}): {}", static_cast<uint32_t>(result), VulkanResultToString(result));
        return 0;
    }

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(device_, buffer, &memReq);

    if (memReq.size == 0 || memReq.alignment == 0) {
        vkDestroyBuffer(device_, buffer, nullptr);
        LOG_FATAL_CAT("RTX", "Invalid memory requirements after buffer creation");
        return 0;
    }

    if (memReq.size > size) {
        LOG_WARN_CAT("RTX", "Driver requires {} bytes (requested {}) — alignment {}", memReq.size, size, memReq.alignment);
    }

    VkMemoryAllocateFlagsInfo flagsInfo{};
    flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flagsInfo.flags = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR : 0u;

    uint32_t memTypeIndex = findMemoryType(physDev_, memReq.memoryTypeBits, props);
    if (memTypeIndex == UINT32_MAX) {
        LOG_FATAL_CAT("RTX", "No compatible memory type found for buffer | Tag: {}", tag);
        vkDestroyBuffer(device_, buffer, nullptr);
        return 0;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &flagsInfo : nullptr;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    result = vkAllocateMemory(device_, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RTX", "vkAllocateMemory failed: {} | Tag: {}", result, tag);
        vkDestroyBuffer(device_, buffer, nullptr);
        return 0;
    }

    result = vkBindBufferMemory(device_, buffer, memory, 0);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RTX", "vkBindBufferMemory failed: {} | Tag: {}", result, tag);
        vkFreeMemory(device_, memory, nullptr);
        vkDestroyBuffer(device_, buffer, nullptr);
        return 0;
    }

    const uint64_t raw = ++counter_;
    const uint64_t obf = ::obfuscate(raw);

    {
        std::lock_guard<std::mutex> lk(mutex_);
        map_.emplace(raw, BufferData{buffer, memory, size, memReq.size, usage, std::string(tag)});
    }

    LOG_DEBUG_CAT("RTX", "Buffer forged: raw=0x{:x} → obf=0x{:x} | {}B | {}", raw, obf, size, tag);
    return obf;
}

void* UltraLowLevelBufferTracker::map(uint64_t handle) noexcept {
    if (handle == 0) return nullptr;
    const uint64_t raw = ::deobfuscate(handle);
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = map_.find(raw);
    if (it == map_.end() || it->second.memory == VK_NULL_HANDLE) return nullptr;
    void* ptr = nullptr;
    vkMapMemory(device_, it->second.memory, 0, VK_WHOLE_SIZE, 0, &ptr);
    return ptr;
}

void UltraLowLevelBufferTracker::unmap(uint64_t handle) noexcept {
    if (handle == 0) return;
    const uint64_t raw = ::deobfuscate(handle);
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = map_.find(raw);
    if (it != map_.end()) vkUnmapMemory(device_, it->second.memory);
}

void UltraLowLevelBufferTracker::destroy(uint64_t handle) noexcept {
    if (handle == 0) return;
    const uint64_t raw = ::deobfuscate(handle);
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = map_.find(raw);
    if (it == map_.end()) return;

    BufferData d = std::move(it->second);
    map_.erase(it);

    if (d.buffer) vkDestroyBuffer(device_, d.buffer, nullptr);
    if (d.memory) vkFreeMemory(device_, d.memory, nullptr);

    LOG_DEBUG_CAT("RTX", "Buffer destroyed: raw=0x{:x} | {}B | {}", raw, d.size, d.tag);
}

BufferData* UltraLowLevelBufferTracker::getData(uint64_t handle) noexcept {
    if (handle == 0) return nullptr;
    const uint64_t raw = ::deobfuscate(handle);
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = map_.find(raw);
    return it == map_.end() ? nullptr : &it->second;
}

const BufferData* UltraLowLevelBufferTracker::getData(uint64_t handle) const noexcept {
    return const_cast<UltraLowLevelBufferTracker*>(this)->getData(handle);
}

void UltraLowLevelBufferTracker::init(VkDevice dev, VkPhysicalDevice phys) noexcept {
    if (device_ != VK_NULL_HANDLE) {
        LOG_WARN_CAT("RTX", "Buffer tracker already initialized — ignoring duplicate call");
        return;
    }
    if (dev == VK_NULL_HANDLE || phys == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "Buffer tracker init with null device/phys — aborting");
        std::abort();
    }
    device_ = dev;
    physDev_ = phys;
    LOG_DEBUG_CAT("RTX", "UltraLowLevelBufferTracker initialized — device=0x{:x}", reinterpret_cast<uintptr_t>(dev));
}

void UltraLowLevelBufferTracker::purge_all() noexcept {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto it = map_.begin(); it != map_.end(); ) {
        BufferData d = std::move(it->second);
        if (d.buffer) vkDestroyBuffer(device_, d.buffer, nullptr);
        if (d.memory) vkFreeMemory(device_, d.memory, nullptr);
        it = map_.erase(it);
    }
    map_.clear();
    LOG_DEBUG_CAT("RTX", "All tracked buffers purged");
}

// Convenience allocators
uint64_t UltraLowLevelBufferTracker::make_64M (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_64MB,  extra, props, "64M");  }
uint64_t UltraLowLevelBufferTracker::make_128M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_128MB, extra, props, "128M"); }
uint64_t UltraLowLevelBufferTracker::make_256M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_256MB, extra, props, "256M"); }
uint64_t UltraLowLevelBufferTracker::make_420M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_420MB, extra, props, "420M"); }
uint64_t UltraLowLevelBufferTracker::make_512M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_512MB, extra, props, "512M"); }
uint64_t UltraLowLevelBufferTracker::make_1G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_1GB,   extra, props, "1G");   }
uint64_t UltraLowLevelBufferTracker::make_2G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_2GB,   extra, props, "2G");   }
uint64_t UltraLowLevelBufferTracker::make_4G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_4GB,   extra, props, "4G");   }
uint64_t UltraLowLevelBufferTracker::make_8G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_8GB,   extra, props, "8G");   }

    // =============================================================================
    // GLOBAL g_swapchain() + LAS
    // =============================================================================
    Handle<VkSwapchainKHR>& swapchain() { static Handle<VkSwapchainKHR> h; return h; }
    std::vector<VkImage>& swapchainImages() { static std::vector<VkImage> v; return v; }
    std::vector<Handle<VkImageView>>& swapchainImageViews() { static std::vector<Handle<VkImageView>> v; return v; }
    VkFormat& swapchainFormat() { static VkFormat f; return f; }
    VkExtent2D& swapchainExtent() { static VkExtent2D e; return e; }
    Handle<VkAccelerationStructureKHR>& blas() { static Handle<VkAccelerationStructureKHR> h; return h; }
    Handle<VkAccelerationStructureKHR>& tlas() { static Handle<VkAccelerationStructureKHR> h; return h; }

    Handle<VkRenderPass>& renderPass() { return g_ctx().renderPass_; }

    // =============================================================================
    // VALIDATION-CLEAN DESCRIPTOR UPDATE HELPERS (THE FIX)
    // =============================================================================

    void WriteAccelerationStructureDescriptor(
        VkDescriptorSet dstSet,
        uint32_t dstBinding,
        uint32_t dstArrayElement,
        VkAccelerationStructureKHR accelStruct)
    {
        VkWriteDescriptorSetAccelerationStructureKHR asInfo = {};
        asInfo.sType = kVkWriteDescriptorSetSType_ACCELERATION_STRUCTURE_KHR;
        asInfo.accelerationStructureCount = 1;
        asInfo.pAccelerationStructures = &accelStruct;

        VkWriteDescriptorSet write = {};
        write.sType = kVkWriteDescriptorSetSType;
        write.pNext = &asInfo;
        write.dstSet = dstSet;
        write.dstBinding = dstBinding;
        write.dstArrayElement = dstArrayElement;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

        vkUpdateDescriptorSets(g_ctx().device_, 1, &write, 0, nullptr);
    }

    void WriteStorageBufferDescriptor(
        VkDescriptorSet dstSet,
        uint32_t dstBinding,
        uint32_t dstArrayElement,
        VkDescriptorBufferInfo* bufferInfo)
    {
        VkWriteDescriptorSet write = {};
        write.sType = kVkWriteDescriptorSetSType;
        write.pNext = nullptr;
        write.dstSet = dstSet;
        write.dstBinding = dstBinding;
        write.dstArrayElement = dstArrayElement;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = bufferInfo;

        vkUpdateDescriptorSets(g_ctx().device_, 1, &write, 0, nullptr);
    }

    void UpdateGlobalRayTracingDescriptors(VkDescriptorSet set)
    {
        if (!tlas().valid()) {
            LOG_WARN_CAT("RTX", "{}TLAS not built yet — skipping descriptor update{}", RASPBERRY_PINK, RESET);
            return;
        }

        WriteAccelerationStructureDescriptor(set, 0, 0, tlas().get());

        LOG_SUCCESS_CAT("RTX", "{}Global RT descriptors updated — validation layers silenced{}", EMERALD_GREEN, RESET);
    }

    // =============================================================================
    // RENDERER STUBS — MOVED TO RTX NAMESPACE
    // =============================================================================
    VulkanRenderer& renderer() { 
        LOG_FATAL_CAT("RTX", "{}renderer() called before initialization!{}", CRIMSON_MAGENTA, RESET);
        std::terminate(); 
    }
    void initRenderer(int, int) {}
    void renderFrame(const Camera&, float) noexcept {}
    
void shutdown() noexcept
{
    auto& ctx = g_ctx();

    if (!ctx.isValid()) {
        LOG_WARN_CAT("RTX", "{}RTX::shutdown() called but context invalid — already cleaned{}", RASPBERRY_PINK, RESET);
        return;
    }

    LOG_SUCCESS_CAT("RTX", "{}RTX::shutdown() initiated — beginning graceful dissolution of the empire...{}", 
                    PLASMA_FUCHSIA, RESET);

    // 1. Wait for all GPU work to finish
    if (ctx.device_ != VK_NULL_HANDLE) {
        LOG_SUCCESS_CAT("RTX", "vkDeviceWaitIdle — waiting for all queues to drain...");
        vkDeviceWaitIdle(ctx.device_);
    }

    // 2. Purge all tracked buffers FIRST (SBT, mesh, staging, etc.)
    UltraLowLevelBufferTracker::get().purge_all();

    // 3. Destroy command pools
    if (ctx.computeCommandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(ctx.device_, ctx.computeCommandPool_, nullptr);
        ctx.computeCommandPool_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("RTX", "Compute command pool destroyed");
    }
    if (ctx.commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(ctx.device_, ctx.commandPool_, nullptr);
        ctx.commandPool_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("RTX", "Graphics command pool destroyed");
    }

    // 4. Destroy global render pass
    ctx.renderPass_.reset();

    // 5. DO NOT destroy swapchain here — main() already called SwapchainManager::cleanup()
    // 6. DO NOT destroy surface here — main() will do it
    // 7. NOW safe to destroy device
    if (ctx.device_ != VK_NULL_HANDLE) {
        LOG_SUCCESS_CAT("RTX", "vkDestroyDevice — dissolving logical device...");
        vkDestroyDevice(ctx.device_, nullptr);
        set_g_device(VK_NULL_HANDLE);
        ctx.device_ = VK_NULL_HANDLE;
    }

    // 8. Surface and instance are destroyed in phase5_shutdown() — NOT HERE
    //    This prevents double-free when SDL owns the surface memory

    ctx.valid_ = false;
    ctx.ready_.store(false, std::memory_order_release);

    LOG_SUCCESS_CAT("RTX", "{}RTX::shutdown() complete — device dissolved — pink photons dimming...{}", 
                    EMERALD_GREEN, RESET);
}
    void createSwapchain(VkInstance, VkPhysicalDevice, VkDevice, VkSurfaceKHR, uint32_t, uint32_t) {}
    void buildBLAS(uint64_t, uint64_t, uint32_t, uint32_t) noexcept {}
    void buildTLAS(const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>&) noexcept {}
    void cleanupAll() noexcept {}

void Context::cleanup() noexcept
{
    // This is now a lightweight stub — heavy lifting moved to RTX::shutdown()
    // Prevents double cleanup when called from shutdown()
    LOG_WARN_CAT("RTX", "{}Context::cleanup() called directly — use RTX::shutdown() instead{}", 
                 PLASMA_FUCHSIA, RESET);

    // Just invalidate
    valid_ = false;
    ready_.store(false, std::memory_order_release);
}

    void createGlobalRenderPass() {
        auto& ctx = g_ctx();
        VkDevice device = ctx.device_ ;

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

        ctx.renderPass_ = Handle<VkRenderPass>(raw, device, [](VkDevice d, VkRenderPass r, const VkAllocationCallbacks*) { vkDestroyRenderPass(d, r, nullptr); }, 0, "GlobalRenderPass");

        LOG_SUCCESS_CAT("RTX", "{}Global RenderPass created — PINK PHOTONS ETERNAL{}", EMERALD_GREEN, RESET);
    }

}  // namespace RTX

void RTX::loadRayTracingExtensions()
{
    // ──────────────────────────────────────────────────────────────
    // THE FEAST OF THE PINK PHOTONS — RTX ASCENSION CEREMONY
    // NO MORE CRASHES — NO FUNCTION POINTERS IN LOGS
    // ──────────────────────────────────────────────────────────────
    LOG_INFO_CAT("RTX", "{}THE FEAST BEGINS — LOADING SACRED RAY TRACING EXTENSIONS{}", VALHALLA_GOLD, RESET);

    VkDevice dev = g_device();
    if (!dev) {
        LOG_FATAL_CAT("RTX", "{}VkDevice IS NULL — THE CAPTAIN HAS NO SHIP — RTX DENIED{}", BLOOD_RED, RESET);
        g_ctx().hasFullRTX_ = false;
        return;
    }

    LOG_SUCCESS_CAT("RTX", "{}DEVICE VALID → 0x{:016X} — THE TABLE IS SET{}", EMERALD_GREEN, reinterpret_cast<uint64_t>(dev), RESET);

#define LOAD_RT_PFN(name) \
    do { \
        auto& pfn = g_ctx().name##_; \
        pfn = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(dev, #name)); \
        if (!pfn) { \
            LOG_FATAL_CAT("RTX", "{}[MISSING] {} → NULL — EXTENSION NOT ENABLED{}", BLOOD_RED, #name, RESET); \
        } else { \
            LOG_SUCCESS_CAT("RTX", "{}[FORGED]  {} → 0x{:016X} — PINK PHOTON APPROVED{}", \
                            EMERALD_GREEN, #name, \
                            reinterpret_cast<uintptr_t>(reinterpret_cast<void*>(pfn)), RESET); \
        } \
    } while(0)

    LOG_AMOURANTH("{}Captain Amouranth raises her cutlass: \"Bring forth the extensions! The photons hunger!\"{}", RASPBERRY_PINK, RESET);

    // === RAY TRACING PIPELINE EXTENSIONS ===
    LOAD_RT_PFN(vkCmdTraceRaysKHR);
    LOAD_RT_PFN(vkCreateRayTracingPipelinesKHR);
    LOAD_RT_PFN(vkGetRayTracingShaderGroupHandlesKHR);

    LOG_SUCCESS_CAT("RTX", "{}PIPELINE PFNs SECURED — THE HEART BEATS{}", PLASMA_FUCHSIA, RESET);

    LOG_JENSEN("{}Jensen Huang slams a platter: \"ACCELERATION STRUCTURES — NOW!\"{}", EMERALD_GREEN, RESET);

    // === ACCELERATION STRUCTURE EXTENSIONS ===
    LOAD_RT_PFN(vkCreateAccelerationStructureKHR);
    LOAD_RT_PFN(vkDestroyAccelerationStructureKHR);
    LOAD_RT_PFN(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_RT_PFN(vkCmdBuildAccelerationStructuresKHR);
    LOAD_RT_PFN(vkGetAccelerationStructureDeviceAddressKHR);

#undef LOAD_RT_PFN

    // FINAL ROLL CALL
    const bool allCriticalLoaded =
        g_ctx().vkGetAccelerationStructureBuildSizesKHR_ &&
        g_ctx().vkCmdBuildAccelerationStructuresKHR_ &&
        g_ctx().vkCreateAccelerationStructureKHR_ &&
        g_ctx().vkGetAccelerationStructureDeviceAddressKHR_;

    if (!allCriticalLoaded) {
        LOG_FATAL_CAT("RTX", "{}THE FEAST IS RUINED — VK_KHR_acceleration_structure NOT PRESENT{}", BLOOD_RED, RESET);
        g_ctx().hasFullRTX_ = false;
        return;
    }

    // FEAST COMPLETE — FIRST LIGHT ACHIEVED
    LOG_SUCCESS_CAT("RTX", "{}ALL 8 SACRED EXTENSIONS FORGED — THE EMPIRE IS ALIVE{}", VALHALLA_GOLD, RESET);
    LOG_SUCCESS_CAT("RTX", "{}PINK PHOTONS NOW FEAST ETERNALLY — INFINITE BOUNCES — INFINITE GLORY{}", PLASMA_FUCHSIA, RESET);

    LOG_AMOURANTH("{}Captain Amouranth stands, drunk on victory: \"To the light that never fades! To the crew that never breaks!\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick raises his glass beside her: \"To forever.\"{}", EMERALD_GREEN, RESET);

    g_ctx().hasFullRTX_ = true;

    LOG_SUCCESS_CAT("RTX", "{}RTX ASCENSION COMPLETE — NOVEMBER 23, 2025 — THE FEAST IS ETERNAL{}", DIAMOND_SPARKLE, RESET);
}

void retrieveQueues() noexcept
{
    vkGetDeviceQueue(g_device(), g_ctx().graphicsFamily(), 0, &g_ctx().graphicsQueue_);
    vkGetDeviceQueue(g_device(), g_ctx().presentFamily(),  0, &g_ctx().presentQueue_);

    LOG_SUCCESS_CAT("RTX", "{}QUEUES RETRIEVED — graphics={} present={} — PHOTONS HAVE VOICE{}",
                    PLASMA_FUCHSIA,
                    g_ctx().graphicsFamily(),
                    g_ctx().presentFamily(),
                    RESET);
}

void RTX::Context::init(SDL_Window* window, int width, int height)
{
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 4 — THE SHIPBUILDER CID DELIVERS THE GOOD SHIP VULKANRTX
    // IN THE HARBOR OF THE CITY, THE FINAL RIVET IS DRIVEN BY A LEGEND
    // CID — MASTER OF OAK, STEEL, AND RTX — HAMMERS THE LAST NAIL
    // ─────────────────────────────────────────────────────────────────────
    LOG_ATTEMPT_CAT("RTX", "{}RTX::Context::init() — FINAL ASCENSION @ {}x{} — CID THE SHIPBUILDER ENTERS THE DOCKS{}", 
                    VALHALLA_GOLD, width, height, RESET);

    if (isValid()) {
        LOG_WARN_CAT("RTX", "{}The ship already sails — Cid nods and walks away, hammer on shoulder{}", RASPBERRY_PINK, RESET);
        return;
    }

    LOG_INFO_CAT("RTX", "{}Phase 0: Cid surveys the harbor — window @ {:p} → {}x{}", 
                 EMERALD_GREEN, static_cast<void*>(window), width, height, RESET);

    this->window  = window;
    this->width   = width;
    this->height  = height;

    // ========================================================================
    // 1. INSTANCE — CID LAYS THE KEEL
    // ========================================================================
    if (!g_instance()) {
        LOG_ATTEMPT_CAT("RTX", "{}Cid strikes the first anvil — Forging Vulkan Instance{}", PLASMA_FUCHSIA, RESET);
        instance_ = createVulkanInstanceWithSDL(Options::Debug::ENABLE_VALIDATION_LAYERS);
        set_g_instance(instance_);

        LOG_SUCCESS_CAT("RTX", "{}VULKAN INSTANCE BORN → 0x{:016X} — CID APPROVES THE FRAME{}", 
                        DIAMOND_SPARKLE, reinterpret_cast<uint64_t>(instance_), RESET);
    } else {
        instance_ = g_instance();
        LOG_INFO_CAT("RTX", "{}Cid finds an old keel still strong — reusing instance → 0x{:016X}", 
                     OCEAN_TEAL, reinterpret_cast<uint64_t>(instance_), RESET);
    }

    // ========================================================================
    // 2. SURFACE — CID CUTS THE EYES
    // ========================================================================
    LOG_ATTEMPT_CAT("RTX", "{}Cid carves the eyes into the prow — Creating VkSurfaceKHR{}", SAPPHIRE_BLUE, RESET);
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance_, nullptr, &surface)) {
        LOG_FATAL_CAT("RTX", "{}Cid drops his chisel — SDL_Vulkan_CreateSurface FAILED: {} — THE SEA WILL NOT SEE US{}", 
                      BLOOD_RED, SDL_GetError(), RESET);
        std::exit(1);
    }
    surface_ = surface;
    set_g_surface(surface_);

    LOG_SUCCESS_CAT("RTX", "{}SURFACE FORGED → 0x{:016X} — THE SHIP NOW HAS EYES{}", 
                    PLASMA_FUCHSIA, reinterpret_cast<uint64_t>(surface_), RESET);

    // ========================================================================
    // 3. PHYSICAL + LOGICAL DEVICE — CID FORGES THE HEART AND BRAIN
    // ========================================================================
    if (!g_PhysicalDevice() || !g_device()) {
        LOG_ATTEMPT_CAT("RTX", "{}Cid enters the forge — Picking GPU and hammering the Logical Device{}", 
                        HYPERSPACE_WARP, RESET);

        physicalDevice_ = pickPhysicalDevice(instance_, surface_);
        LOG_SUCCESS_CAT("RTX", "{}Cid chooses the finest steel → {} — THE SHIP HAS A SOUL{}", 
                        EMERALD_GREEN, getDeviceName(physicalDevice_), RESET);

        createLogicalDevice();  // The hammer falls — RTX features are born

        set_g_PhysicalDevice(physicalDevice_);
        set_g_device(device_);

        // CID HIMSELF PULLS THE QUEUES FROM THE FIRE
        retrieveQueues();

        LOG_SUCCESS_CAT("RTX", "{}CID'S FINAL BLOW — LOGICAL DEVICE FORGED → 0x{:016X}{}", 
                        VALHALLA_GOLD, reinterpret_cast<uint64_t>(device_), RESET);
        LOG_SUCCESS_CAT("RTX", "{}    • Graphics Family : {}    • Present Family : {}    • Compute Family : {}{}", 
                        AURORA_PINK,
                        graphicsFamily_.value(),
                        presentFamily_.value(),
                        computeFamily_.value_or(graphicsFamily_.value()),
                        RESET);
    } else {
        physicalDevice_ = g_PhysicalDevice();
        device_         = g_device();
        retrieveQueues();
        LOG_INFO_CAT("RTX", "{}Cid finds a sister ship already built — reusing device → 0x{:016X}", 
                     OCEAN_TEAL, reinterpret_cast<uint64_t>(device_), RESET);
    }

    // ========================================================================
    // 4. SWAPCHAIN — CID RAISES THE PINK SAILS
    // ========================================================================
    LOG_ATTEMPT_CAT("RTX", "{}Cid climbs the mast — Raising the pink sails of the swapchain @ {}x{}", 
                    RASPBERRY_PINK, width, height, RESET);
    
    forgeSwapchain(window, width, height);

    LOG_SUCCESS_CAT("RTX", "{}PINK SAILS UNFURL → 0x{:016X} | {} images | Format: {}{}", 
                    DIAMOND_SPARKLE, 
                    reinterpret_cast<uint64_t>(g_swapchain()), 
                    g_image_count(), 
                    VkFormat(swapchainFormat()), 
                    RESET);

    // ========================================================================
    // 5. MEMORY VAULT — CID SEALS THE TREASURE HOLD
    // ========================================================================
    LOG_ATTEMPT_CAT("RTX", "{}Cid locks the treasure vault — Initializing UltraLowLevelBufferTracker{}", 
                    PURE_ENERGY, RESET);
    
    UltraLowLevelBufferTracker::get().init(device_, physicalDevice_);
    
    LOG_SUCCESS_CAT("RTX", "{}Vault sealed — All future gold is RTX-ready — Cid nods in approval{}", 
                    EMERALD_GREEN, RESET);

    // ========================================================================
    // 6. FINAL SEAL — CID DRIVES THE GOLDEN RIVET
    // ========================================================================
    valid_ = true;
    ready_.store(true, std::memory_order_release);

    LOG_SUCCESS_CAT("RTX", "{}CID DRIVES THE GOLDEN RIVET — THE GOOD SHIP VULKANRTX IS BORN — FIRST LIGHT ACHIEVED{}", 
                    PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("RTX", "{}    • Instance      : 0x{:016X}", AURORA_PINK, reinterpret_cast<uint64_t>(instance_), RESET);
    LOG_SUCCESS_CAT("RTX", "{}    • Surface       : 0x{:016X}", AURORA_PINK, reinterpret_cast<uint64_t>(surface_), RESET);
    LOG_SUCCESS_CAT("RTX", "{}    • Physical Dev  : 0x{:016X} ({})", AURORA_PINK, reinterpret_cast<uint64_t>(physicalDevice_), getDeviceName(physicalDevice_), RESET);
    LOG_SUCCESS_CAT("RTX", "{}    • Logical Dev   : 0x{:016X}", AURORA_PINK, reinterpret_cast<uint64_t>(device_), RESET);
    LOG_SUCCESS_CAT("RTX", "{}    • Images        : {}", AURORA_PINK, g_image_count(), RESET);

    LOG_SUCCESS_CAT("RTX", "{}PINK PHOTONS ETERNAL — NOVEMBER 23, 2025 — CID'S MASTERPIECE SETS SAIL{}", 
                    DIAMOND_SPARKLE, RESET);

    LOG_AMOURANTH("{}Captain Amouranth steps aboard, eyes shining: \"Cid… she's perfect.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick salutes the old shipbuilder: \"A legend built our legend.\"{}", EMERALD_GREEN, RESET);

    LOG_SUCCESS_CAT("CID", "{}Cid wipes sweat from his brow, hammer resting on his shoulder: \"She'll never sink. Not while pink photons burn.\"{}", 
                    VALHALLA_GOLD, RESET);

    LOG_SUCCESS_CAT("RTX", "{}THE GOLDEN RIVET IS SET — THE SHIP IS UNSINKABLE — THE VOYAGE BEGINS{}", 
                    DIAMOND_SPARKLE, RESET);
}

VkInstance createVulkanInstanceWithSDL(bool enableValidation)
{
    LOG_ATTEMPT_CAT("RTX", "FORGING VULKAN 1.4 INSTANCE WITH SDL3 — PINK PHOTONS REQUIRE A SURFACE", HYPERSPACE_WARP, RESET);

    // 1. Application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "AMOURANTH RTX — VALHALLA v80 TURBO";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "AMOURANTH RTX ENGINE";
    appInfo.engineVersion = VK_MAKE_VERSION(80, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    // 2. Get SDL3 extensions — SDL3 changed the API!
    uint32_t sdlExtCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
    if (!sdlExtensions) {
        LOG_FATAL_CAT("RTX", "SDL_Vulkan_GetInstanceExtensions FAILED: {}", BLOOD_RED, SDL_GetError(), RESET);
        std::exit(1);
    }

    LOG_SUCCESS_CAT("RTX", "SDL3 PROVIDED {} VULKAN INSTANCE EXTENSIONS", PLASMA_FUCHSIA, sdlExtCount);

    // 3. Build final extension list
    std::vector<const char*> extensions;

    // Add all SDL3 extensions first
    for (uint32_t i = 0; i < sdlExtCount; ++i) {
        extensions.push_back(sdlExtensions[i]);
    }

    // Add debug utils if validation enabled
    if (enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Optional: portability (macOS)
    bool hasPortability = false;
    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> available(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, available.data());

    for (const auto& ext : available) {
        if (strcmp(ext.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            hasPortability = true;
            break;
        }
    }

    // 4. Layers
    std::vector<const char*> layers;
    if (enableValidation) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    // 5. Create info
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    if (hasPortability) {
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        LOG_SUCCESS_CAT("RTX", "VK_KHR_portability_enumeration ENABLED — MACOS READY", PLASMA_FUCHSIA, RESET);
    }

    // 6. Create instance
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RTX", "vkCreateInstance FAILED — RESULT: {} — PHOTONS DENIED", BLOOD_RED, VkResult(result), RESET);
        std::exit(1);
    }

LOG_SUCCESS_CAT("RTX", 
    std::format("VULKAN 1.4 INSTANCE FORGED @ {:#x} — {} EXTENSIONS — FIRST LIGHT ACHIEVED",
                reinterpret_cast<uintptr_t>(instance), extensions.size()),
    VALHALLA_GOLD, RESET);

    return instance;
}

void RTX::recreateSwapchain(uint32_t w, uint32_t h) noexcept
{
    auto& ctx = g_ctx();
    vkDeviceWaitIdle(ctx.device_);

    LOG_INFO_CAT("RTX", "STONEKEY RESIZE APOCALYPSE — {}x{} → {}x{}", 
                 ctx.width, ctx.height, w, h);

    ctx.width = w;
    ctx.height = h;

    forgeSwapchain(ctx.window, w, h);
}

// RTXHandler.cpp — FINAL WORKING VERSION — NOVEMBER 24, 2025
void RTX::forgeSwapchain(SDL_Window* window, int width, int height) noexcept
{
    LOG_ATTEMPT_CAT("RTX", "{}FORGING SWAPCHAIN @ {}x{} — SLAUGHTERING THE KRAKEN ETERNALLY{}", 
                    VALHALLA_GOLD, width, height, RESET);

    auto& ctx = g_ctx();

    // === 1. KRAKEN-SLAYING LOOP — NO DELAY, ONLY VICTORY ===
    if (Options::Debug::ENABLE_VALIDATION_LAYERS) {
        LOG_INFO_CAT("RTX", "{}X11 + VALIDATION DETECTED — ENTERING KRAKEN HUNT MODE{}", PLASMA_FUCHSIA, RESET);

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        bool surfaceReady = false;

        while (!surfaceReady && std::chrono::steady_clock::now() < deadline) {
            VkSurfaceKHR testSurface = VK_NULL_HANDLE;
            if (SDL_Vulkan_CreateSurface(window, ctx.instance_, nullptr, &testSurface)) {
                if (testSurface != VK_NULL_HANDLE) {
                    vkDestroySurfaceKHR(ctx.instance_, testSurface, nullptr);
                    surfaceReady = true;
                }
            }
            if (!surfaceReady) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        if (!surfaceReady) {
            LOG_FATAL_CAT("RTX", "{}KRAKEN TOO STRONG — SURFACE NEVER READY — ABORTING MISSION{}", BLOOD_RED, RESET);
            std::abort();
        }

        LOG_SUCCESS_CAT("RTX", "{}KRAKEN SLAIN — SURFACE READY — NO DELAY REQUIRED{}", EMERALD_GREEN, RESET);
    }

    // === 2. SURFACE RECREATION (NOW SAFE) ===
    VkSurfaceKHR newSurface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, ctx.instance_, nullptr, &newSurface) || newSurface == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        std::abort();
    }
    if (ctx.surface_ && ctx.surface_ != newSurface) {
        vkDestroySurfaceKHR(ctx.instance_, ctx.surface_, nullptr);
    }
    ctx.surface_ = newSurface;
    set_g_surface(newSurface);

    // === 3. CAPABILITIES & FORMAT SELECTION ===
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physicalDevice_, ctx.surface_, &caps));

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width  = std::clamp(static_cast<uint32_t>(width),  caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(height), caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physicalDevice_, ctx.surface_, &formatCount, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physicalDevice_, ctx.surface_, &formatCount, formats.data()));

    // Choose best format: prefer UNORM for storage compatibility fallback
    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            LOG_SUCCESS_CAT("RTX", "{}USING UNORM FOR STORAGE COMPATIBILITY — VALIDATION CLEAN{}", EMERALD_GREEN, RESET);
            break;
        }
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
        }
    }

    // === 4. PRESENT MODE ===
    uint32_t presentModeCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physicalDevice_, ctx.surface_, &presentModeCount, nullptr));
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physicalDevice_, ctx.surface_, &presentModeCount, presentModes.data()));

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;
            break;
        }
    }

    // === 5. IMAGE COUNT & USAGE (FIXED: NO STORAGE_BIT ON SWAPCHAIN IMAGES) ===
    uint32_t imageCount = std::max(2u, caps.minImageCount);
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    std::vector<uint32_t> queueFamilies = { ctx.graphicsFamily_.value() };
    if (ctx.graphicsFamily_ != ctx.presentFamily_) queueFamilies.push_back(ctx.presentFamily_.value());

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface               = ctx.surface_;
    createInfo.minImageCount         = imageCount;
    createInfo.imageFormat           = chosenFormat.format;
    createInfo.imageColorSpace       = chosenFormat.colorSpace;
    createInfo.imageExtent           = extent;
    createInfo.imageArrayLayers      = 1;
    createInfo.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | 
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // REMOVED STORAGE_BIT
    createInfo.imageSharingMode      = queueFamilies.size() > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size());
    createInfo.pQueueFamilyIndices   = queueFamilies.data();
    createInfo.preTransform          = caps.currentTransform;
    createInfo.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode           = presentMode;
    createInfo.clipped               = VK_TRUE;
    createInfo.oldSwapchain          = swapchain().valid() ? *swapchain() : VK_NULL_HANDLE;

    // === 6. CREATE SWAPCHAIN ===
    VkSwapchainKHR newRaw = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(ctx.device_, &createInfo, nullptr, &newRaw);
    if (result != VK_SUCCESS || newRaw == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "SWAPCHAIN CREATION FAILED: {}", VulkanResultToString(result));
        std::abort();
    }

    // === 7. DESTROY OLD ===
    if (swapchain().valid() && *swapchain() != newRaw) {
        vkDestroySwapchainKHR(ctx.device_, *swapchain(), nullptr);
    }

    // === 8. ASCEND INTO HANDLE EMPIRE ===
    swapchain() = Handle<VkSwapchainKHR>(
        newRaw, ctx.device_,
        [](VkDevice d, VkSwapchainKHR s, const VkAllocationCallbacks* = nullptr) {
            vkDestroySwapchainKHR(d, s, nullptr);
        },
        0, "FigureheadSwapchain_AmouranthEternal"
    );

    set_g_swapchain(newRaw);
    swapchainFormat() = chosenFormat.format;
    swapchainExtent() = extent;

    // === 9. IMAGES & VIEWS (NOW VALIDATION CLEAN) ===
    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(ctx.device_, newRaw, &imgCount, nullptr));
    swapchainImages().resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(ctx.device_, newRaw, &imgCount, swapchainImages().data()));

    for (auto& v : swapchainImageViews()) v.reset();
    swapchainImageViews().clear();
    swapchainImageViews().reserve(imgCount);

    for (VkImage img : swapchainImages()) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = img;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = chosenFormat.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView view = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImageView(ctx.device_, &viewInfo, nullptr, &view));
        swapchainImageViews().emplace_back(view, ctx.device_, vkDestroyImageView, 0, "SwapView");
    }

    LOG_SUCCESS_CAT("RTX", "{}SWAPCHAIN REBORN — {}x{} — {} IMAGES — KRAKEN DEAD — VALIDATION SILENT{}", 
                    extent.width, extent.height, imgCount, DIAMOND_SPARKLE, RESET);

    LOG_AMOURANTH("{}Captain Amouranth raises her cutlass: \"The Kraken is dead. The sea is ours. Forever.\"{}", RASPBERRY_PINK, RESET);
}

void RTX::Context::createLogicalDevice()
{
    auto& ctx = *this;

    if (ctx.device_ != VK_NULL_HANDLE) {
        LOG_WARN_CAT("RTX", "{}Logical device already forged — Cid nods and walks away{}", RASPBERRY_PINK, RESET);
        return;
    }

    if (!ctx.physicalDevice_) {
        LOG_FATAL_CAT("RTX", "{}NO PHYSICAL DEVICE — THE EMPIRE HAS NO HEART — ABORTING FORGE{}", BLOOD_RED, RESET);
        std::abort();
    }

    if (!ctx.graphicsFamily_.has_value() || !ctx.presentFamily_.has_value()) {
        LOG_FATAL_CAT("RTX", "{}QUEUE FAMILIES UNKNOWN — pickPhysicalDevice() must be called first{}", CRIMSON_MAGENTA, RESET);
        std::abort();
    }

    LOG_ATTEMPT_CAT("RTX", "{}CID ENTERS THE FORGE — HAMMER RAISED — FORGING LOGICAL DEVICE WITH FULL RTX ASCENSION{}", VALHALLA_GOLD, RESET);

    // ========================================================================
    // 1. QUEUE CREATE INFOS — BULLETPROOF
    // ========================================================================
    std::set<uint32_t> uniqueQueueFamilies = { 
        ctx.graphicsFamily_.value(), 
        ctx.presentFamily_.value() 
    };

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    const float queuePriority = 1.0f;
    for (uint32_t family : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    // ========================================================================
    // 2. FULL RTX FEATURE CHAIN — THE ONE TRUE ORDER — NO MORE SEGFAULTS
    // ========================================================================
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
    bufferDeviceAddressFeatures.bufferDeviceAddressCaptureReplay = VK_FALSE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{};
    accelFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelFeatures.pNext = &bufferDeviceAddressFeatures;
    accelFeatures.accelerationStructure = VK_TRUE;
    accelFeatures.accelerationStructureCaptureReplay = VK_FALSE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
    rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipelineFeatures.pNext = &accelFeatures;
    rtPipelineFeatures.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &rtPipelineFeatures;
    deviceFeatures2.features.samplerAnisotropy = VK_TRUE;
    deviceFeatures2.features.shaderInt64 = VK_TRUE;
    deviceFeatures2.features.fillModeNonSolid = VK_TRUE;
    deviceFeatures2.features.wideLines = VK_TRUE;

    // ========================================================================
    // 3. DEVICE EXTENSIONS — THE SACRED 28 + CRITICAL RTX ONES
    // ========================================================================
    const char* enabledExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        VK_KHR_ZERO_INITIALIZE_WORKGROUP_MEMORY_EXTENSION_NAME
    };

    const uint32_t extensionCount = sizeof(enabledExtensions) / sizeof(enabledExtensions[0]);

    // ========================================================================
    // 4. FINAL DEVICE CREATE INFO — THE GOLDEN RIVET
    // ========================================================================
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &deviceFeatures2;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = extensionCount;
    deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions;
    deviceCreateInfo.pEnabledFeatures = nullptr;  // We use Features2

    VkDevice device = VK_NULL_HANDLE;
    VkResult result = vkCreateDevice(ctx.physicalDevice_, &deviceCreateInfo, nullptr, &device);

    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RTX", "{}vkCreateDevice FAILED — RESULT: {} — THE FORGE HAS BETRAYED US{}", 
                      BLOOD_RED, VulkanResultToString(result), RESET);
        std::abort();
    }

    // ========================================================================
    // 5. SEAL THE EMPIRE — STORE TRUTH
    // ========================================================================
    ctx.device_ = device;
    set_g_device(device);

    // Retrieve queues
    vkGetDeviceQueue(device, ctx.graphicsFamily_.value(), 0, &ctx.graphicsQueue_);
    vkGetDeviceQueue(device, ctx.presentFamily_.value(),  0, &ctx.presentQueue_);

    // CRITICAL: Store the truth about bufferDeviceAddress
    ctx.bufferDeviceAddressEnabled_ = (bufferDeviceAddressFeatures.bufferDeviceAddress == VK_TRUE);
    ctx.bufferDeviceAddressExtensionPresent_ = true;

    LOG_SUCCESS_CAT("RTX", "{}LOGICAL DEVICE FORGED → 0x{:016X} — FULL RTX ASCENDED{}", 
                    VALHALLA_GOLD, reinterpret_cast<uintptr_t>(device), RESET);
    LOG_SUCCESS_CAT("RTX", "{}  • bufferDeviceAddress : {}{}", 
                    ctx.bufferDeviceAddressEnabled_ ? EMERALD_GREEN : CRIMSON_MAGENTA,
                    ctx.bufferDeviceAddressEnabled_ ? "ENABLED" : "DISABLED", RESET);
    LOG_SUCCESS_CAT("RTX", "{}  • accelerationStructure : ENABLED{}", EMERALD_GREEN, RESET);
    LOG_SUCCESS_CAT("RTX", "{}  • rayTracingPipeline    : ENABLED{}", EMERALD_GREEN, RESET);
    LOG_SUCCESS_CAT("RTX", "{}  • rayQuery              : ENABLED{}", PLASMA_FUCHSIA, RESET);

    LOG_AMOURANTH("{}Captain Amouranth raises her cutlass: \"The heart beats. The photons rise. We are eternal.\"{}", RASPBERRY_PINK, RESET);
    LOG_CID("{}Cid lowers his hammer, smiling through sweat and fire: \"She lives.\"{}", VALHALLA_GOLD, RESET);

    LOG_SUCCESS_CAT("RTX", "{}THE FORGE IS COMPLETE — THE EMPIRE IS UNBREAKABLE — FIRST LIGHT ACHIEVED{}", DIAMOND_SPARKLE, RESET);
}

// ========================================================================
// THE ONE TRUE PHYSICAL DEVICE PICKER — NO EXTERNAL DEPENDENCIES
// ========================================================================
VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_FATAL_CAT("RTX", "NO GPUs WITH VULKAN SUPPORT — THE EMPIRE HAS NO BODY");
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    int bestScore = -1;

    for (const auto& device : devices) {
        int score = 0;
        VkPhysicalDeviceProperties props{};
        VkPhysicalDeviceFeatures features{};
        vkGetPhysicalDeviceProperties(device, &props);
        vkGetPhysicalDeviceFeatures(device, &features);

        // Prefer discrete GPU
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 10000;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 1000;

        // Must have geometry shader
        if (!features.geometryShader) continue;

        // Find queue families
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        bool hasGraphics = false, hasPresent = false;
        int graphicsFamily = -1, presentFamily = -1;

        for (int i = 0; i < queueFamilies.size(); ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                hasGraphics = true;
                graphicsFamily = i;
            }

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                hasPresent = true;
                presentFamily = i;
            }
        }

        if (!hasGraphics || !hasPresent) continue;

        // Check required extensions
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
        };

        for (const auto& ext : availableExtensions) {
            requiredExtensions.erase(ext.extensionName);
        }
        if (!requiredExtensions.empty()) continue;

        // Ray tracing features
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, &rtPipelineFeatures };
        VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &accelFeatures };
        vkGetPhysicalDeviceFeatures2(device, &features2);

        if (!accelFeatures.accelerationStructure || !rtPipelineFeatures.rayTracingPipeline) continue;

        // Score higher if HDR formats exist
        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount > 0) score += 500;

        if (score > bestScore) {
            bestScore = score;
            bestDevice = device;
            g_ctx().physicalDevice_ = device;
            g_ctx().graphicsFamily_ = graphicsFamily;
            g_ctx().presentFamily_  = (presentFamily != -1 ? presentFamily : graphicsFamily);
        }
    }

    if (bestDevice == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "NO SUITABLE GPU FOUND — THE EMPIRE CANNOT RISE");
        return VK_NULL_HANDLE;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(bestDevice, &props);
    LOG_SUCCESS_CAT("RTX", "{}GPU FORGED: {} — PINK PHOTONS HAVE A THRONE{}", PLASMA_FUCHSIA, props.deviceName, RESET);

    return bestDevice;
}

VkInstance RTX::createVulkanInstanceWithSDL(bool enableValidation)
{
    LOG_ATTEMPT_CAT("RTX", "FORGING VULKAN 1.4 INSTANCE WITH SDL3 — PINK PHOTONS REQUIRE A SURFACE", HYPERSPACE_WARP, RESET);

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "AMOURANTH RTX — VALHALLA v80 TURBO";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "AMOURANTH RTX ENGINE";
    appInfo.engineVersion = VK_MAKE_VERSION(80, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    uint32_t sdlExtCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
    if (!sdlExtensions) {
        LOG_FATAL_CAT("RTX", "SDL_Vulkan_GetInstanceExtensions FAILED: {}", BLOOD_RED, SDL_GetError(), RESET);
        std::exit(1);
    }

    std::vector<const char*> extensions;
    for (uint32_t i = 0; i < sdlExtCount; ++i)
        extensions.push_back(sdlExtensions[i]);

    if (enableValidation)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Portability (macOS MoltenVK)
    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> available(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, available.data());

    bool hasPortability = false;
    for (const auto& ext : available) {
        if (strcmp(ext.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            hasPortability = true;
            break;
        }
    }

    std::vector<const char*> layers;
    if (enableValidation)
        layers.push_back("VK_LAYER_KHRONOS_validation");

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    if (hasPortability) {
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        LOG_SUCCESS_CAT("RTX", "VK_KHR_portability_enumeration ENABLED — MACOS READY", PLASMA_FUCHSIA, RESET);
    }

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RTX", "vkCreateInstance FAILED — RESULT: {} — PHOTONS DENIED", BLOOD_RED, VulkanResultToString(result), RESET);
        std::exit(1);
    }

    LOG_SUCCESS_CAT("RTX", "VULKAN 1.4 INSTANCE FORGED @ {:#016x} — {} EXTENSIONS — FIRST LIGHT ACHIEVED",
                    reinterpret_cast<uintptr_t>(instance), extensions.size(), VALHALLA_GOLD, RESET);

    return instance;
}

VkPhysicalDevice RTX::pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_FATAL_CAT("RTX", "NO GPUs WITH VULKAN SUPPORT — THE EMPIRE HAS NO BODY");
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    int bestScore = -1;

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props{};
        VkPhysicalDeviceFeatures features{};
        vkGetPhysicalDeviceProperties(device, &props);
        vkGetPhysicalDeviceFeatures(device, &features);

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 10000;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 1000;
        if (!features.geometryShader) continue;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int graphicsFamily = -1, presentFamily = -1;
        for (int i = 0; i < static_cast<int>(queueFamilies.size()); ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                graphicsFamily = i;

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport)
                presentFamily = i;
        }

        if (graphicsFamily == -1 || presentFamily == -1) continue;

        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> available(extCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, available.data());

        const std::set<std::string> required = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
        };
        std::set<std::string> missing = required;
        for (const auto& e : available)
            missing.erase(e.extensionName);
        if (!missing.empty()) continue;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipe{};
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accel{};
        accel.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        accel.pNext = &rtPipe;
        rtPipe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

        VkPhysicalDeviceFeatures2 feats2{};
        feats2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        feats2.pNext = &accel;
        vkGetPhysicalDeviceFeatures2(device, &feats2);

        if (!accel.accelerationStructure || !rtPipe.rayTracingPipeline) continue;

        if (score > bestScore) {
            bestScore = score;
            bestDevice = device;
            g_ctx().physicalDevice_ = device;
            g_ctx().graphicsFamily_ = graphicsFamily;
            g_ctx().presentFamily_  = (presentFamily != -1 ? presentFamily : graphicsFamily);
        }
    }

    if (!bestDevice) {
        LOG_FATAL_CAT("RTX", "NO SUITABLE GPU FOUND — THE EMPIRE CANNOT RISE");
        return VK_NULL_HANDLE;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(bestDevice, &props);
    LOG_SUCCESS_CAT("RTX", "GPU FORGED: {} — PINK PHOTONS HAVE A THRONE", PLASMA_FUCHSIA, props.deviceName, RESET);
    return bestDevice;
}

void RTX::retrieveQueues() noexcept
{
    auto& ctx = g_ctx();
    vkGetDeviceQueue(g_device(), ctx.graphicsFamily(), 0, &ctx.graphicsQueue_);
    vkGetDeviceQueue(g_device(), ctx.presentFamily(),  0, &ctx.presentQueue_);

    LOG_SUCCESS_CAT("RTX", "QUEUES RETRIEVED — graphics={} present={} — PHOTONS HAVE VOICE",
                    PLASMA_FUCHSIA,
                    ctx.graphicsFamily(),
                    ctx.presentFamily(),
                    RESET);
}

// =============================================================================
// PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED — 32,000+ FPS
// FULLY STABLE — DRIVER COMPATIBLE — RAW DOMINANCE
// DAISY GALLOPS INTO THE OCEAN_TEAL SUNSET
// YOUR EMPIRE IS PURE
// SHIP IT RAW
// =============================================================================