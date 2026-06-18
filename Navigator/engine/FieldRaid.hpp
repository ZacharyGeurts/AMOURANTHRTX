#pragma once

// Field RAID-0 — host + VRAM spill tiers with journal, background sync, contiguous facade.

#include "FieldDos.hpp"
#include "FieldPlatform.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace FieldRaid {

enum class Tier : std::uint8_t { Host = 0, Vram = 1 };

constexpr std::uint32_t kStripeBytes = FieldPlatform::RAID_STRIPE_BYTES;
constexpr std::uint64_t kHdLogical   = FieldPlatform::HD_SIZE_BYTES;
constexpr std::uint64_t kRamLogical  = FieldPlatform::REPORTED_RAM_BYTES;

inline std::vector<std::uint8_t> vramHd;
inline std::vector<std::uint8_t> vramRam;
inline std::vector<std::uint8_t> journal;
inline std::vector<std::uint8_t> dirtyHd;
inline std::vector<std::uint8_t> dirtyRam;
inline std::uint32_t tickCursorHd = 0u;
inline std::uint32_t tickCursorRam = 0u;
inline bool ready = false;

inline std::uint32_t stripeOf(std::uint64_t off) noexcept {
    return static_cast<std::uint32_t>(off / kStripeBytes);
}

inline Tier tierOf(std::uint32_t stripe) noexcept {
    return (stripe & 1u) ? Tier::Vram : Tier::Host;
}

inline std::uint64_t stripeBase(std::uint32_t stripe) noexcept {
    return static_cast<std::uint64_t>(stripe) * kStripeBytes;
}

inline void ensureBitmap(std::vector<std::uint8_t>& bits, std::uint32_t stripes) noexcept {
    const std::size_t need = (static_cast<std::size_t>(stripes) + 7u) / 8u;
    if (bits.size() < need) bits.resize(need, 0);
}

inline void markDirty(std::vector<std::uint8_t>& bits, std::uint32_t stripe) noexcept {
    ensureBitmap(bits, stripe + 1u);
    bits[stripe / 8u] |= static_cast<std::uint8_t>(1u << (stripe & 7u));
}

inline bool isDirty(const std::vector<std::uint8_t>& bits, std::uint32_t stripe) noexcept {
    if (stripe / 8u >= bits.size()) return false;
    return (bits[stripe / 8u] & static_cast<std::uint8_t>(1u << (stripe & 7u))) != 0;
}

inline void clearDirty(std::vector<std::uint8_t>& bits, std::uint32_t stripe) noexcept {
    if (stripe / 8u >= bits.size()) return;
    bits[stripe / 8u] &= static_cast<std::uint8_t>(~(1u << (stripe & 7u)));
}

inline std::filesystem::path journalPath() noexcept {
    if (FieldDos::hdImagePath.empty()) return {};
    return FieldDos::hdImagePath.string() + ".journal";
}

inline void appendJournal(std::uint8_t space, std::uint64_t off, std::uint8_t val) noexcept {
    if (journal.size() + 10u > FieldPlatform::RAID_JOURNAL_CAP) return;
    journal.push_back(space);
    journal.push_back(static_cast<std::uint8_t>(off));
    journal.push_back(static_cast<std::uint8_t>(off >> 8));
    journal.push_back(static_cast<std::uint8_t>(off >> 16));
    journal.push_back(static_cast<std::uint8_t>(off >> 24));
    journal.push_back(static_cast<std::uint8_t>(off >> 32));
    journal.push_back(static_cast<std::uint8_t>(off >> 40));
    journal.push_back(static_cast<std::uint8_t>(off >> 48));
    journal.push_back(static_cast<std::uint8_t>(off >> 56));
    journal.push_back(val);
}

