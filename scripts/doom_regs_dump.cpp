#include "FieldBios.hpp"
#include "FieldDoom.hpp"
#include "FieldDpmi.hpp"
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
    for (int step = 0; step < 200; ++step) {
        FieldDoom::pump(ram, buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, 0u, false, 100'000u);
        auto* ex = FieldX86Emu::emu;
        if (ex && ex->x86.R_EIP == 0x8542bu) {
            std::fprintf(stderr, "hit 8542b step=%d ebx=%08x [ebx+2]=%08x\n", step,
                ex->x86.R_EBX, x86emu_read_dword(ex, ex->x86.R_DS_BASE + ex->x86.R_EBX + 2u));
            break;
        }
    }
    auto* e = FieldX86Emu::emu;
    const std::uint32_t eip = e->x86.R_EIP;
    const std::uint32_t pc = e->x86.R_CS_BASE + eip;
    std::fprintf(stderr, "eip=%08x esp=%08x eax=%08x ebx=%08x ecx=%08x edx=%08x ebp=%08x\n",
        eip, e->x86.R_ESP, e->x86.R_EAX, e->x86.R_EBX, e->x86.R_ECX, e->x86.R_EDX, e->x86.R_EBP);
    std::fprintf(stderr, "bytes@pc-16:");
    for (int i = -16; i < 16; ++i)
        std::fprintf(stderr, " %02x", static_cast<unsigned>(x86emu_read_byte(e, pc + static_cast<unsigned>(i))));
    std::fputc('\n', stderr);
    const std::uint32_t sp = e->x86.R_SS_BASE + e->x86.R_ESP;
    std::fprintf(stderr, "stack top:");
    for (int i = 0; i < 8; ++i)
        std::fprintf(stderr, " %08x", x86emu_read_dword(e, sp + static_cast<unsigned>(i * 4u)));
    std::fputc('\n', stderr);
    std::fprintf(stderr, "mem@254e0:");
    for (int i = 0; i < 8; ++i)
        std::fprintf(stderr, " %08x", x86emu_read_dword(e, e->x86.R_DS_BASE + 0x254e0u + static_cast<unsigned>(i * 4u)));
    std::fputc('\n', stderr);
    const std::uint32_t tbl = e->x86.R_EBX + 2u;
    std::fprintf(stderr, "dword@[ebx+2]=%08x tbl@ebx:",
        x86emu_read_dword(e, e->x86.R_DS_BASE + tbl));
    for (int i = 0; i < 8; ++i)
        std::fprintf(stderr, " %08x", x86emu_read_dword(e, e->x86.R_DS_BASE + e->x86.R_EBX + static_cast<unsigned>(i * 4u)));
    std::fputc('\n', stderr);
    return 0;
}