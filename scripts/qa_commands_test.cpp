// QA: verify shell commands are real (not DOS stub banners).
#include "FieldAmmoFat.hpp"
#include "FieldAmmoTools.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxBoot.hpp"
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

static bool runCmd(const char* line, const char* needle) {
    std::vector<std::uint8_t> buf(FieldPlatform::GUEST_RAM_BYTES, 0);
    std::uint8_t* ram = buf.data();
    FieldRtxBoot::paintWelcome(ram);
    FieldRtxShell::execLine(line, ram, FieldRtxShell::echoChar,
        FieldRtxShell::defaultNewline, FieldRtxShell::defaultPrompt);
    if (!screenHas(ram, needle)) {
        std::fprintf(stderr, "FAIL cmd [%s] missing [%s]\n", line, needle);
        return false;
    }
    std::printf("OK %s\n", line);
    return true;
}

int main() {
    if (!FieldDos::loadHdImage(FieldDos::defaultHdPath("."))) {
        std::fprintf(stderr, "FAIL hd load\n");
        return 1;
    }
    if (!FieldAmmoFat::mount()) {
        std::fprintf(stderr, "FAIL mount\n");
        return 1;
    }
    if (!runCmd("PORTS", "COM1")) return 1;
    if (!runCmd("DEVICES", "usb_host")) return 1;
    if (!runCmd("FIELDC C:\\SAMPLES\\PADTEST.FLD", "Field Compiler")) return 1;
    if (!runCmd("TOOLS", "FIELDC")) return 1;
    if (!runCmd("SCALE", "render")) return 1;
    auto tr = FieldAmmoTools::fieldcFile({"FIELDC", "C:\\SAMPLES\\PADTEST.FLD"});
    if (!tr.ok) {
        std::fprintf(stderr, "FAIL FIELDC compile: %s\n", tr.message.c_str());
        return 1;
    }
    std::printf("OK FIELDC object %s\n", tr.message.c_str());
    std::printf("RTX command QA passed\n");
    return 0;
}