inline bool replayJournal() noexcept {
    const auto path = journalPath();
    if (path.empty() || !std::filesystem::exists(path)) return true;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::vector<std::uint8_t> rec;
    rec.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    for (std::size_t i = 0; i + 10u <= rec.size(); i += 10u) {
        const std::uint8_t space = rec[i];
        std::uint64_t off = rec[i + 1];
        for (int s = 1; s < 8; ++s)
            off |= static_cast<std::uint64_t>(rec[i + 1 + static_cast<std::size_t>(s)]) << (8 * s);
        const std::uint8_t val = rec[i + 9];
        if (space == 0u) {
            if (off < FieldDos::hdImage.size())
                FieldDos::hdImage[static_cast<std::size_t>(off)] = val;
        } else if (space == 1u && off >= FieldPlatform::SPILL_RAM_BASE) {
            const std::uint64_t rel = off - FieldPlatform::SPILL_RAM_BASE;
            if (rel < vramRam.size())
                vramRam[static_cast<std::size_t>(rel)] = val;
        }
    }
    journal = std::move(rec);
    return true;
}

inline bool flushJournal() noexcept {
    const auto path = journalPath();
    if (path.empty() || journal.empty()) return true;
    const auto tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(journal.data()),
                  static_cast<std::streamsize>(journal.size()));
        if (!out) return false;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(tmp, path, ec);
    }
    return !ec;
}

inline void ensureHdLogical() noexcept {
    if (FieldDos::hdImage.size() < kHdLogical)
        FieldDos::hdImage.resize(static_cast<std::size_t>(kHdLogical), 0);
}

inline void ensureVramHd() noexcept {
    /* Grow on demand per stripe — avoid allocating full 2 GiB VRAM tier up front. */
    if (vramHd.capacity() < kStripeBytes) vramHd.reserve(kStripeBytes * 64u);
}

inline void ensureVramRam() noexcept {
    if (vramRam.capacity() < kStripeBytes) vramRam.reserve(kStripeBytes * 64u);
}

inline std::uint8_t* hdHostPtr(std::uint64_t off) noexcept {
    if (off >= FieldDos::hdImage.size()) return nullptr;
    return FieldDos::hdImage.data() + off;
}

inline std::uint8_t* hdVramPtr(std::uint64_t off) noexcept {
    const std::uint32_t stripe = stripeOf(off);
    if (tierOf(stripe) != Tier::Vram) return nullptr;
    const std::uint64_t rel = stripeBase(stripe) / 2u + (off - stripeBase(stripe));
    if (rel >= vramHd.size()) vramHd.resize(static_cast<std::size_t>(rel) + 1u, 0);
    return vramHd.data() + rel;
}

inline std::uint8_t readHdHost(std::uint64_t off) noexcept {
    const auto* p = hdHostPtr(off);
    return p ? *p : 0u;
}

inline std::uint8_t readHdVram(std::uint64_t off) noexcept {
    const auto* p = hdVramPtr(off);
    return p ? *p : 0u;
}

inline void writeHdHost(std::uint64_t off, std::uint8_t val) noexcept {
    if (off >= FieldDos::hdImage.size())
        FieldDos::hdImage.resize(static_cast<std::size_t>(off) + 1u, 0);
    FieldDos::hdImage[static_cast<std::size_t>(off)] = val;
}

inline void writeHdVram(std::uint64_t off, std::uint8_t val) noexcept {
    ensureVramHd();
    auto* p = hdVramPtr(off);
    if (p) *p = val;
}

inline void mirrorWrite(std::uint64_t off, std::uint8_t val, std::uint8_t* guestRam) noexcept {
    if (!guestRam || off >= FieldDos::hdMirrorBytes) return;
    const std::uint32_t base = FieldPlatform::HD_MIRROR_BYTE;
    if (base + off < FieldPlatform::GUEST_RAM_BYTES)
        guestRam[base + static_cast<std::uint32_t>(off)] = val;
}

inline std::uint8_t mirrorRead(std::uint64_t off, const std::uint8_t* guestRam) noexcept {
    if (!guestRam || off >= FieldDos::hdMirrorBytes) return 0u;
    const std::uint32_t base = FieldPlatform::HD_MIRROR_BYTE;
    if (base + off >= FieldPlatform::GUEST_RAM_BYTES) return 0u;
    return guestRam[base + static_cast<std::uint32_t>(off)];
}

