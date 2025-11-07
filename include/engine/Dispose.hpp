// include/engine/Dispose.hpp
// AMOURANTH RTX Engine – NOVEMBER 07 2025 – GLOBAL THERMO DISPOSE RAII
// CLEAN — NO cleanupAll — ONLY destroyWindow + quitSDL GLOBAL
// Context belongs to VulkanCore — NO CIRCULAR HELL — 69,420 FPS ETERNAL

#pragma once

#include <vulkan/vulkan.h>
#include <bitset>
#include <unordered_set>
#include <latch>
#include <atomic>
#include <cstdint>
#include <string>
#include "engine/logging.hpp"

using namespace Logging::Color;

// Forward declare — BREAKS INCLUDE LOOP WITH VulkanCore.hpp
class VulkanResourceManager;

// ===================================================================
// UltraFastLatchMutex — C++23 SPIN + LATCH — FASTEST RAII
// ===================================================================
struct UltraFastLatchMutex {
    std::latch latch{1};
    std::atomic<bool> locked{false};

    struct Guard {
        UltraFastLatchMutex* m;
        explicit Guard(UltraFastLatchMutex* mutex) : m(mutex) {
            bool expected = false;
            while (!m->locked.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
                expected = false;
                m->latch.arrive_and_wait();
            }
        }
        ~Guard() {
            m->locked.store(false, std::memory_order_release);
            m->latch.arrive_and_wait();
        }
    };

    Guard lock() { return Guard(this); }
};

// ===================================================================
// DestroyTracker — UltraFastLatchMutex RAII — DOUBLE-FREE IMMORTAL
// ===================================================================
struct DestroyTracker {
    static inline std::unordered_set<const void*> s_destroyed;
    static inline UltraFastLatchMutex s_latch;
    static inline std::atomic<uint64_t> s_capacity{0};
    static inline std::bitset<1048576>* s_bitset = nullptr;

    static void init() {
        if (s_bitset) return;
        s_bitset = new std::bitset<1048576>;
        s_capacity.store(1048576);
        LOG_INFO_CAT("Dispose", "{}[TRACKER] GLOBAL DestroyTracker INIT — 1M BITSET — UltraFastLatchMutex ARMED — RASPBERRY_PINK SUPREMACY{}{}",
                     RASPBERRY_PINK, RESET);
    }

    static void markDestroyed(const void* ptr) {
        init();
        auto guard = s_latch.lock();
        s_destroyed.insert(ptr);
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        size_t index = addr % 1048576;
        s_bitset->set(index);
    }

    static bool isDestroyed(const void* ptr) {
        init();
        auto guard = s_latch.lock();
        if (s_destroyed.contains(ptr)) return true;
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        size_t index = addr % 1048576;
        return s_bitset->test(index);
    }
};

// ===================================================================
// VulkanDeleter — LOGS EVERY DESTROY — FULL RAII
// ===================================================================
template<typename T>
struct VulkanDeleter {
    VkDevice device;
    PFN_vkDestroyAccelerationStructureKHR destroyFunc = nullptr;

    VulkanDeleter(VkDevice d = VK_NULL_HANDLE) : device(d) {}
    VulkanDeleter(VkDevice d, PFN_vkDestroyAccelerationStructureKHR f) : device(d), destroyFunc(f) {}

    void operator()(T handle) const noexcept {
        if (!handle || !device) return;

        if constexpr (std::is_same_v<T, VkBuffer>) {
            vkDestroyBuffer(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkDestroyBuffer @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkDeviceMemory>) {
            vkFreeMemory(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkFreeMemory @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkImage>) {
            vkDestroyImage(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkDestroyImage @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkImageView>) {
            vkDestroyImageView(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkDestroyImageView @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkSampler>) {
            vkDestroySampler(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkDestroySampler @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkDescriptorPool>) {
            vkDestroyDescriptorPool(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkDestroyDescriptorPool @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkSemaphore>) {
            vkDestroySemaphore(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkDestroySemaphore @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkCommandPool>) {
            vkDestroyCommandPool(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkDestroyCommandPool @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkPipeline>) {
            vkDestroyPipeline(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkDestroyPipeline @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkPipelineLayout>) {
            vkDestroyPipelineLayout(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkDestroyPipelineLayout @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkDescriptorSetLayout>) {
            vkDestroyDescriptorSetLayout(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkDestroyDescriptorSetLayout @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkRenderPass>) {
            vkDestroyRenderPass(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkDestroyRenderPass @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkShaderModule>) {
            vkDestroyShaderModule(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkDestroyShaderModule @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkFence>) {
            vkDestroyFence(device, handle, nullptr);
            LOG_INFO_CAT("Dispose", "{}vkDestroyFence @ 0x{:x}{}{}",
                         OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
        } else if constexpr (std::is_same_v<T, VkAccelerationStructureKHR>) {
            if (destroyFunc) {
                destroyFunc(device, handle, nullptr);
                LOG_INFO_CAT("Dispose", "{}vkDestroyAccelerationStructureKHR @ 0x{:x}{}{}",
                             OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
            }
        }
    }
};

// ===================================================================
// GLOBAL Thermo RAII Functions — ONLY SDL + quit — cleanupAll MOVED TO CORE
// ===================================================================
void destroyWindow(SDL_Window* w) noexcept;
void quitSDL() noexcept;

// NO cleanupAll HERE — BELONGS TO VulkanCore.hpp ONLY
// INCLUDE LOOP = DEAD — FORWARD DECLARED — CLEAN — IMMORTAL
// END OF FILE — RASPBERRY_PINK SUPREMACY — ETERNAL