#include "FieldBios.hpp"
#include "FieldDoom.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <vector>

int main() {
    FieldDosConfig::applyPreset(FieldDosConfig::Preset::Full);
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);
    FieldDosConfig::cfg.s1e1Playthrough = false;
    FieldDos::bootGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, ".");
    auto* ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldDos::hdGuestRamPtr = ram;
    FieldX86Emu::Ctx ctx{};
    FieldBios::guestBootSettled = true;
    FieldBios::rtxShellActive = true;
    FieldDoom::launch(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, ram);
    auto* e = FieldX86Emu::emu;
    const std::uint32_t base = e->x86.R_DS_BASE;
    std::fprintf(stderr, "bytes 853f0:");
    for (int i = 0; i < 96; ++i)
        std::fprintf(stderr, " %02x", static_cast<unsigned>(x86emu_read_byte(e, base + 0x853f0u + static_cast<unsigned>(i))));
    std::fputc('\n', stderr);
    for (std::uint32_t va : {0x254e0u, 0x254f8u, 0x75e08u, 0x75effu}) {
        std::fprintf(stderr, "%08x:", va);
        for (int i = 0; i < 6; ++i)
            std::fprintf(stderr, " %08x", x86emu_read_dword(e, base + va + static_cast<unsigned>(i * 4u)));
        std::fputc('\n', stderr);
    }
    return 0;
}