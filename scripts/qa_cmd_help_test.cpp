// QA: RTX-DOS 7.0 per-command /? help dispatch.
#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxCmdHelp.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRtxShell.hpp"  // echoChar only

#include <cstdio>
#include <cstring>
#include <vector>

static bool screenFlatHas(const std::uint8_t* ram, const char* needle) {
    char flat[80 * 25 + 1]{};
    int bi = 0;
    for (int i = 0; i < 80 * 25 && bi < 80 * 25; ++i) {
        const char c = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>(i * 2)]);
        if (c) flat[bi++] = c;
    }
    return std::strstr(flat, needle) != nullptr;
}

int main() {
    std::vector<std::uint8_t> buf(FieldPlatform::GUEST_RAM_BYTES, 0);
    std::uint8_t* ram = buf.data();
    FieldRtxGui::initTextMode(ram);
    FieldRtxShell::defaultPrompt(ram);

    if (!FieldRtxCmdHelp::print("AMMOASM", ram, FieldRtxShell::echoChar, FieldRtxShell::defaultNewline)
        || !screenFlatHas(ram, "MASM subset")) {
        std::fprintf(stderr, "FAIL AMMOASM help\n");
        return 1;
    }
    std::memset(ram + 0xB8000u, 0, 80u * 25u * 2u);
    FieldRtxGui::initTextMode(ram);

    if (!FieldRtxCmdHelp::print("AMMOCC", ram, FieldRtxShell::echoChar, FieldRtxShell::defaultNewline)
        || !screenFlatHas(ram, "tiny C")) {
        std::fprintf(stderr, "FAIL AMMOCC help\n");
        return 1;
    }
    std::memset(ram + 0xB8000u, 0, 80u * 25u * 2u);
    FieldRtxGui::initTextMode(ram);

    if (!FieldRtxCmdHelp::print("DIR", ram, FieldRtxShell::echoChar, FieldRtxShell::defaultNewline)
        || !screenFlatHas(ram, "/W")) {
        std::fprintf(stderr, "FAIL DIR help\n");
        return 1;
    }
    std::printf("OK RTX-DOS 7.0 /? help QA passed\n");
    return 0;
}