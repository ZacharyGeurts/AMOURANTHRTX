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
#include "engine/Vulkan/VkSafeSTypes.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/GlobalBindings.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"  // StoneKey: The One True Global Authority

#include <vulkan/vulkan.h>
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
using namespace RTX;

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

static std::unique_ptr<RTX::PipelineManager> g_pipelineManager = nullptr;

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
                  reinterpret_cast<uintptr_t>(::g_PhysicalDevice()),  // StoneKey secured
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

VulkanRTX::VulkanRTX(int w, int h, RTX::PipelineManager* mgr)
    : extent_({static_cast<uint32_t>(w), static_cast<uint32_t>(h)})
    , pipelineMgr_(mgr)
{
    LOG_TRACE_CAT("RTX", "VulkanRTX constructor — {}×{} — [LINE {}]", w, h, __LINE__);
    LOG_DEBUG_CAT("RTX", "Constructor params: width={}, height={}, pipelineMgr={}", w, h, mgr ? "valid" : "null");
    RTX::AmouranthAI::get().onMemoryEvent("VulkanRTX Instance", sizeof(VulkanRTX));
    RTX::AmouranthAI::get().onPhotonDispatch(w, h);

    device_ = g_device();
    LOG_DEBUG_CAT("RTX", "RTX::g_ctx().device() returned: 0x{:x}", reinterpret_cast<uintptr_t>(device_));
    if (!device_) {
        LOG_ERROR_CAT("RTX", "device_ is null from RTX::g_ctx().device() — THROWING runtime_error!");
        throw std::runtime_error("Invalid Vulkan device");
    }
    LOG_SUCCESS_CAT("RTX", "Device fetched successfully: 0x{:x}", reinterpret_cast<uintptr_t>(device_));

    // -----------------------------------------------------------------
    // LOAD ALL RT FUNCTION POINTERS FROM RTX::g_ctx()
    // -----------------------------------------------------------------
    LOG_TRACE_CAT("RTX", "Loading RT function pointers from RTX::g_ctx()");
    vkGetBufferDeviceAddressKHR              = RTX::g_ctx().vkGetBufferDeviceAddressKHR();
    vkCmdTraceRaysKHR                        = RTX::g_ctx().vkCmdTraceRaysKHR();
    vkGetRayTracingShaderGroupHandlesKHR     = RTX::g_ctx().vkGetRayTracingShaderGroupHandlesKHR();
    vkGetAccelerationStructureDeviceAddressKHR = RTX::g_ctx().vkGetAccelerationStructureDeviceAddressKHR();

    LOG_INFO_CAT("RTX", "Function pointers loaded — vkCmdTraceRaysKHR @ 0x{:x} | vkGetBufferDeviceAddressKHR @ 0x{:x} | vkGetRayTracingShaderGroupHandlesKHR @ 0x{:x} | vkGetAccelerationStructureDeviceAddressKHR @ 0x{:x}",
                 reinterpret_cast<uintptr_t>(vkCmdTraceRaysKHR),
                 reinterpret_cast<uintptr_t>(vkGetBufferDeviceAddressKHR),
                 reinterpret_cast<uintptr_t>(vkGetRayTracingShaderGroupHandlesKHR),
                 reinterpret_cast<uintptr_t>(vkGetAccelerationStructureDeviceAddressKHR));

    LOG_SUCCESS_CAT("RTX",
        "{}AMOURANTH RTX CORE v80 TURBO — {}×{} — g_rtx() ONLINE — STONEKEY v∞ ACTIVE{}",
        PLASMA_FUCHSIA, w, h, RESET);

    LOG_TRACE_CAT("RTX", "Calling buildAccelerationStructures()");
    buildAccelerationStructures();
    LOG_TRACE_CAT("RTX", "buildAccelerationStructures() returned");

    LOG_TRACE_CAT("RTX", "Calling initBlackFallbackImage()");
    initBlackFallbackImage();
    LOG_TRACE_CAT("RTX", "initBlackFallbackImage() returned");

    LOG_SUCCESS_CAT("RTX", "{}VulkanRTX initialization complete — TITAN DOMINANCE ETERNAL{}", PLASMA_FUCHSIA, RESET);
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
    VK_CHECK(vkAllocateCommandBuffers(RTX::g_ctx().device(), &allocInfo, &cmd),
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
    VK_CHECK(vkCreateFence(RTX::g_ctx().device(), &fenceInfo, nullptr, &fence),
             "Failed to create transient fence");

    // 3. Submit with fence
    VkSubmitInfo submit{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence),
             "Failed to submit transient command buffer");

    // 4. Wait with timeout & error resilience — Amazing Fences: Detect & mitigate DEVICE_LOST
    const uint64_t timeout_ns = 5'000'000'000ULL;  // 5s timeout (tighter for perf, adjustable)
    VkResult waitResult = vkWaitForFences(RTX::g_ctx().device(), 1, &fence, VK_TRUE, timeout_ns);

    switch (waitResult) {
        case VK_SUCCESS:
            LOG_TRACE_CAT("RTX", "Transient fence signaled successfully");
            break;
        case VK_TIMEOUT:
            LOG_FATAL_CAT("RTX", "Transient fence TIMED OUT after 5s — GPU potential hang");
            // Aggressive recovery: Reset fence & wait idle as last resort
            vkResetFences(RTX::g_ctx().device(), 1, &fence);
            vkDeviceWaitIdle(RTX::g_ctx().device());
            break;
        case VK_ERROR_DEVICE_LOST:  // -4: Handle imminent loss gracefully
            LOG_FATAL_CAT("RTX", "vkWaitForFences: DEVICE LOST (-4) — Triggering recovery");
            // Do NOT destroy fence here; leak-prevent but prioritize recovery
            vkDeviceWaitIdle(RTX::g_ctx().device());  // Sync device state
            // Optional: Notify app to recreate swapchain/device if recurrent
            break;
        default:
            LOG_FATAL_CAT("RTX", "vkWaitForFences unexpected error: {} ({}) — Falling back to idle",
                          static_cast<int>(waitResult), waitResult);
            vkDeviceWaitIdle(RTX::g_ctx().device());
            break;
    }

    // 5. Cleanup: Destroy fence & free buffer (safe post-wait/error)
    vkDestroyFence(RTX::g_ctx().device(), fence, nullptr);
    vkFreeCommandBuffers(RTX::g_ctx().device(), pool, 1, &cmd);

    LOG_TRACE_CAT("RTX", "endSingleTimeCommands — COMPLETE (resilient fence sync)");
}

