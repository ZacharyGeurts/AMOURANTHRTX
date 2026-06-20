#pragma once

#include "AmmoA2600/FieldA2600Core.hpp"
#include "FieldAmmoEmuCommon.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldMix.hpp"
#include "FieldVga.hpp"
#include "FieldWinApp.hpp"

#include <vector>

namespace FieldAmmoA2600Config {

inline const char* kCfgPath = "C:\\A2600\\AMMOA26.CFG";
inline const char* kRomDir = "C:\\A2600\\";

struct Options : FieldAmmoEmu::BaseOptions {};

inline Options g;

inline void setDefaults(Options& o = g) noexcept {
    o = Options{};
    o.lastRom = kRomDir;
}

inline void load(Options& o = g) noexcept { FieldAmmoEmu::loadConfig(kCfgPath, o); }
inline void save(const Options& o = g) noexcept {
    FieldAmmoEmu::saveConfig(kCfgPath, o, "# AmmoA2600 config v1");
}

} // namespace FieldAmmoA2600Config

namespace FieldAmmoA2600Setup {

inline bool active = false;
inline bool overlay = false;
inline int page = 0;
inline int sel = 0;

inline const char* kPages[] = { "Audio", "System", "Help" };
inline constexpr int kPageCount = 3;

inline void paintPage(FieldRtxWidgets::Ui& ui, int pg, int s, FieldAmmoEmu::BaseOptions& o, int& y) noexcept {
    if (pg == 0) FieldAmmoEmu::paintAudioRows(ui, s, o, y);
    else if (pg == 1) FieldAmmoEmu::paintSystemRows(ui, s, o, y);
    else {
        const char* lines[] = {
            "A2600 HELP  SETUP  [rom.bin|.a26]",
            "TIA audio — Classic harsh tones vs Modern bandlimited",
            "F2 save  F3 defaults  Tab next page",
        };
        FieldAmmoEmu::paintHelpRows(ui, lines, 3, y);
    }
}

inline void toggle(int pg, int s, FieldAmmoEmu::BaseOptions& o) noexcept {
    if (pg == 0) FieldAmmoEmu::toggleAudio(s, o);
}

inline int maxSel(int pg) noexcept {
    if (pg == 0) return FieldAmmoEmu::maxSelAudio(pg);
    if (pg == 1) return FieldAmmoEmu::maxSelSystem(pg);
    return FieldAmmoEmu::maxSelHelp(pg);
}

inline FieldAmmoEmu::SetupCtx ctx() noexcept {
    return {
        "AmmoA2600 Setup", 13u, FieldAosAppIdentity::AppId::A2600Setup,
        active, page, sel, kPageCount, kPages,
        FieldAmmoA2600Config::g,
        []() { FieldAmmoA2600Config::load(); },
        []() { FieldAmmoA2600Config::save(); },
        []() { FieldAmmoA2600Config::setDefaults(); },
        maxSel, paintPage, toggle
    };
}

inline void open(bool asOverlay = false) noexcept {
    active = true;
    overlay = asOverlay;
    page = sel = 0;
    FieldAmmoA2600Config::load();
}
inline void paint(std::uint8_t* ram) noexcept { FieldAmmoEmu::paintSetup(ctx(), ram); }
inline void pump(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept {
    auto c = ctx();
    FieldAmmoEmu::pumpSetup(c, ram, key, keyDown, overlay);
}

} // namespace FieldAmmoA2600Setup

namespace FieldA2600 {

constexpr int FB_W = 160;
constexpr int FB_H = 192;

inline AmmoA2600::State chip{};
inline bool active = false;
inline bool optionsOpen = false;
inline bool graphicsMode = false;
inline bool paused = false;
inline float audioLevel = 0.f;
inline std::string romPath;
inline FieldAmmoA2600Config::Options cfg;
inline std::uint8_t fb[FB_W * FB_H]{};
inline bool vgaMode13 = false;

using Audio = FieldAmmoEmu::AudioStream<14, active, audioLevel>;

inline void applyConfig(const FieldAmmoA2600Config::Options& o) noexcept {
    cfg = o;
    AmmoA2600::applyAudioConfig(chip, o.audioStyle, Audio::RATE);
}

inline bool loadRom(const char* path) {
    std::vector<std::uint8_t> data;
    if (!FieldAmmoFat::readFile(path, data) || data.size() < 256) return false;
    if (!AmmoA2600::loadRom(chip, data.data(), data.size())) return false;
    applyConfig(cfg);
    romPath = path;
    vgaMode13 = false;
    return true;
}

inline bool findAnyRom(std::string& out) {
    if (FieldAmmoEmu::findRomInVfsDir("C:\\A2600", ".bin", out)) return true;
    return FieldAmmoEmu::findRomInVfsDir("C:\\A2600", ".a26", out);
}

inline void renderFrame(std::uint8_t* guestRam) noexcept {
    if (!guestRam) return;
    AmmoA2600::renderFrame(chip, fb, FB_W, FB_H);
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
    const int n = AmmoA2600::renderAudioFrame(chip, block, kN);
    const float gain = static_cast<float>(cfg.masterVolume) / 512.f;
    for (int i = 0; i < n; ++i) block[i] *= gain;
    Audio::pushSamples(block, static_cast<std::uint32_t>(n));
}

inline void runFrame(std::uint8_t* guestRam) noexcept {
    if (paused) return;
    AmmoA2600::runFrameCpu(chip);
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
    FieldWinApp::beginEmu(ram, " AmmoA2600 ",
        " File>Open ROM  Options>Settings  Esc quit  P pause ");
}

inline bool handleMenu(int action, std::uint8_t* ram) noexcept {
    switch (action) {
    case FieldWinFrame::A_FILE_OPEN:
        FieldAmmoEmu::openRomDialog(FieldAmmoEmu::Kind::A2600, cfg.lastRom.c_str());
        FieldEmuFileDialog::paint(ram);
        paused = true;
        return true;
    case FieldWinFrame::A_OPT_SETTINGS:
        FieldAmmoA2600Setup::open(true);
        optionsOpen = true;
        paused = true;
        FieldAmmoA2600Setup::paint(ram);
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
                    FieldAmmoA2600Config::save(cfg);
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
    if (optionsOpen && FieldAmmoA2600Setup::active) {
        FieldAmmoA2600Setup::pump(ram, key, keyDown);
        if (!FieldAmmoA2600Setup::active) {
            optionsOpen = false;
            paused = false;
            cfg = FieldAmmoA2600Config::g;
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
    FieldAmmoA2600Config::load(cfg);
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
        FieldAmmoA2600Setup::open(true);
        optionsOpen = true;
        paused = true;
        FieldAmmoA2600Setup::paint(ram);
    }
}

inline void close(std::uint8_t* ram) noexcept {
    active = false;
    optionsOpen = false;
    FieldAmmoA2600Setup::active = false;
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

} // namespace FieldA2600