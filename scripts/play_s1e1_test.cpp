// Automated S1E1 (Episode 1) playthrough — bot walks E1M1..E1M9 to exit.
// Build: g++ -std=c++20 -O2 -I Navigator/engine -I third_party/libx86emu/include \
//   scripts/play_s1e1_test.cpp build/libx86emu.a -o build/bin/Linux/play_s1e1_test

#include "FieldBios.hpp"
#include "FieldDos.hpp"
#include "FieldDoomGpu.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <vector>

static bool bootSettled(const std::uint8_t* ram) {
    return ram[0x449] == 0x13u;
}

int main() {
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);

    const auto floppy = FieldDos::defaultImagePath(".");
    const auto hd = FieldDos::defaultHdPath(".");
    if (!FieldDos::loadHdImage(hd)) {
        std::fprintf(stderr, "HD load failed\n");
        return 1;
    }
    if (!FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, floppy)) {
        std::fprintf(stderr, "floppy load failed\n");
        return 1;
    }

    auto* ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldX86Emu::Ctx ctx{};

    for (int round = 0; round < 80 && !FieldBios::guestBootSettled; ++round) {
        if (round >= 8 && round < 12) ctx.key = 0x1C0Du;
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 4'194'304u);
    }

    if (!FieldBios::guestBootSettled || !bootSettled(ram)) {
        std::fprintf(stderr, "FAIL boot: settled=%d mode=%u\n",
            FieldBios::guestBootSettled ? 1 : 0, static_cast<unsigned>(ram[0x449]));
        return 1;
    }

    std::printf("S1E1 boot ok level=%u\n",
        static_cast<unsigned>(FieldDoomGpu::doomState(ram, FieldDoomGpu::ST_LEVEL)));

    int maxFrames = 12000;
    for (int f = 0; f < maxFrames; ++f) {
        FieldDoomGpu::stepS1e1(ram, 0u, true);
        if (FieldDoomGpu::episodeComplete(ram)) {
            std::printf("S1E1 complete: frames=%d levels=%d\n", f + 1, FieldDoomGpu::S1E1_LEVELS);
            return 0;
        }
        if (f > 0 && (f % 500) == 0)
            std::printf("  progress frame=%d level=%u\n", f,
                static_cast<unsigned>(FieldDoomGpu::doomState(ram, FieldDoomGpu::ST_LEVEL)));
    }

    std::fprintf(stderr, "FAIL timeout level=%u frames=%u\n",
        static_cast<unsigned>(FieldDoomGpu::doomState(ram, FieldDoomGpu::ST_LEVEL)),
        static_cast<unsigned>(FieldDoomGpu::doomState(ram, FieldDoomGpu::ST_FRAMES)));
    return 1;
}