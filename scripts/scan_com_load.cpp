#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <vector>

static const std::uint8_t kShell[] = {
    0x8C, 0xC8, 0x8E, 0xD8, 0x8E, 0xC0, 0xB4, 0x09
};

int main() {
    std::vector<std::uint8_t> buf(FieldPlatform::FIELD_X86_DIE_UINTS * 4, 0);
    FieldDos::loadHdImage(FieldDos::defaultHdPath("."));
    FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
        FieldDos::defaultImagePath("."));
    auto* ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldX86Emu::Ctx ctx{};
    FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
        FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 40'000'000u);
    for (std::uint32_t off = 0; off < 0x100000u; ++off)
        if (std::memcmp(ram + off, kShell, sizeof kShell) == 0)
            std::printf("minishell@%05x\n", off);
    auto* e = FieldX86Emu::emu;
    std::printf("cs=%04x ip=%04x mode=%u ticks=%llu\n",
        static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
        static_cast<unsigned>(e->x86.R_EIP & 0xFFFFu),
        static_cast<unsigned>(ram[0x449]),
        static_cast<unsigned long long>(ctx.ticks));
    for (int row = 20; row < 25; ++row) {
        for (int col = 0; col < 80; ++col)
            std::putchar(static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>((row * 80 + col) * 2)]));
        std::putchar('\n');
    }
    return 0;
}