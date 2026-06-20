#pragma once

// RTX-LE — host-side LZEXE 0.91 expand + Watcom LE discovery (Keen / DOS4GW).

#include "FieldDpmi.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdint>
#include <cstdio>
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

    FieldBios::patchDos4gwCpuProbe(e, base + loadOff, loadSize);

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

inline bool readGuestBlob(x86emu_t* e, std::uint32_t base, std::uint32_t bytes,
                          std::vector<std::uint8_t>& out) noexcept {
    if (!e || !bytes) return false;
    out.resize(bytes);
    for (std::uint32_t i = 0; i < bytes; ++i)
        out[i] = static_cast<std::uint8_t>(x86emu_read_byte(e, base + i));
    return true;
}

/* Run LZEXE 0.91 stub under live DOS traps; copy expanded MZ+LE image from guest. */
inline bool expandLzexe91(x86emu_t* e, void* mapped, std::size_t offset,
                          const std::vector<std::uint8_t>& packed,
                          std::vector<std::uint8_t>& expanded,
                          FieldX86Emu::Ctx& ctx) noexcept {
    if (!e || !mapped || packed.empty()) return false;
    if (!isLzexe91(packed.data(), packed.size())) {
        expanded = packed;
        return true;
    }

    const std::uint32_t allocBytes = static_cast<std::uint32_t>(packed[0x0A] | (packed[0x0B] << 8)) * 16u;
    constexpr std::uint16_t kLoadSeg = 0x1000u;
    constexpr std::uint32_t kBase = static_cast<std::uint32_t>(kLoadSeg) << 4;
    if (!seedLzexe91Load(e, packed, kLoadSeg)) return false;

    FieldX86Emu::syncToDie(mapped);
    const bool prevSettled = FieldBios::guestBootSettled;
    const bool prevExec = FieldX86Emu::guestAppExecute;
    FieldBios::guestBootSettled = true;
    FieldX86Emu::guestAppExecute = true;
    std::uint32_t leLin = 0;
    for (int round = 0; round < 48; ++round) {
        FieldX86Emu::syncToDie(mapped);
        FieldX86Emu::runMapped(mapped, offset, FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET,
            ctx, 2'000'000u);
        const std::uint32_t csBase = (static_cast<std::uint32_t>(e->x86.R_CS & 0xFFFFu) << 4);
        const std::uint32_t bases[] = {
            csBase, 0x8000u, kBase, kBase + 0x4000u, kBase + 0x8000u, kBase + 0xBF00u};
        for (std::uint32_t scanBase : bases) {
            if (!scanGuestLe(e, scanBase, allocBytes, leLin)) continue;
            const std::uint32_t mzBase = leLin >= scanBase + 0x80u ? leLin - 0x80u : scanBase;
            if (!readGuestBlob(e, mzBase, allocBytes, expanded)) return false;
            std::size_t leOff = 0;
            if (!findLeOffset(expanded.data(), expanded.size(), leOff)) return false;
            std::fprintf(stderr, "[RTX-LE] LZEXE expanded %zu bytes LE@%zu (guest@%05x round=%d)\n",
                expanded.size(), leOff, leLin, round);
            return true;
        }
        const std::uint16_t ip = static_cast<std::uint16_t>(e->x86.R_EIP & 0xFFFFu);
        const std::uint16_t cs = static_cast<std::uint16_t>(e->x86.R_CS & 0xFFFFu);
        if (round % 8 == 0)
            std::fprintf(stderr, "[RTX-LE] lz round=%d cs=%04x ip=%04x\n", round,
                static_cast<unsigned>(cs), static_cast<unsigned>(ip));
        if (round > 0 && cs == 0x0800u && ip == 0x0100u)
            break;
    }
    FieldBios::guestBootSettled = prevSettled;
    FieldX86Emu::guestAppExecute = prevExec;

    const std::uint32_t csBase = (static_cast<std::uint32_t>(e->x86.R_CS & 0xFFFFu) << 4);
    const std::uint32_t bases[] = {csBase, 0x8000u, kBase};
    for (std::uint32_t scanBase : bases) {
        if (!scanGuestLe(e, scanBase, allocBytes, leLin)) continue;
        const std::uint32_t mzBase = leLin >= scanBase + 0x80u ? leLin - 0x80u : scanBase;
        if (!readGuestBlob(e, mzBase, allocBytes, expanded)) return false;
        std::size_t leOff = 0;
        if (!findLeOffset(expanded.data(), expanded.size(), leOff)) return false;
        std::fprintf(stderr, "[RTX-LE] LZEXE expanded %zu bytes LE@%zu (guest@%05x)\n",
            expanded.size(), leOff, leLin);
        return true;
    }

    std::fprintf(stderr, "[RTX-LE] LZEXE expand failed\n");
    return false;
}

} // namespace FieldRtxLe