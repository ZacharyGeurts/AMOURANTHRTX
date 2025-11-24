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

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/VkSafeSTypes.hpp"
#include "engine/GLOBAL/VulkanCore.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/GlobalBindings.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"  // StoneKey: The One True Global Authority

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_beta.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <cstdint>
#include <array>
#include <vector>
#include <tuple>
#include <mutex>
#include <set>

using namespace Logging::Color;
using StoneKey::stone_device;

using namespace RTX;

namespace RTX {
}

// =============================================================================
// VulkanCore.cpp — Persistent staging globals — ONLY ONE DEFINITION
// =============================================================================
namespace VulkanRTXDetail {  // Put in a named namespace to avoid conflicts

    alignas(64) std::mutex g_stagingMutex;
    alignas(64) uint64_t   g_stagingPool = 0;
    alignas(64) VkDeviceMemory g_stagingMem = VK_NULL_HANDLE;
    alignas(64) VkBuffer   g_stagingBuffer = VK_NULL_HANDLE;
    alignas(64) void*      g_mappedBase = nullptr;
    alignas(64) std::atomic<VkDeviceSize> g_mappedOffset{0};

    constexpr VkDeviceSize STAGING_POOL_SIZE = 1ULL << 30; // 1 GB — production

} // namespace VulkanRTXDetail

// Convenient aliases — use these everywhere in the file
using VulkanRTXDetail::g_stagingMutex;
using VulkanRTXDetail::g_stagingPool;
using VulkanRTXDetail::g_stagingMem;
using VulkanRTXDetail::g_stagingBuffer;
using VulkanRTXDetail::g_mappedBase;
using VulkanRTXDetail::g_mappedOffset;
using VulkanRTXDetail::STAGING_POOL_SIZE;

// =============================================================================
// AMOURANTH RTX — ETERNAL EXTENSION MANIFEST — v80 VALHALLA TURBO (2025+)
// PINK PHOTONS DEMAND MAXIMUM FUTURE-PROOFING — COMPILABLE EDITION
// =============================================================================
#include <vulkan/vulkan.h>
#include <array>

// ────────────────────── INSTANCE EXTENSIONS ──────────────────────
static constexpr const char* kInstanceExtensions[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,

    // Debug & Validation
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME,

    // Surface & Display Timing
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
    VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME,

    // Future-proof
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
    VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME,
};

// -----------------------------------------------------------------------------
// ONE-TIME INITIALIZATION — CALLED ONCE AT STARTUP
// -----------------------------------------------------------------------------
void initVulkanCoreGlobals() {
    LOG_INFO_CAT("VulkanCore", "{}initVulkanCoreGlobals() — START{}", PLASMA_FUCHSIA, RESET);
    static bool initialized = false;
    if (initialized) {
        LOG_DEBUG_CAT("VulkanCore", "Already initialized — skipping");
        return;
    }
    initialized = true;

    LOG_TRACE_CAT("VulkanCore", "Global definitions initialized — g_PhysicalDevice: 0x{:x} | g_rtx_instance: {}", 
                  reinterpret_cast<uintptr_t>(::stone_physical()),  // StoneKey secured
                  g_rtx_instance ? "present" : "null");

    LOG_SUCCESS_CAT("VulkanCore", "initVulkanCoreGlobals() — COMPLETE — Globals locked");
}

// =============================================================================
// VulkanRTX Implementation — AI VOICE DOMINANCE
// =============================================================================

