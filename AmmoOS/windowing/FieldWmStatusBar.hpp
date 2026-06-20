#pragma once

// Window manager footer — app status text for GPU gray status bar.

#include "FieldAmouranthHudRam.hpp"
#include "FieldAmmoTextFonts.hpp"
#include "FieldAmmoText.hpp"
#include "FieldAmouranthFileCmd.hpp"
#include "FieldAmouranthOs.hpp"
#include "FieldDrives.hpp"
#include "FieldRtxTerm.hpp"
#include "FieldX86Emu.hpp"

#include <cstdint>
#include <cstring>
#include <string>

namespace FieldNes {
extern bool active;
extern bool paused;
extern bool romLoaded;
extern std::string romPath;
}

namespace FieldWmStatusBar {

constexpr std::uint32_t FOOTER_RAM     = FieldAmouranthHudRam::FOOTER_RAM;
constexpr int           FOOTER_LEN     = 128;

enum class Mode : std::uint8_t {
    System   = 0,
    Editor   = 1,
    Terminal = 2,
    FileCmd  = 3,
};

inline void writeStr(std::uint8_t* ram, const char* s) noexcept {
    if (!ram) return;
    for (int i = 0; i < FOOTER_LEN; ++i) {
        const char ch = (s && s[i]) ? s[i] : ' ';
        ram[FOOTER_RAM + 1u + static_cast<std::uint32_t>(i)] = static_cast<std::uint8_t>(ch);
    }
}

inline void packEditor(std::uint8_t* ram) noexcept {
    if (!ram || !FieldAmmoText::active) return;
    int words = 0;
    int chars = 0;
    bool inWord = false;
    for (const auto& ln : FieldAmmoText::lines) {
        chars += static_cast<int>(ln.size());
        for (char c : ln) {
            if (c == ' ' || c == '\t') inWord = false;
            else if (!inWord) { ++words; inWord = true; }
        }
    }
    chars += static_cast<int>(FieldAmmoText::lines.size()) > 1
        ? static_cast<int>(FieldAmmoText::lines.size()) - 1 : 0;

    const char* base = FieldAmmoText::path.empty() ? "Untitled" : FieldAmmoText::path.c_str();
    const char* slash = std::strrchr(base, '\\');
    const char* name = slash ? slash + 1 : base;

    char shortName[25]{};
    char fontName[21]{};
    std::snprintf(shortName, sizeof shortName, "%s", name);
    std::snprintf(fontName, sizeof fontName, "%s", FieldAmmoTextFonts::activeName());
    char buf[FOOTER_LEN + 1]{};
    std::snprintf(buf, sizeof buf,
        " Ln %d, Col %d  |  %d lines  |  %d words  |  %d chars  |  %s%s  |  %s ",
        FieldAmmoText::curRow + 1, FieldAmmoText::curCol + 1,
        static_cast<int>(FieldAmmoText::lines.size()), words, chars,
        shortName, FieldAmmoText::dirty ? "*" : "",
        fontName);
    ram[FOOTER_RAM] = static_cast<std::uint8_t>(Mode::Editor);
    writeStr(ram, buf);
}

inline void packTerminal(std::uint8_t* ram) noexcept {
    if (!ram) return;
    char buf[FOOTER_LEN + 1]{};
    const int total = FieldRtxTerm::totalLines();
    const char letter = FieldDrives::currentLetter ? FieldDrives::currentLetter : 'C';
    std::snprintf(buf, sizeof buf,
        " %c:\\  Ln %d  %d lines  F1 Help ",
        letter,
        (FieldX86Emu::ramHost ? FieldX86Emu::ramHost[0x451u] : 0u) + 1,
        total);
    ram[FOOTER_RAM] = static_cast<std::uint8_t>(Mode::Terminal);
    writeStr(ram, buf);
}

inline void packSystem(std::uint8_t* ram) noexcept {
    if (!ram) return;
    ram[FOOTER_RAM] = static_cast<std::uint8_t>(Mode::System);
    writeStr(ram, " RTX-AMMOS  |  Ready ");
}

inline void packFileCmd(std::uint8_t* ram) noexcept {
    if (!ram || !FieldAmouranthFileCmd::active) return;
    char buf[FOOTER_LEN + 1]{};
    FieldAmouranthFileCmd::formatFooter(buf, sizeof buf);
    ram[FOOTER_RAM] = static_cast<std::uint8_t>(Mode::FileCmd);
    writeStr(ram, buf);
}

inline void packNes(std::uint8_t* ram) noexcept {
    if (!ram || !FieldNes::active) return;
    char buf[FOOTER_LEN + 1]{};
    if (!FieldNes::romLoaded) {
        std::snprintf(buf, sizeof buf,
            " AmmoNES — File>Open or Recent — Emulation>Pause/Turbo ");
    } else {
        const char* base = FieldNes::romPath.c_str();
        const char* slash = std::strrchr(base, '\\');
        const char* name = slash ? slash + 1 : base;
        std::snprintf(buf, sizeof buf, " %s%s  |  %s  |  File>Open  Recent  Emulation ",
            name, FieldNes::paused ? " [Paused]" : "", name);
    }
    ram[FOOTER_RAM] = static_cast<std::uint8_t>(Mode::System);
    writeStr(ram, buf);
}

inline void packFooter(std::uint8_t* ram) noexcept {
    if (!ram) return;
    if (FieldNes::active)
        packNes(ram);
    else if (FieldAmmoText::active)
        packEditor(ram);
    else if (FieldAmouranthFileCmd::active)
        packFileCmd(ram);
    else if (FieldAmouranthOs::consoleShell)
        packTerminal(ram);
    else
        packSystem(ram);
}

} // namespace FieldWmStatusBar