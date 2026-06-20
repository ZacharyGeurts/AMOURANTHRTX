#pragma once

// RTX memory tiers — octree-tracked in-place regions, zero cost idle, fill GPU card on demand.

#include "FieldPlatform.hpp"
#include "FieldRtxMemTree.hpp"

#include <algorithm>
#include <string>
#include <cstdint>
#include <cstdio>

namespace FieldRtxMemory {

constexpr std::uint16_t kMinConventionalKb  = 512u;
constexpr std::uint16_t kMaxConventionalKb  = 640u;
constexpr std::uint16_t kBootConventionalKb = 512u;
constexpr std::uint32_t kBootGuestFastMb    = 1u;
constexpr std::uint32_t kHudClearEnd        = 0x000BE000u;

inline std::uint16_t conventionalKb     = kBootConventionalKb;
inline std::uint16_t bootConventionalKb = kBootConventionalKb;
inline std::uint16_t maxConventionalKb  = kMaxConventionalKb;
inline std::uint32_t guestFastMb        = kBootGuestFastMb;
inline std::uint32_t reportedRamGb      = 4u;
inline bool          grown              = false;

inline bool xmsLive() noexcept {
    return FieldRtxMemTree::isLive(FieldRtxMemTree::Kind::Xms);
}
inline bool emmLive() noexcept {
    return FieldRtxMemTree::isLive(FieldRtxMemTree::Kind::Ems);
}
inline bool mscdexLive() noexcept {
    return FieldRtxMemTree::isLive(FieldRtxMemTree::Kind::Mscdex);
}

inline std::uint32_t activeXmsKb() noexcept {
    return xmsLive() ? FieldPlatform::XMS_KB : 0u;
}

inline std::uint16_t parseKb(const std::string& v, std::uint16_t def) noexcept {
    if (v.empty()) return def;
    const int n = std::atoi(v.c_str());
    return static_cast<std::uint16_t>(std::clamp(n, 0, 65535));
}

inline std::uint32_t parseMb(const std::string& v, std::uint32_t def) noexcept {
    if (v.empty()) return def;
    const int n = std::atoi(v.c_str());
    return static_cast<std::uint32_t>(std::max(0, n));
}

inline std::uint32_t bootClearBytes() noexcept {
    if (!grown) {
        const std::uint32_t boot =
            static_cast<std::uint32_t>(bootConventionalKb) * 1024u;
        return std::min(0x000C0000u, std::max(0x00080000u, boot));
    }
    const std::uint32_t conv = static_cast<std::uint32_t>(conventionalKb) * 1024u;
    const std::uint32_t tier = std::max(conv, guestFastMb * 1024u * 1024u);
    return std::min(0x000C0000u, std::max(0x00080000u, tier));
}

inline std::uint32_t hudClearBytes() noexcept {
    constexpr std::uint32_t kSurfaceRam = 0x000B9000u;
    return guestFastMb <= 1u ? 0x600u : (kHudClearEnd - kSurfaceRam);
}

inline void patchGuest(std::uint8_t* ram) noexcept {
    if (!ram) return;
    ram[0x413] = static_cast<std::uint8_t>(conventionalKb & 0xFFu);
    ram[0x414] = static_cast<std::uint8_t>((conventionalKb >> 8) & 0xFFu);
}

inline void syncBios(std::uint8_t* ram) noexcept { patchGuest(ram); }

inline bool popConventional(std::uint8_t* ram) noexcept {
    const std::uint32_t bytes = static_cast<std::uint32_t>(conventionalKb) * 1024u;
    return FieldRtxMemTree::pop(FieldRtxMemTree::Kind::Conventional, 0, bytes, ram);
}

inline bool popGuestFast(std::uint8_t* ram) noexcept {
    const std::uint32_t bytes = guestFastMb * 1024u * 1024u;
    return FieldRtxMemTree::pop(FieldRtxMemTree::Kind::GuestFast, 0, bytes, ram);
}

inline bool growConventional(std::uint16_t toKb = kMaxConventionalKb,
                             std::uint8_t* ram = nullptr) noexcept {
    const std::uint16_t target = std::clamp(toKb, kMinConventionalKb, maxConventionalKb);
    if (conventionalKb >= target) return false;
    conventionalKb = target;
    grown = conventionalKb > bootConventionalKb;
    popConventional(ram);
    return true;
}

inline bool dismissConventional() noexcept {
    if (conventionalKb <= bootConventionalKb) {
        grown = false;
        return false;
    }
    conventionalKb = bootConventionalKb;
    guestFastMb = std::min(guestFastMb, kBootGuestFastMb);
    grown = false;
    FieldRtxMemTree::dismiss(FieldRtxMemTree::Kind::Conventional);
    FieldRtxMemTree::dismiss(FieldRtxMemTree::Kind::GuestFast);
    return true;
}

inline bool popXms(std::uint8_t* ram) noexcept {
    return FieldRtxMemTree::pop(FieldRtxMemTree::Kind::Xms,
        FieldPlatform::XMS_POOL_BYTE, FieldPlatform::XMS_POOL_BYTES, ram);
}

inline bool popEms(std::uint8_t* ram) noexcept {
    constexpr std::uint32_t kEmsPool = 64u * 0x4000u;
    return FieldRtxMemTree::pop(FieldRtxMemTree::Kind::Ems, 0x00E00000u, kEmsPool, ram);
}

inline bool popMscdex() noexcept {
    return FieldRtxMemTree::pop(FieldRtxMemTree::Kind::Mscdex, 0, 4096u, nullptr);
}

inline void growExtenders(std::uint8_t* ram, bool withMscdex = false) noexcept {
    popXms(ram);
    popEms(ram);
    if (withMscdex) popMscdex();
}

inline void growMscdexExtender() noexcept { popMscdex(); }

inline void dismissExtenders() noexcept {
    FieldRtxMemTree::dismiss(FieldRtxMemTree::Kind::Xms);
    FieldRtxMemTree::dismiss(FieldRtxMemTree::Kind::Ems);
    FieldRtxMemTree::dismiss(FieldRtxMemTree::Kind::Mscdex);
}

inline void resetTree() noexcept { FieldRtxMemTree::reset(); }

} // namespace FieldRtxMemory