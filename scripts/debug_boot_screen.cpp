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
    for (int round = 0; round < 20; ++round) {
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 1'000'000u);
        auto* e = FieldX86Emu::emu;
        if (round == 2) {
            std::printf("scan COM: ");
            for (std::uint32_t off = 0; off < 0xA0000u; off += 0x10u)
                if (ram[off] == 0x8Cu && ram[off + 1] == 0xC8u)
                    std::printf("%05x ", off);
            std::putchar('\n');
        }
        std::printf("round %d cs=%04x ip=%04x ticks=%llu\n", round,
            static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
            static_cast<unsigned>(e->x86.R_EIP & 0xFFFFu),
            static_cast<unsigned long long>(ctx.ticks));
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 80; ++col)
                std::putchar(static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>((row * 80 + col) * 2)]));
            std::putchar('\n');
        }
        std::putchar('\n');
    }
    return 0;
}