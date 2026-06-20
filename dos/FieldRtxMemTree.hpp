#pragma once

// RTX memory octree — in-place regions, zero cost at idle, resume checkpoints on dismiss.
// Linear guest address space is subdivided power-of-two (sparse-voxel style); leaves pop
// when a workload needs them and dismiss when refs hit zero. Spill grows toward GPU card.

#include "FieldPlatform.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace FieldRtxMemTree {

enum class Kind : std::uint8_t {
    None = 0,
    Conventional,
    GuestFast,
    Xms,
    Ems,
    Mscdex,
    HdMirror,
    Hud,
};

struct Leaf {
    Kind          kind     = Kind::None;
    std::uint64_t base     = 0;
    std::uint32_t size     = 0;
    std::uint8_t  depth    = 0;
    bool          live     = false;
    std::uint32_t refs     = 0;
    std::uint64_t resume   = 0;
};

constexpr std::uint8_t  kMaxDepth  = 24u;
constexpr std::uint64_t kRootSpan  = 1ull << 32;

inline std::vector<Leaf> leaves;
inline std::uint64_t       bytesLive   = 0;
inline std::uint64_t       cardBudget  = 0;

inline void setCardBudget(std::uint64_t bytes) noexcept { cardBudget = bytes; }

inline std::uint64_t budgetBytes() noexcept {
    if (cardBudget > 0) return cardBudget;
    return static_cast<std::uint64_t>(FieldPlatform::GUEST_RAM_BYTES);
}

inline std::uint8_t depthForSize(std::uint32_t size) noexcept {
    std::uint64_t span = std::max<std::uint64_t>(size, 1u);
    std::uint8_t d = 0;
    while (span < kRootSpan && d < kMaxDepth) {
        ++d;
        span <<= 1;
    }
    return d;
}

inline Leaf* findKind(Kind k) noexcept {
    for (auto& L : leaves)
        if (L.kind == k) return &L;
    return nullptr;
}

inline bool touchRange(std::uint8_t* ram, std::uint64_t base, std::uint32_t size) noexcept {
    if (size == 0) return true;
    const std::uint64_t end = base + size;
    const std::uint64_t dieEnd = FieldPlatform::GUEST_RAM_BYTES;
    if (end <= dieEnd) {
        if (ram) {
            const std::uint32_t b = static_cast<std::uint32_t>(base);
            const std::uint32_t e = static_cast<std::uint32_t>(std::min(end, dieEnd));
            if (e > b) std::memset(ram + b, 0, e - b);
        }
        return true;
    }
    return true;
}

inline bool pop(Kind k, std::uint64_t base, std::uint32_t size,
                std::uint8_t* ram = nullptr, std::uint32_t refs = 1) noexcept {
    Leaf* L = findKind(k);
    if (!L) {
        leaves.push_back({});
        L = &leaves.back();
        L->kind = k;
        L->base = base;
        L->size = size;
        L->depth = depthForSize(size);
    }
    if (L->live) {
        L->refs += refs;
        return true;
    }
    if (bytesLive + size > budgetBytes()) {
        std::fprintf(stderr, "[RTXMEM] Octree pop %u blocked — card budget %llu\n",
            static_cast<unsigned>(k),
            static_cast<unsigned long long>(budgetBytes()));
        return false;
    }
    if (!touchRange(ram, base, size)) return false;
    L->live = true;
    L->refs = refs;
    L->resume = base;
    bytesLive += size;
    std::fprintf(stderr,
        "[RTXMEM] Pop in-place kind=%u base=%llx size=%u depth=%u (live %llu)\n",
        static_cast<unsigned>(k),
        static_cast<unsigned long long>(base),
        size, static_cast<unsigned>(L->depth),
        static_cast<unsigned long long>(bytesLive));
    return true;
}

inline bool dismiss(Kind k, std::uint64_t resumeOff = 0) noexcept {
    Leaf* L = findKind(k);
    if (!L || !L->live) return false;
    if (L->refs > 0) --L->refs;
    if (L->refs > 0) return false;
    L->resume = resumeOff ? resumeOff : L->base;
    L->live = false;
    if (bytesLive >= L->size) bytesLive -= L->size;
    std::fprintf(stderr,
        "[RTXMEM] Dismiss kind=%u resume=%llx (live %llu)\n",
        static_cast<unsigned>(k),
        static_cast<unsigned long long>(L->resume),
        static_cast<unsigned long long>(bytesLive));
    return true;
}

inline bool isLive(Kind k) noexcept {
    const Leaf* L = findKind(k);
    return L && L->live;
}

inline std::uint64_t resumeAt(Kind k) noexcept {
    const Leaf* L = findKind(k);
    return L ? L->resume : 0;
}

inline void reset() noexcept {
    leaves.clear();
    bytesLive = 0;
}

} // namespace FieldRtxMemTree