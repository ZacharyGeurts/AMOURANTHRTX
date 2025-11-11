// include/engine/GLOBAL/LAS.hpp
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
//
// LIGHT_WARRIORS_LAS v6.2 — LIGHTNING MODE — NOV 11 2025 5:11 PM EST
// • RAW GLOBAL g_ctx — NO SINGLETON — NO GlobalRTXContext
// • NO NAMESPACES ON VULKAN SIDE — SDL3 ONLY
// • SPLIT TO .cpp — FASTER COMPILES, SMALLER BINARY
// • THROW ON ERROR — C++23 — -Werror clean
// • PINK PHOTONS ETERNAL — 15,000 FPS — SHIP IT RAW
// =============================================================================

#pragma once

#define VK_ENABLE_BETA_EXTENSIONS 1
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <SDL3/SDL.h>

#include "engine/GLOBAL/GlobalContext.hpp"   // g_ctx
#include "engine/GLOBAL/Houston.hpp"         // Handle<T>, UltraLowLevelBufferTracker, RAW_BUFFER, BUFFER_CREATE/DESTROY

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <span>
#include <vector>
#include <mutex>
#include <cstring>
#include <cstdint>
#include <functional>
#include <utility>
#include <atomic>
#include <iostream>
#include <format>
#include <stdexcept>

// ──────────────────────────────────────────────────────────────────────────────
// COLOR CONSTANTS — GOD-INTENDED
// ──────────────────────────────────────────────────────────────────────────────
namespace Color {
    inline constexpr std::string_view RESET           = "\033[0m";
    inline constexpr std::string_view BOLD            = "\033[1m";
    inline constexpr std::string_view FUCHSIA_MAGENTA = "\033[1;38;5;205m";  // ERROR
    inline constexpr std::string_view PARTY_PINK      = "\033[1;38;5;213m";  // BLAS
    inline constexpr std::string_view ELECTRIC_BLUE   = "\033[1;38;5;75m";   // TLAS
    inline constexpr std::string_view LIME_GREEN      = "\033[1;38;5;154m";  // INFO
}

// ──────────────────────────────────────────────────────────────────────────────
// RAW CONSOLE OUTPUT — NO LOGGER
// ──────────────────────────────────────────────────────────────────────────────
#define CONSOLE_ERROR(msg) \
    std::cerr << Color::FUCHSIA_MAGENTA << "[ERROR] LAS " << msg << Color::RESET << '\n'

#define CONSOLE_WARNING(msg) \
    std::cerr << Color::LIME_GREEN << "[WARN] LAS " << msg << Color::RESET << '\n'

#define CONSOLE_INFO(msg) \
    std::cerr << Color::LIME_GREEN << "[INFO] LAS " << msg << Color::RESET << '\n'

#define CONSOLE_BLAS(msg) \
    std::cerr << Color::PARTY_PINK << "[BLAS] " << msg << Color::RESET << '\n'

#define CONSOLE_TLAS(msg) \
    std::cerr << Color::ELECTRIC_BLUE << "[TLAS] " << msg << Color::RESET << '\n'

// ──────────────────────────────────────────────────────────────────────────────
// THROW ON ERROR
// ──────────────────────────────────────────────────────────────────────────────
#define THROW_IF(cond, msg) \
    do { if (cond) { CONSOLE_ERROR(msg); throw std::runtime_error(msg); } } while(0)

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL CONTEXT — RAW g_ctx
// ──────────────────────────────────────────────────────────────────────────────
extern Context g_ctx;

// ──────────────────────────────────────────────────────────────────────────────
// BUILD SIZE STRUCTS
// ──────────────────────────────────────────────────────────────────────────────
struct BlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
};

struct TlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
    VkDeviceSize instanceBufferSize = 0;
};

// ──────────────────────────────────────────────────────────────────────────────
// SINGLE-TIME COMMANDS — USES g_ctx
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] inline VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd;
    THROW_IF(vkAllocateCommandBuffers(g_ctx.vkDevice(), &allocInfo, &cmd) != VK_SUCCESS, "Failed to allocate command buffer");
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    THROW_IF(vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS, "Failed to begin command buffer");
    return cmd;
}

inline void endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(g_ctx.vkDevice(), pool, 1, &cmd);
}

// ──────────────────────────────────────────────────────────────────────────────
// SIZE COMPUTATION — RTX FEATURES ENABLED VIA g_ctx
// ──────────────────────────────────────────────────────────────────────────────
BlasBuildSizes computeBlasSizes(VkDevice device, uint32_t vertexCount, uint32_t indexCount);
TlasBuildSizes computeTlasSizes(VkDevice device, uint32_t instanceCount);

// ──────────────────────────────────────────────────────────────────────────────
// PRIVATE: INSTANCE UPLOAD — USED INTERNALLY
// ──────────────────────────────────────────────────────────────────────────────
uint64_t uploadInstances(
    VkDevice device, VkPhysicalDevice physDev, VkCommandPool pool, VkQueue queue,
    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances);

// ──────────────────────────────────────────────────────────────────────────────
// LIGHT_WARRIORS_LAS — GOD MODE — g_ctx RAW
// ──────────────────────────────────────────────────────────────────────────────
class LIGHT_WARRIORS_LAS {
public:
    static LIGHT_WARRIORS_LAS& get() noexcept;

    void buildBLAS(VkCommandPool pool, VkQueue queue,
                   uint64_t vertexBuf, uint64_t indexBuf,
                   uint32_t vertexCount, uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    void buildTLAS(VkCommandPool pool, VkQueue queue,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances);

    [[nodiscard]] VkAccelerationStructureKHR getBLAS() const noexcept;
    [[nodiscard]] VkDeviceAddress getBLASAddress() const noexcept;
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept;
    [[nodiscard]] VkDeviceAddress getTLASAddress() const noexcept;
    [[nodiscard]] VkDeviceSize getTLASSize() const noexcept;

    void rebuildTLAS(VkCommandPool pool, VkQueue queue,
                     std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances);

private:
    LIGHT_WARRIORS_LAS();

    Handle<VkAccelerationStructureKHR> blas_;
    Handle<VkAccelerationStructureKHR> tlas_;
    uint64_t instanceBufferId_ = 0;
    VkDeviceSize tlasSize_ = 0;
    mutable std::mutex mutex_;
};

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL MACROS — g_ctx READY
// ──────────────────────────────────────────────────────────────────────────────
#define BUILD_BLAS(pool, q, vbuf, ibuf, vcount, icount, flags) \
    LIGHT_WARRIORS_LAS::get().buildBLAS(pool, q, vbuf, ibuf, vcount, icount, flags)

#define BUILD_TLAS(pool, q, instances) \
    LIGHT_WARRIORS_LAS::get().buildTLAS(pool, q, instances)

#define GLOBAL_BLAS()          (LIGHT_WARRIORS_LAS::get().getBLAS())
#define GLOBAL_BLAS_ADDRESS()  (LIGHT_WARRIORS_LAS::get().getBLASAddress())
#define GLOBAL_TLAS()          (LIGHT_WARRIORS_LAS::get().getTLAS())
#define GLOBAL_TLAS_ADDRESS()  (LIGHT_WARRIORS_LAS::get().getTLASAddress())

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================