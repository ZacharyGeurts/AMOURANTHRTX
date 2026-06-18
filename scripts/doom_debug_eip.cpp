#include "FieldAmmoExec.hpp"
#include "FieldBios.hpp"
#include "FieldDoom.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldDpmi.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <vector>

static void dumpInsn(x86emu_t* e, std::uint32_t eip, int n) {
    const std::uint32_t base = e->x86.R_CS_BASE;
    std::fprintf(stderr, "  %08x:", eip);
    for (int i = 0; i < n; ++i)
        std::fprintf(stderr, " %02x",
            static_cast<unsigned>(x86emu_read_byte(e, base + eip + static_cast<std::uint32_t>(i))));
    std::fputc('\n', stderr);
}

int main() {
    FieldDosConfig::applyPreset(FieldDosConfig::Preset::Full);
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);
    std::uint8_t* ram = nullptr;
    FieldX86Emu::Ctx ctx{};

    FieldDosConfig::cfg.s1e1Playthrough = false;
    FieldDosConfig::cfg.cyclesBoot = 4'000'000u;
    FieldDos::bootGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, ".");
    ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldDos::hdGuestRamPtr = ram;
    for (int round = 0; round < 80; ++round) {
        if (round >= 4 && round < 10) ctx.key = 0x1C0Du;
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 2'000'000u);
        if (FieldBios::guestBootSettled) break;
    }
    FieldBios::guestBootSettled = true;

    if (!FieldDoom::launch(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, ram))
        return 1;

    auto* e = FieldX86Emu::emu;
    std::fprintf(stderr, "leBootEsp=%08x before pump\n", FieldDpmi::leBootEsp);

    const std::uint32_t base = e->x86.R_CS_BASE;
    for (std::uint32_t slot = 0x254e0u; slot < 0x25500u; slot += 6u) {
        const std::uint32_t v = x86emu_read_dword(e, base + slot + 2u);
        if (v) std::fprintf(stderr, "import %08x -> %08x\n", slot, v);
    }
    for (int round = 0; round < 6; ++round) {
        FieldDoom::pump(ram, buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, 0u, false, 8'000'000u);
        if (round == 0 || round == 39)
            std::fprintf(stderr, "round=%d eip=%08x mode=%u\n", round,
                static_cast<unsigned>(e->x86.R_EIP),
                static_cast<unsigned>(ram[0x449u]));
    }

    /* Force D_DoomMain and re-pump */
    constexpr std::uint32_t kMain = 0x5BBE4u;
    e->x86.R_EIP = kMain;
    e->x86.saved_eip = kMain;
    e->x86.R_ESP = FieldDpmi::leBootEsp;
    x86emu_write_dword(e, e->x86.R_SS_BASE + FieldDpmi::leBootEsp, 0x000FFFF0u);
    for (int round = 0; round < 80; ++round)
        FieldDoom::pump(ram, buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, 0u, false, 16'000'000u);
    std::fprintf(stderr, "after force main eip=%08x mode=%u nz-check later\n",
        static_cast<unsigned>(e->x86.R_EIP), static_cast<unsigned>(ram[0x449u]));
    const std::uint32_t eip = e->x86.R_EIP;
    std::fprintf(stderr,
        "eip=%08x cs_base=%08x eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x esp=%08x ebp=%08x\n",
        eip, e->x86.R_CS_BASE,
        static_cast<unsigned>(e->x86.R_EAX), static_cast<unsigned>(e->x86.R_EBX),
        static_cast<unsigned>(e->x86.R_ECX), static_cast<unsigned>(e->x86.R_EDX),
        static_cast<unsigned>(e->x86.R_ESI), static_cast<unsigned>(e->x86.R_EDI),
        static_cast<unsigned>(e->x86.R_ESP), static_cast<unsigned>(e->x86.R_EBP));
    std::fprintf(stderr, "ds_base=%08x es_base=%08x mode=%u ram449=%u\n",
        e->x86.R_DS_BASE, e->x86.R_ES_BASE,
        static_cast<unsigned>(ram[0x449u]), static_cast<unsigned>(ram[0x449u]));
    dumpInsn(e, eip, 16);
    dumpInsn(e, eip - 8, 16);
    const std::uint32_t sp = e->x86.R_ESP;
    std::fprintf(stderr, "stack @ esp:\n");
    for (int i = 0; i < 8; ++i) {
        const std::uint32_t v = x86emu_read_dword(e, e->x86.R_SS_BASE + sp + static_cast<std::uint32_t>(i * 4));
        std::fprintf(stderr, "  [%08x] = %08x\n", sp + static_cast<std::uint32_t>(i * 4), v);
    }
    return 0;
}