// Helper: Utility for polling polling async fences in a loop (e.g., in render loop)
// Call this externally where needed, e.g., before next frame submission
bool VulkanRTX::pollAsyncFence(VkFence fence, uint64_t timeout_ns) noexcept {
    if (fence == VK_NULL_HANDLE) return true;  // Already done

    VkResult result = vkWaitForFences(RTX::g_ctx().device(), 1, &fence, VK_TRUE, timeout_ns);
    if (result == VK_SUCCESS) {
        return true;  // Signaled
    } else if (result == VK_TIMEOUT) {
        return false;  // Keep polling
    } else {
        LOG_ERROR_CAT("RTX", "Async fence poll error: {} — Resetting", result);
        vkResetFences(RTX::g_ctx().device(), 1, &fence);
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
    rtPipeline_ = RTX::Handle<VkPipeline>(p, RTX::g_ctx().device(),
        [](VkDevice d, VkPipeline pp, const VkAllocationCallbacks*) {
            LOG_TRACE_CAT("RTX", "Destroying RTPipeline: 0x{:x}", reinterpret_cast<uintptr_t>(pp));
            vkDestroyPipeline(d, pp, nullptr);
        }, 0, "RTPipeline");

    LOG_INFO_CAT("RTX", "HANDLE_CREATE: {} | Tag: {}", "rtPipelineLayout", "RTPipelineLayout");
    rtPipelineLayout_ = RTX::Handle<VkPipelineLayout>(l, RTX::g_ctx().device(),
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
    VkCommandBuffer cmd = beginSingleTimeCommands(RTX::g_ctx().commandPool());

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

    endSingleTimeCommands(cmd, RTX::g_ctx().graphicsQueue(), RTX::g_ctx().commandPool());

    LOG_SUCCESS_CAT("RTX", "Geometry uploaded — building BLAS/TLAS via global LAS");

    // === BUILD VIA GLOBAL LAS — PASS OBFUSCATED HANDLES DIRECTLY ===
    las().buildBLAS(
        RTX::g_ctx().commandPool(),
        vbuf,      // ← obfuscated uint64_t — CORRECT
        ibuf,      // ← obfuscated uint64_t — CORRECT
        static_cast<uint32_t>(vertices.size()),
        static_cast<uint32_t>(indices.size()),
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
    );

    std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>> instances{
        { las().getBLAS(), glm::mat4(1.0f) }
    };
    las().buildTLAS(RTX::g_ctx().commandPool(), instances);

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

    VkDevice dev = RTX::g_ctx().device();
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

void VulkanRTX::initDescriptorPoolAndSets() {
    LOG_TRACE_CAT("RTX", "initDescriptorPoolAndSets — START — {} frames", MAX_FRAMES_IN_FLIGHT);

    // Step 1: Define pool sizes - ensure only supported types are used and counts are safe
    std::array<VkDescriptorPoolSize, 10> poolSizes{};  // Zero-init for safety
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 3)};
    poolSizes[2] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};
    poolSizes[3] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 4)};
    poolSizes[4] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2)};
    poolSizes[5] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2)};
    poolSizes[6] = {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};
    poolSizes[7] = {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};
    poolSizes[8] = {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};
    poolSizes[9] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};
    LOG_DEBUG_CAT("RTX", "Descriptor pool sizes configured — 10 types for {} sets", MAX_FRAMES_IN_FLIGHT * 8);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;  // Bulletproof: Allow free
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 8);
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool rawPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &rawPool), "Failed to create descriptor pool");
    LOG_DEBUG_CAT("RTX", "Raw descriptor pool created: 0x{:x}", reinterpret_cast<uintptr_t>(rawPool));
    LOG_INFO_CAT("RTX", "HANDLE_CREATE: {} | Tag: {}", "descriptorPool", "RTXDescriptorPool");
    descriptorPool_ = RTX::Handle<VkDescriptorPool>(rawPool, device_,
        [](VkDevice d, VkDescriptorPool p, const VkAllocationCallbacks*) {
            LOG_TRACE_CAT("RTX", "Destroying descriptor pool: 0x{:x}", reinterpret_cast<uintptr_t>(p));
            if (p != VK_NULL_HANDLE) vkDestroyDescriptorPool(d, p, nullptr);
        }, 0, "RTXDescriptorPool");
    RTX::AmouranthAI::get().onMemoryEvent("Descriptor Pool", 0);

    // Step 2: Create or validate descriptor set layouts (CRITICAL: Ensure non-null!)
    // If rtDescriptorSetLayout_ is invalid/null, create a fallback or throw
    VkDescriptorSetLayout fallbackLayout = VK_NULL_HANDLE;
    if (!rtDescriptorSetLayout_.valid()) {
        LOG_WARN_CAT("RTX", "rtDescriptorSetLayout invalid — creating fallback RT layout");

        // Fallback RT layout: Minimal for AS + storage image
        VkDescriptorSetLayoutBinding rtBindings[] = {
            {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr}
        };
        VkDescriptorSetLayoutCreateInfo rtLayoutInfo{};
        rtLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        rtLayoutInfo.bindingCount = 2;
        rtLayoutInfo.pBindings = rtBindings;
        VK_CHECK(vkCreateDescriptorSetLayout(device_, &rtLayoutInfo, nullptr, &fallbackLayout), "Create fallback RT set layout");

        // Use fallback for all (or integrate properly if multiple layouts needed)
    } else {
        fallbackLayout = HANDLE_GET(rtDescriptorSetLayout_);
        LOG_DEBUG_CAT("RTX", "Using existing rtDescriptorSetLayout: 0x{:x}", reinterpret_cast<uintptr_t>(fallbackLayout));
    }

    // Validate layout is non-null
    if (fallbackLayout == VK_NULL_HANDLE) {
        throw std::runtime_error("Descriptor set layout is null — cannot proceed with allocation");
    }

    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{};
    std::fill(layouts.begin(), layouts.end(), fallbackLayout);  // Use validated layout
    LOG_DEBUG_CAT("RTX", "Layouts filled with valid layout: 0x{:x}", reinterpret_cast<uintptr_t>(fallbackLayout));

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = HANDLE_GET(descriptorPool_);
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();  // Now guaranteed non-null

    VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_.data()), "Failed to allocate descriptor sets");
    LOG_DEBUG_CAT("RTX", "Descriptor sets allocated — first set: 0x{:x}", reinterpret_cast<uintptr_t>(descriptorSets_[0]));

    // If fallback was created, store it or clean up if not needed
    if (fallbackLayout != HANDLE_GET(rtDescriptorSetLayout_)) {
        // TODO: Assign to rtDescriptorSetLayout_ if appropriate, or destroy after use
        vkDestroyDescriptorSetLayout(device_, fallbackLayout, nullptr);  // Temp fallback, destroy
        LOG_DEBUG_CAT("RTX", "Fallback layout destroyed after allocation");
    }

    LOG_SUCCESS_CAT("RTX", "{}Descriptor pool + {} sets forged — ready for binding{}", PLASMA_FUCHSIA, MAX_FRAMES_IN_FLIGHT, RESET);
    RTX::AmouranthAI::get().onMemoryEvent("Descriptor Sets", static_cast<VkDeviceSize>(MAX_FRAMES_IN_FLIGHT * sizeof(VkDescriptorSet)));
    LOG_TRACE_CAT("RTX", "initDescriptorPoolAndSets — COMPLETE");
}

