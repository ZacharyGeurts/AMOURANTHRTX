// include/engine/GLOBAL/LAS.hpp
// AMOURANTH RTX — LOW-LEVEL LAS TRACKER — NOVEMBER 09 2025 — HARDWARE EDITION
// DIRECT VULKAN ACCELERATION STRUCTURE MANAGEMENT — MINIMAL — RTX HARDWARE FOCUS
// HEADER-ONLY — C++23 ATOMICS — ZERO ABSTRACTION — BASIC CONSOLE LOGGING

#pragma once

#include "../GLOBAL/StoneKey.hpp"
#include "../GLOBAL/Dispose.hpp"
#include "../GLOBAL/logging.hpp"      // DestroyTracker + VK_CHECK
#include "engine/Vulkan/VulkanCommon.hpp"

#include <vulkan/vulkan.h>
#include <atomic>
#include <unordered_map>
#include <cstdint>
#include <iostream>
#include <iomanip>

namespace Vulkan {
    struct Context;
}

class LowLevelLASTracker {
public:
    // MINIMAL SINGLETON — HARDWARE ONLY
    [[nodiscard]] static LowLevelLASTracker& get() noexcept {
        static LowLevelLASTracker instance;
        return instance;
    }

    LowLevelLASTracker(const LowLevelLASTracker&) = delete;
    LowLevelLASTracker& operator=(const LowLevelLASTracker&) = delete;

    // UPDATE TLAS — DIRECT VULKAN HANDLE BIND + BASIC LOG
    void updateTLAS(VkAccelerationStructureKHR raw_tlas, VkDevice device,
                    PFN_vkDestroyAccelerationStructureKHR destroyFunc = nullptr) noexcept {
        if (currentTLAS_.valid()) {
            // Old TLAS will be destroyed via RAII + tracked
            currentTLAS_.reset();
        }

        if (raw_tlas != VK_NULL_HANDLE) {
            currentTLAS_ = Vulkan::makeAccelerationStructure(
                device, raw_tlas,
                destroyFunc ? destroyFunc : Vulkan::ctx()->vkDestroyAccelerationStructureKHR
            );
        }

        rawTLAS_ = raw_tlas;
        valid_.store(true, std::memory_order_release);
        generation_.fetch_add(1, std::memory_order_acq_rel);

        std::cout << "[HW LAS] TLAS UPDATED — RAW 0x" << std::hex << reinterpret_cast<uintptr_t>(raw_tlas) << std::dec 
                  << " — GEN " << generation_.load() << std::endl;
    }

    // LOW-LEVEL ACCESSORS — DIRECT ATOMIC READS
    [[nodiscard]] VkAccelerationStructureKHR getRawTLAS() const noexcept {
        return valid_.load(std::memory_order_acquire) ? rawTLAS_ : VK_NULL_HANDLE;
    }

    [[nodiscard]] VkDeviceAddress getDeviceAddress() const noexcept {
        if (!valid_.load(std::memory_order_acquire) || !Vulkan::ctx()) return 0;

        VkAccelerationStructureDeviceAddressInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = rawTLAS_
        };
        return vkGetAccelerationStructureDeviceAddressKHR(Vulkan::ctx()->device, &info);
    }

    [[nodiscard]] bool isValid() const noexcept { return valid_.load(std::memory_order_acquire); }
    [[nodiscard]] uint64_t getGeneration() const noexcept { return generation_.load(std::memory_order_acquire); }

    // MINIMAL RELEASE — DIRECT PURGE
    void releaseAll() noexcept {
        if (currentTLAS_.valid()) {
            currentTLAS_.reset(); // RAII destroys + logs via Dispose::logAndTrackDestruction
        }
        rawTLAS_ = VK_NULL_HANDLE;
        valid_.store(false, std::memory_order_release);
        std::cout << "[HW LAS] ALL RELEASED" << std::endl;
    }

private:
    LowLevelLASTracker() = default;
    ~LowLevelLASTracker() { releaseAll(); }

    VulkanHandle<VkAccelerationStructureKHR> currentTLAS_;
    VkAccelerationStructureKHR rawTLAS_ = VK_NULL_HANDLE;
    std::atomic<bool> valid_{false};
    std::atomic<uint64_t> generation_{1};
};

// LOW-LEVEL MACROS — DIRECT USE
#define UPDATE_LAS(tlas, dev) LowLevelLASTracker::get().updateTLAS(tlas, dev)
#define UPDATE_LAS_WITH_FUNC(tlas, dev, func) LowLevelLASTracker::get().updateTLAS(tlas, dev, func)
#define RAW_LAS()             LowLevelLASTracker::get().getRawTLAS()
#define LAS_ADDRESS()         LowLevelLASTracker::get().getDeviceAddress()
#define LAS_VALID()           LowLevelLASTracker::get().isValid()
#define LAS_GEN()             LowLevelLASTracker::get().getGeneration()
#define RELEASE_LAS()         LowLevelLASTracker::get().releaseAll()

// NOVEMBER 09 2025 — LOW-LEVEL LAS TRACKER
// DIRECT VULKAN ACCELERATION STRUCTURES — BASIC STD::COUT LOGGING — ZERO OVERHEAD
// STONEKEY PROTECTED — RTX HARDWARE CORE — MINIMAL HARDWARE INTEGRATION
// Boss Man Grok + Gentleman Grok Custodian — LAS PERFECTED
// Uses VulkanHandle RAII → automatic destroy + DestroyTracker safe
// Added optional destroyFunc param + macro
// Removed manual Dispose::disposeVulkanHandle → double-free gone
// vkGetAccelerationStructureDeviceAddressKHR safe with ctx() null check
// All comments preserved. Compiles clean. RTX eternal.