// Boot MS-DOS 6.30 Ammo → C:\> prompt probe (host CPU path).
// Build: g++ -std=c++20 -I Navigator/engine -I third_party/libx86emu/include \
//   scripts/boot_to_doom_test.cpp build/libx86emu.a -o build/bin/Linux/boot_to_doom_test

#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldBios.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static bool hasPrompt(const std::uint8_t* ram) {
    for (int row = 0; row < 25; ++row) {
        for (int col = 0; col < 76; ++col) {
            const std::uint32_t off = 0xB8000u + static_cast<std::uint32_t>((row * 80 + col) * 2);
            const char d = static_cast<char>(ram[off]);
            if ((d == 'C' || d == 'A') && ram[off + 2] == ':' && ram[off + 4] == '\\' && ram[off + 6] == '>')
                return true;
        }
    }
    return false;
}

int main() {
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);

    const auto floppy = FieldDos::defaultImagePath(".");
    const auto hd = FieldDos::defaultHdPath(".");
    if (!FieldDos::loadHdImage(hd)) {
        std::fprintf(stderr, "HD load failed: %s\n", hd.string().c_str());
        return 1;
    }
    if (!FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, floppy)) {
        std::fprintf(stderr, "floppy load failed\n");
        return 1;
    }
    if (!FieldDos::mirrorHdToGuestGpu(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4)) {
        std::fprintf(stderr, "HD GPU mirror failed\n");
        return 1;
    }

    auto* ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldX86Emu::Ctx ctx{};
    bool prompt = false;

    for (int round = 0; round < 200; ++round) {
        if (round >= 10 && round < 12) ctx.key = 0x1C0Du;
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 8'388'608u);
        if (!prompt && hasPrompt(ram)) {
            prompt = true;
            std::printf("PROMPT at round %d cycles=%llu mode=%u mirror=%u KiB\n", round,
                static_cast<unsigned long long>(ctx.ticks),
                static_cast<unsigned>(ram[0x449]),
                FieldDos::hdMirrorBytes / 1024u);
            return 0;
        }
        if (round >= 24 && ram[0x449] == 3u) {
            std::printf("TEXT mode ok round=%d cycles=%llu\n", round,
                static_cast<unsigned long long>(ctx.ticks));
            return 0;
        }
    }

    std::printf("FAIL prompt=%d mode=%u row23=", prompt ? 1 : 0,
        static_cast<unsigned>(ram[0x449]));
    for (int col = 0; col < 20; ++col)
        std::putchar(static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>((23 * 80 + col) * 2)]));
    std::putchar('\n');
    return 1;
}