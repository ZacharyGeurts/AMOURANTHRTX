#include "FieldDoom.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <vector>

static int g_n = 0;

static int traceInt(x86emu_t* e, u8 num, unsigned type) {
    if (g_n < 40 && (num == 0x10 || num == 0x21)) {
        const u8 ah = static_cast<u8>(e->x86.R_EAX >> 8);
        const u8 al = static_cast<u8>(e->x86.R_EAX & 0xFFu);
        std::fprintf(stderr, "INT %02X ah=%02X al=%02X ax=%04X\n", num, ah, al,
            static_cast<unsigned>(e->x86.R_EAX & 0xFFFFu));
        ++g_n;
    }
    return FieldX86Emu::intrThunk(e, num, type);
}

int main() {
    FieldDosConfig::applyPreset(FieldDosConfig::Preset::Full);
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);
    FieldX86Emu::Ctx ctx{};
    FieldDos::bootGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, ".");
    auto* ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldDos::hdGuestRamPtr = ram;
    for (int i = 0; i < 80; ++i) {
        if (i >= 4 && i < 10) ctx.key = 0x1C0Du;
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 2'000'000u);
    }
    FieldBios::guestBootSettled = true;
    FieldX86Emu::ensure(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    x86emu_set_intr_handler(FieldX86Emu::emu, traceInt);
    FieldDoom::launch(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, ram);
    for (int i = 0; i < 10; ++i)
        FieldDoom::pump(ram, buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, 0u, false, 6'000'000u);
    std::fprintf(stderr, "done eip=%08x mode=%u\n",
        static_cast<unsigned>(FieldX86Emu::emu->x86.R_EIP),
        static_cast<unsigned>(ram[0x449u]));
    return 0;
}