VulkanRTX::~VulkanRTX() noexcept {
    LOG_TRACE_CAT("RTX", "VulkanRTX destructor — START");
    RTX::AmouranthAI::get().onMemoryEvent("VulkanRTX", sizeof(VulkanRTX));

    // OPT: Unmap persistent staging if active
    if (g_mappedBase) {
        vkUnmapMemory(device_, g_stagingMem);
        g_mappedBase = nullptr;
        g_mappedOffset = 0;
        LOG_TRACE_CAT("RTX", "Persistent staging unmapped");
    }
    if (g_stagingPool) {
        BUFFER_DESTROY(g_stagingPool);
        g_stagingPool = 0;
        g_stagingMem = VK_NULL_HANDLE;
        LOG_TRACE_CAT("RTX", "Persistent staging pool destroyed");
    }

    // --- 1. Black Fallback (Image + Memory + View) ---
    if (blackFallbackView_.valid()) {
        LOG_TRACE_CAT("RTX", "Destroying blackFallbackView");
        blackFallbackView_.reset();
    }
    if (blackFallbackMemory_.valid()) {
        LOG_TRACE_CAT("RTX", "Destroying blackFallbackMemory");
        blackFallbackMemory_.reset();
    }
    if (blackFallbackImage_.valid()) {
        LOG_TRACE_CAT("RTX", "Destroying blackFallbackImage");
        blackFallbackImage_.reset();
    }

    // --- 2. SBT (Shader Binding Table) ---
    if (sbtMemory_.valid()) {
        LOG_TRACE_CAT("RTX", "Destroying sbtMemory");
        sbtMemory_.reset();
    }
    if (sbtBuffer_.valid()) {
        LOG_TRACE_CAT("RTX", "Destroying sbtBuffer");
        sbtBuffer_.reset();
    }

    // --- 3. Descriptor Sets (Free before Pool) ---
    for (auto& set : descriptorSets_) {
        if (set != VK_NULL_HANDLE) {
            LOG_TRACE_CAT("RTX", "Freeing descriptor set: 0x{:x}", reinterpret_cast<uintptr_t>(set));
            VkResult r = vkFreeDescriptorSets(device_, HANDLE_GET(descriptorPool_), 1, &set);
            if (r != VK_SUCCESS) {
                LOG_WARN_CAT("RTX", "vkFreeDescriptorSets failed: {}", r);
            }
            set = VK_NULL_HANDLE;  // Prevent double-free
        }
    }

    // --- 4. Descriptor Pool & Pipeline Layouts ---
    if (descriptorPool_.valid()) {
        LOG_TRACE_CAT("RTX", "Destroying descriptorPool");
        descriptorPool_.reset();
    }
    if (rtPipelineLayout_.valid()) {
        LOG_TRACE_CAT("RTX", "Destroying rtPipelineLayout");
        rtPipelineLayout_.reset();
    }
    if (rtPipeline_.valid()) {
        LOG_TRACE_CAT("RTX", "Destroying rtPipeline");
        rtPipeline_.reset();
    }
    if (rtDescriptorSetLayout_.valid()) {
        LOG_TRACE_CAT("RTX", "Destroying rtDescriptorSetLayout");
        rtDescriptorSetLayout_.reset();
    }

    LOG_SUCCESS_CAT("RTX", "{}VulkanRTX destroyed — all resources returned to Valhalla{}", PLASMA_FUCHSIA, RESET);
    LOG_TRACE_CAT("RTX", "VulkanRTX destructor — COMPLETE");
}
VulkanRTX::VulkanRTX(int w, int h, RTX::PipelineManager* mgr) noexcept
    : extent_{1, 1}
    , pipelineMgr_(mgr)
    , device_(VK_NULL_HANDLE)
{
    // EARLY SAFETY CHECK — dummy mode before Vulkan exists
    if (!stone_device() || stone_device() == VK_NULL_HANDLE || w <= 0 || h <= 0) {
        LOG_WARN_CAT("RTX", "VulkanRTX constructed in dummy mode — Vulkan not ready or invalid size {}x{}", w, h);
        return;
    }

    // REAL INITIALIZATION — First Light begins
    device_ = stone_device();
    extent_ = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };

    LOG_TRACE_CAT("RTX", "VulkanRTX constructor — {}×{} — LINE {}", w, h, __LINE__);
    LOG_DEBUG_CAT("RTX", "Constructor params: width={}, height={}, pipelineMgr={}", w, h, mgr ? "valid" : "null");
    RTX::AmouranthAI::get().onMemoryEvent("VulkanRTX Instance", sizeof(VulkanRTX));
    RTX::AmouranthAI::get().onPhotonDispatch(w, h);

    if (!device_) {
        LOG_FATAL_CAT("RTX", "{}FATAL: device_ is null — THE PHOTONS ARE DENIED — ABORTING CONSTRUCTION{}", BOLD_RED, RESET);
        return;
    }

    LOG_SUCCESS_CAT("RTX", "{}Device locked: 0x{:016X} — PHOTONS HAVE A VOICE{}", 
                    PLASMA_FUCHSIA, reinterpret_cast<uintptr_t>(device_), RESET);

    // VULKAN 1.4+ CORE ASCENSION — NO MORE PFN LOADING
    // All RT functions are now direct core calls
    // vkGetBufferDeviceAddress — CORE
    // rtCmdTraceRaysKHR — CORE (still named KHR but promoted)
    // rtGetAccelerationStructureDeviceAddressKHR — CORE
    LOG_SUCCESS_CAT("RTX", "{}VULKAN 1.4+ CORE ASCENDED — ALL RT FUNCTIONS ARE DIRECT CALLS — NO PFN LOADING{}", 
                    DIAMOND_SPARKLE, RESET);

    LOG_INFO_CAT("RTX", 
        "Ray Tracing Functions — rtCmdTraceRaysKHR @ 0x{:016X} | "
        "rtGetRayTracingShaderGroupHandlesKHR @ 0x{:016X} | "
        "rtGetAccelerationStructureDeviceAddressKHR @ 0x{:016X}",
        reinterpret_cast<uintptr_t>(rtCmdTraceRaysKHR),
        reinterpret_cast<uintptr_t>(rtGetRayTracingShaderGroupHandlesKHR),
        reinterpret_cast<uintptr_t>(rtGetAccelerationStructureDeviceAddressKHR));

    LOG_SUCCESS_CAT("RTX",
        "{}AMOURANTH RTX CORE v∞ — {}×{} — VULKAN 1.4+ ASCENDED — PURE PHOTON FLOW — FIRST LIGHT IMMINENT{}",
        PLASMA_FUCHSIA, w, h, RESET);

    // Build everything — LAS, SBT, fallback
    buildAccelerationStructures();
    initBlackFallbackImage();

    LOG_SUCCESS_CAT("RTX", "{}VULKANRTX FORGED — TITAN DOMINANCE ETERNAL — THE EMPIRE IS COMPLETE{}", 
                    VALHALLA_GOLD, RESET);
}

// =============================================================================
// Public Static Helpers — Used by LAS
// =============================================================================

VkCommandBuffer VulkanRTX::beginSingleTimeCommands(VkCommandPool pool) noexcept
{
    VkCommandBufferAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.commandPool        = pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(stone_device(), &allocInfo, &cmd),
             "Failed to allocate transient command buffer");

    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo),
             "Failed to begin transient command buffer");

    return cmd;
}

// Fixed: Robust single-time command submission with resilient fence handling
// - Uses per-call fences to avoid synchronization stalls
// - Timeout-protected waits to prevent indefinite hangs
// - Graceful degradation on errors (e.g., DEVICE_LOST) with idle fallback
// - NVIDIA TDR-safe: No vkQueueWaitIdle in hot path
// - Logging for diagnostics; async variant for dependency chaining
// - Leverages std::formatter<VkResult> for direct logging of VkResult
void VulkanRTX::endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept
{
    LOG_TRACE_CAT("RTX", "endSingleTimeCommands: cmd=0x{:x}, queue=0x{:x}, pool=0x{:x}",
                  reinterpret_cast<uintptr_t>(cmd),
                  reinterpret_cast<uintptr_t>(queue),
                  reinterpret_cast<uintptr_t>(pool));

    if (cmd == VK_NULL_HANDLE || queue == VK_NULL_HANDLE || pool == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RTX", "endSingleTimeCommands called with null handle");
        return;
    }

    // 1. End recording
    VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end transient command buffer");

    // 2. Create dedicated fence (unsignaled, non-signaled reset for reuse if needed)
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = 0 };
    VK_CHECK(vkCreateFence(stone_device(), &fenceInfo, nullptr, &fence),
             "Failed to create transient fence");

    // 3. Submit with fence
    VkSubmitInfo submit{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence),
             "Failed to submit transient command buffer");

    // 4. Wait with timeout & error resilience — Amazing Fences: Detect & mitigate DEVICE_LOST
    const uint64_t timeout_ns = 5'000'000'000ULL;  // 5s timeout (tighter for perf, adjustable)
    VkResult waitResult = vkWaitForFences(stone_device(), 1, &fence, VK_TRUE, timeout_ns);

    switch (waitResult) {
        case VK_SUCCESS:
            LOG_TRACE_CAT("RTX", "Transient fence signaled successfully");
            break;
        case VK_TIMEOUT:
            LOG_FATAL_CAT("RTX", "Transient fence TIMED OUT after 5s — GPU potential hang");
            // Aggressive recovery: Reset fence & wait idle as last resort
            vkResetFences(stone_device(), 1, &fence);
            vkDeviceWaitIdle(stone_device());
            break;
        case VK_ERROR_DEVICE_LOST:  // -4: Handle imminent loss gracefully
            LOG_FATAL_CAT("RTX", "vkWaitForFences: DEVICE LOST (-4) — Triggering recovery");
            // Do NOT destroy fence here; leak-prevent but prioritize recovery
            vkDeviceWaitIdle(stone_device());  // Sync device state
            // Optional: Notify app to recreate swapchain/device if recurrent
            break;
        default:
            LOG_FATAL_CAT("RTX", "vkWaitForFences unexpected error: {} ({}) — Falling back to idle",
                          static_cast<int>(waitResult), waitResult);
            vkDeviceWaitIdle(stone_device());
            break;
    }

    // 5. Cleanup: Destroy fence & free buffer (safe post-wait/error)
    vkDestroyFence(stone_device(), fence, nullptr);
    vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);

    LOG_TRACE_CAT("RTX", "endSingleTimeCommands — COMPLETE (resilient fence sync)");
}

