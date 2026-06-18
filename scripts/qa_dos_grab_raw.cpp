#include "FieldBios.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

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

static bool dumpVga(std::uint8_t* ram, const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(ram + 0xB8000u), 80 * 25 * 2);
    return static_cast<bool>(out);
}

int main(int argc, char** argv) {
    const std::filesystem::path outDir = (argc >= 2) ? argv[1] : "build/grabs/raw";
    std::filesystem::create_directories(outDir);
    std::vector<std::uint8_t> buf(FieldPlatform::FIELD_X86_DIE_UINTS * 4, 0);
    std::uint8_t* ram = nullptr;
    FieldX86Emu::Ctx ctx{};
    bootToPrompt(buf, ram, ctx);
    if (!FieldBios::guestBootSettled) return 1;
    dumpVga(ram, outDir / "00_welcome.vga");
    const struct { const char* cmd; const char* tag; } seq[] = {
        {"VER", "01_ver"}, {"DRIVES", "02_drives"}, {"DIR", "03_dir_c"},
        {"DIR A:", "04_dir_a"}, {"DIR E:", "05_dir_e"}, {"CHKDSK", "06_chkdsk"}, {"HELP", "07_help"},
    };
    for (const auto& s : seq) {
        typeLine(ctx, buf, s.cmd);
        dumpVga(ram, outDir / (std::string(s.tag) + ".vga"));
    }
    std::printf("OK raw vga dumps -> %s\n", outDir.string().c_str());
    return 0;
}