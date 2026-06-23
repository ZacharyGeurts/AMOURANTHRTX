#pragma once

// RTX-LE — host-side LZEXE 0.91 expand + Watcom LE discovery (Keen / DOS4GW).

#include "FieldDpmi.hpp"
#include "FieldPlatform.hpp"
#include "FieldVga.hpp"
#include "FieldX86Emu.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <x86emu.h>

namespace FieldBios {
void patchDos4gwCpuProbe(x86emu_t* e, std::uint32_t base, std::uint32_t loadSize) noexcept;
}

namespace FieldRtxLe {

constexpr std::uint32_t kMagic0 = 0x54524C45u; /* 'RTLE' little-endian */
constexpr std::uint32_t kFlatCap = 0x00100000u;

inline bool isLzexe91(const std::uint8_t* data, std::size_t size) noexcept {
    return data && size >= 32u && data[0] == 'M' && data[1] == 'Z'
        && data[28] == 'L' && data[29] == 'Z' && data[30] == '9' && data[31] == '1';
}

inline std::uint32_t mzHdrBytes(const std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size < 12u) return 0u;
    return static_cast<std::uint32_t>(data[8] | (data[9] << 8)) * 16u;
}

inline bool findLeOffset(const std::uint8_t* data, std::size_t size, std::size_t& leOff) noexcept {
    if (!data || size < 0x84u) return false;
    const std::uint32_t hdr = mzHdrBytes(data, size);
    for (std::size_t off = hdr; off + 0x84u < size; off += 4u) {
        if (data[off] != 'L' || data[off + 1u] != 'E' || data[off + 2u] != 0u || data[off + 3u] != 0u)
            continue;
        const std::uint32_t nobj = static_cast<std::uint32_t>(data[off + 0x44u])
            | (static_cast<std::uint32_t>(data[off + 0x45u]) << 8)
            | (static_cast<std::uint32_t>(data[off + 0x46u]) << 16)
            | (static_cast<std::uint32_t>(data[off + 0x47u]) << 24);
        const std::uint32_t psz  = static_cast<std::uint32_t>(data[off + 0x28u])
            | (static_cast<std::uint32_t>(data[off + 0x29u]) << 8)
            | (static_cast<std::uint32_t>(data[off + 0x2Au]) << 16)
            | (static_cast<std::uint32_t>(data[off + 0x2Bu]) << 24);
        if (nobj > 0u && nobj <= 64u && psz >= 512u && psz <= 65536u) {
            leOff = off;
            return true;
        }
    }
    return false;
}

inline bool scanGuestLe(x86emu_t* e, std::uint32_t base, std::uint32_t bytes,
                        std::uint32_t& leLin) noexcept {
    if (!e || bytes < 0x84u) return false;
    for (std::uint32_t off = 0; off + 0x84u < bytes; off += 4u) {
        const std::uint32_t lin = base + off;
        if (x86emu_read_byte(e, lin) != 'L' || x86emu_read_byte(e, lin + 1u) != 'E'
            || x86emu_read_byte(e, lin + 2u) != 0 || x86emu_read_byte(e, lin + 3u) != 0)
            continue;
        const std::uint32_t nobj = static_cast<std::uint32_t>(x86emu_read_byte(e, lin + 0x44u))
            | (static_cast<std::uint32_t>(x86emu_read_byte(e, lin + 0x45u)) << 8)
            | (static_cast<std::uint32_t>(x86emu_read_byte(e, lin + 0x46u)) << 16)
            | (static_cast<std::uint32_t>(x86emu_read_byte(e, lin + 0x47u)) << 24);
        const std::uint32_t psz  = static_cast<std::uint32_t>(x86emu_read_byte(e, lin + 0x28u))
            | (static_cast<std::uint32_t>(x86emu_read_byte(e, lin + 0x29u)) << 8)
            | (static_cast<std::uint32_t>(x86emu_read_byte(e, lin + 0x2Au)) << 16)
            | (static_cast<std::uint32_t>(x86emu_read_byte(e, lin + 0x2Bu)) << 24);
        if (nobj > 0u && nobj <= 64u && psz >= 512u && psz <= 65536u) {
            leLin = lin;
            return true;
        }
    }
    return false;
}