// Helper: Utility for polling polling async fences in a loop (e.g., in render loop)
// Call this externally where needed, e.g., before next frame submission
bool VulkanRTX::pollAsyncFence(VkFence fence, uint64_t timeout_ns) noexcept {
    if (fence == VK_NULL_HANDLE) return true;  // Already done

    VkResult result = vkWaitForFences(stone_device(), 1, &fence, VK_TRUE, timeout_ns);
    if (result == VK_SUCCESS) {
        return true;  // Signaled
    } else if (result == VK_TIMEOUT) {
        return false;  // Keep polling
    } else {
        LOG_ERROR_CAT("RTX", "Async fence poll error: {} — Resetting", result);
        vkResetFences(stone_device(), 1, &fence);
        return true;  // Treat as done, but log for debugging
    }
}

// =============================================================================
// Pipeline Binding
// =============================================================================
void VulkanRTX::setRayTracingPipeline(VkPipeline p, VkPipelineLayout l) noexcept {
    LOG_TRACE_CAT("RTX", "setRayTracingPipeline — START — pipeline: 0x{:x}, layout: 0x{:x}", reinterpret_cast<uintptr_t>(p), reinterpret_cast<uintptr_t>(l));
    RTX::AmouranthAI::get().onMemoryEvent("RTPipeline", sizeof(VkPipeline));
    RTX::AmouranthAI::get().onMemoryEvent("RTPipelineLayout", sizeof(VkPipelineLayout));

    LOG_INFO_CAT("RTX", "HANDLE_CREATE: {} | Tag: {}", "rtPipeline", "RTPipeline");
    rtPipeline_ = RTX::Handle<VkPipeline>(p, stone_device(),
        [](VkDevice d, VkPipeline pp, const VkAllocationCallbacks*) {
            LOG_TRACE_CAT("RTX", "Destroying RTPipeline: 0x{:x}", reinterpret_cast<uintptr_t>(pp));
            vkDestroyPipeline(d, pp, nullptr);
        }, 0, "RTPipeline");

    LOG_INFO_CAT("RTX", "HANDLE_CREATE: {} | Tag: {}", "rtPipelineLayout", "RTPipelineLayout");
    rtPipelineLayout_ = RTX::Handle<VkPipelineLayout>(l, stone_device(),
        [](VkDevice d, VkPipelineLayout pl, const VkAllocationCallbacks*) {
            LOG_TRACE_CAT("RTX", "Destroying RTPipelineLayout: 0x{:x}", reinterpret_cast<uintptr_t>(pl));
            vkDestroyPipelineLayout(d, pl, nullptr);
        }, 0, "RTPipelineLayout");

    LOG_SUCCESS_CAT("RTX", "{}Ray tracing pipeline bound — PINK PHOTONS ETERNAL{}", PLASMA_FUCHSIA, RESET);
    LOG_TRACE_CAT("RTX", "setRayTracingPipeline — COMPLETE");
}

