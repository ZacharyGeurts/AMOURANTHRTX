// QA: Ammo Super-Shell — VER DIR CHKDSK HELP MEM ERA WIN31
#include "FieldBios.hpp"
#include "FieldRtxHelp.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static bool screenHas(const std::uint8_t* ram, const char* needle) {
    const std::size_t nlen = std::strlen(needle);
    if (nlen == 0) return true;
    char flat[80 * 25 + 1]{};
    int bi = 0;
    for (int i = 0; i < 80 * 25 && bi < 80 * 25; ++i) {
        const char c = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>(i * 2)]);
        if (c) flat[bi++] = c;
    }
    flat[bi] = '\0';
    return std::strstr(flat, needle) != nullptr;
}

static void bootToPrompt(std::vector<std::uint8_t>& buf, std::uint8_t*& ram, FieldX86Emu::Ctx& ctx) {
    FieldDosConfig::cfg.s1e1Playthrough = false;
    FieldDosConfig::cfg.cyclesBoot = 2'000'000u;
    FieldDosConfig::cfg.cyclesRun = 500'000u;
    FieldDos::loadHdImage(FieldDos::defaultHdPath("."));
    FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
        FieldDos::defaultImagePath("."));
    FieldDos::mirrorHdToGuestGpu(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    for (int round = 0; round < 80; ++round) {
        if (round >= 4 && round < 8) ctx.key = 0x1C0Du;
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 2'000'000u);
        if (FieldBios::guestBootSettled) break;
    }
}

static void typeLine(FieldX86Emu::Ctx& ctx, std::vector<std::uint8_t>& buf, const char* line) {
    for (const char* p = line; *p; ++p) {
        ctx.key = static_cast<std::uint32_t>(static_cast<std::uint8_t>(*p));
        ctx.keyDown = true;
        for (int i = 0; i < 3; ++i)
            FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
                FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 500'000u);
        ctx.keyDown = false;
        ctx.key = 0;
    }
    ctx.key = 0x1C0Du;
    ctx.keyDown = true;
    for (int i = 0; i < 4; ++i)
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 500'000u);
    ctx.keyDown = false;
    ctx.key = 0;
}

static bool runCmd(std::uint8_t* ram, std::vector<std::uint8_t>& buf, FieldX86Emu::Ctx& ctx,
                   const char* cmd, const char* needle) {
    typeLine(ctx, buf, cmd);
    if (!screenHas(ram, needle)) {
        std::fprintf(stderr, "FAIL cmd %s missing %s\n", cmd, needle);
        return false;
    }
    std::printf("OK %s\n", cmd);
    return true;
}

int main(int argc, char** argv) {
    if (FieldRtxHelp::argcWantsHelp(argc, argv)) {
        FieldRtxHelp::printBinaryHelp(stderr);
        return 0;
    }
    std::vector<std::uint8_t> buf(FieldPlatform::FIELD_X86_DIE_UINTS * 4, 0);
    std::uint8_t* ram = nullptr;
    FieldX86Emu::Ctx ctx{};
    bootToPrompt(buf, ram, ctx);
    if (!FieldBios::guestBootSettled || !screenHas(ram, "RTX-DOS")) {
        std::fprintf(stderr, "FAIL boot/welcome\n");
        return 1;
    }
    const struct { const char* cmd; const char* needle; } tests[] = {
        {"VER", "7.0"},
        {"DIR", "FIELDLAY.TXT"},
        {"CHKDSK", "CD-ROM"},
        {"HELP", "COMMAND /?"},
        {"AMMOASM /?", "MASM subset"},
        {"MEM", "XMS"},
        {"DEVICES", "sb16"},
        {"AMMOS", "RTX-AMMOS"},
        {"FIELD", "vga"},
        {"DRIVES", "field drives"},
    };
    for (const auto& t : tests)
        if (!runCmd(ram, buf, ctx, t.cmd, t.needle))
            return 1;
    typeLine(ctx, buf, "WIN31");
    if (ram[0x449] != 0x13u) {
        std::fprintf(stderr, "FAIL WIN31 mode=%u\n", static_cast<unsigned>(ram[0x449]));
        return 1;
    }
    std::printf("OK WIN31 graphics mode\n");
    std::printf("RTX-DOS command QA passed\n");
    return 0;
}