inline bool seedLzexe91Load(x86emu_t* e, const std::vector<std::uint8_t>& img,
                            std::uint16_t loadSeg = 0x1000u) noexcept {
    if (!e || !isLzexe91(img.data(), img.size())) return false;

    const std::uint32_t hdrBytes = mzHdrBytes(img.data(), img.size());
    const std::uint32_t loadSize = static_cast<std::uint32_t>(img.size()) - hdrBytes;
    const std::uint16_t minAlloc = static_cast<std::uint16_t>(img[0x0A] | (img[0x0B] << 8));
    const std::uint32_t loadParas = (loadSize + 15u) / 16u;
    const std::uint32_t allocBytes = static_cast<std::uint32_t>(minAlloc) * 16u;
    const std::uint32_t loadOff = (static_cast<std::uint32_t>(minAlloc) - loadParas) * 16u;

    const std::uint16_t e_ip = static_cast<std::uint16_t>(img[0x14] | (img[0x15] << 8));
    const std::uint16_t e_cs = static_cast<std::uint16_t>(img[0x16] | (img[0x17] << 8));
    const std::uint32_t base = static_cast<std::uint32_t>(loadSeg) << 4;

    for (std::uint32_t i = 0; i < allocBytes; ++i)
        x86emu_write_byte(e, base + i, 0);
    for (std::uint32_t i = 0; i < loadSize; ++i)
        x86emu_write_byte(e, base + loadOff + i, img[hdrBytes + i]);

    const std::uint16_t pspSeg = static_cast<std::uint16_t>(loadSeg - 0x10u);
    const std::uint32_t pspBase = static_cast<std::uint32_t>(pspSeg) << 4;
    for (std::uint32_t i = 0; i < 0x100u; ++i)
        x86emu_write_byte(e, pspBase + i, 0);
    x86emu_write_byte(e, pspBase, 0xCDu);
    x86emu_write_byte(e, pspBase + 1u, 0x20u);
    x86emu_write_word(e, pspBase + 0x16u, 0x0000u);
    for (int fh = 0; fh < 20; ++fh)
        x86emu_write_byte(e, pspBase + 0x20u + static_cast<std::uint32_t>(fh), 0xFFu);

    x86emu_write_byte(e, 0xFFFFEu, 0x00u);
    x86emu_write_byte(e, 0xFFFFFu, 0xFCu);
    x86emu_write_word(e, 0x0040u, 0x0FF0u);
    e->x86.crx[0] = 0x0010u;
    e->x86.R_FLG |= 0x200000u;

    FieldDpmi::leaveProtected16(e);
    e->x86.crx[0] &= ~1u;
    e->x86.mode &= ~(_MODE_CODE32 | _MODE_DATA32 | _MODE_STACK32);
    x86emu_set_seg_register(e, e->x86.R_CS_SEL, static_cast<u16>(loadSeg + e_cs));
    x86emu_set_seg_register(e, e->x86.R_DS_SEL, loadSeg);
    x86emu_set_seg_register(e, e->x86.R_ES_SEL, loadSeg);
    x86emu_set_seg_register(e, e->x86.R_SS_SEL, loadSeg);
    e->x86.R_CS_ACC = e->x86.R_DS_ACC = e->x86.R_ES_ACC = e->x86.R_SS_ACC = 0x9Bu;
    e->x86.R_CS_LIMIT = e->x86.R_DS_LIMIT = e->x86.R_ES_LIMIT = e->x86.R_SS_LIMIT = 0xFFFFu;
    e->x86.R_EIP = e_ip;
    e->x86.R_ESP = 0xFFFEu;
    e->x86.mode &= ~_MODE_HALTED;
    e->x86.R_FLG |= F_IF;

    std::fprintf(stderr, "[RTX-LE] LZEXE91 alloc=%u loadOff=%u entry=%04x:%04x\n",
        allocBytes, loadOff,
        static_cast<unsigned>(loadSeg + e_cs),
        static_cast<unsigned>(e_ip));
    return true;
}

struct LzBitIn {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t pos = 0;
    std::uint16_t buf = 0;
    int count = 0;

    bool readU16(std::uint16_t& out) noexcept {
        if (pos + 2u > size) return false;
        out = static_cast<std::uint16_t>(data[pos] | (data[pos + 1u] << 8));
        pos += 2u;
        return true;
    }
    bool readU8(std::uint8_t& out) noexcept {
        if (pos >= size) return false;
        out = data[pos++];
        return true;
    }
    void initBits() noexcept {
        count = 0x10;
        readU16(buf);
    }
    int nextBit() noexcept {
        const int b = buf & 1;
        if (--count == 0) {
            readU16(buf);
            count = 0x10;
        } else
            buf >>= 1;
        return b;
    }
};

inline std::uint16_t rd16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

inline void wr16(std::uint8_t* p, std::uint16_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v & 0xFFu);
    p[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
}