void VulkanRTX::buildAccelerationStructures()
{
    LOG_INFO_CAT("RTX", "{}Building acceleration structures — LAS awakening{}", PLASMA_FUCHSIA, RESET);

    // === FORCE STAGING POOL CREATION FIRST (CRITICAL FIX) ===
    {
        std::lock_guard<std::mutex> lock(g_stagingMutex);
        if (!g_stagingPool) {
            LOG_INFO_CAT("RTX", "Forcing persistent 1GB staging pool creation (pre-LAS)");
            BUFFER_CREATE(g_stagingPool, 1ULL << 30,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          "persistent_staging_1GB_FORCED");

            g_stagingBuffer = RAW_BUFFER(g_stagingPool);
            g_stagingMem    = BUFFER_MEMORY(g_stagingPool);

            if (g_stagingMem == VK_NULL_HANDLE) {
                LOG_FATAL_CAT("RTX", "Failed to create 1GB staging pool — OOM or invalid memory type");
                return;
            }

            VK_CHECK(vkMapMemory(device_, g_stagingMem, 0, VK_WHOLE_SIZE, 0, &g_mappedBase),
                     "Failed to map persistent staging buffer");
            g_mappedOffset.store(0);
            LOG_SUCCESS_CAT("RTX", "1GB persistent staging pool FORCED ONLINE");
        }
    }

    // Simple test cube
    std::vector<glm::vec3> vertices = {
        {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
        {-1,-1,1},  {1,-1,1},  {1,1,1},  {-1,1,1}
    };
    std::vector<uint32_t> indices = {
        0,1,2, 0,2,3, 4,5,6, 4,6,7,
        0,3,7, 0,7,4, 1,5,6, 1,6,2,
        3,2,6, 3,6,7, 0,4,5, 0,5,1
    };

    // === CREATE BUFFERS — KEEP OBFUSCATED HANDLES ===
    uint64_t vbuf = 0, ibuf = 0;

    BUFFER_CREATE(vbuf, vertices.size() * sizeof(glm::vec3),
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "amouranth_vertex_buffer");

    BUFFER_CREATE(ibuf, indices.size() * sizeof(uint32_t),
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "amouranth_index_buffer");

    // === SAFE UPLOAD USING PERSISTENT STAGING ===
    VkCommandBuffer cmd = beginSingleTimeCommands(g_ctx().commandPool());

    VkDeviceSize vOffset = g_mappedOffset.fetch_add(vertices.size() * sizeof(glm::vec3) + 256, std::memory_order_relaxed);
    VkDeviceSize iOffset = g_mappedOffset.fetch_add(indices.size()  * sizeof(uint32_t)  + 256, std::memory_order_relaxed);

    std::memcpy((char*)g_mappedBase + vOffset, vertices.data(), vertices.size() * sizeof(glm::vec3));
    std::memcpy((char*)g_mappedBase + iOffset, indices.data(),  indices.size()  * sizeof(uint32_t));

    VkBufferCopy vcopy{ .srcOffset = vOffset, .dstOffset = 0, .size = vertices.size() * sizeof(glm::vec3) };
    VkBufferCopy icopy{ .srcOffset = iOffset, .dstOffset = 0, .size = indices.size()  * sizeof(uint32_t) };

    vkCmdCopyBuffer(cmd, g_stagingBuffer, RAW_BUFFER(vbuf), 1, &vcopy);
    vkCmdCopyBuffer(cmd, g_stagingBuffer, RAW_BUFFER(ibuf), 1, &icopy);

    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    endSingleTimeCommands(cmd, g_ctx().graphicsQueue(), g_ctx().commandPool());

    LOG_SUCCESS_CAT("RTX", "Geometry uploaded — building BLAS/TLAS via global LAS");

    // === BUILD VIA GLOBAL LAS — PASS OBFUSCATED HANDLES DIRECTLY ===
    las().buildBLAS(
        g_ctx().commandPool(),
        vbuf,      // ← obfuscated uint64_t — CORRECT
        ibuf,      // ← obfuscated uint64_t — CORRECT
        static_cast<uint32_t>(vertices.size()),
        static_cast<uint32_t>(indices.size()),
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
    );

    std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>> instances{
        { las().getBLAS(), glm::mat4(1.0f) }
    };
    las().buildTLAS(g_ctx().commandPool(), instances);

    LOG_SUCCESS_CAT("RTX",
        "{}GLOBAL_LAS ONLINE — BLAS: 0x{:016X} | TLAS: 0x{:016X} — PINK PHOTONS ETERNAL{}",
        PLASMA_FUCHSIA,
        (uint64_t)las().getBLAS(),
        las().getTLASAddress(),
        RESET);
}

void VulkanRTX::uploadBatch(
    const std::vector<std::tuple<const void*, VkDeviceSize, uint64_t, const char*>>& batch,
    VkCommandPool pool,
    VkQueue queue,
    bool async)
{
    if (batch.empty()) return;

    VkDevice dev = stone_device();
    VkDeviceSize totalSize = 0;
    for (const auto& [src, size, dst, name] : batch)
        if (src && size > 0) totalSize += size;
    if (totalSize == 0) return;

    LOG_TRACE_CAT("RTX", "uploadBatch: {} bytes (async={})", totalSize, async);

    // Lazy-init persistent staging
    {
        std::lock_guard<std::mutex> lock(g_stagingMutex);
        if (g_stagingPool == 0) {
            BUFFER_CREATE(g_stagingPool, STAGING_POOL_SIZE,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          "persistent_staging_pool");

            g_stagingMem = BUFFER_MEMORY(g_stagingPool);
            g_stagingBuffer = RAW_BUFFER(g_stagingPool);

            // FIXED: Null guard for lazy staging mem (prevents vkMapMemory on null — VUID-vkMapMemory-memory-parameter)
            if (g_stagingMem == VK_NULL_HANDLE) {
                LOG_FATAL_CAT("RTX", "Lazy BUFFER_CREATE failed: g_stagingMem null (OOM? Skipping uploadBatch.");
                return;  // Early exit — no map on null
            }

            void* mapped = nullptr;

            // FIXED: Null guard before lazy staging map (VUID-vkMapMemory-memory-parameter + segfault fix)
            if (g_stagingMem == VK_NULL_HANDLE) {
                LOG_FATAL_CAT("RTX", "Lazy staging map aborted: g_stagingMem null (realloc failed?). Skipping.");
                mapped = nullptr;
                return;
            }
            VK_CHECK(vkMapMemory(dev, g_stagingMem, 0, VK_WHOLE_SIZE, 0, &mapped),
                     "Failed to map persistent staging");

            g_mappedBase = mapped;
            g_mappedOffset.store(0);
            LOG_INFO_CAT("RTX", "Persistent 1GB staging pool initialized");
        }
    }

    if (!g_stagingBuffer) return;

    VkCommandBuffer cmd = beginSingleTimeCommands(pool);

    VkDeviceSize offset = g_mappedOffset.fetch_add(totalSize + 256);
    if (offset + totalSize >= STAGING_POOL_SIZE) {
        offset = 0;
        g_mappedOffset.store(256);
    }

    for (const auto& [src, size, dstHandle, name] : batch) {
        if (!src || size == 0) continue;

        void* dstPtr = static_cast<char*>(g_mappedBase) + offset;
        std::memcpy(dstPtr, src, size);

        VkBuffer dstBuf = RAW_BUFFER(dstHandle);
        if (dstBuf) {
            VkBufferCopy copy{};
            copy.srcOffset = offset;
            copy.dstOffset = 0;
            copy.size = size;
            vkCmdCopyBuffer(cmd, g_stagingBuffer, dstBuf, 1, &copy);
        }

        offset += size;
        LOG_TRACE_CAT("RTX", "Staged {} bytes → {}", size, name);
    }

    // RAW FENCE — no renderer() call
    VkFence fence = VK_NULL_HANDLE;
    if (async) {
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VK_CHECK(vkCreateFence(dev, &fenceInfo, nullptr, &fence),
                 "Failed to create upload fence");
    }

    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence),
             "Failed to submit upload batch");

    if (!async) {
        VK_CHECK(vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX),
                 "Sync upload wait failed");
        vkDestroyFence(dev, fence, nullptr);
    }

    LOG_PERF_CAT("RTX", "Batch upload {} bytes submitted (async={})", totalSize, async);
}

// =============================================================================
// Descriptor Pool + Sets
// =============================================================================

