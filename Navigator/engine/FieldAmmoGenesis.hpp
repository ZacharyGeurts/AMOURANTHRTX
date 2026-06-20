#pragma once

#include "AmmoGenesis/FieldGenesisCore.hpp"
#include "FieldAmmoEmuCommon.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldMix.hpp"
#include "FieldVga.hpp"
#include "FieldWinApp.hpp"

#include <vector>

namespace FieldAmmoGenesisConfig {

inline const char* kCfgPath = "C:\\GENESIS\\AMMOGEN.CFG";
inline const char* kRomDir = "C:\\GENESIS\\";

struct Options : FieldAmmoEmu::BaseOptions {
    bool fmBoost = true;
    bool psgOn = true;
};

inline Options g;

inline void setDefaults(Options& o = g) noexcept {
    o = Options{};
    o.lastRom = kRomDir;
}

inline void load(Options& o = g) noexcept {
    FieldAmmoEmu::loadConfig(kCfgPath, o, [](FieldAmmoEmu::BaseOptions& base, const char* key, const char* val) {
        auto& go = static_cast<Options&>(base);
        if (std::strcmp(key, "fm_boost") == 0) go.fmBoost = std::atoi(val) != 0;
        else if (std::strcmp(key, "psg_on") == 0) go.psgOn = std::atoi(val) != 0;
    });
}

inline void save(const Options& o = g) noexcept {
    char buf[4096];
    const int n = std::snprintf(buf, sizeof buf,
        "# AmmoGenesis config v1\n"
        "sound_on=%d\n"
        "audio_style=%d\n"
        "master_volume=%d\n"
        "last_rom=%s\n"
        "fm_boost=%d\n"
        "psg_on=%d\n",
        o.soundOn ? 1 : 0,
        static_cast<int>(o.audioStyle),
        o.masterVolume,
        o.lastRom.c_str(),
        o.fmBoost ? 1 : 0,
        o.psgOn ? 1 : 0);
    FieldAmmoVfs::writePath(kCfgPath,
        reinterpret_cast<const std::uint8_t*>(buf), static_cast<std::size_t>(n));
}

} // namespace FieldAmmoGenesisConfig

