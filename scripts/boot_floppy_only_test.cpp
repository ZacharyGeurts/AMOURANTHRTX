#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static bool hasPrompt(const std::uint8_t* ram) {
    for (int row = 0; row < 25; ++row)
        for (int col = 0; col < 76; ++col) {
            const std::uint32_t off = 0xB8000u + static_cast<std::uint32_t>((row * 80 + col) * 2);
            const char d = static_cast<char>(ram[off]);
            if ((d == 'C' || d == 'A') && ram[off + 2] == ':' && ram[off + 4] == '\\' && ram[off + 6] == '>')
                return true;
        }
    return false;
}

int main() {
    std::vector<std::uint8_t> buf(FieldPlatform::FIELD_X86_DIE_UINTS * 4, 0);
    FieldDos::hdReady = false;
    FieldDos::hdImage.clear();
    FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
        FieldDos::defaultImagePath("."));
    auto* ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldX86Emu::Ctx ctx{};
    for (int round = 0; round < 120; ++round) {
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 2'000'000u);
        if (hasPrompt(ram)) {
            std::printf("PROMPT ok round=%d\n", round);
            return 0;
        }
        if (std::strstr(reinterpret_cast<char*>(ram + 0xB8000), "Bad or missing"))
            std::printf("shell-fail round=%d\n", round);
    }
    for (int row = 20; row < 25; ++row) {
        for (int col = 0; col < 80; ++col)
            std::putchar(static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>((row * 80 + col) * 2)]));
        std::putchar('\n');
    }
    return 1;
}