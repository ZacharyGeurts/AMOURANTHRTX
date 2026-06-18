// QA: RTX-AMMOS GPU-only shell — no host CPU / libx86emu required.
#include "FieldAmmoFat.hpp"
#include "FieldAmmoToolchain.hpp"
#include "FieldDos.hpp"
#include "FieldMscdex.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxBasicIde.hpp"
#include "FieldRtxBoot.hpp"
#include "FieldRtxHelp.hpp"
#include "FieldRtxShell.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static bool screenHas(const std::uint8_t* ram, const char* needle) {
    char buf[80 * 25 + 1]{};
    for (int i = 0; i < 80 * 25; ++i)
        buf[i] = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>(i * 2)]);
    return std::strstr(buf, needle) != nullptr;
}

static void runCmd(std::uint8_t* ram, const char* line) {
    FieldRtxShell::execLine(line, ram, FieldRtxShell::echoChar,
        FieldRtxShell::defaultNewline, FieldRtxShell::defaultPrompt);
}

int main(int argc, char** argv) {
    if (FieldRtxHelp::argcWantsHelp(argc, argv)) {
        FieldRtxHelp::printBinaryHelp(stderr);
        return 0;
    }
    std::vector<std::uint8_t> buf(FieldPlatform::GUEST_RAM_BYTES, 0);
    std::uint8_t* ram = buf.data();

    FieldDos::loadHdImage(FieldDos::defaultHdPath("."));
    if (!FieldAmmoFat::mount()) {
        std::fprintf(stderr, "FAIL AMMOFAT mount\n");
        return 1;
    }
    std::printf("OK AMMOFAT mount %s\n", FieldAmmoFat::volumeLabel().c_str());

    FieldMscdex::install();
    std::uint16_t bx = 0, cx = 0, dx = 0;
    std::uint32_t eax = 0;
    FieldMscdex::handle(0x1500, 0, 0, bx, cx, dx, eax);
    if (cx != 0 && !FieldCdRom::ready) {
        /* no ISO ok */
    }
    std::printf("OK MSCDEX installed drives=%u\n", cx);

    auto checkCmd = [&](const char* cmd, const char* needle, const char* label) -> bool {
        FieldRtxBoot::paintWelcome(ram);
        runCmd(ram, cmd);
        if (!screenHas(ram, needle)) {
            std::fprintf(stderr, "FAIL %s missing %s\n", label, needle);
            return false;
        }
        std::printf("OK %s\n", label);
        return true;
    };

    FieldRtxBoot::paintWelcome(ram);
    if (!screenHas(ram, "RTX-AMMOS")) {
        std::fprintf(stderr, "FAIL GPU welcome\n");
        return 1;
    }
    std::printf("OK GPU welcome\n");

    ram[0x450] = 0;
    ram[0x451] = 0;
    FieldRtxShell::echoChar(ram, 'Z');
    if (ram[0xB8000u] != 'Z') {
        std::fprintf(stderr, "FAIL echoChar cursor=%u,%u got=%c\n",
            ram[0x450], ram[0x451], ram[0xB8000u]);
        return 1;
    }
    std::printf("OK echoChar\n");

    if (!checkCmd("VER", "AMMOFAT", "VER")) return 1;
    if (!checkCmd("VER", "GPU-only", "VER GPU-only")) return 1;
    if (!checkCmd("AMMOFAT", "AMMOFAT v1", "AMMOFAT")) return 1;
    if (!checkCmd("MSCDEX", "MSCDEX", "MSCDEX")) return 1;
    if (!checkCmd("TOOLS", "AMMOASM", "TOOLS")) return 1;

    static const char kSampleAsm[] =
        ".MODEL TINY\n.CODE\nORG 100h\nstart:\n"
        "mov ah,9\nmov dx,offset msg\nint 21h\nmov ax,4C00h\nint 21h\n"
        ".DATA\nmsg db 'BUILD QA','$'\nEND start\n";
    FieldAmmoFat::writeRootFile("C:\\HELLO.ASM",
        reinterpret_cast<const std::uint8_t*>(kSampleAsm), sizeof kSampleAsm - 1u);

    if (!checkCmd("BUILD", "HELLO.COM", "BUILD")) return 1;
    if (!checkCmd("DRIVERS", "RTXCD", "DRIVERS")) return 1;
    if (!checkCmd("SCALE", "scale=", "SCALE")) return 1;
    if (!checkCmd("DIR", "FIELDLAY.TXT", "DIR")) return 1;
    if (!checkCmd("DIR", "file(s)", "DIR summary")) return 1;
    if (!checkCmd("HELP", "Golden Era", "HELP")) return 1;
    if (!checkCmd("QBASIC /HELP", "AmmoCode", "QBASIC /HELP")) return 1;
    if (!checkCmd("AMMOCODE /HELP", "Turbo Pascal", "AMMOCODE /HELP")) return 1;
    if (!checkCmd("FIELDC /HELP", "usage", "FIELDC /HELP")) return 1;
    if (!checkCmd("PADTEST /HELP", "Xbox360", "PADTEST /HELP")) return 1;
    if (!checkCmd("VER", "Field Compiler", "VER runtime")) return 1;
    if (!checkCmd("PORTS", "COM1", "PORTS")) return 1;
    if (!checkCmd("SCALE", "render", "SCALE")) return 1;

    bx = 0; cx = 0; dx = 0; eax = 0;
    FieldMscdex::handle(0x1506, 0, 0, bx, cx, dx, eax);
    if (bx != 0x0201u) {
        std::fprintf(stderr, "FAIL MSCDEX version bx=%04X\n", bx);
        return 1;
    }
    std::printf("OK MSCDEX 2.1 API\n");

    std::printf("RTX-AMMOS GPU-only QA passed\n");
    return 0;
}