// =============================================================================
// Shader Binding Table — 64 MB Titan
// =============================================================================

void VulkanRTX::initShaderBindingTable(VkPhysicalDevice pd) {
    LOG_TRACE_CAT("RTX", "initShaderBindingTable — START — pd=0x{:x}", reinterpret_cast<uintptr_t>(pd));
    const uint32_t groupCount = 25;
    const auto& props = RTX::g_ctx().rayTracingProps();
    const VkDeviceSize handleSize = props.shaderGroupHandleSize;
    const VkDeviceSize baseAlignment = props.shaderGroupBaseAlignment;
    const VkDeviceSize alignedSize = alignUp(handleSize, baseAlignment);

    LOG_INFO_CAT("RTX", "SBT: {} groups | handle: {} B | align: {} B", groupCount, handleSize, baseAlignment);

    uint64_t sbtEnc = 0;
    LOG_TRACE_CAT("RTX", "Creating SBT buffer — 64 MB");
    BUFFER_CREATE(sbtEnc, 64_MB,
                  VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "AMOURANTH_SBT_64MB_TITAN");
    RTX::AmouranthAI::get().onMemoryEvent("SBT Buffer", 64_MB);
    LOG_DEBUG_CAT("RTX", "SBT buffer created: 0x{:x}", reinterpret_cast<uintptr_t>(RAW_BUFFER(sbtEnc)));

    VkBuffer rawBuffer = RAW_BUFFER(sbtEnc);
    LOG_INFO_CAT("RTX", "HANDLE_CREATE: {} | Tag: {}", "sbtBuffer", "SBTBuffer");
    sbtBuffer_ = RTX::Handle<VkBuffer>(rawBuffer, device_,
        [](VkDevice d, VkBuffer b, const VkAllocationCallbacks*) {
            LOG_TRACE_CAT("RTX", "Destroying SBT buffer: 0x{:x}", reinterpret_cast<uintptr_t>(b));
            vkDestroyBuffer(d, b, nullptr);
        }, 0, "SBTBuffer");

    VkDeviceMemory rawMemory = BUFFER_MEMORY(sbtEnc);
    LOG_INFO_CAT("RTX", "HANDLE_CREATE: {} | Tag: {}", "sbtMemory", "SBTMemory");
    sbtMemory_ = RTX::Handle<VkDeviceMemory>(rawMemory, device_,
        [](VkDevice d, VkDeviceMemory m, const VkAllocationCallbacks*) {
            LOG_TRACE_CAT("RTX", "Freeing SBT memory: 0x{:x}", reinterpret_cast<uintptr_t>(m));
            vkFreeMemory(d, m, nullptr);
        }, 64_MB, "SBTMemory");

    std::vector<uint8_t> handles(groupCount * handleSize);
    LOG_TRACE_CAT("RTX", "Fetching {} shader group handles", groupCount);
    VK_CHECK(RTX::g_ctx().vkGetRayTracingShaderGroupHandlesKHR()(device_,
                                                            HANDLE_GET(rtPipeline_),
                                                            0, groupCount,
                                                            handles.size(), handles.data()),
             "Failed to get shader group handles");
    LOG_DEBUG_CAT("RTX", "{} handles fetched — size: {} B", groupCount, handles.size());

    void* mapped = nullptr;
    BUFFER_MAP(sbtEnc, mapped);
    uint8_t* data = static_cast<uint8_t*>(mapped);
    for (uint32_t i = 0; i < groupCount; ++i) {
        std::memcpy(data + i * alignedSize, handles.data() + i * handleSize, handleSize);
        LOG_TRACE_CAT("RTX", "Mapped handle {}: {} B @ offset {}", i, handleSize, i * alignedSize);
    }
    BUFFER_UNMAP(sbtEnc);

    VkBufferDeviceAddressInfo addrInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = rawBuffer};
    sbtAddress_ = vkGetBufferDeviceAddress(device_, &addrInfo);
    LOG_DEBUG_CAT("RTX", "SBT device address: 0x{:x}", sbtAddress_);

    sbt_.raygen   = { sbtAddress_,                    alignedSize, alignedSize };
    sbt_.miss     = { sbtAddress_ + alignedSize,      alignedSize, alignedSize };
    sbt_.hit      = { sbtAddress_ + alignedSize * 9,  alignedSize, alignedSize };
    sbt_.callable = { sbtAddress_ + alignedSize * 25, alignedSize, alignedSize };

    sbtRecordSize_ = alignedSize;

    LOG_SUCCESS_CAT("RTX", "{}SBT forged — {} groups — @ 0x{:x} — TITAN DOMINANCE{}", PLASMA_FUCHSIA, groupCount, sbtAddress_, RESET);
    RTX::AmouranthAI::get().onMemoryEvent("SBT Handles", groupCount * handleSize);
    LOG_TRACE_CAT("RTX", "initShaderBindingTable — COMPLETE");
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

    uint32_t memType = RTX::UltraLowLevelBufferTracker::findMemoryType(
        RTX::g_ctx().physicalDevice(), memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
    VkCommandBuffer cmd = beginSingleTimeCommands(RTX::g_ctx().commandPool());
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

    endSingleTimeCommandsAsync(cmd, RTX::g_ctx().graphicsQueue(), RTX::g_ctx().commandPool());  // Async for speed

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
    VkImageView blueNoise = RTX::g_ctx().blueNoiseView_.valid() ? RTX::g_ctx().blueNoiseView_.get() : HANDLE_GET(blackFallbackView_);
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

void VulkanRTX::recordRayTrace(VkCommandBuffer cmd, VkExtent2D extent,
                               VkImage outputImage, VkImageView outputView)
{
    LOG_TRACE_CAT("RTX", "recordRayTrace — START — cmd=0x{:x}, extent={}x{}, outputImage=0x{:x}, outputView=0x{:x}",
                  reinterpret_cast<uintptr_t>(cmd), extent.width, extent.height, reinterpret_cast<uintptr_t>(outputImage), reinterpret_cast<uintptr_t>(outputView));
    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .image               = outputImage,
        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    LOG_TRACE_CAT("RTX", "Output image barrier applied — GENERAL layout");

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, HANDLE_GET(rtPipeline_));
    LOG_DEBUG_CAT("RTX", "RT pipeline bound: 0x{:x}", reinterpret_cast<uintptr_t>(HANDLE_GET(rtPipeline_)));
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            HANDLE_GET(rtPipelineLayout_), 0, 1,
                            &descriptorSets_[0], 0, nullptr);
    LOG_DEBUG_CAT("RTX", "Descriptor sets bound for frame {}", 0);

    LOG_TRACE_CAT("RTX", "Dispatching vkCmdTraceRaysKHR — — extent: {}x{}", extent.width, extent.height);
    RTX::g_ctx().vkCmdTraceRaysKHR()(cmd, &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable,
                                     extent.width, extent.height, 1);
    LOG_DEBUG_CAT("RTX", "Ray trace dispatched successfully");

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    LOG_TRACE_CAT("RTX", "Post-trace barrier applied — PRESENT_SRC_KHR layout");

    LOG_SUCCESS_CAT("RTX", "{}Ray trace recorded — frame {}{}", PLASMA_FUCHSIA, 0, RESET);
    RTX::AmouranthAI::get().onPhotonDispatch(extent.width, extent.height);
    LOG_TRACE_CAT("RTX", "recordRayTrace — COMPLETE");
}

