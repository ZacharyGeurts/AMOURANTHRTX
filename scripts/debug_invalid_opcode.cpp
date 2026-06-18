#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static bool screenHas(const std::uint8_t* ram, const char* needle) {
    char buf[80 * 25 * 2 + 1]{};
    for (int i = 0; i < 80 * 25; ++i)
        buf[i] = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>(i * 2)]);
    return std::strstr(buf, needle) != nullptr;
}

int main() {
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);

    if (!FieldDos::loadHdImage(FieldDos::defaultHdPath("."))) return 1;
    if (!FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldDos::defaultImagePath(".")))
        return 1;

    auto* ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldX86Emu::Ctx ctx{};
    FieldX86Emu::ensure(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);

    for (int round = 0; round < 200; ++round) {
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 2'000'000u);
        if (round == 6) {
            const std::uint32_t base = 0x8F990u;
            std::printf("mem@8f99:100 ");
            for (int i = 0; i < 32; ++i) std::printf("%02x ", ram[base + 0x100u + static_cast<std::uint32_t>(i)]);
            std::putchar('\n');
        }
        if (round >= 5 && round <= 8) {
            char line[81]{};
            for (int col = 0; col < 80; ++col)
                line[col] = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>(24 * 80 + col) * 2]);
            auto* e = FieldX86Emu::emu;
            std::printf("round %d cs=%04x ip=%04x lastline=%.80s\n", round,
                static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
                static_cast<unsigned>(e->x86.R_EIP & 0xFFFFu), line);
        }
        if (screenHas(ram, "C:\\>") || screenHas(ram, "C:>")) {
            auto* e = FieldX86Emu::emu;
            std::printf("PROMPT round=%d cs=%04x ip=%04x\n", round,
                static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
                static_cast<unsigned>(e->x86.R_EIP & 0xFFFFu));
            return 0;
        }
        if (screenHas(ram, "Invalid Opcode")) {
            auto* e = FieldX86Emu::emu;
            const std::uint16_t cs = static_cast<std::uint16_t>(e->x86.R_CS & 0xFFFFu);
            const std::uint16_t ip = static_cast<std::uint16_t>(e->x86.R_EIP & 0xFFFFu);
            const std::uint32_t addr = (static_cast<std::uint32_t>(cs) << 4) + ip;
            std::printf("Invalid Opcode round=%d cs=%04x ip=%04x linear=%05x\n", round, cs, ip, addr);
            std::printf("EAX=%08x EBX=%08x ECX=%08x EDX=%08x\n",
                static_cast<unsigned>(e->x86.R_EAX), static_cast<unsigned>(e->x86.R_EBX),
                static_cast<unsigned>(e->x86.R_ECX), static_cast<unsigned>(e->x86.R_EDX));
            std::printf("bytes@ip: ");
            for (int i = 0; i < 16; ++i)
                std::printf("%02x ", ram[addr + static_cast<std::uint32_t>(i)]);
            std::putchar('\n');
            return 0;
        }
    }
    std::printf("no invalid opcode seen\n");
    return 1;
}