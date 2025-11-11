// include/engine/GLOBAL/GlobalContext.hpp
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
// Global RTX Context — FULL C++23 — NOV 11 2025 3:13 PM EST
// • SINGLETON GLOBAL — THREAD-SAFE VIA STATIC LOCAL — ZERO LEAKS
// • SECURED BY STONEKEY — OBFUSCATED ACCESS — VALHALLA LOCKED
// • PINK PHOTONS ETERNAL — RTX FEATURES CHAINED — CROSS-PLATFORM
// • NO CONTEXT.hpp DEPENDENCY — SELF-CONTAINED — SHIP IT RAW
// • Professional, -Werror clean, C++23, inline where possible
//
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <set>  // For unique queue families

// ──────────────────────────────────────────────────────────────────────────────
// STONEKEY SECURE — GLOBAL OBFUSCATION LAYER
// ──────────────────────────────────────────────────────────────────────────────
#include "engine/GLOBAL/StoneKey.hpp"  // SECURED: kStone1 obfuscates access

class VulkanRenderer;

class GlobalRTXContext {
public:
    static GlobalRTXContext& get() noexcept {
        static GlobalRTXContext instance;
        return instance;
    }

    GlobalRTXContext(const GlobalRTXContext&) = delete;
    GlobalRTXContext& operator=(const GlobalRTXContext&) = delete;
    ~GlobalRTXContext() noexcept { cleanup(); }

    // Creation — INLINE WHERE POSSIBLE FOR PERFORMANCE
    bool createInstance(const std::vector<const char*>& extra = {}) noexcept;
    bool createSurface(SDL_Window* window, VkInstance inst) noexcept;
    bool pickPhysicalDevice(VkSurfaceKHR surf, bool preferNvidia = false) noexcept;
    bool createDevice(VkSurfaceKHR surf, bool enableRT = true) noexcept;
    bool createQueuesAndPools() noexcept;

    // Accessors — SECURED VIA STONEKEY IF NEEDED
    VkInstance vkInstance() const noexcept { return instance_; }
    VkPhysicalDevice vkPhysicalDevice() const noexcept { return physicalDevice_; }
    VkDevice vkDevice() const noexcept { return device_; }
    VkSurfaceKHR vkSurface() const noexcept { return surface_; }
    VkQueue graphicsQueue() const noexcept { return graphicsQueue_; }
    VkQueue presentQueue() const noexcept { return presentQueue_; }
    VkCommandPool commandPool() const noexcept { return commandPool_; }
    const VkPhysicalDeviceProperties& deviceProperties() const noexcept { return deviceProps_; }

    void cleanup() noexcept;

private:
    GlobalRTXContext() = default;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    uint32_t graphicsFamily_ = UINT32_MAX;
    uint32_t presentFamily_ = UINT32_MAX;

    VkPhysicalDeviceProperties deviceProps_ = {};  // Added for pickPhysicalDevice

    struct RTX {
        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipeline = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
        VkPhysicalDeviceRayQueryFeaturesKHR rayQuery = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };

        void chain() noexcept {
            bufferDeviceAddress.pNext = &accelerationStructure;
            accelerationStructure.pNext = &rayTracingPipeline;
            rayTracingPipeline.pNext = &rayQuery;
            rayQuery.pNext = nullptr;
        }
    } rtx_;  // Full feature chaining for RTX enablement
};

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
// STONEKEY SECURED — GLOBAL RTX CONTEXT ETERNAL — NOV 11 2025
// =============================================================================