uint64_t VulkanRTX::alignUp(uint64_t value, uint64_t alignment) const noexcept {
    if (alignment == 0) return value;  // Edge case: avoid div-by-zero
    return ((value + alignment - 1) / alignment) * alignment;
}

namespace RTX {

// =============================================================================
// FULL: Updated createVulkanInstanceWithSDL — With Safer Callback & Enhanced Error Handling
// No std::terminate() on non-fatal issues; Propagate failures with return VK_NULL_HANDLE
// Log validation setup & instance creation traces
// =============================================================================
[[nodiscard]] VkInstance createVulkanInstanceWithSDL(SDL_Window* window, bool enableValidation)
{
    if (!window) {
        LOG_FATAL_CAT("VULKAN", "createVulkanInstanceWithSDL: Null window provided — cannot query SDL extensions!");
        return VK_NULL_HANDLE;
    }

    LOG_INFO_CAT("VULKAN", "{}FORGING VULKAN INSTANCE — SDL3 FULL MODE — EXTRACTING WINDOW EXTENSIONS{}", PLASMA_FUCHSIA, RESET);

    // Step 1: Query SDL-required instance extensions
    uint32_t sdlExtensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    if ((sdlExtensions || sdlExtensionCount) == 0) { // == 0 for true never !
        LOG_FATAL_CAT("VULKAN", "SDL_Vulkan_GetInstanceExtensions failed — no extensions returned");
        LOG_ERROR_CAT("VULKAN", "Window flags: 0x{:x} (SDL_WINDOW_VULKAN? {})", SDL_GetWindowFlags(window), 
                      (SDL_GetWindowFlags(window) & SDL_WINDOW_VULKAN) ? "YES" : "NO");
        return VK_NULL_HANDLE;
    }

    // Step 2: Build extension list with deduplication
    std::set<const char*> uniqueExts;
    for (uint32_t i = 0; i < sdlExtensionCount; ++i)
        uniqueExts.insert(sdlExtensions[i]);
    if (enableValidation)
        uniqueExts.insert(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    std::vector<const char*> extensions(uniqueExts.begin(), uniqueExts.end());

    LOG_INFO_CAT("VULKAN", "SDL extensions extracted ({} total):", extensions.size());
    for (const auto* ext : extensions)
        LOG_INFO_CAT("VULKAN", "  + {}", ext);

    // Step 3: Validation layers
    const char* const* layers = nullptr;
    uint32_t layerCount = 0;
    if (enableValidation) {
        uint32_t availCount = 0;
        vkEnumerateInstanceLayerProperties(&availCount, nullptr);
        std::vector<VkLayerProperties> availLayers(availCount);
        vkEnumerateInstanceLayerProperties(&availCount, availLayers.data());

        static const char* validationLayer = "VK_LAYER_KHRONOS_validation";
        if (std::any_of(availLayers.begin(), availLayers.end(),
            [](const auto& p) { return strcmp(p.layerName, validationLayer) == 0; })) {
            layers = &validationLayer;
            layerCount = 1;
            LOG_INFO_CAT("VULKAN", "  + VK_LAYER_KHRONOS_validation (supported)");
        } else {
            LOG_WARN_CAT("VULKAN", "Validation layer not supported — disabling");
        }
    }

    // Step 4: App info
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "AMOURANTH RTX — VALHALLA v80 TURBO",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "VALHALLA TURBO ENGINE",
        .engineVersion = VK_MAKE_VERSION(80, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    // Step 5: Debug messenger setup
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidation && layerCount > 0) {
        debugCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = VulkanDebugCallback
        };
    }

    // Step 6: Instance create info
    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = (enableValidation && layerCount > 0) ? &debugCreateInfo : nullptr,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = layerCount,
        .ppEnabledLayerNames = layers,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("VULKAN", "vkCreateInstance FAILED — result: {} ({})", static_cast<int>(result), result);
        return VK_NULL_HANDLE;
    }