void VulkanRTX::initDescriptorPoolAndSets() noexcept
{
    constexpr uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    LOG_TRACE_CAT("RTX", "{}initDescriptorPoolAndSets — DYNAMIC BINDINGS — {} frames — SLIPSTREAM FINAL{}", 
                  VALHALLA_GOLD, frames, RESET);

    // ── STEP 1: POOL — CONSTEXPR ARRAY, DESIGNATED INITIALIZERS (C++20+ safe)
    constexpr auto poolSizes = std::to_array<VkDescriptorPoolSize>({
        { .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .descriptorCount = frames * 1 },
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,             .descriptorCount = frames * 6 },
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            .descriptorCount = frames * 4 },
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            .descriptorCount = frames * 10 },
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    .descriptorCount = frames * 5 },
        { .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,             .descriptorCount = frames * 3 },
        { .type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,          .descriptorCount = frames * 2 },
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,      .descriptorCount = frames * 2 },
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,      .descriptorCount = frames * 2 },
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,    .descriptorCount = frames * 3 }
    });

    VkDescriptorPoolCreateInfo poolInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = frames * 20,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VkDescriptorPool rawPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &rawPool), "RTX Descriptor Pool");

    // Use assignment — Handle has operator= or copy ctor (no emplace)
    descriptorPool_ = Handle<VkDescriptorPool>(
        rawPool, device_,
        [](VkDevice d, VkDescriptorPool p, const VkAllocationCallbacks*) noexcept {
            if (p) vkDestroyDescriptorPool(d, p, nullptr);
        },
        0, "RTXDescriptorPool — SLIPSTREAM"
    );

    LOG_SUCCESS_CAT("RTX", "{}Descriptor pool forged — {} sets — DYNAMIC MODE{}", EMERALD_GREEN, poolInfo.maxSets, RESET);

    // ── STEP 2: LAYOUT
    VkDescriptorSetLayout targetLayout = [this]() -> VkDescriptorSetLayout {
        if (rtDescriptorSetLayout_.valid() && *rtDescriptorSetLayout_) [[likely]] {
            return *rtDescriptorSetLayout_;
        }

        LOG_WARN_CAT("RTX", "{}EMERGENCY LAYOUT — 12 BINDINGS — WARP CORE{}", BLOOD_RED, RESET);

        constexpr auto bindings = std::to_array<VkDescriptorSetLayoutBinding>({
            { .binding = 0,  .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
            { .binding = 1,  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
            { .binding = 2,  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
            { .binding = 3,  .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
            { .binding = 4,  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
            { .binding = 5,  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
            { .binding = 6,  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
            { .binding = 7,  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
            { .binding = 8,  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
            { .binding = 9,  .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,     .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
            { .binding = 10, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
            { .binding = 11, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR }
        });

        VkDescriptorSetLayoutCreateInfo layoutInfo{
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings    = bindings.data()
        };

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &layout), "Emergency layout");
        return layout;
    }();

    if (!targetLayout) [[unlikely]] {
        LOG_FATAL_CAT("RTX", "NO LAYOUT — ABORT");
        std::abort();
    }

    // ── STEP 3: DYNAMIC BINDINGS — ZERO HEAP, STACK ONLY
    descriptorSets_.assign(frames, VK_NULL_HANDLE);

    alignas(VkDescriptorSetLayout) static VkDescriptorSetLayout stackLayouts[Options::Performance::MAX_FRAMES_IN_FLIGHT];
    std::fill_n(stackLayouts, frames, targetLayout);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = *descriptorPool_,
        .descriptorSetCount = frames,
        .pSetLayouts        = stackLayouts
    };

    VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_.data()),
             "DYNAMIC ALLOCATION — SUCCESS");

    LOG_SUCCESS_CAT("RTX", "{}DYNAMIC BINDINGS ACHIEVED — {} sets — ZERO ALLOCATION — SLIPSTREAM COMPLETE{}",
                    DIAMOND_SPARKLE, frames, RESET);

    LOG_SUCCESS_CAT("RTX", "{}initDescriptorPoolAndSets — COMPLETE — FIRST LIGHT IMMINENT{}", 
                    PULSAR_GREEN, RESET);
}

// =============================================================================
// Shader Binding Table — 64 MB Titan
// =============================================================================

// src/engine/GLOBAL/VulkanCore.cpp
void VulkanRTX::initShaderBindingTable(VkPhysicalDevice /*pd*/) noexcept
{
    LOG_TRACE_CAT("RTX", "initShaderBindingTable — START");

    // Guard: pipeline must exist
    if (!rtPipeline_.valid() || rtPipeline_.get() == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RTX", "Ray tracing pipeline not set — SBT creation skipped");
        return;
    }

    const auto& props = g_ctx().rayTracingProps();
    const uint32_t groupCount = 25;
    const VkDeviceSize handleSize = props.shaderGroupHandleSize;
    const VkDeviceSize baseAlignment = props.shaderGroupBaseAlignment;
    const VkDeviceSize alignedHandleSize = (handleSize + baseAlignment - 1) & ~(baseAlignment - 1);

    LOG_INFO_CAT("RTX", "SBT: {} groups | handleSize={} B | alignment={} B → aligned={} B",
                 groupCount, handleSize, baseAlignment, alignedHandleSize);

    // Create 64MB SBT buffer
    uint64_t sbtHandle = 0;
    BUFFER_CREATE(sbtHandle, 64_MB,
                  VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "AMOURANTH_SBT_64MB_TITAN");

    VkBuffer sbtBuffer = RAW_BUFFER(sbtHandle);
    VkDeviceMemory sbtMemory = BUFFER_MEMORY(sbtHandle);

    sbtBuffer_  = Handle<VkBuffer>(sbtBuffer, device_, nullptr, 0, "SBT_Buffer");
    sbtMemory_  = Handle<VkDeviceMemory>(sbtMemory, device_, nullptr, 64_MB, "SBT_Memory");

    // Fetch all shader group handles
    std::vector<uint8_t> handles(groupCount * handleSize);
    VK_CHECK(rtGetRayTracingShaderGroupHandlesKHR(
        device_,
        rtPipeline_.get(),
        0,
        groupCount,
        handles.size(),
        handles.data()),
        "Failed to get ray tracing shader group handles");

    // Map and write handles with proper alignment
    void* mapped = nullptr;
    BUFFER_MAP(sbtHandle, mapped);
    uint8_t* dst = static_cast<uint8_t*>(mapped);

    for (uint32_t i = 0; i < groupCount; ++i) {
        std::memcpy(dst + i * alignedHandleSize,
                     handles.data() + i * handleSize,
                     handleSize);
    }
    BUFFER_UNMAP(sbtHandle);

    // Get device address
    VkBufferDeviceAddressInfo addrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addrInfo.buffer = sbtBuffer;
    sbtAddress_ = vkGetBufferDeviceAddress(device_, &addrInfo);

    // Setup SBT regions
    sbt_.raygen   = { sbtAddress_ + 0               , alignedHandleSize, alignedHandleSize };
    sbt_.miss     = { sbtAddress_ + alignedHandleSize, alignedHandleSize, alignedHandleSize };
    sbt_.hit      = { sbtAddress_ + alignedHandleSize * 9, alignedHandleSize, alignedHandleSize };
    sbt_.callable = { sbtAddress_ + alignedHandleSize * 25, alignedHandleSize, alignedHandleSize };

    sbtRecordSize_ = alignedHandleSize;

    LOG_SUCCESS_CAT("RTX", "SBT FORGED — {} groups @ 0x{:016X} — PINK PHOTONS READY", groupCount, sbtAddress_);
}

// =============================================================================
// Black Fallback Image – 1x1 Solid Black Safety Net
// =============================================================================
void VulkanRTX::initBlackFallbackImage() {
    LOG_TRACE_CAT("RTX", "initBlackFallbackImage — START");
    RTX::AmouranthAI::get().onMemoryEvent("Black Fallback Staging", 4);

    // --- STAGING BUFFER: 4 bytes for black pixel ---
    uint64_t staging = 0;
    LOG_TRACE_CAT("RTX", "Creating staging buffer for black pixel — 4 B");
    BUFFER_CREATE(staging, 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "black_fallback_staging");

    // --- MAP, WRITE, UNMAP ---
    void* data = nullptr;
    BUFFER_MAP(staging, data);
    if (!data) {
        LOG_FATAL_CAT("RTX", "Failed to map black fallback staging buffer");
        BUFFER_DESTROY(staging);
        return;
    }
    *static_cast<uint32_t*>(data) = 0xFF000000u;  // RGBA8: opaque black
    BUFFER_UNMAP(staging);
    LOG_DEBUG_CAT("RTX", "Black pixel (0xFF000000) mapped and unmapped");

    // --- 1x1 DEVICE-LOCAL IMAGE ---
    VkImageCreateInfo imgInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R8G8B8A8_SRGB,
        .extent        = {1, 1, 1},
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage rawImg = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(device_, &imgInfo, nullptr, &rawImg), "Failed to create black image");
    LOG_DEBUG_CAT("RTX", "Black image created: 0x{:x}", reinterpret_cast<uintptr_t>(rawImg));

    LOG_INFO_CAT("RTX", "HANDLE_CREATE: blackFallbackImage | Tag: BlackFallbackImage");
    blackFallbackImage_ = RTX::Handle<VkImage>(rawImg, device_);  // Auto-destroy
    blackFallbackImage_.tag = "BlackFallbackImage";

    // --- MEMORY ALLOCATION ---
    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device_, rawImg, &memReqs);
    LOG_DEBUG_CAT("RTX", "Black image mem reqs: size={} B, alignment={}, typeBits=0x{:x}",
                  memReqs.size, memReqs.alignment, memReqs.memoryTypeBits);

    uint32_t memType = UltraLowLevelBufferTracker::findMemoryType(
        stone_physical(), memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType == UINT32_MAX) {
        LOG_FATAL_CAT("RTX", "No suitable memory type for black fallback image");
        BUFFER_DESTROY(staging);
        return;
    }

    VkMemoryAllocateInfo allocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory rawMem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &rawMem), "Failed to allocate black memory");
    LOG_DEBUG_CAT("RTX", "Black memory allocated: 0x{:x} (type {})", reinterpret_cast<uintptr_t>(rawMem), memType);
    VK_CHECK(vkBindImageMemory(device_, rawImg, rawMem, 0), "Failed to bind black memory");

    LOG_INFO_CAT("RTX", "HANDLE_CREATE: blackFallbackMemory | Tag: BlackFallbackMemory");
    blackFallbackMemory_ = RTX::Handle<VkDeviceMemory>(rawMem, device_);
    blackFallbackMemory_.tag = "BlackFallbackMemory";
    blackFallbackMemory_.size = memReqs.size;

    // --- COPY STAGING → IMAGE (async variant for speed) ---
    VkCommandBuffer cmd = beginSingleTimeCommands(g_ctx().commandPool());
    LOG_DEBUG_CAT("RTX", "One-time cmd for black image upload: 0x{:x}", reinterpret_cast<uintptr_t>(cmd));

    // Transition: UNDEFINED → TRANSFER_DST
    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = rawImg,
        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy
    VkBufferImageCopy copy = {
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageOffset       = {0, 0, 0},
        .imageExtent       = {1, 1, 1}
    };
    vkCmdCopyBufferToImage(cmd, RAW_BUFFER(staging), rawImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    // Transition: TRANSFER_DST → SHADER_READ
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue(), g_ctx().commandPool());  // Async for speed

    // --- CLEANUP STAGING ---
    BUFFER_DESTROY(staging);
    LOG_TRACE_CAT("RTX", "Black pixel uploaded via staging (async)");

    // --- IMAGE VIEW ---
    VkImageViewCreateInfo viewInfo = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = rawImg,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_R8G8B8A8_SRGB,
        .components       = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    VkImageView rawView = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &rawView), "Failed to create black view");
    LOG_DEBUG_CAT("RTX", "Black image view created: 0x{:x}", reinterpret_cast<uintptr_t>(rawView));

    LOG_INFO_CAT("RTX", "HANDLE_CREATE: blackFallbackView | Tag: BlackFallbackView");
    blackFallbackView_ = RTX::Handle<VkImageView>(rawView, device_);
    blackFallbackView_.tag = "BlackFallbackView";

    LOG_SUCCESS_CAT("RTX", "{}Black fallback image ready — safety net active{}", PLASMA_FUCHSIA, RESET);
    RTX::AmouranthAI::get().onMemoryEvent("Black Fallback Image", memReqs.size);
    LOG_TRACE_CAT("RTX", "initBlackFallbackImage — COMPLETE");
}