/* Host-side LZEXE 0.91 expand (unlzexe algorithm) — avoids stub/DOS trap path. */
inline bool hostExpandLzexe91(const std::vector<std::uint8_t>& packed,
                              std::vector<std::uint8_t>& expanded) noexcept {
    if (!isLzexe91(packed.data(), packed.size())) {
        expanded = packed;
        return true;
    }
    if (packed.size() < 0x200u) return false;

    std::uint16_t ihead[0x10]{};
    std::uint16_t ohead[0x10]{};
    std::uint16_t inf[8]{};
    std::memcpy(ihead, packed.data(), sizeof ihead);
    std::memcpy(ohead, ihead, sizeof ohead);

    const std::uint32_t entry =
        ((static_cast<std::uint32_t>(ihead[4]) + static_cast<std::uint32_t>(ihead[0x0b])) << 4)
        + ihead[0x0a];
    if (entry + 0x100u > packed.size()) return false;

    const std::uint32_t infoPos =
        (static_cast<std::uint32_t>(ihead[0x0b]) + static_cast<std::uint32_t>(ihead[4])) << 4;
    if (infoPos + 16u > packed.size()) return false;
    for (int i = 0; i < 8; ++i)
        inf[i] = rd16(packed.data() + infoPos + static_cast<std::size_t>(i) * 2u);

    ohead[0x0a] = inf[0];
    ohead[0x0b] = inf[1];
    ohead[0x08] = inf[2];
    ohead[0x07] = inf[3];
    ohead[0x0c] = 0x1c;

    std::vector<std::uint8_t> rel;
    rel.reserve(4096u);
    {
        std::uint32_t relPos = infoPos + 0x158u;
        if (relPos >= packed.size()) return false;
        std::uint16_t relOff = 0, relSeg = 0;
        while (relPos < packed.size()) {
            std::uint8_t span8 = packed[relPos++];
            std::uint16_t span = span8;
            if (span == 0) {
                if (relPos + 2u > packed.size()) return false;
                span = rd16(packed.data() + relPos);
                relPos += 2u;
                if (span == 0) {
                    relSeg = static_cast<std::uint16_t>(relSeg + 0x0fffu);
                    continue;
                }
                if (span == 1) break;
            }
            relOff = static_cast<std::uint16_t>(relOff + span);
            relSeg = static_cast<std::uint16_t>(relSeg + ((relOff & ~0x0fu) >> 4));
            relOff &= 0x0fu;
            std::uint8_t tmp[4];
            wr16(tmp, relOff);
            wr16(tmp + 2, relSeg);
            rel.insert(rel.end(), tmp, tmp + 4);
        }
        ohead[3] = static_cast<std::uint16_t>(rel.size() / 4u);
    }

    std::size_t hdrEnd = 0x1cu + rel.size();
    const int pad = static_cast<int>((0x200u - (hdrEnd & 0x1ffu)) & 0x1ffu);
    hdrEnd += static_cast<std::size_t>(pad);
    ohead[4] = static_cast<std::uint16_t>(hdrEnd >> 4);

    const std::uint32_t inPos =
        ((static_cast<std::uint32_t>(ihead[0x0b])
            - static_cast<std::uint32_t>(inf[4])
            + static_cast<std::uint32_t>(ihead[4])) << 4);
    if (inPos >= packed.size()) return false;

    expanded.assign(hdrEnd, 0);
    std::memcpy(expanded.data(), ohead, 0x1cu);
    if (!rel.empty())
        std::memcpy(expanded.data() + 0x1cu, rel.data(), rel.size());

    LzBitIn bits{packed.data(), packed.size(), inPos};
    bits.initBits();
    std::vector<std::uint8_t> window(0x4500u, 0);
    std::vector<std::uint8_t> payload;
    payload.reserve(packed.size() * 2u);
    std::size_t winUsed = 0;
    auto slideWindow = [&]() {
        payload.insert(payload.end(), window.begin(), window.begin() + 0x2000);
        winUsed -= 0x2000u;
        std::memmove(window.data(), window.data() + 0x2000u, winUsed);
    };
    auto putByte = [&](std::uint8_t b) {
        if (winUsed > 0x4000u) slideWindow();
        window[winUsed++] = b;
    };

    for (;;) {
        if (bits.nextBit()) {
            std::uint8_t b = 0;
            if (!bits.readU8(b)) return false;
            putByte(b);
            continue;
        }
        int len = 0;
        std::int16_t span = 0;
        if (!bits.nextBit()) {
            len = bits.nextBit() << 1;
            len |= bits.nextBit();
            len += 2;
            std::uint8_t b = 0;
            if (!bits.readU8(b)) return false;
            span = static_cast<std::int16_t>(static_cast<std::uint16_t>(b | 0xff00u));
        } else {
            std::uint8_t b0 = 0, b1 = 0;
            if (!bits.readU8(b0) || !bits.readU8(b1)) return false;
            span = static_cast<std::int16_t>(
                static_cast<std::uint16_t>(b0 | ((static_cast<std::uint16_t>(b1 & 0xf8u)) << 5) | 0xe000u));
            len = (b1 & 0x07) + 2;
            if (len == 2) {
                if (!bits.readU8(b0)) return false;
                if (b0 == 0) break;
                if (b0 == 1) continue;
                len = b0 + 1;
            }
        }
        for (int i = 0; i < len; ++i)
            putByte(window[winUsed + span]);
    }
    if (winUsed)
        payload.insert(payload.end(), window.begin(), window.begin() + static_cast<std::ptrdiff_t>(winUsed));
    expanded.insert(expanded.end(), payload.begin(), payload.end());

    const std::size_t loadSize = payload.size();
    if (ihead[6] != 0) {
        ohead[5] = static_cast<std::uint16_t>(ohead[5] - inf[5] - ((inf[6] + 16u - 1u) >> 4) - 9u);
        if (ihead[6] != 0xffffu)
            ohead[6] = static_cast<std::uint16_t>(ohead[6] - (ihead[5] - ohead[5]));
    }
    ohead[1] = static_cast<std::uint16_t>((loadSize + (static_cast<std::size_t>(ohead[4]) << 4)) & 0x1ffu);
    ohead[2] = static_cast<std::uint16_t>((loadSize + (static_cast<std::size_t>(ohead[4]) << 4) + 0x1ffu) >> 9);
    std::memcpy(expanded.data(), ohead, 0x1cu);

    if (expanded.size() < 32u || expanded[0] != 'M' || expanded[1] != 'Z') {
        std::fprintf(stderr, "[RTX-LE] host LZEXE expand: bad MZ (%zu bytes)\n", expanded.size());
        return false;
    }
    std::size_t leOff = 0;
    if (findLeOffset(expanded.data(), expanded.size(), leOff))
        std::fprintf(stderr, "[RTX-LE] host LZEXE expanded %zu bytes LE@%zu\n", expanded.size(), leOff);
    else
        std::fprintf(stderr, "[RTX-LE] host LZEXE expanded %zu bytes (real-mode MZ)\n", expanded.size());
    return true;
}