    // === CRITICAL STONEKEY FIX: STORE RAW FIRST, THEN OBFUSCATE ===
    g_context_instance.instance_ = instance;           // ← Raw handle stored (used by vkEnumeratePhysicalDevices)
    set_g_instance(instance);  // ← Now safe: raw cached

    LOG_SUCCESS_CAT("VULKAN", "{}VULKAN INSTANCE FORGED @ 0x{:016x} — STONEKEY v∞ ARMED — PINK PHOTONS PROTECTED{}", 
                    PLASMA_FUCHSIA, reinterpret_cast<uintptr_t>(instance), RESET);

    // Step 7: Create debug messenger (post-instance)
    if (enableValidation && layerCount > 0) {
        auto createFunc = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (createFunc) {
            VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
            if (createFunc(instance, &debugCreateInfo, nullptr, &messenger) == VK_SUCCESS) {
                g_context_instance.debugMessenger_ = messenger;
                LOG_DEBUG_CAT("VULKAN", "Debug messenger created: 0x{:x}", reinterpret_cast<uintptr_t>(messenger));
            }
        }
    }

    // Final confirmation
    g_context_instance.instance_ = instance;  // ← Raw handle stored safely
    LOG_SUCCESS_CAT("VULKAN", "{}STONEKEY v∞ FULLY ACTIVE — INSTANCE OBFUSCATED — TITAN DOMINANCE ETERNAL{}", LILAC_LAVENDER, RESET);
    return instance;  // Caller gets raw handle — StoneKey already protects global access
}