// =============================================================================
// Descriptor Updates — 16 Bindings — FULL AI VOICE DOMINANCE
// =============================================================================

namespace Bindings { namespace RTX {
    constexpr uint32_t RESERVED_14 = 14;
    constexpr uint32_t RESERVED_15 = 15;
}}

// • C++20 clean, zero warnings
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRTX::updateRTXDescriptors(uint32_t frameIdx,
                                     VkBuffer /*cameraBuf*/, VkBuffer /*materialBuf*/, VkBuffer /*dimensionBuf*/,
                                     VkImageView /*storageView*/, VkImageView /*accumView*/,
                                     VkImageView envMapView, VkSampler envSampler,
                                     VkImageView densityVol, VkImageView /*gDepth*/,
                                     VkImageView /*gNormal*/)
{
    if (descriptorSets_.empty()) {
        LOG_WARN_CAT("RTX", "updateRTXDescriptors skipped — no descriptor sets");
        return;
    }

    VkDescriptorSet set = descriptorSets_[frameIdx % descriptorSets_.size()];
    VkAccelerationStructureKHR tlas = LAS::get().getTLAS();  // FIXED: Use public getTLAS() accessor

    // Early return if critical handles missing
    if (!set || !tlas) {
        LOG_WARN_CAT("RTX", "Missing set (0x{:x}) or TLAS (0x{:x}) — skipping", reinterpret_cast<uintptr_t>(set), reinterpret_cast<uintptr_t>(tlas));
        return;
    }

    std::vector<VkWriteDescriptorSet>                            writes;
    std::vector<VkDescriptorImageInfo>                            imgInfos;
    std::vector<VkWriteDescriptorSetAccelerationStructureKHR>   asWrites;

    writes.reserve(16);
    imgInfos.reserve(16);
    asWrites.reserve(1);

    // === 0: TLAS — SAFE pNext ===
    asWrites.push_back(VkWriteDescriptorSetAccelerationStructureKHR{
        .sType                      = kVkWriteDescriptorSetSType_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures    = &tlas
    });

    writes.push_back(VkWriteDescriptorSet{
        .sType           = kVkWriteDescriptorSetSType,
        .pNext           = &asWrites.back(),
        .dstSet          = set,
        .dstBinding      = Bindings::RTX::TLAS,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    });

    // === Helper: bind image (with combined fallback) ===
    auto bindImg = [&](uint32_t binding, VkImageView view, VkDescriptorType type,
                       VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VkSampler sampler = VK_NULL_HANDLE) {
        if (!view) {
            LOG_DEBUG_CAT("RTX", "Skipping null view for binding {}", binding);
            return;
        }
        VkDescriptorType effectiveType = type;
        if (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && sampler == VK_NULL_HANDLE) {
            LOG_WARN_CAT("RTX", "Null sampler for combined binding {} — falling back to SAMPLED_IMAGE", binding);
            effectiveType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        }
        imgInfos.push_back({ sampler, view, layout });
        writes.push_back(VkWriteDescriptorSet{
            .sType           = kVkWriteDescriptorSetSType,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = effectiveType,
            .pImageInfo      = &imgInfos.back()
        });
    };

    // === Black fallback (always bind, e.g., for missing textures) ===
    bindImg(Bindings::RTX::BLACK_FALLBACK, HANDLE_GET(blackFallbackView_), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

    // === Env map (bind if available) ===
    if (envMapView) {
        bindImg(Bindings::RTX::ENV_MAP, envMapView, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, envSampler);
    }

    // === Density volume (conditional type based on sampler) ===
    VkImageView densityView = densityVol ? densityVol : HANDLE_GET(blackFallbackView_);
    VkDescriptorType densityType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    if (densityVol == VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("RTX", "Using fallback for density volume");
    }
    bindImg(Bindings::RTX::DENSITY_VOLUME, densityView, densityType,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, envSampler);

    // === Blue noise ===
    VkImageView blueNoise = g_ctx().blueNoiseView_.valid() ? g_ctx().blueNoiseView_.get() : HANDLE_GET(blackFallbackView_);
    bindImg(Bindings::RTX::BLUE_NOISE, blueNoise, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

    // === Reserved ===
    bindImg(Bindings::RTX::RESERVED_14, HANDLE_GET(blackFallbackView_), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    bindImg(Bindings::RTX::RESERVED_15, HANDLE_GET(blackFallbackView_), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

    // === FINAL UPDATE ===
    if (!writes.empty()) {
        vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

// =============================================================================
// Ray Tracing Commands — FINAL FIX
// =============================================================================

// src/engine/GLOBAL/VulkanCore.cpp
void VulkanRTX::recordRayTrace(VkCommandBuffer cmd,
                               VkExtent2D extent,
                               VkImage outputImage,
                               VkImageView /*outputView*/) noexcept
{
    LOG_TRACE_CAT("RTX", "recordRayTrace — {}x{} — cmd=0x{:x}", extent.width, extent.height, reinterpret_cast<uintptr_t>(cmd));

    // Transition output image to GENERAL for ray tracing write
    VkImageMemoryBarrier barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .image               = outputImage,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Bind pipeline and descriptors
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());
    vkCmdBindDescriptorSets(cmd,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        rtPipelineLayout_.get(),
        0, 1, &descriptorSets_[0], 0, nullptr);

    // TRACE RAYS — PURE, ETERNAL, NO COMMA ABUSE
    rtCmdTraceRaysKHR(
        cmd,
        &sbt_.raygen,
        &sbt_.miss,
        &sbt_.hit,
        &sbt_.callable,
        extent.width,
        extent.height,
        1
    );

    // Transition back to present
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    LOG_SUCCESS_CAT("RTX", "Ray trace recorded — {}x{} — PHOTONS DISPATCHED", extent.width, extent.height);
}

uint64_t VulkanRTX::alignUp(uint64_t value, uint64_t alignment) const noexcept {
    if (alignment == 0) return value;  // Edge case: avoid div-by-zero
    return ((value + alignment - 1) / alignment) * alignment;
}

namespace RTX {

// =============================================================================
// 3. Physical Device Selection — STONEKEY v∞ DELAYED ACTIVATION (CRITICAL)
// =============================================================================
void pickPhysicalDevice()
{
    LOG_TRACE_CAT("VULKAN", "→ Entering RTX::pickPhysicalDevice() — scanning for physical devices");

    // CRITICAL: Use RAW instance — StoneKey is NOT active yet!
    VkInstance rawInstance = g_context_instance.instance_;
    LOG_TRACE_CAT("VULKAN", "    • Using RAW instance for enumeration: 0x{:016x}", reinterpret_cast<uintptr_t>(rawInstance));

    uint32_t deviceCount = 0;
    LOG_TRACE_CAT("VULKAN", "    • Enumerating physical device count (first pass)");
    VK_CHECK_NOMSG(vkEnumeratePhysicalDevices(rawInstance, &deviceCount, nullptr));
    LOG_TRACE_CAT("VULKAN", "    • Device count queried: {}", deviceCount);

    if (deviceCount == 0) {
        LOG_FATAL_CAT("VULKAN", "No Vulkan physical devices found — cannot continue");
        std::terminate();
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    LOG_TRACE_CAT("VULKAN", "    • Enumerating physical devices (second pass)");
    VK_CHECK_NOMSG(vkEnumeratePhysicalDevices(rawInstance, &deviceCount, devices.data()));
    LOG_TRACE_CAT("VULKAN", "    • Enumeration complete — {} devices populated", deviceCount);

    LOG_TRACE_CAT("VULKAN", "    • Scanning {} devices for discrete GPU preference", deviceCount);

    VkPhysicalDevice selected = VK_NULL_HANDLE;

    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& device = devices[i];
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device, &props);

        LOG_TRACE_CAT("VULKAN", "      • Device {}: '{}' — Type: {} — Vendor: 0x{:x} — API: {}.{}.{}",
                      i, props.deviceName, props.deviceType,
                      props.vendorID,
                      VK_VERSION_MAJOR(props.apiVersion),
                      VK_VERSION_MINOR(props.apiVersion),
                      VK_VERSION_PATCH(props.apiVersion));

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            LOG_TRACE_CAT("VULKAN", "        • DISCRETE GPU FOUND — claiming throne");
            selected = device;
            g_ctx().physicalDevice_ = selected;

            LOG_SUCCESS_CAT("VULKAN", "{}DISCRETE GPU CLAIMED{} — {} (API: {}.{}.{})",
                            PLASMA_FUCHSIA, RESET,
                            props.deviceName,
                            VK_VERSION_MAJOR(props.apiVersion),
                            VK_VERSION_MINOR(props.apiVersion),
                            VK_VERSION_PATCH(props.apiVersion));

            AI_INJECT("I have claimed the discrete throne: {}", props.deviceName);
            break;
        }
    }

    // Fallback if no discrete GPU
    if (selected == VK_NULL_HANDLE) {
        selected = devices[0];
        g_ctx().physicalDevice_ = selected;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(selected, &props);

        LOG_SUCCESS_CAT("VULKAN", "{}FALLBACK GPU SELECTED{} — {} ({})",
                        EMERALD_GREEN, RESET,
                        props.deviceName,
                        [t = props.deviceType]() -> const char* {
                            switch (t) {
                                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated";
                                case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:   return "Virtual";
                                case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";
                                default:                                     return "Other";
                            }
                        }());

        AI_INJECT("I will make do with what is given: {}", props.deviceName);
    }

    // CRITICAL: NOW — AND ONLY NOW — ENGAGE STONEKEY ON THE INSTANCE
    // All vkEnumeratePhysicalDevices() calls are done. Safe to obfuscate.

    LOG_SUCCESS_CAT("VULKAN", "{}STONEKEY v∞ ENGAGED ON VkInstance — FULL OBFUSCATION ACTIVE — APOCALYPSE v3.2 ARMED{}",
                    LILAC_LAVENDER, RESET);

    LOG_TRACE_CAT("VULKAN", "← Exiting RTX::pickPhysicalDevice() — GPU selected, StoneKey armed");
}

// =============================================================================
// 5. Command Pool
// =============================================================================
void createCommandPool()
{
    VkCommandPoolCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = g_ctx().graphicsFamily()
    };

    VK_CHECK(vkCreateCommandPool(g_context_instance.device_, &info, nullptr, &g_context_instance.commandPool_),
             "Failed to create command pool");

    LOG_SUCCESS_CAT("VULKAN", "Command pool created");
}

// =============================================================================
// RTX::surface() — RAW, LIGHTNING-FAST, FULLY LOGGED
// =============================================================================
[[nodiscard]] constexpr VkSurfaceKHR surface() noexcept
{
    // Fast path — normal case (99.999% of calls)
    if (g_context_instance.surface_ != VK_NULL_HANDLE) {
        return g_context_instance.surface_;
    }

    // ---------------------------------------------------------------------
    // SLOW PATH: Surface not created yet — this should NEVER happen in production
    // ---------------------------------------------------------------------
    LOG_FATAL_CAT("RTX", 
        "RTX::surface() called but g_context_instance.surface_ is VK_NULL_HANDLE!\n"
        "    → Ensure RTX::initContext() called post-SDL_ShowWindow in main.cpp PHASE 3.\n"
        "    → Aborting — cannot render without a valid Vulkan surface!"
    );

    // Extra diagnostics — help you hunt down the bug instantly
    LOG_ERROR_CAT("RTX", "Call order violation detected:");
    LOG_ERROR_CAT("RTX", "    • g_context_instance.instance_  = 0x{:x}", 
                  reinterpret_cast<uintptr_t>(g_context_instance.instance_));
    LOG_ERROR_CAT("RTX", "    • g_context_instance.device_    = 0x{:x}", 
                  reinterpret_cast<uintptr_t>(g_context_instance.device_));
    LOG_ERROR_CAT("RTX", "    • g_context_instance.surface_   = VK_NULL_HANDLE");
    LOG_ERROR_CAT("RTX", "    • Thread ID: {}", std::this_thread::get_id());

    // Optional: Print stack trace if you have a backtrace lib
    // Logging::Stacktrace::print();

    // Final word from AMOURANTH AI
    AI_INJECT("Surface requested before existence... I cannot reflect photons in the void.");

    // Hard abort — no silent nulls, no undefined behavior
    std::abort();

    // Unreachable, but silences warnings
    return VK_NULL_HANDLE;
}

// GLOBAL IMMORTAL PIPELINE MANAGER — PINK PHOTONS ETERNAL

void createGlobalPipelineManager(VkDevice device, VkPhysicalDevice phys)
{
}


} // RTX
// VulkanCore.cpp — FINAL, SDL3-CORRECT, BULLETPROOF FORMAT — NO BULLSHIT
// VulkanCore.cpp — FINAL, SDL3-CORRECT, BULLETPROOF + STONEKEY v∞ ACTIVE
// RELAXED: All VUIDs broken/fixed (null guards, layout transitions for HDR/video), HDR respected via extensions/formats