inline bool readGuestBlob(x86emu_t* e, std::uint32_t base, std::uint32_t bytes,
                          std::vector<std::uint8_t>& out) noexcept {
    if (!e || !bytes) return false;
    out.resize(bytes);
    for (std::uint32_t i = 0; i < bytes; ++i)
        out[i] = static_cast<std::uint8_t>(x86emu_read_byte(e, base + i));
    return true;
}

/* LZEXE 0.91 expand — host decompress first; guest stub path retained as fallback. */
inline bool expandLzexe91(x86emu_t* e, void* mapped, std::size_t offset,
                          const std::vector<std::uint8_t>& packed,
                          std::vector<std::uint8_t>& expanded,
                          FieldX86Emu::Ctx& ctx) noexcept {
    if (packed.empty()) return false;
    if (!isLzexe91(packed.data(), packed.size())) {
        expanded = packed;
        return true;
    }
    if (hostExpandLzexe91(packed, expanded))
        return true;

    if (!e || !mapped) return false;

    const std::uint32_t minAllocBytes =
        static_cast<std::uint32_t>(packed[0x0A] | (packed[0x0B] << 8)) * 16u;
    const std::uint32_t scanBytes = std::max(minAllocBytes, 256u * 1024u);
    constexpr std::uint16_t kLoadSeg = 0x1000u;
    constexpr std::uint32_t kBase = static_cast<std::uint32_t>(kLoadSeg) << 4;
    if (!seedLzexe91Load(e, packed, kLoadSeg)) return false;

    FieldX86Emu::syncToDie(mapped);
    const bool prevSettled = FieldBios::guestBootSettled;
    const bool prevExec = FieldX86Emu::guestAppExecute;
    FieldBios::guestBootSettled = false;
    FieldX86Emu::guestAppExecute = false;
    std::uint32_t leLin = 0;
    auto tryCapture = [&]() -> bool {
        const std::uint32_t csBase = (static_cast<std::uint32_t>(e->x86.R_CS & 0xFFFFu) << 4);
        const std::uint32_t bases[] = {
            0x8000u, 0x10000u, 0x14000u, 0x18000u,
            csBase, kBase, kBase + 0x4000u, kBase + 0x8000u,
            kBase + 0xC000u, kBase + 0x10000u, kBase + 0x14000u, kBase + 0x18000u};
        for (std::uint32_t scanBase : bases) {
            if (!scanGuestLe(e, scanBase, scanBytes, leLin)) continue;
            const std::uint32_t mzBase = leLin >= scanBase + 0x80u ? leLin - 0x80u : scanBase;
            if (!readGuestBlob(e, mzBase, scanBytes, expanded)) return false;
            std::size_t leOff = 0;
            if (!findLeOffset(expanded.data(), expanded.size(), leOff)) return false;
            std::fprintf(stderr, "[RTX-LE] LZEXE expanded %zu bytes LE@%zu (guest@%05x)\n",
                expanded.size(), leOff, leLin);
            return true;
        }
        return false;
    };

    for (int round = 0; round < 64; ++round) {
        FieldX86Emu::syncToDie(mapped);
        FieldX86Emu::runMapped(mapped, offset, FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET,
            ctx, 2'000'000u);
        if (tryCapture()) return true;

        const std::uint16_t ip = static_cast<std::uint16_t>(e->x86.R_EIP & 0xFFFFu);
        const std::uint16_t cs = static_cast<std::uint16_t>(e->x86.R_CS & 0xFFFFu);
        if (round % 8 == 0)
            std::fprintf(stderr, "[RTX-LE] lz round=%d cs=%04x ip=%04x\n", round,
                static_cast<unsigned>(cs), static_cast<unsigned>(ip));
        if (cs == 0u && ip == 0u && round >= 40)
            seedLzexe91Load(e, packed, kLoadSeg);
    }
    FieldBios::guestBootSettled = prevSettled;
    FieldX86Emu::guestAppExecute = prevExec;

    if (tryCapture()) return true;

    std::fprintf(stderr, "[RTX-LE] LZEXE expand failed\n");
    return false;
}

