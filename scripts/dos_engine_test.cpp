// DOS engine hardware probe — SB16, OPL, mouse INT 33, keyboard, gameport.
// Build: g++ -std=c++20 -O2 -I Navigator/engine -I third_party/libx86emu/include \
//   scripts/dos_engine_test.cpp build/libx86emu.a -o build/bin/Linux/dos_engine_test

#include "FieldBios.hpp"
#include "FieldDevices.hpp"
#include "FieldDosConfig.hpp"
#include "FieldInput.hpp"
#include "FieldPlatform.hpp"
#include "FieldSb16.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static int fail(const char* msg) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int testSb16() {
    FieldDosConfig::applyPreset(FieldDosConfig::Preset::Full);
    const std::uint16_t b = FieldDosConfig::cfg.sbBasePort;
    FieldSb16::portOut(static_cast<std::uint16_t>(b + 6u), 1u);
    const std::uint8_t sig = FieldSb16::portIn(static_cast<std::uint16_t>(b + 0xAu));
    if (sig != 0xAAu) return fail("SB16 DSP reset signature");
    FieldSb16::portOut(static_cast<std::uint16_t>(b + 0xCu), 0xE1u);
    const std::uint8_t maj = FieldSb16::portIn(static_cast<std::uint16_t>(b + 0xAu));
    if (maj != 4u) return fail("SB16 DSP version major");
    FieldSb16::portOut(static_cast<std::uint16_t>(b + 0xCu), 0x14u);
    FieldSb16::portOut(static_cast<std::uint16_t>(b + 0xCu), 0x00u);
    FieldSb16::portOut(static_cast<std::uint16_t>(b + 0xCu), 0xFFu);
    if (!FieldSb16::isIrqPending()) return fail("SB16 DMA should raise IRQ");
    std::printf("OK SB16: base=0x%03X irq=%u dma=%u sig=0x%02X ver=%u.%u\n",
        static_cast<unsigned>(b), static_cast<unsigned>(FieldDosConfig::cfg.sbIrq),
        static_cast<unsigned>(FieldDosConfig::cfg.sbDma8),
        static_cast<unsigned>(sig), static_cast<unsigned>(maj),
        static_cast<unsigned>(FieldSb16::dspVersionMinor()));
    return 0;
}

static int testOpl() {
    FieldSb16::portOut(0x388u, 0x01u);
    FieldSb16::portOut(0x389u, 0x20u);
    const std::uint8_t idx = FieldSb16::portIn(0x388u);
    if (idx != 0x01u) return fail("OPL index latch");
    std::printf("OK OPL: ports 0x388/0x389\n");
    return 0;
}

static int testGameport() {
    FieldInput::state.joystick.x1 = 64;
    FieldInput::state.joystick.y1 = -64;
    FieldInput::state.joystick.buttons1 = 1u;
    const std::uint8_t gp = FieldDevices::portIn(0x201u);
    if (gp == 0xFFu) return fail("gameport read");
    std::printf("OK gameport: 0x%02X\n", static_cast<unsigned>(gp));
    return 0;
}

static std::vector<std::uint8_t> g_guest;

static int testInt33() {
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    g_guest.assign(bytes, 0);
    FieldX86Emu::ensure(g_guest.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    auto* e = FieldX86Emu::emu;
    if (!e) return fail("x86emu init");

    e->x86.R_EAX = 0x0000u;
    FieldBios::handleInt(e, 0x33, 0, false, 0);
    if ((e->x86.R_EAX & 0xFFFFu) != 0xFFFFu) return fail("INT33 reset");
    if ((e->x86.R_EBX & 0xFFFFu) != 2u) return fail("INT33 button count");

    FieldInput::state.mouse.x = 120;
    FieldInput::state.mouse.y = 80;
    FieldInput::state.mouse.buttons = 1u;
    e->x86.R_EAX = 0x0003u;
    FieldBios::handleInt(e, 0x33, 0, false, 0);
    if ((e->x86.R_ECX & 0xFFFFu) != 120u) return fail("INT33 mouse X");
    if ((e->x86.R_EDX & 0xFFFFu) != 80u) return fail("INT33 mouse Y");
    std::printf("OK INT33: mouse at (%u,%u)\n",
        static_cast<unsigned>(e->x86.R_ECX & 0xFFFFu),
        static_cast<unsigned>(e->x86.R_EDX & 0xFFFFu));
    return 0;
}

static int testInt16() {
    auto* e = FieldX86Emu::emu;
    if (!e) return fail("x86emu for INT16");
    FieldInput::state.keyboard.biosKey = 0x1C0Du;
    e->x86.R_EAX = 0x0000u;
    FieldBios::handleInt(e, 0x16, 0, true, 0);
    if ((e->x86.R_EAX & 0xFFFFu) != 0x1C0Du) return fail("INT16 Enter key");
    std::printf("OK INT16: BIOS key 0x%04X\n", static_cast<unsigned>(e->x86.R_EAX & 0xFFFFu));
    return 0;
}

int main() {
    FieldDosConfig::applyPreset(FieldDosConfig::Preset::Full);
    if (testSb16()) return 1;
    if (testOpl()) return 1;
    if (testGameport()) return 1;
    if (testInt33()) return 1;
    if (testInt16()) return 1;
    std::printf("DOS engine probe passed (SB16+OPL+mouse+keyboard+joystick)\n");
    return 0;
}