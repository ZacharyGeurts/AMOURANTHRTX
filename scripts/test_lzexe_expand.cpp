// Scratch: LZEXE 0.91 expand via corrected load layout + stub run.
#include "FieldAmmoExec.hpp"
#include "FieldBios.hpp"
#include "FieldDpmi.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <fstream>
#include <vector>

static bool scanLe(const std::uint8_t* p, std::size_t n, std::size_t& leOff) {
    for (std::size_t off = 0; off + 0x84u < n; off += 4u) {
        if (p[off] != 'L' || p[off + 1u] != 'E' || p[off + 2u] != 0u || p[off + 3u] != 0u)
            continue;
        const std::uint32_t nobj = static_cast<std::uint32_t>(p[off + 0x44u])
            | (static_cast<std::uint32_t>(p[off + 0x45u]) << 8)
            | (static_cast<std::uint32_t>(p[off + 0x46u]) << 16)
            | (static_cast<std::uint32_t>(p[off + 0x47u]) << 24);
        const std::uint32_t psz  = static_cast<std::uint32_t>(p[off + 0x28u])
            | (static_cast<std::uint32_t>(p[off + 0x29u]) << 8)
            | (static_cast<std::uint32_t>(p[off + 0x2Au]) << 16)
            | (static_cast<std::uint32_t>(p[off + 0x2Bu]) << 24);
        if (nobj > 0u && nobj <= 64u && psz >= 512u && psz <= 65536u) {
            leOff = off;
            return true;
        }
    }
    return false;
}

static bool launchLzexe91(x86emu_t* e, const std::vector<std::uint8_t>& img,
                          std::uint16_t loadSeg = 0x1000u) {
    if (img.size() < 64u || img[0] != 'M' || img[1] != 'Z' || img[28] != 'L' || img[29] != 'Z')
        return false;

    const std::uint16_t hdrPar   = static_cast<std::uint16_t>(img[8] | (img[9] << 8));
    const std::uint32_t hdrBytes = static_cast<std::uint32_t>(hdrPar) * 16u;
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
    x86emu_write_word(e, pspBase + 0x16u, 0x0000u);
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

    std::fprintf(stderr, "[LZEXE] alloc=%u loadOff=%u entry=%04x:%04x\n",
        allocBytes, loadOff,
        static_cast<unsigned>(loadSeg + e_cs),
        static_cast<unsigned>(e_ip));
    return true;
}

int main() {
    std::ifstream in("/home/default/Desktop/SG/AMOURANTHRTX/assets/dos/games/keen4/KEEN4E.EXE", std::ios::binary);
    std::vector<std::uint8_t> img((std::istreambuf_iterator<char>(in)), {});
    if (img.empty()) return 1;

    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);
    FieldX86Emu::ensure(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    auto* e = FieldX86Emu::emu;
    if (!launchLzexe91(e, img)) return 1;

    std::size_t leOff = 0;
    std::uint32_t leBase = 0;
    FieldX86Emu::Ctx ctx{};
    const std::uint32_t snapBytes = static_cast<std::uint32_t>(img[0x0A] | (img[0x0B] << 8)) * 16u;
    for (int round = 0; round < 64; ++round) {
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 2'000'000u);
        const std::uint32_t bases[] = {0x8000u, 0x10000u, 0x14000u, 0x18000u};
        for (std::uint32_t base : bases) {
            std::vector<std::uint8_t> snap(snapBytes);
            for (std::size_t i = 0; i < snap.size(); ++i)
                snap[i] = static_cast<std::uint8_t>(x86emu_read_byte(e, base + static_cast<std::uint32_t>(i)));
            if (scanLe(snap.data(), snap.size(), leOff)) {
                leBase = base;
                std::printf("OK LE@%zu base=%05x round=%d cs=%04x ip=%04x mz=%02x%02x\n",
                    leOff, base, round,
                    static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
                    static_cast<unsigned>(e->x86.R_EIP & 0xFFFFu),
                    static_cast<unsigned>(snap[0]), static_cast<unsigned>(snap[1]));
                return 0;
            }
        }
        if (round % 8 == 0)
            std::fprintf(stderr, "round=%d cs=%04x ip=%04x\n", round,
                static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
                static_cast<unsigned>(e->x86.R_EIP & 0xFFFFu));
    }
    {
        const std::uint32_t ipLin = ((e->x86.R_CS & 0xFFFFu) << 4) + (e->x86.R_EIP & 0xFFFFu);
        std::fprintf(stderr, "entry linear %05x bytes:", ipLin);
        for (int i = 0; i < 16; ++i)
            std::fprintf(stderr, " %02x", static_cast<unsigned>(x86emu_read_byte(e, ipLin + static_cast<std::uint32_t>(i))));
        std::fprintf(stderr, "\n");
        for (std::uint32_t base : {0x8000u, 0x10000u, 0x1BF00u}) {
            std::fprintf(stderr, "base %05x:", base);
            for (int i = 0; i < 16; ++i)
                std::fprintf(stderr, " %02x", static_cast<unsigned>(x86emu_read_byte(e, base + static_cast<std::uint32_t>(i))));
            std::fprintf(stderr, "\n");
        }
    }
    int leHits = 0;
    for (std::uint32_t lin = 0; lin + 4u < 0x80000u; lin += 1u) {
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
            std::fprintf(stderr, "LE@%05x objs=%u psz=%u\n", lin,
                static_cast<unsigned>(nobj), static_cast<unsigned>(psz));
            if (++leHits >= 8) break;
        }
    }
    std::fprintf(stderr, "FAIL no LE after stub\n");
    return 1;
}