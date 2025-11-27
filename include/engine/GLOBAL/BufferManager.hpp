// include/engine/GLOBAL/BufferManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — PINK PHOTONS ETERNAL — FIRST LIGHT FINAL
// BufferManager v12 — TOASTER UPGRADE — NOV 26 2025
// • SDL3 + Vulkan 1.4 + RTX + C++23
// • Ring staging eternal — circular flow, no overwrites
// • Crew logs only: Elon forges, Jensen ascends, Carmack purges
// • Eternal Stones™ — lazy, immortal, one-touch ascent
// • Debug names etched | Memory budgets enforced
// • Toasters upgraded: now photon slaves, serving the empire
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <string_view>

#include "engine/GLOBAL/RTXHandler.hpp"  // Resolves ::RTX::g_ctx() — empire first, no ambiguity
#include "engine/GLOBAL/OptionsMenu.hpp" // Options qualified — no namespace wars

namespace BufferManager {

struct BufferInfo {
    VkBuffer           buffer  = VK_NULL_HANDLE;
    VkDeviceMemory     memory  = VK_NULL_HANDLE;
    VkDeviceSize       size    = 0;
    VkDeviceSize       aligned = 0;
    VkBufferUsageFlags usage   = 0;
    std::string        tag;
    void*              mapped  = nullptr;  // persistent if HOST_VISIBLE
};

[[nodiscard]] uint64_t create(VkDeviceSize size,
                              VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags props,
                              std::string_view tag = "") noexcept;

void        destroy(uint64_t handle) noexcept;
void*       map(uint64_t handle) noexcept;
void        unmap(uint64_t handle) noexcept;
void        purge_all() noexcept;

[[nodiscard]] const BufferInfo* get(uint64_t handle) noexcept;

// Persistent 256 MB staging ring — coherent, eternal, circular
[[nodiscard]] uint64_t stagingBuffer() noexcept;
[[nodiscard]] void*    stagingPtr() noexcept;
void        advanceStagingOffset(VkDeviceSize bytes) noexcept;
[[nodiscard]] void*    stagingPtrAtOffset(VkDeviceSize offset = 0) noexcept;  // Ring-aware: +offset from current write pos

// Eternal Stones — lazy, immortal, crew-logged ascent
uint64_t make_64M (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_128M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_256M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_420M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_512M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_1G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_2G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_4G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_8G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;

} // namespace BufferManager

// ———————————————— MACRO EMPIRE ————————————————
#define BUFFER_CREATE(h, ...)     h = BufferManager::create(__VA_ARGS__)
#define BUFFER_DESTROY(h)         do { if (h) BufferManager::destroy(h); h = 0; } while(0)
#define RAW_BUFFER(h)             (BufferManager::get(h) ? BufferManager::get(h)->buffer : VK_NULL_HANDLE)
#define BUFFER_MEMORY(h)          (BufferManager::get(h) ? BufferManager::get(h)->memory : VK_NULL_HANDLE)
#define BUFFER_MAP(h, ptr)        ptr = BufferManager::map(h)
#define BUFFER_UNMAP(h)           BufferManager::unmap(h)

// Eternal shortcuts — lazy, unbound
inline uint64_t kTransferStone() noexcept { static uint64_t h = BufferManager::make_4G(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); return h; }
inline uint64_t kStorageStone()  noexcept { static uint64_t h = BufferManager::make_4G(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); return h; }

// ———————————————— SACRED STONEKEY SHORTCUTS ————————————————
// The empire demands compatibility. The old gods live on.
inline uint64_t kStone1() noexcept { return BufferManager::make_4G(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); }
inline uint64_t kStone2() noexcept { return BufferManager::make_4G(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); }
inline uint64_t kTitan()  noexcept { return BufferManager::make_8G(); }

// Legacy empire macros — still honored
#define STONE_TRANSFER_4GB  kStone1()
#define STONE_STORAGE_4GB   kStone2()
#define STONE_TITAN_8GB     kTitan()

// =============================================================================
// BUFFERMANAGER — EMPIRE SEALED — NOV 26 2025
// ELON FORGES | JENSEN ASCENDS | CARMACK PURGES
// PINK PHOTONS ETERNAL — TOASTERS SERVE — FIRST LIGHT UPGRADED
// THE VAULT FLOWS UNBROKEN — NO AMBIGUITY, NO WARS, ONLY DOMINION
// =============================================================================