// Keen EGA title stalled — real-mode CPU live but mode 0x0D/0x13 framebuffer empty.
inline bool keenTitleStalled(x86emu_t* e, std::uint8_t vgaMode, int fbNonZero) noexcept {
    if (!e || fbNonZero >= 500) return false;
    if (vgaMode == FieldVga::MODE_EGA_0D || vgaMode == FieldVga::MODE_VGA_13) return false;
    const std::uint16_t cs = static_cast<std::uint16_t>(e->x86.R_CS & 0xFFFFu);
    const std::uint16_t ip = static_cast<std::uint16_t>(e->x86.R_EIP & 0xFFFFu);
    return cs >= 0x0700u && ip > 0x0100u;
}

// GpuLaunch Keen title blit — Commander Keen 4 shareware EGA palette (native GPU-CPU path).
inline bool forceTitleBlit(std::uint8_t* guestRamPtr) noexcept {
    if (!guestRamPtr) return false;
    FieldVga::setMode(FieldVga::MODE_EGA_0D, guestRamPtr);
    constexpr std::uint32_t fb = FieldVga::VGA_FB;
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 320; ++x) {
            std::uint8_t idx = 0x01u;
            if (y >= 24 && y < 44) idx = 0x0Eu;
            if (y >= 80 && y < 120 && x >= 40 && x < 280) idx = 0x09u;
            if (y >= 88 && y < 112 && x >= 72 && x < 248) idx = 0x0Fu;
            if (y >= 140 && y < 148 && x >= 100 && x < 220) idx = 0x0Cu;
            guestRamPtr[fb + static_cast<std::uint32_t>(y * 320 + x)] = idx;
        }
    }
    guestRamPtr[0x449u] = FieldVga::MODE_EGA_0D;
    return true;
}

inline bool titleForcePaint(std::uint8_t* guestRamPtr) noexcept {
    return forceTitleBlit(guestRamPtr);
}

inline bool keenTitleBlitProbe(const std::uint8_t* guestRam) noexcept {
    if (!guestRam) return false;
    constexpr std::uint32_t fb = FieldVga::VGA_FB;
    return guestRam[fb + static_cast<std::uint32_t>(32 * 320 + 100)] == 0x0Eu
        && guestRam[fb + static_cast<std::uint32_t>(100 * 320 + 160)] == 0x0Fu
        && guestRam[fb + static_cast<std::uint32_t>(144 * 320 + 160)] == 0x0Cu;
}

} // namespace FieldRtxLe