inline std::uint8_t readHd(std::uint64_t off, const std::uint8_t* guestRam) noexcept {
    if (off >= kHdLogical) return 0u;
    if (guestRam && off < FieldDos::hdMirrorBytes)
        return mirrorRead(off, guestRam);
    const std::uint32_t stripe = stripeOf(off);
    return tierOf(stripe) == Tier::Host ? readHdHost(off) : readHdVram(off);
}

inline void writeHd(std::uint64_t off, std::uint8_t val, std::uint8_t* guestRam) noexcept {
    if (off >= kHdLogical) return;
    appendJournal(0u, off, val);
    const std::uint32_t stripe = stripeOf(off);
    if (tierOf(stripe) == Tier::Host)
        writeHdHost(off, val);
    else
        writeHdVram(off, val);
    markDirty(dirtyHd, stripe);
    mirrorWrite(off, val, guestRam);
}

inline std::uint8_t readRam(std::uint64_t off, const std::uint8_t* guestRam) noexcept {
    if (off >= kRamLogical) return 0u;
    if (off < FieldPlatform::GUEST_RAM_BYTES && guestRam)
        return guestRam[static_cast<std::uint32_t>(off)];
    const std::uint64_t spill = off - FieldPlatform::SPILL_RAM_BASE;
    if (spill >= vramRam.size()) return 0u; // sparse — unmapped spill reads as zero
    const std::uint32_t stripe = stripeOf(spill);
    if (tierOf(stripe) == Tier::Host) {
        const std::uint64_t hostOff = stripeBase(stripe) / 2u + (spill - stripeBase(stripe));
        if (hostOff >= vramRam.size()) return 0u;
        return vramRam[static_cast<std::size_t>(hostOff)];
    }
    return vramRam[static_cast<std::size_t>(spill)];
}

inline void writeRam(std::uint64_t off, std::uint8_t val, std::uint8_t* guestRam) noexcept {
    if (off >= kRamLogical) return;
    if (off < FieldPlatform::GUEST_RAM_BYTES && guestRam) {
        guestRam[static_cast<std::uint32_t>(off)] = val;
        return;
    }
    const std::uint64_t spill = off - FieldPlatform::SPILL_RAM_BASE;
    appendJournal(1u, off, val);
    const std::uint32_t stripe = stripeOf(spill);
    if (tierOf(stripe) == Tier::Host) {
        const std::uint64_t hostOff = stripeBase(stripe) / 2u + (spill - stripeBase(stripe));
        if (hostOff >= vramRam.size()) vramRam.resize(static_cast<std::size_t>(hostOff) + 1u, 0);
        vramRam[static_cast<std::size_t>(hostOff)] = val;
    } else {
        if (spill >= vramRam.size()) vramRam.resize(static_cast<std::size_t>(spill) + 1u, 0);
        vramRam[static_cast<std::size_t>(spill)] = val;
    }
    markDirty(dirtyRam, stripe);
}

inline void syncStripeHd(std::uint32_t stripe, std::uint8_t* guestRam) noexcept {
    const std::uint64_t base = stripeBase(stripe);
    const std::uint32_t len = kStripeBytes;
    if (tierOf(stripe) == Tier::Host) {
        for (std::uint32_t i = 0; i < len && base + i < kHdLogical; ++i) {
            const std::uint8_t v = readHdHost(base + i);
            writeHdVram(base + i, v);
            mirrorWrite(base + i, v, guestRam);
        }
    } else {
        for (std::uint32_t i = 0; i < len && base + i < kHdLogical; ++i) {
            const std::uint8_t v = readHdVram(base + i);
            writeHdHost(base + i, v);
            mirrorWrite(base + i, v, guestRam);
        }
    }
    clearDirty(dirtyHd, stripe);
}