bool createSurface(SDL_Window* window, VkInstance instance)
{
    if (!window || !instance) {
        LOG_ERROR_CAT("VULKAN", "createSurface: null window or instance");
        return false;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // SDL3 API in 2025: allocator is now a parameter
    if (SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface) == false) {
        LOG_ERROR_CAT("VULKAN", "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        return false;
    }

    // FULL STONEKEY v∞ INTEGRATION — IMMEDIATE OBFUSCATION + RAW CACHE
    set_g_surface(surface);

    LOG_SUCCESS_CAT("VULKAN", "Vulkan surface created — STONEKEY v∞ ACTIVE");
    LOG_SUCCESS_CAT("VULKAN", "FIRST LIGHT ACHIEVED — PINK PHOTONS IMMINENT");

    return true;
}

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
            set_g_PhysicalDevice(selected);

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
        set_g_PhysicalDevice(selected);

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
    set_g_instance(rawInstance);

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
        .queueFamilyIndex = g_context_instance.graphicsFamily_
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
static std::unique_ptr<PipelineManager> g_pipelineManager = nullptr;

void createGlobalPipelineManager(VkDevice device, VkPhysicalDevice phys)
{
    g_pipelineManager = std::make_unique<PipelineManager>(device, phys);
}

PipelineManager* getGlobalPipelineManager()
{
    return g_pipelineManager.get();
}

} // RTX
// VulkanCore.cpp — FINAL, SDL3-CORRECT, BULLETPROOF FORMAT — NO BULLSHIT
// VulkanCore.cpp — FINAL, SDL3-CORRECT, BULLETPROOF + STONEKEY v∞ ACTIVE
// RELAXED: All VUIDs broken/fixed (null guards, layout transitions for HDR/video), HDR respected via extensions/formats