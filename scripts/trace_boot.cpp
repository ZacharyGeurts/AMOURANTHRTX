#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"
#include <cstdio>
#include <vector>

int main() {
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);
    FieldDos::loadHdImage(FieldDos::defaultHdPath("."));
    FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
        FieldDos::defaultImagePath("."));
    auto* ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldX86Emu::Ctx ctx{};
    for (int round = 0; round < 80; ++round) {
        if (round >= 10 && round < 14) ctx.key = 0x1C0Du;
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 4'194'304u);
        auto* e = FieldX86Emu::emu;
        char line[81]{};
        for (int i = 0; i < 80; ++i)
            line[i] = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>((24 * 80 + i) * 2)]);
        std::printf("r%02d cs=%04x ip=%04x mode=%u row24=[%s]\n", round,
            static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
            static_cast<unsigned>(e->x86.R_EIP & 0xFFFFu),
            static_cast<unsigned>(ram[0x449]),
            line);
        for (int row = 22; row < 25; ++row) {
            for (int col = 0; col < 80; ++col)
                std::putchar(static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>((row * 80 + col) * 2)]));
            std::putchar('\n');
        }
        std::putchar('\n');
    }
    return 0;
}