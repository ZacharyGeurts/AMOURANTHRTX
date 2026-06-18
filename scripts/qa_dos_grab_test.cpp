// Boot RTX-DOS, run field-drive commands, dump VGA frames as PPM for image grab QA.
#include "FieldBios.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

static bool screenText(std::uint8_t* ram, char* out, std::size_t cap) {
    if (!ram || !out || cap < 80u * 25u + 1u) return false;
    for (int i = 0; i < 80 * 25; ++i)
        out[i] = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>(i * 2)]);
    out[80 * 25] = '\0';
    return true;
}

static std::uint8_t vgaAttr(std::uint8_t* ram, int cell) {
    return ram[0xB8000u + static_cast<std::uint32_t>(cell * 2 + 1)];
}

static std::uint8_t vgaCh(std::uint8_t* ram, int cell) {
    return ram[0xB8000u + static_cast<std::uint32_t>(cell * 2)];
}

static std::uint32_t attrToRgb(std::uint8_t attr) {
    static const std::uint32_t pal[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
        0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
        0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
    };
    const int fg = attr & 0x0Fu;
    const int bg = (attr >> 4) & 0x0Fu;
    const std::uint32_t fgRgb = pal[fg & 15];
    const std::uint32_t bgRgb = pal[bg & 15];
    return (fgRgb & 0xFFFFFFu) | ((bgRgb & 0xFFFFFFu) << 24);
}

static bool writePpm(const std::filesystem::path& path, std::uint8_t* ram, int scale) {
    const int w = 80 * scale;
    const int h = 25 * scale;
    std::vector<std::uint32_t> px(static_cast<std::size_t>(w * h));
    for (int row = 0; row < 25; ++row) {
        for (int col = 0; col < 80; ++col) {
            const int cell = row * 80 + col;
            const std::uint32_t pair = attrToRgb(vgaAttr(ram, cell));
            const std::uint32_t fg = pair & 0xFFFFFFu;
            const std::uint32_t bg = (pair >> 24) & 0xFFFFFFu;
            const std::uint8_t ch = vgaCh(ram, cell);
            const std::uint32_t color = (ch >= 32 && ch < 127) ? fg : bg;
            for (int sy = 0; sy < scale; ++sy) {
                for (int sx = 0; sx < scale; ++sx) {
                    px[static_cast<std::size_t>((row * scale + sy) * w + col * scale + sx)] = color;
                }
            }
        }
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << w << " " << h << "\n255\n";
    for (std::uint32_t c : px) {
        out.put(static_cast<char>((c >> 16) & 0xFF));
        out.put(static_cast<char>((c >> 8) & 0xFF));
        out.put(static_cast<char>(c & 0xFF));
    }
    return static_cast<bool>(out);
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

static bool grabCmd(std::uint8_t* ram, std::vector<std::uint8_t>& buf, FieldX86Emu::Ctx& ctx,
                    const char* cmd, const std::filesystem::path& outDir, const char* tag) {
    typeLine(ctx, buf, cmd);
    char text[80 * 25 + 1]{};
    screenText(ram, text, sizeof text);
    const auto ppm = outDir / (std::string(tag) + ".ppm");
    if (!writePpm(ppm, ram, 8)) {
        std::fprintf(stderr, "FAIL grab %s ppm\n", tag);
        return false;
    }
    std::printf("GRAB %s\n", ppm.string().c_str());
    std::printf("TEXT %s:\n%s\n", tag, text);
    return true;
}

int main(int argc, char** argv) {
    const std::filesystem::path outDir = (argc >= 2) ? argv[1] : "build/grabs";
    std::filesystem::create_directories(outDir);

    std::vector<std::uint8_t> buf(FieldPlatform::FIELD_X86_DIE_UINTS * 4, 0);
    std::uint8_t* ram = nullptr;
    FieldX86Emu::Ctx ctx{};
    bootToPrompt(buf, ram, ctx);
    if (!FieldBios::guestBootSettled) {
        std::fprintf(stderr, "FAIL boot\n");
        return 1;
    }
    if (!writePpm(outDir / "00_welcome.ppm", ram, 8)) return 1;
    std::printf("GRAB %s\n", (outDir / "00_welcome.ppm").string().c_str());

    const struct { const char* cmd; const char* tag; } seq[] = {
        {"VER", "01_ver"},
        {"DRIVES", "02_drives"},
        {"DIR", "03_dir_c"},
        {"DIR A:", "04_dir_a"},
        {"DIR E:", "05_dir_e"},
        {"CHKDSK", "06_chkdsk"},
        {"HELP", "07_help"},
    };
    for (const auto& s : seq)
        if (!grabCmd(ram, buf, ctx, s.cmd, outDir, s.tag)) return 1;

    std::printf("OK dos grab test (%s)\n", outDir.string().c_str());
    return 0;
}