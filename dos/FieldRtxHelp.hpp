#pragma once

// Golden Era Man+Machine — shared help for host binaries and RTX-DOS shell.

#include <cstdio>
#include <cstring>

namespace FieldRtxHelp {

constexpr const char* GOLDEN_ERA   = "Golden Era Man+Machine";
constexpr const char* TAGLINE      = "Human intent + silicon — MS-DOS luxuries forever";
constexpr const char* AUTHOR       = "Zachary Geurts MCSE+I";
constexpr const char* PRODUCT      = "AMOURANTHRTX RTX-DOS 7.0 Field Die";

inline bool isHelpArg(const char* arg) noexcept {
    if (!arg || !*arg) return false;
    return std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0
        || std::strcmp(arg, "/h") == 0 || std::strcmp(arg, "/help") == 0
        || std::strcmp(arg, "/?") == 0 || std::strcmp(arg, "-help") == 0
        || std::strcmp(arg, "help") == 0;
}

inline bool argcWantsHelp(int argc, char** argv) noexcept {
    for (int i = 1; i < argc; ++i)
        if (isHelpArg(argv[i])) return true;
    return false;
}

inline void printBinaryHelp(FILE* out = stdout) noexcept {
    std::fprintf(out,
        "\n"
        "  ╔══════════════════════════════════════════════════════════════╗\n"
        "  ║         %s — FOREVER PLACE          ║\n"
        "  ╚══════════════════════════════════════════════════════════════╝\n"
        "\n"
        "  %s\n"
        "  %s — %s\n"
        "\n"
        "  USAGE\n"
        "    AMOURANTHRTX              Launch RTX-DOS 7.0 GPU Field Die\n"
        "    AMOURANTHRTX -h           This help (/h --help /help)\n"
        "    AMOURANTHRTX --extended-field\n"
        "                              Non-point wave dispatch (+35-60%% throughput)\n"
        "\n"
        "  ENVIRONMENT\n"
        "    AMOURANTHRTX_EXTENDED_FIELD=1  Same as --extended-field\n"
        "    AMOURANTHRTX_HEADLESS=1   Offscreen window, dummy audio\n"
        "    AMOURANTHRTX_MAX_FRAMES=N Bounded headless run (N>0)\n"
        "    AMOURANTHRTX_DEBUG=1      Same as headless hints\n"
        "\n"
        "  RTX-DOS SHELL (inside guest)\n"
        "    HELP                      COMMAND.COM reference\n"
        "    EDIT file                 MS-DOS EDIT-style GUI (F1 F2 F10)\n"
        "    QBASIC                    Interactive BASIC + F1 help menu\n"
        "    QBASIC /HELP              Tutorial, commands, defines, examples\n"
        "    TYPE GOLDEN.TXT           Golden Era manifesto on C:\n"
        "\n"
        "  LAUNCHER\n"
        "    ./linux.sh -h             Build/run help for all realms\n"
        "    ./linux.sh dos            Install RTX-DOS images\n"
        "    ./linux.sh qa             Full DOS QA suite\n"
        "\n"
        "  — Man writes intent. Machine executes forever. —\n"
        "\n",
        GOLDEN_ERA, TAGLINE, PRODUCT, AUTHOR);
}

inline void printDosGoldenIntro() noexcept {
    std::printf(
        "\r\n %s\r\n"
        " %s\r\n"
        " TYPE GOLDEN.TXT | EDIT file | QBASIC /HELP\r\n",
        GOLDEN_ERA, TAGLINE);
}

} // namespace FieldRtxHelp