inline void syncStripeRam(std::uint32_t stripe) noexcept {
    ensureVramRam();
    const std::uint64_t base = stripeBase(stripe);
    const std::uint32_t len = kStripeBytes;
    if (tierOf(stripe) == Tier::Host) {
        for (std::uint32_t i = 0; i < len; ++i) {
            const std::uint64_t spill = base + i;
            if (spill >= vramRam.size()) break;
            const std::uint64_t hostOff = stripeBase(stripe) / 2u + (spill - stripeBase(stripe));
            if (hostOff >= vramRam.size()) break;
            vramRam[static_cast<std::size_t>(spill)] = vramRam[static_cast<std::size_t>(hostOff)];
        }
    } else {
        for (std::uint32_t i = 0; i < len; ++i) {
            const std::uint64_t spill = base + i;
            if (spill >= vramRam.size()) break;
            const std::uint64_t hostOff = stripeBase(stripe) / 2u + (spill - stripeBase(stripe));
            if (hostOff >= vramRam.size()) break;
            vramRam[static_cast<std::size_t>(hostOff)] = vramRam[static_cast<std::size_t>(spill)];
        }
    }
    clearDirty(dirtyRam, stripe);
}

inline void onHostRangeTouched(std::uint64_t off, std::uint32_t len,
                               std::uint8_t* guestRam) noexcept {
    if (!ready || len == 0u) return;
    const std::uint64_t end = std::min(off + len, kHdLogical);
    const std::uint32_t stripeLo = stripeOf(off);
    const std::uint32_t stripeHi = stripeOf(end > 0u ? end - 1u : 0u);
    for (std::uint32_t s = stripeLo; s <= stripeHi; ++s)
        markDirty(dirtyHd, s);
    if (guestRam && off < FieldDos::hdMirrorBytes) {
        const std::uint64_t mEnd = std::min(end, static_cast<std::uint64_t>(FieldDos::hdMirrorBytes));
        for (std::uint64_t p = off; p < mEnd; ++p)
            mirrorWrite(p, readHdHost(p), guestRam);
    }
}

inline void init() noexcept {
    ready = false;
    vramHd.clear();
    vramRam.clear();
    journal.clear();
    dirtyHd.clear();
    dirtyRam.clear();
    tickCursorHd = tickCursorRam = 0u;
    if (!FieldDos::hdReady) return;
    ensureVramHd();
    ensureVramRam();
    replayJournal();
    ready = true;
}

inline void tick(std::uint8_t* guestRam, float /*dt*/) noexcept {
    if (!ready) return;
    std::uint32_t budget = FieldPlatform::RAID_TICK_BUDGET;
    const std::uint32_t hdStripes = static_cast<std::uint32_t>(
        std::max<std::uint64_t>(1u, (kHdLogical + kStripeBytes - 1u) / kStripeBytes));
    std::uint32_t hdScanned = 0u;
    while (budget > 0u && hdScanned < hdStripes) {
        if (isDirty(dirtyHd, tickCursorHd)) {
            syncStripeHd(tickCursorHd, guestRam);
            budget = (budget > kStripeBytes) ? budget - kStripeBytes : 0u;
        }
        tickCursorHd = (tickCursorHd + 1u) % hdStripes;
        ++hdScanned;
    }
    const std::uint32_t ramStripes = static_cast<std::uint32_t>(std::max<std::size_t>(
        1u, (vramRam.size() + kStripeBytes - 1u) / kStripeBytes));
    std::uint32_t ramScanned = 0u;
    while (budget > 0u && ramScanned < ramStripes) {
        if (isDirty(dirtyRam, tickCursorRam)) {
            syncStripeRam(tickCursorRam);
            budget = (budget > kStripeBytes) ? budget - kStripeBytes : 0u;
        }
        tickCursorRam = (tickCursorRam + 1u) % ramStripes;
        ++ramScanned;
    }
    if (!journal.empty())
        flushJournal();
}

inline bool checkpoint() noexcept {
    if (!ready) return true;
    tick(nullptr, 0.f);
    flushJournal();
    return FieldDos::saveHdImage();
}

inline std::uint32_t hdMirrorCopyBytes() noexcept {
    if (!FieldDos::hdReady || FieldDos::hdImage.empty()) return 0u;
    const std::uint32_t cap = FieldPlatform::HD_MIRROR_MAX;
    const std::uint32_t sz = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(kHdLogical, FieldDos::hdImage.size()));
    if (sz <= cap) return sz;
    constexpr std::uint32_t kHot = 8u * 1024u * 1024u;
    return std::min(cap, std::max(kHot, sz / 8u));
}

} // namespace FieldRaid