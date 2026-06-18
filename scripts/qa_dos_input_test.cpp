// QA: boot MS-DOS Ammo, type VER+Enter, confirm VGA text and BIOS keys.
#include "FieldBios.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldInput.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <vector>

static void dumpRow(const std::uint8_t* ram, int row) {
    for (int col = 0; col < 80; ++col) {
        const char ch = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>((row * 80 + col) * 2)]);
        std::putchar((ch >= 32 && ch < 127) ? ch : '.');
    }
    std::putchar('\n');
}

static bool screenHas(const std::uint8_t* ram, const char* needle) {
    char buf[80 * 25 + 1]{};
    for (int i = 0; i < 80 * 25; ++i)
        buf[i] = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>(i * 2)]);
    return std::strstr(buf, needle) != nullptr;
}

int main() {
    FieldDosConfig::cfg.s1e1Playthrough = false;
    FieldDosConfig::cfg.cyclesBoot = 2'000'000u;
    FieldDosConfig::cfg.cyclesRun = 2'000'000u;
    std::vector<std::uint8_t> buf(FieldPlatform::FIELD_X86_DIE_UINTS * 4, 0);
    if (!FieldDos::loadHdImage(FieldDos::defaultHdPath("."))) {
        std::fprintf(stderr, "FAIL: HD load\n");
        return 1;
    }
    if (!FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldDos::defaultImagePath("."))) {
        std::fprintf(stderr, "FAIL: floppy load\n");
        return 1;
    }
    FieldDos::mirrorHdToGuestGpu(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    auto* ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldX86Emu::Ctx ctx{};

    for (int round = 0; round < 80; ++round) {
        if (round >= 4 && round < 8) ctx.key = 0x1C0Du;
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 2'000'000u);
        if (FieldBios::guestBootSettled && screenHas(ram, "C:\\>"))
            break;
    }
    if (!FieldBios::guestBootSettled) {
        std::fprintf(stderr, "FAIL: boot not settled\n");
        for (int row = 20; row < 25; ++row) dumpRow(ram, row);
        return 1;
    }
    std::printf("OK boot settled cursor=%u,%u\n",
        static_cast<unsigned>(ram[0x450]), static_cast<unsigned>(ram[0x451]));

    const std::uint16_t keys[] = {
        0x2F00u | 'V', 0x1200u | 'E', 0x1300u | 'R', 0x1C0Du
    };
    for (std::uint16_t k : keys) {
        ctx.key = k;
        ctx.keyDown = true;
        for (int i = 0; i < 4; ++i)
            FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
                FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 500'000u);
        ctx.keyDown = false;
        ctx.key = 0;
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 100'000u);
    }

    if (!screenHas(ram, "VER") && !screenHas(ram, "ver")) {
        std::fprintf(stderr, "FAIL: VER not echoed — row 23:\n");
        dumpRow(ram, 23);
        return 1;
    }
    std::printf("OK typed VER visible on screen\n");
    dumpRow(ram, 23);

    bool keyState[SDL_SCANCODE_COUNT]{};
    keyState[SDL_SCANCODE_PERIOD] = true;
    if (FieldInput::pollBiosKey(keyState) != 0x342Eu) {
        std::fprintf(stderr, "FAIL: period key not polled\n");
        return 1;
    }
    keyState[SDL_SCANCODE_PERIOD] = false;
    keyState[SDL_SCANCODE_SLASH] = true;
    if (FieldInput::pollBiosKey(keyState) != 0x352Fu) {
        std::fprintf(stderr, "FAIL: slash key not polled\n");
        return 1;
    }
    keyState[SDL_SCANCODE_SLASH] = false;
    keyState[SDL_SCANCODE_BACKSLASH] = true;
    if (FieldInput::pollBiosKey(keyState) != 0x2B5Cu) {
        std::fprintf(stderr, "FAIL: backslash key not polled\n");
        return 1;
    }
    std::printf("OK punctuation keys . / \\\n");
    return 0;
}