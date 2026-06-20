// QA: RTX-AMMOS GPU-only shell — no host CPU / libx86emu required.
#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldMscdex.hpp"
#include "FieldRtxMemory.hpp"
#include "FieldAmmoToolchain.hpp"
#include "FieldAmmoExec.hpp"
#include "FieldRegistry.hpp"
#include "FieldRtxHelp.hpp"
#include "FieldRtxShell.hpp"  // pulls FieldAmouranthOs/SDL before FieldRtxBoot
#include "FieldRtxThemes.hpp"
#include "FieldRtxVfs.hpp"
#include "FieldDrives.hpp"
#include "FieldRtxVgaText.hpp"
#include "FieldRtxTerm.hpp"
#include "FieldRtxConsoleGui.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static bool screenHas(const std::uint8_t* ram, const char* needle) {
    char buf[200 * 60 + 1]{};
    int n = 0;
    const int cols = FieldRtxVgaText::cols();
    const int rows = FieldRtxVgaText::rows();
    for (int row = 0; row < rows && n < static_cast<int>(sizeof buf) - 1; ++row) {
        for (int col = 0; col < cols && n < static_cast<int>(sizeof buf) - 1; ++col) {
            const std::uint32_t off = FieldRtxVgaText::cellOffset(row, col);
            if (off + 1u >= FieldPlatform::GUEST_RAM_BYTES) continue;
            buf[n++] = static_cast<char>(ram[off]);
        }
    }
    buf[n] = '\0';
    return std::strstr(buf, needle) != nullptr;
}

static bool screenHasTermClient(const std::uint8_t* ram, const char* needle) {
    const int cols = FieldRtxTerm::screenCols();
    const int rows = FieldRtxTerm::screenRows();
    std::string flat;
    flat.reserve(static_cast<std::size_t>(cols * rows));
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const std::uint32_t off = FieldRtxVgaText::cellOffset(
                FieldRtxTerm::clientR0 + row, FieldRtxTerm::clientC0 + col);
            if (off + 1u >= FieldPlatform::GUEST_RAM_BYTES) continue;
            flat += static_cast<char>(ram[off]);
        }
    }
    return flat.find(needle) != std::string::npos;
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

    if (FieldCdRom::autoMountHost()) {
        std::printf("OK host optical mounted %s label=%s sectors=%u\n",
            FieldCdRom::isoPath.c_str(), FieldCdRom::volumeLabel.c_str(),
            FieldCdRom::sectorCount());
    }
    FieldRtxMemory::growMscdexExtender();
    FieldMscdex::install();
    std::uint16_t bx = 0, cx = 0, dx = 0;
    std::uint32_t eax = 0;
    FieldMscdex::handle(0x1500, 0, 0, bx, cx, dx, eax);
    if (FieldCdRom::ready && cx == 0) {
        std::fprintf(stderr, "FAIL MSCDEX drive count with mounted media\n");
        return 1;
    }
    std::printf("OK MSCDEX installed drives=%u\n", cx);

    FieldRtxVfs::vfsInit();
    FieldRegistry::init();
    FieldRtxThemes::init();
    FieldDrives::refresh(true);
    FieldRtxVgaText::initMode(ram, 80, 25);

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
    if (!screenHas(ram, "RTX-DOS") && !screenHas(ram, "Golden Era")) {
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

    if (!checkCmd("VER", "RTX COMMAND", "VER")) return 1;
    if (!checkCmd("VER", "Field Die runtime", "VER runtime")) return 1;
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
    if (!checkCmd("DIR", "COMMAND", "DIR")) return 1;
    if (!checkCmd("DIR", "file(s)", "DIR summary")) return 1;
    if (!checkCmd("HELP", "LAUNCHER", "HELP")) return 1;
    if (!checkCmd("AMMOCODE /HELP", "Turbo Pascal", "AMMOCODE /HELP")) return 1;
    if (!checkCmd("QBASIC /HELP", "QBASIC", "QBASIC /HELP")) return 1;
    if (!checkCmd("FIELDC /HELP", "FIELDC", "FIELDC /HELP")) return 1;
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

    FieldRtxShell::paintTerminalShell(ram);
    if (!screenHasTermClient(ram, "Golden Era command shell")
            || !screenHasTermClient(ram, "LAUNCHER for program")) {
        std::fprintf(stderr, "FAIL terminal welcome missing seed text\n");
        return 1;
    }
    std::printf("OK terminal welcome seed\n");

    std::printf("RTX-AMMOS GPU-only QA passed\n");
    return 0;
}