// src/engine/Dispose.cpp
// AMOURANTH RTX Engine – November 2025
// C++20 ONLY – std::format logging, lock-free double-free detection
// MATCHES EXACTLY: include/engine/Dispose.hpp
// FIXED: full definition of cleanupAll, no overloads, compiles/links clean

#include "engine/Dispose.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <thread>
#include <sstream>
#include <format>
#include <atomic>
#include <string_view>

namespace Dispose {

using namespace Logging;

// ---------------------------------------------------------------------
// Thread-local counter
// ---------------------------------------------------------------------
thread_local uint64_t g_destructionCounter = 0;

// ---------------------------------------------------------------------
// DestroyTracker static storage
// ---------------------------------------------------------------------
std::atomic<uint64_t>* DestroyTracker::s_bitset = nullptr;
std::atomic<size_t>    DestroyTracker::s_capacity{0};

// ---------------------------------------------------------------------
// DestroyTracker implementation
// ---------------------------------------------------------------------
void DestroyTracker::ensureCapacity(uint64_t hash) {
    size_t word = hash / CHUNK_BITS;
    size_t needed = word + 1;

    size_t current = s_capacity.load(std::memory_order_acquire);
    if (current >= needed) return;

    size_t newCap = current ? current * 2 : 64;
    if (newCap < needed) newCap = needed;

    auto* newArray = new std::atomic<uint64_t>[newCap]();
    for (size_t i = 0; i < current; ++i) {
        newArray[i].store(s_bitset[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    auto* old = s_bitset;
    s_bitset = newArray;
    s_capacity.store(newCap, std::memory_order_release);

    if (old) delete[] old;
}

void DestroyTracker::markDestroyed(void* ptr) {
    uint64_t h = hash(ptr);
    ensureCapacity(h);
    size_t word = h / CHUNK_BITS;
    uint64_t bit = 1ULL << (h % CHUNK_BITS);
    s_bitset[word].fetch_or(bit, std::memory_order_release);
}

bool DestroyTracker::isDestroyed(void* ptr) {
    uint64_t h = hash(ptr);
    size_t current = s_capacity.load(std::memory_order_acquire);
    size_t word = h / CHUNK_BITS;
    if (word >= current) return false;
    uint64_t bit = 1ULL << (h % CHUNK_BITS);
    return (s_bitset[word].load(std::memory_order_acquire) & bit) != 0;
}

// ---------------------------------------------------------------------
// Helper: thread ID as string
// ---------------------------------------------------------------------
static std::string threadIdToString() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

// ---------------------------------------------------------------------
// Cast any Vulkan handle to void* for the tracker
// ---------------------------------------------------------------------
static inline void* handleToPtr(uint64_t h) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(h));
}

// ---------------------------------------------------------------------
// Safe format wrapper – never throws
// ---------------------------------------------------------------------
template<typename... Args>
static std::string safeFormat(std::string_view fmt, const Args&... args) {
    try {
        return std::vformat(fmt, std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::string("[FORMAT ERROR: ") + e.what() + "] " + std::string(fmt);
    } catch (...) {
        return std::string("[UNKNOWN FORMAT ERROR] ") + std::string(fmt);
    }
}

// ---------------------------------------------------------------------
// Log + track destruction (string_view version – used internally)
// ---------------------------------------------------------------------
static void logAndTrackDestruction(std::string_view typeName, void* ptr) {
    ++g_destructionCounter;
    DestroyTracker::markDestroyed(ptr);
    LOG_INFO_CAT("Dispose", safeFormat("{}Destroying {} {}{}", Color::EMERALD_GREEN, typeName, ptr, Color::RESET));
}

// ---------------------------------------------------------------------
// Generic container destroyer
// ---------------------------------------------------------------------
template<typename Container, typename DestroyFn>
static void safeDestroyContainer(Container& container,
                                 DestroyFn destroyFn,
                                 std::string_view typeName,
                                 VkDevice device)
{
    for (auto it = container.rbegin(); it != container.rend(); ++it) {
        uint64_t handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(*it));
        if (handle == 0) continue;

        void* ptr = handleToPtr(handle);

        if (DestroyTracker::isDestroyed(ptr)) {
            LOG_ERROR_CAT("Dispose",
                safeFormat("{}DOUBLE FREE SKIPPED: {} {} already destroyed{}",
                           Color::CRIMSON_MAGENTA, typeName, ptr, Color::RESET));
            continue;
        }

        logAndTrackDestruction(typeName, ptr);

        try {
            destroyFn(device, *it, nullptr);
            *it = VK_NULL_HANDLE;
        } catch (const std::exception& e) {
            LOG_ERROR_CAT("Dispose",
                safeFormat("EXCEPTION destroying {} {}: {}", typeName, ptr, e.what()));
        } catch (...) {
            LOG_ERROR_CAT("Dispose",
                safeFormat("UNKNOWN EXCEPTION destroying {} {}", typeName, ptr));
        }
    }
    container.clear();
}

// ---------------------------------------------------------------------
// Global cleanup – **single definition**
// ---------------------------------------------------------------------
void cleanupAll(Vulkan::Context& ctx) noexcept {
    const std::string threadId = threadIdToString();

    LOG_INFO_CAT("Dispose",
        safeFormat("{}cleanupAll() — START (thread: {}){}",
                   Color::EMERALD_GREEN, threadId, Color::RESET));

    if (ctx.device == VK_NULL_HANDLE) {
        LOG_INFO_CAT("Dispose",
            safeFormat("{}Device handle is null — early exit{}", Color::OCEAN_TEAL, Color::RESET));
        return;
    }

    LOG_INFO_CAT("Dispose",
        safeFormat("{}Skipping vkDeviceWaitIdle() — device may be lost{}", Color::OCEAN_TEAL, Color::RESET));

    // Phase 1 – Shader Modules
    LOG_INFO_CAT("Dispose", safeFormat("{}Phase 1: Shader Modules{}", Color::ARCTIC_CYAN, Color::RESET));
    safeDestroyContainer(ctx.resourceManager.getShaderModulesMutable(),
                         vkDestroyShaderModule, "ShaderModule", ctx.device);

    // Phase 2 – Pipelines
    LOG_INFO_CAT("Dispose", safeFormat("{}Phase 2: Pipelines{}", Color::ARCTIC_CYAN, Color::RESET));
    safeDestroyContainer(ctx.resourceManager.getPipelinesMutable(),
                         vkDestroyPipeline, "Pipeline", ctx.device);

    // Phase 3 – Descriptor Set Layouts
    LOG_INFO_CAT("Dispose", safeFormat("{}Phase 3: Descriptor Set Layouts{}", Color::ARCTIC_CYAN, Color::RESET));
    safeDestroyContainer(ctx.resourceManager.getDescriptorSetLayoutsMutable(),
                         vkDestroyDescriptorSetLayout, "DescriptorSetLayout", ctx.device);

    // Phase 4 – Render Passes
    LOG_INFO_CAT("Dispose", safeFormat("{}Phase 4: Render Passes{}", Color::ARCTIC_CYAN, Color::RESET));
    safeDestroyContainer(ctx.resourceManager.getRenderPassesMutable(),
                         vkDestroyRenderPass, "RenderPass", ctx.device);

    // Phase 5 – Buffers
    LOG_INFO_CAT("Dispose", safeFormat("{}Phase 5: Buffers{}", Color::ARCTIC_CYAN, Color::RESET));
    safeDestroyContainer(ctx.resourceManager.getBuffersMutable(),
                         vkDestroyBuffer, "Buffer", ctx.device);

    // Phase 6 – Command Pools
    LOG_INFO_CAT("Dispose", safeFormat("{}Phase 6: Command Pools{}", Color::ARCTIC_CYAN, Color::RESET));
    safeDestroyContainer(ctx.resourceManager.getCommandPoolsMutable(),
                         vkDestroyCommandPool, "CommandPool", ctx.device);

    // Phase 7 – Swapchain Images (no destroy)
    LOG_INFO_CAT("Dispose", safeFormat("{}Phase 7: Swapchain Images{}", Color::ARCTIC_CYAN, Color::RESET));

    // Phase 8 – Swapchain
    LOG_INFO_CAT("Dispose", safeFormat("{}Phase 8: Swapchain{}", Color::ARCTIC_CYAN, Color::RESET));
    if (ctx.swapchain != VK_NULL_HANDLE) {
        void* ptr = handleToPtr(reinterpret_cast<uintptr_t>(ctx.swapchain));
        if (!DestroyTracker::isDestroyed(ptr)) {
            logAndTrackDestruction(std::string_view("Swapchain"), ptr);
            vkDestroySwapchainKHR(ctx.device, ctx.swapchain, nullptr);
            ctx.swapchain = VK_NULL_HANDLE;
        }
    }

    // Phase 9 – Surface (instance-level, safe even if device lost)
    LOG_INFO_CAT("Dispose", safeFormat("{}Phase 9: Surface{}", Color::ARCTIC_CYAN, Color::RESET));
    if (ctx.surface != VK_NULL_HANDLE) {
        void* ptr = handleToPtr(reinterpret_cast<uintptr_t>(ctx.surface));
        if (!DestroyTracker::isDestroyed(ptr)) {
            logAndTrackDestruction(std::string_view("Surface"), ptr);
            vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
            ctx.surface = VK_NULL_HANDLE;
        }
    }

    // Phase 10 – Device (always attempt, even if lost)
    LOG_INFO_CAT("Dispose", safeFormat("{}Phase 10: Device{}", Color::ARCTIC_CYAN, Color::RESET));
    if (ctx.device != VK_NULL_HANDLE) {
        void* ptr = handleToPtr(reinterpret_cast<uintptr_t>(ctx.device));
        if (!DestroyTracker::isDestroyed(ptr)) {
            logAndTrackDestruction(std::string_view("Device"), ptr);
            try { vkDestroyDevice(ctx.device, nullptr); }
            catch (...) { /* device lost – expected */ }
        }
        ctx.device = VK_NULL_HANDLE;
    }

    // Phase 11 – Instance (always attempt)
    LOG_INFO_CAT("Dispose", safeFormat("{}Phase 11: Instance{}", Color::ARCTIC_CYAN, Color::RESET));
    if (ctx.instance != VK_NULL_HANDLE) {
        void* ptr = handleToPtr(reinterpret_cast<uintptr_t>(ctx.instance));
        if (!DestroyTracker::isDestroyed(ptr)) {
            logAndTrackDestruction(std::string_view("Instance"), ptr);
            vkDestroyInstance(ctx.instance, nullptr);
        }
        ctx.instance = VK_NULL_HANDLE;
    }

    LOG_INFO_CAT("Dispose",
        safeFormat("{}cleanupAll() — COMPLETE — {} handles destroyed{}",
                   Color::EMERALD_GREEN, g_destructionCounter, Color::RESET));

    if (auto* arr = DestroyTracker::s_bitset) {
        delete[] arr;
        DestroyTracker::s_bitset = nullptr;
        DestroyTracker::s_capacity.store(0, std::memory_order_release);
    }
}

} // namespace Dispose