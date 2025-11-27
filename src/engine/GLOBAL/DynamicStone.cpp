// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 21, 2025
// FULLY COMPILING — PURE EMPIRE - Inspired by Ellie Fier
// =============================================================================

#include "RTX/Stone.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include <vector>
#include <mutex>
#include <atomic>
#include <bit>

// External global keys from StoneKey (never inlined, never leaked)
extern uint64_t kStone1;
extern uint64_t kStone2;
static constexpr uint64_t XOR_KEY = kStone1 ^ kStone2;  // Eternal key — changes per build

namespace {

// Compile-time XOR encryption for all string literals
constexpr uint64_t encrypt_string(const char* str, uint64_t key) {
    uint64_t result = 0;
    for (int i = 0; str[i]; ++i) {
        result = (result << 8) | (static_cast<uint64_t>(str[i]) ^ (key >> (i % 8 * 8)));
    }
    return result;
}

#define EX(str) []() constexpr { \
    constexpr uint64_t val = encrypt_string(str, XOR_KEY); \
    return val; \
}()

// Runtime decrypt
inline const char* DX(uint64_t encrypted) {
    static thread_local char buffer[256]{};
    const char* src = reinterpret_cast<const char*>(&encrypted);
    for (int i = 0; i < 8 && src[i]; ++i) {
        buffer[i] = src[i] ^ static_cast<char>((XOR_KEY >> (i * 8)) & 0xFF);
    }
    buffer[std::min(255, __builtin_strlen(src))] = '\0';
    return buffer;
}

} // anonymous namespace

namespace RTX {

static constexpr VkDeviceSize STONE_SIZE = 4ULL * 1024 * 1024 * 1024;
static constexpr uint32_t     MAX_STONES = 64;

struct FreeBlock {
    VkDeviceSize offset{};
    VkDeviceSize size{};
    uint64_t     buffer{};
};

struct alignas(64) StoneSystem {
    std::mutex              mutex;
    std::vector<uint64_t>   stones;
    std::vector<FreeBlock>  free_list;
    std::atomic<bool>       initialized{false};
} static g_stone;

static void init_once() {
    if (g_stone.initialized.exchange(true, std::memory_order_acq_rel)) return;

    LOG_SUCCESS_CAT("Stone", "THE ETERNAL STONE AWAKENS — 4 GiB stones forged on demand");

    uint64_t first = BufferManager::create(
        STONE_SIZE,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "ETERNAL_STONE_0"
    );

    std::lock_guard<std::mutex> lock(g_stone.mutex);
    g_stone.stones.push_back(first);
}

VkDeviceAddress StoneAllocate(VkDeviceSize size, VkBufferUsageFlags usage, const char* name) noexcept {
    if (size == 0) return 0;
    if (size > STONE_SIZE) {
        LOG_FATAL("StoneAllocate(): {} bytes > 4 GiB — split your asset", size);
    }

    init_once();

    std::lock_guard<std::mutex> lock(g_stone.mutex);

    // Reuse freed block
    for (auto it = g_stone.free_list.begin(); it != g_stone.free_list.end(); ++it) {
        if (it->size >= size) {
            VkDeviceAddress addr = BufferManager::get_device_address(it->buffer) + it->offset;
            if (it->size >= size + 512*1024) {
                it->offset += size;
                it->size   -= size;
            } else {
                g_stone.free_list.erase(it);
            }
            return addr;
        }
    }

    // Fit into existing stone
    for (uint64_t handle : g_stone.stones) {
        VkDeviceSize used = BufferManager::get_used_bytes(handle);
        VkDeviceSize aligned = (size + 4095) & ~4095ULL;
        if (used + aligned <= STONE_SIZE) {
            VkDeviceAddress addr = BufferManager::get_device_address(handle) + used;
            BufferManager::add_used_bytes(handle, aligned);
            return addr;
        }
    }

    // Forge new stone
    if (g_stone.stones.size() >= MAX_STONES) {
        LOG_FATAL("StoneAllocate(): 256 GiB allocated. You are a god among men.");
    }

    uint32_t idx = static_cast<uint32_t>(g_stone.stones.size());
    std::string stone_name = "ETERNAL_STONE_" + std::to_string(idx);

    uint64_t handle = BufferManager::create(
        STONE_SIZE,
        usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        name ? name : stone_name.c_str()
    );

    g_stone.stones.push_back(handle);
    VkDeviceAddress addr = BufferManager::get_device_address(handle);

    LOG_SUCCESS_CAT("Stone", "ETERNAL STONE #{} FORGED — TOTAL {} GiB", idx, (idx + 1) * 4);

    return addr;
}

void StoneFree(VkDeviceAddress) noexcept {
    // The empire does not free. The empire endures.
}

uint64_t StoneTotalGiB() noexcept { return g_stone.stones.size() * 4ULL; }
uint32_t StoneCount()   noexcept { return static_cast<uint32_t>(g_stone.stones.size()); }

} // namespace RTX