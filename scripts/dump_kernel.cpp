#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <vector>

int main(int argc, char** argv) {
    const unsigned target = argc > 1 ? static_cast<unsigned>(std::strtoul(argv[1], nullptr, 16)) : 0x600u;
    std::vector<std::uint8_t> buf(FieldPlatform::FIELD_X86_DIE_UINTS * 4, 0);
    FieldDos::loadHdImage(FieldDos::defaultHdPath("."));
    FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
        FieldDos::defaultImagePath("."));
    auto* ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldX86Emu::Ctx ctx{};
    FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
        FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 3'000'000u);
    const std::uint32_t lin = 0x7000u + target;
    std::printf("bytes@0070:%04x: ", target);
    for (int i = 0; i < 32; ++i) std::printf("%02x ", ram[lin + static_cast<std::uint32_t>(i)]);
    std::putchar('\n');
    return 0;
}