namespace FieldAmmoGenesisSetup {

inline bool active = false;
inline bool overlay = false;
inline int page = 0;
inline int sel = 0;

inline const char* kPages[] = { "Audio", "System", "Help" };
inline constexpr int kPageCount = 3;

inline void paintPage(FieldRtxWidgets::Ui& ui, int pg, int s, FieldAmmoEmu::BaseOptions& o, int& y) noexcept {
    auto& go = static_cast<FieldAmmoGenesisConfig::Options&>(o);
    if (pg == 0) {
        char v[64];
        auto row = [&](const char* label, const char* val, int idx) {
            const bool hi = s == idx;
            char line[96];
            std::snprintf(line, sizeof line, "%s%s", hi ? "> " : "  ", label);
            ui.label(32, y, 360, y + 32, line, hi ? 1 : 0);
            ui.label(380, y, 960, y + 32, val, hi ? 1 : 0);
            y += 40;
        };
        row("Sound", o.soundOn ? "On" : "Muted", 0);
        row("Audio style", FieldAmmoEmu::audioStyleLabel(o.audioStyle), 1);
        std::snprintf(v, sizeof v, "%d", o.masterVolume);
        row("Master volume", v, 2);
        row("FM boost", go.fmBoost ? "On" : "Off", 3);
    } else if (pg == 1) {
        FieldAmmoEmu::paintSystemRows(ui, s, o, y);
        char line[96];
        std::snprintf(line, sizeof line, "%sPSG (SN76489)", s == 1 ? "> " : "  ");
        ui.label(32, y, 360, y + 32, line, s == 1 ? 1 : 0);
        ui.label(380, y, 960, y + 32, go.psgOn ? "On" : "Off", s == 1 ? 1 : 0);
        y += 40;
    } else {
        const char* lines[] = {
            "GENESIS HELP  SETUP  [rom.md|.bin|.gen]",
            "YM2612 + SN76489 — Classic vs Modern on both chips",
            "F2 save  F3 defaults  Tab next page",
        };
        FieldAmmoEmu::paintHelpRows(ui, lines, 3, y);
    }
}

inline void toggle(int pg, int s, FieldAmmoEmu::BaseOptions& o) noexcept {
    auto& go = static_cast<FieldAmmoGenesisConfig::Options&>(o);
    if (pg == 0) {
        if (s <= 2) FieldAmmoEmu::toggleAudio(s, o);
        else if (s == 3) go.fmBoost = !go.fmBoost;
    } else if (pg == 1 && s == 1) go.psgOn = !go.psgOn;
}

inline int maxSel(int pg) noexcept {
    if (pg == 0) return 3;
    if (pg == 1) return 1;
    return 0;
}

inline FieldAmmoEmu::SetupCtx ctx() noexcept {
    return {
        "AmmoGenesis Setup", 15u, FieldAosAppIdentity::AppId::GenesisSetup,
        active, page, sel, kPageCount, kPages,
        FieldAmmoGenesisConfig::g,
        []() { FieldAmmoGenesisConfig::load(); },
        []() { FieldAmmoGenesisConfig::save(); },
        []() { FieldAmmoGenesisConfig::setDefaults(); },
        maxSel, paintPage, toggle
    };
}

inline void open(bool asOverlay = false) noexcept {
    active = true;
    overlay = asOverlay;
    page = sel = 0;
    FieldAmmoGenesisConfig::load();
}
inline void paint(std::uint8_t* ram) noexcept { FieldAmmoEmu::paintSetup(ctx(), ram); }
inline void pump(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept {
    auto c = ctx();
    FieldAmmoEmu::pumpSetup(c, ram, key, keyDown, overlay);
}

} // namespace FieldAmmoGenesisSetup

namespace FieldGenesis {

constexpr int FB_W = 320;
constexpr int FB_H = 224;

inline AmmoGenesis::State chip{};
inline bool active = false;
inline bool optionsOpen = false;
inline bool graphicsMode = false;
inline bool paused = false;
inline float audioLevel = 0.f;
inline std::string romPath;
inline FieldAmmoGenesisConfig::Options cfg;
inline std::uint8_t fb[FB_W * FB_H]{};
inline bool vgaMode13 = false;

using Audio = FieldAmmoEmu::AudioStream<16, active, audioLevel>;

inline void applyConfig(const FieldAmmoGenesisConfig::Options& o) noexcept {
    cfg = o;
    AmmoGenesis::applyAudioConfig(chip, o.audioStyle, Audio::RATE);
}

inline bool loadRom(const char* path) {
    std::vector<std::uint8_t> data;
    if (!FieldAmmoFat::readFile(path, data) || data.size() < 512) return false;
    if (!AmmoGenesis::loadRom(chip, data.data(), data.size())) return false;
    applyConfig(cfg);
    romPath = path;
    vgaMode13 = false;
    return true;
}

inline bool findAnyRom(std::string& out) {
    if (FieldAmmoEmu::findRomInVfsDir("C:\\GENESIS", ".md", out)) return true;
    if (FieldAmmoEmu::findRomInVfsDir("C:\\GENESIS", ".bin", out)) return true;
    return FieldAmmoEmu::findRomInVfsDir("C:\\GENESIS", ".gen", out);
}

inline void renderFrame(std::uint8_t* guestRam) noexcept {
    if (!guestRam) return;
    AmmoGenesis::renderFrame(chip, fb, FB_W, FB_H);
    if (!vgaMode13) {
        FieldVga::setMode(FieldVga::MODE_VGA_13, guestRam);
        vgaMode13 = true;
    }
    for (int y = 0; y < FB_H; ++y)
        for (int x = 0; x < FB_W; ++x)
            guestRam[FieldVga::VGA_FB + static_cast<std::size_t>(y * 320 + x)] =
                fb[y * FB_W + x];
}

inline void pushFrameAudio() noexcept {
    if (!cfg.soundOn) return;
    constexpr int kN = Audio::RATE / 60;
    float block[kN]{};
    const int n = AmmoGenesis::renderAudioFrame(chip, block, kN);
    const float gain = static_cast<float>(cfg.masterVolume) / 512.f;
    for (int i = 0; i < n; ++i) block[i] *= gain;
    Audio::pushSamples(block, static_cast<std::uint32_t>(n));
}

inline void runFrame(std::uint8_t* guestRam) noexcept {
    if (paused) return;
    AmmoGenesis::runFrameCpu(chip);
    pushFrameAudio();
    if (graphicsMode && guestRam) renderFrame(guestRam);
}

inline void ensureAudio(void* mix) noexcept {
    if (!mix || !cfg.soundOn) return;
    Audio::mixer = static_cast<MIX_Mixer*>(mix);
    if (!Audio::ready) Audio::init(Audio::mixer);
}

inline void close(std::uint8_t* ram) noexcept;

inline void beginChrome(std::uint8_t* ram) noexcept {
    FieldWinApp::beginEmu(ram, " AmmoGenesis ",
        " File>Open ROM  Options>Settings  Esc quit  P pause ");
}

inline bool handleMenu(int action, std::uint8_t* ram) noexcept {
    switch (action) {
    case FieldWinFrame::A_FILE_OPEN:
        FieldAmmoEmu::openRomDialog(FieldAmmoEmu::Kind::Genesis, cfg.lastRom.c_str());
        FieldEmuFileDialog::paint(ram);
        paused = true;
        return true;
    case FieldWinFrame::A_OPT_SETTINGS:
        FieldAmmoGenesisSetup::open(true);
        optionsOpen = true;
        paused = true;
        FieldAmmoGenesisSetup::paint(ram);
        return true;
    case FieldWinFrame::A_FILE_EXIT:
        close(ram);
        return true;
    default:
        return false;
    }
}

inline bool pumpOverlays(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept {
    if (FieldEmuFileDialog::active) {
        if (FieldAmmoEmu::pumpFileDialog(ram, key, keyDown)) {
            if (!FieldEmuFileDialog::active && !FieldEmuFileDialog::result.empty()) {
                if (loadRom(FieldEmuFileDialog::result.c_str())) {
                    cfg.lastRom = FieldEmuFileDialog::result;
                    FieldAmmoGenesisConfig::save(cfg);
                    graphicsMode = true;
                    for (int f = 0; f < 4; ++f) runFrame(ram);
                    ensureAudio(Audio::mixer);
                }
                paused = false;
            } else if (!FieldEmuFileDialog::active)
                paused = false;
            return true;
        }
    }
    if (optionsOpen && FieldAmmoGenesisSetup::active) {
        FieldAmmoGenesisSetup::pump(ram, key, keyDown);
        if (!FieldAmmoGenesisSetup::active) {
            optionsOpen = false;
            paused = false;
            cfg = FieldAmmoGenesisConfig::g;
            applyConfig(cfg);
            if (graphicsMode) runFrame(ram);
        }
        return true;
    }
    return false;
}

inline void open(std::uint8_t* ram, const char* path = nullptr,
                 bool openOptions = false) noexcept {
    FieldWinApp::reset();
    beginChrome(ram);
    FieldAmmoGenesisConfig::load(cfg);
    if (path && path[0]) loadRom(path);
    else {
        std::string any;
        if (findAnyRom(any)) loadRom(any.c_str());
    }
    active = true;
    graphicsMode = !chip.cart.rom.empty();
    paused = false;
    if (graphicsMode) {
        for (int f = 0; f < 4; ++f) runFrame(ram);
        ensureAudio(Audio::mixer);
    }
    if (openOptions) {
        FieldAmmoGenesisSetup::open(true);
        optionsOpen = true;
        paused = true;
        FieldAmmoGenesisSetup::paint(ram);
    }
}

inline void close(std::uint8_t* ram) noexcept {
    active = false;
    optionsOpen = false;
    FieldAmmoGenesisSetup::active = false;
    FieldEmuFileDialog::close();
    graphicsMode = false;
    Audio::shutdown();
    FieldWinApp::reset();
    FieldVga::setMode(3u, ram);
}

inline void pump(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept {
    if (!active) return;
    if (pumpOverlays(ram, key, keyDown)) return;
    int menuAction = 0;
    if (keyDown && FieldWinApp::pumpMenuKey(key, menuAction)) {
        if (menuAction) handleMenu(menuAction, ram);
        return;
    }
    if (keyDown && key == 0x011Bu) { close(ram); return; }
    if (keyDown && ((key & 0xFFu) == 'p' || (key & 0xFFu) == 'P')) paused = !paused;
    if (graphicsMode && !paused) runFrame(ram);
}

inline void tick(std::uint8_t* ram, const bool*) noexcept {
    if (!active || !graphicsMode || paused || optionsOpen || FieldEmuFileDialog::active)
        return;
    runFrame(ram);
}

} // namespace FieldGenesis