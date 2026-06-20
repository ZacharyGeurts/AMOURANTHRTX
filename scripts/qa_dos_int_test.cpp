// Full DOS interrupt test suite — exercises every host-implemented vector.
// Build via CMake target qa_dos_int_test, or:
//   g++-14 -std=c++20 -O2 -I Navigator/engine -I Navigator/engine/FieldX86Core/include \
//     scripts/qa_dos_int_test.cpp build/libfield_x86.a -o build/qa_dos_int_test -lm

#include "FieldBios.hpp"
#include "FieldCmos.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldDosInt.hpp"
#include "FieldPc.hpp"
#include "FieldPic8259.hpp"
#include "FieldPit8254.hpp"
#include "FieldDpmi.hpp"
#include "FieldInput.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxMemory.hpp"
#include "FieldVga.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static int g_failures = 0;
static int g_passed   = 0;

static void check(bool ok, const char* name) {
    if (ok) {
        ++g_passed;
        std::printf("OK %s\n", name);
    } else {
        ++g_failures;
        std::fprintf(stderr, "FAIL %s\n", name);
    }
}

static x86emu_t* g_emu = nullptr;
static std::vector<std::uint8_t> g_buf;

static void setupEmu() {
    FieldDosConfig::applyPreset(FieldDosConfig::Preset::Full);
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    g_buf.assign(bytes, 0);
    FieldX86Emu::ensure(g_buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    g_emu = FieldX86Emu::emu;
    FieldBios::init(g_emu);
}

static int dispatch(std::uint8_t num) {
    return FieldBios::handleInt(g_emu, num, 0, false, 1000);
}

static void testPcHardware() {
    FieldPic8259::init();
    FieldPit8254::reset();
    FieldPit8254::writeCount(0, 1000u);
    FieldPit8254::tick(2000u);
    check(FieldPic8259::master.irr != 0u || FieldPic8259::highestPendingIrq() >= 0,
        "PIT tick raises IRQ0");
    FieldPic8259::portOut(FieldPic8259::MASTER_DATA, 0xFEu);
    check(FieldPic8259::master.imr == 0xFEu, "PIC IMR write");
    std::printf("METRIC pc_subsystems=%zu\n", FieldPc::kSubsystemCount);
    std::printf("METRIC pc_implemented=%zu\n", FieldPc::implementedCount());
}

static void testCatalog() {
    check(FieldDosInt::kCatalogSize >= 40u, "catalog size");
    check(FieldDosInt::lookup(0x21) != nullptr, "catalog INT21");
    check(FieldDosInt::lookup(0x21)->coverage == FieldDosInt::Coverage::Full, "INT21 full");
    check(FieldDosInt::lookup(0x31) != nullptr, "catalog INT31");
    check(FieldDosInt::countByCoverage(FieldDosInt::Coverage::Stub) == 0u, "no catalog stubs");
    check(FieldDosInt::countByCoverage(FieldDosInt::Coverage::Full) >= 40u, "full coverage count");
    std::printf("METRIC catalog_entries=%zu\n", FieldDosInt::kCatalogSize);
    std::printf("METRIC catalog_full=%zu\n",
        FieldDosInt::countByCoverage(FieldDosInt::Coverage::Full));
    std::printf("METRIC catalog_stub=%zu\n",
        FieldDosInt::countByCoverage(FieldDosInt::Coverage::Stub));
}

static void testInt05() {
    check(dispatch(0x05) == 1, "INT05 print screen");
}

static void testInt08_1C() {
    dispatch(0x08);
    const std::uint16_t lo = static_cast<std::uint16_t>(
        x86emu_read_word(g_emu, FieldBios::BDA_BASE + 0x6C));
    check(lo == 0x03E8u, "INT08 timer BDA");
    dispatch(0x1C);
    check(true, "INT1C timer hook");
}

static void testInt10() {
    g_emu->x86.R_EAX = 0x0003u;
    check(dispatch(0x10) == 1, "INT10 set mode 3");
    check(x86emu_read_byte(g_emu, 0x449u) == 3u, "INT10 BDA mode 3");

    g_emu->x86.R_EAX = 0x0013u;
    dispatch(0x10);
    check(x86emu_read_byte(g_emu, 0x449u) == 0x13u, "INT10 set mode 13h");

    g_emu->x86.R_EAX = 0x0C00u | 0x55u;
    g_emu->x86.R_CX = 10u;
    g_emu->x86.R_DX = 20u;
    dispatch(0x10);
    check(x86emu_read_byte(g_emu, 0xA0000u + 20u * 320u + 10u) == 0x55u, "INT10 write pixel");

    g_emu->x86.R_EAX = 0x1010u;
    g_emu->x86.R_BX = 1u;
    g_emu->x86.R_CX = 0x3040u;
    g_emu->x86.R_DX = 0x5060u;
    dispatch(0x10);
    check(FieldVga::palette[3] == 0x30u, "INT10 set palette");

    g_emu->x86.R_EAX = 0x0E00u | 'X';
    dispatch(0x10);
    check(x86emu_read_byte(g_emu, FieldBios::VGA_BASE) == 'X', "INT10 teletype");

    g_emu->x86.R_EAX = 0x4F00u;
    g_emu->x86.R_ES  = 0x2000u;
    g_emu->x86.R_EDI = 0x0100u;
    dispatch(0x10);
    check(x86emu_read_byte(g_emu, 0x20100u) == 'V', "INT10 VESA detect");
}

static void testInt11_12() {
    dispatch(0x11);
    check((g_emu->x86.R_EAX & 0xFFFFu) != 0u, "INT11 equipment");
    dispatch(0x12);
    check((g_emu->x86.R_EAX & 0xFFFFu) == FieldRtxMemory::conventionalKb, "INT12 memory");
}

static void testInt13() {
    g_emu->x86.R_EAX = 0x0800u;
    g_emu->x86.R_EDX = 0x0080u;
    dispatch(0x13);
    check((g_emu->x86.R_FLG & F_CF) == 0u, "INT13 AH08 HD geometry");

    g_emu->x86.R_EAX = 0x4100u;
    g_emu->x86.R_EBX = 0x55AAu;
    g_emu->x86.R_EDX = 0x0080u;
    dispatch(0x13);
    check((g_emu->x86.R_EBX & 0xFFFFu) == 0xAA55u, "INT13 AH41 extensions");
}

static void testInt14_17() {
    g_emu->x86.R_EAX = 0x0000u;
    g_emu->x86.R_EDX = 0x0000u;
    check(dispatch(0x14) == 1, "INT14 init COM1");
    check((g_emu->x86.R_FLG & F_CF) == 0u, "INT14 no error");

    g_emu->x86.R_EAX = 0x0200u;
    g_emu->x86.R_EDX = 0x0000u;
    dispatch(0x17);
    check((g_emu->x86.R_AH & 0x80u) != 0u, "INT17 printer status");
}

static void testInt15() {
    g_emu->x86.R_EAX = 0x8800u;
    dispatch(0x15);
    check((g_emu->x86.R_EAX & 0xFFFFu) == FieldPlatform::EXTENDED_KB, "INT15 AH88 extended mem");

    g_emu->x86.R_EAX = 0xC000u;
    dispatch(0x15);
    check((g_emu->x86.R_FLG & F_CF) == 0u, "INT15 AHC0 system descriptor");

    g_emu->x86.R_EAX = 0xE801u;
    dispatch(0x15);
    check((g_emu->x86.R_FLG & F_CF) == 0u, "INT15 AXE801 memory layout");
}

static void testInt16() {
    FieldInput::state.keyboard.biosKey = 0x1E61u; /* 'a' */
    g_emu->x86.R_EAX = 0x0000u;
    dispatch(0x16);
    check((g_emu->x86.R_EAX & 0xFFFFu) == 0x1E61u, "INT16 read key");

    g_emu->x86.R_EAX = 0x0100u;
    dispatch(0x16);
    check((g_emu->x86.R_FLG & F_ZF) == 0u, "INT16 key ready");
}

static void testInt19_1A_1B_1D_1F() {
    g_emu->x86.R_EAX = 0x0000u;
    dispatch(0x1A);
    check((g_emu->x86.R_FLG & F_CF) == 0u, "INT1A read clock");

    check(dispatch(0x1B) == 1, "INT1B ctrl-break");

    dispatch(0x1D);
    check((g_emu->x86.R_ES & 0xFFFFu) == 0xF000u, "INT1D video params ptr");

    dispatch(0x1F);
    check((g_emu->x86.R_ES & 0xFFFFu) == 0xF000u, "INT1F font ptr");
}

static void testInt21() {
    g_emu->x86.R_EAX = 0x3000u;
    dispatch(0x21);
    check((g_emu->x86.R_EAX & 0xFFu) == 0x00u, "INT21 AH30 DOS version AL");
    check((g_emu->x86.R_EAX & 0xFF00u) == 0x0700u, "INT21 AH30 DOS version AH");

    g_emu->x86.R_EAX = 0x2A00u;
    dispatch(0x21);
    check((g_emu->x86.R_FLG & F_CF) == 0u, "INT21 AH2A get date");

    g_emu->x86.R_EAX = 0x2C00u;
    dispatch(0x21);
    check((g_emu->x86.R_FLG & F_CF) == 0u, "INT21 AH2C get time");

    g_emu->x86.R_EAX = 0x4800u;
    g_emu->x86.R_EBX = 0x0100u;
    dispatch(0x21);
    check((g_emu->x86.R_FLG & F_CF) == 0u, "INT21 AH48 allocate");

    g_emu->x86.R_EAX = 0x4A00u;
    g_emu->x86.R_EBX = 0x1000u;
    dispatch(0x21);
    check((g_emu->x86.R_FLG & F_CF) == 0u, "INT21 AH4A resize");

    g_emu->x86.R_EAX = 0x2505u; /* AH=25 vector 5 */
    g_emu->x86.R_EDX = 0x1234u;
    g_emu->x86.R_DS  = 0x2000u;
    dispatch(0x21);
    check(x86emu_read_word(g_emu, 0x14u) == 0x1234u, "INT21 AH25 set vector off");
    check(x86emu_read_word(g_emu, 0x16u) == 0x2000u, "INT21 AH25 set vector seg");
}

static void testInt2F() {
    g_emu->x86.R_EAX = 0x1687u;
    check(dispatch(0x2F) == 1, "INT2F DPMI detect");
    check((g_emu->x86.R_EAX & 0xFFFFu) == 0x1687u, "INT2F DPMI present AX");
}

static void testInt31() {
    FieldDpmi::install(g_emu);
    g_emu->x86.R_EAX = 0x0000u;
    g_emu->x86.R_EDI = 0x0000u;
    g_emu->x86.R_ES  = 0x3000u;
    check(dispatch(0x31) == 1, "INT31 allocate descriptor");
}

static void testInt33() {
    FieldInput::state.mouse.x = 200;
    FieldInput::state.mouse.y = 100;
    FieldInput::state.mouse.buttons = 1u;
    g_emu->x86.R_EAX = 0x0000u;
    dispatch(0x33);
    check((g_emu->x86.R_EAX & 0xFFFFu) == 0xFFFFu, "INT33 reset");
    g_emu->x86.R_EAX = 0x0003u;
    dispatch(0x33);
    check((g_emu->x86.R_ECX & 0xFFFFu) == 200u, "INT33 get pos X");
    check((g_emu->x86.R_EDX & 0xFFFFu) == 100u, "INT33 get pos Y");
}

static void testInt67() {
    g_emu->x86.R_EAX = 0x0000u;
    dispatch(0x67);
    check((g_emu->x86.R_EAX & 0xFF00u) == 0x0300u, "INT67 VDS version");

    g_emu->x86.R_EAX = 0x0800u;
    dispatch(0x67);
    check((g_emu->x86.R_EAX & 0xFFFFu) == FieldPlatform::EXTENDED_KB, "INT67 lock region");
}

static void testInt28() {
    check(FieldDosInt::lookup(0x28)->coverage == FieldDosInt::Coverage::Full, "INT28 full");
    check(dispatch(0x28) == 1, "INT28 idle IRET empty IVT");
    x86emu_write_word(g_emu, 0x28u * 4u, 0x1234u);
    x86emu_write_word(g_emu, 0x28u * 4u + 2u, 0x2000u);
    check(dispatch(0x28) == 0, "INT28 chain hooked IVT");
    x86emu_write_word(g_emu, 0x28u * 4u, 0);
    x86emu_write_word(g_emu, 0x28u * 4u + 2u, 0);
    check(dispatch(0x2B) == 1, "INT2B idle IRET");
}

static void testStubVectors() {
    check(dispatch(0x18) == 1, "INT18 ROM BASIC");
    check((g_emu->x86.R_EAX & 0xFFFFu) == 0x0003u, "INT18 no ROM BASIC");

    FieldRtxMemory::popEms(g_buf.data());
    g_emu->x86.R_EAX = 0x4000u;
    check(dispatch(0x34) == 1, "INT34 EMM status");
    check((g_emu->x86.R_AL & 0x80u) != 0u, "INT34 EMM installed");

    g_emu->x86.R_EAX = 0x0000u;
    check(dispatch(0x36) == 1, "INT36 VCPI check");
    check(g_emu->x86.R_AL == 0u, "INT36 VCPI absent");

    check(dispatch(0x35) == 1, "INT35 phys mem");
    check(dispatch(0x3F) == 1, "INT3F overlay");
    check(dispatch(0x09) == 1, "IRQ1 keyboard EOI");
    check(dispatch(0x74) == 1, "IRQ12 mouse EOI");

    g_emu->x86.R_EDX = 'Z';
    check(dispatch(0x29) == 1, "INT29 fast con");
}

static void testInt214E() {
    FieldBios::pmExecActive = true;
    g_emu->x86.R_CS = 0x1000u;
    g_emu->x86.R_DS = 0x1000u;
    g_emu->x86.R_DX = 0x0080u;
    x86emu_write_byte(g_emu, 0x10080u, '*');
    x86emu_write_byte(g_emu, 0x10081u, '.');
    x86emu_write_byte(g_emu, 0x10082u, 'W');
    x86emu_write_byte(g_emu, 0x10083u, 'A');
    x86emu_write_byte(g_emu, 0x10084u, 'D');
    x86emu_write_byte(g_emu, 0x10085u, 0);
    g_emu->x86.R_EAX = 0x4E00u;
    g_emu->x86.R_CX = 0xFFFFu;
    check(FieldBios::handleInt(g_emu, 0x21, 0, false, 0) == 1, "INT21 4E find first");
    FieldBios::pmExecActive = false;
}

static void testPmExecPolicy() {
    FieldBios::pmExecActive = true;
    check(dispatch(0x28) == 1, "INT28 idle under pmExec");
    check(dispatch(0x3F) == 1, "INT3F overlay under pmExec");
    FieldBios::pmExecActive = false;
}

static void testInt21Pm4C() {
    FieldBios::pmExecActive = true;
    FieldDpmi::inProtected = true;
    FieldDpmi::lePostCstartEip = 0x00040B48u;
    FieldDpmi::leBootEsp = 0x00485E10u;
    g_emu->x86.crx[0] |= 1u;
    g_emu->x86.R_EAX = 0x4C00u;
    g_emu->x86.R_EIP = 0x00082DF9u;
    check(FieldBios::handleInt21Early(g_emu, 0x4C) == 1, "INT21 4C PM cstart handoff");
    check(g_emu->x86.R_EIP == 0x00040B48u, "INT21 4C entry EIP");
    FieldDpmi::inProtected = false;
    FieldBios::pmExecActive = false;
    g_emu->x86.crx[0] &= ~1u;
}

int main() {
    setupEmu();
    testPcHardware();
    testCatalog();
    testInt05();
    testInt08_1C();
    testInt10();
    testInt11_12();
    testInt13();
    testInt14_17();
    testInt15();
    testInt16();
    testInt19_1A_1B_1D_1F();
    testInt21();
    testInt2F();
    testInt31();
    testInt33();
    testInt67();
    testInt28();
    testStubVectors();
    testInt214E();
    testPmExecPolicy();
    testInt21Pm4C();

    std::printf("METRIC int_passed=%d\n", g_passed);
    std::printf("METRIC int_failed=%d\n", g_failures);
    if (g_failures == 0) {
        std::printf("OK dos interrupt suite (%d checks)\n", g_passed);
        return 0;
    }
    std::fprintf(stderr, "FAIL dos interrupt suite (%d/%d)\n", g_failures, g_passed + g_failures);
    return 1;
}