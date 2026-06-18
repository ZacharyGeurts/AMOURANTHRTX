#include "FieldBios.hpp"
#include "FieldDpmi.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <vector>

static int g_trace = 0;
static char g_msg[256];
static int g_msgn = 0;

static int traceInt(x86emu_t* e, u8 num, unsigned type) {
    const u8 ah = static_cast<u8>(e->x86.R_EAX >> 8);
    if (num == 0x10 && ah == 0x0E && g_msgn < static_cast<int>(sizeof g_msg) - 1) {
        g_msg[g_msgn++] = static_cast<char>(e->x86.R_EAX & 0xFFu);
        g_msg[g_msgn] = '\0';
    }
    if (num == 0x06 && g_trace < 30) {
        const std::uint16_t cs = static_cast<std::uint16_t>(e->x86.R_CS & 0xFFFFu);
        const std::uint32_t ip = FieldDpmi::isProtected(e) ? e->x86.R_EIP : (e->x86.R_EIP & 0xFFFFu);
        const std::uint32_t addr = FieldDpmi::isProtected(e) ? e->x86.R_CS_BASE + ip
            : (static_cast<std::uint32_t>(cs) << 4) + ip;
        std::fprintf(stderr, "#UD cs=%04x ip=%05x pm=%d disasm=%s bytes=",
            cs, ip, FieldDpmi::isProtected(e) ? 1 : 0, e->x86.disasm_buf);
        for (int i = 0; i < 12; ++i)
            std::fprintf(stderr, " %02x", static_cast<unsigned>(x86emu_read_byte(e, addr + static_cast<std::uint32_t>(i))));
        std::fputc('\n', stderr);
        ++g_trace;
    } else if (num == 0x2F || num == 0x31 || num == 0x15 || num == 0x21 || num == 0x67) {
        std::fprintf(stderr, "INT %02X ah=%02X ax=%04X bx=%04X cx=%04X dx=%04X edi=%08x\n", num, ah,
            static_cast<unsigned>(e->x86.R_EAX & 0xFFFFu),
            static_cast<unsigned>(e->x86.R_BX & 0xFFFFu),
            static_cast<unsigned>(e->x86.R_ECX & 0xFFFFu),
            static_cast<unsigned>(e->x86.R_EDX & 0xFFFFu),
            static_cast<unsigned>(e->x86.R_EDI));
    } else if (num == 0x13 && ah == 0x02 && g_trace < 20) {
        std::fprintf(stderr, "INT13 read dl=%u cyl=%u head=%u sec=%u\n",
            static_cast<unsigned>(e->x86.R_EDX & 0xFFu),
            static_cast<unsigned>(((e->x86.R_ECX >> 8) & 0xFF) | ((e->x86.R_ECX & 0xC0u) << 2)),
            static_cast<unsigned>((e->x86.R_EDX >> 8) & 0xFFu),
            static_cast<unsigned>(e->x86.R_ECX & 0x3Fu));
        ++g_trace;
    } else if (g_trace < 20) {
        std::fprintf(stderr, "INT %02X ah=%02X ax=%04X\n", num, ah,
            static_cast<unsigned>(e->x86.R_EAX & 0xFFFFu));
        ++g_trace;
    }
    return FieldX86Emu::intrThunk(e, num, type);
}

int main() {
    FieldDosConfig::applyPreset(FieldDosConfig::Preset::Full);
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);
    FieldDos::loadHdImage(FieldDos::defaultHdPath("."));
    FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
        FieldDos::defaultImagePath("."));
    std::vector<std::uint8_t> exe;
    if (!FieldDos::readHostFile("C:\\DOOM.EXE", exe)) return 1;
    FieldX86Emu::ensure(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    x86emu_set_intr_handler(FieldX86Emu::emu, traceInt);
    FieldBios::launchMzExec(FieldX86Emu::emu, exe, "C:\\DOOM.EXE");
    FieldX86Emu::guestAppExecute = true;
    FieldX86Emu::Ctx ctx{};
    for (int i = 0; i < 8; ++i)
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 5'000'000u);
    auto* e = FieldX86Emu::emu;
    std::fprintf(stderr, "DOS4GW msg: %s\n", g_msg);
    std::fprintf(stderr, "done halted=%d cs=%04x ip=%04x ticks=%llu\n",
        (e->x86.mode & _MODE_HALTED) ? 1 : 0,
        static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
        static_cast<unsigned>(e->x86.R_EIP & 0xFFFFu),
        static_cast<unsigned long long>(ctx.ticks));
    return 0;
}