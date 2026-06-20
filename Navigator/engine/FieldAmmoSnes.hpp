#pragma once

#include "AmmoSnes/FieldSnesCore.hpp"
#include "FieldAmmoEmuCommon.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldMix.hpp"
#include "FieldVga.hpp"
#include "FieldWinApp.hpp"

#include <vector>

namespace FieldAmmoSnesConfig {

inline const char* kCfgPath = "C:\\SNES\\AMMOSNES.CFG";
inline const char* kRomDir = "C:\\SNES\\";

struct Options : FieldAmmoEmu::BaseOptions {
    bool thermoGovernor = true;
    int maxBurst = 4;
    bool superFxFastPath = true;
};

inline Options g;

inline void setDefaults(Options& o = g) noexcept {
    o = Options{};
    o.lastRom = kRomDir;
}

inline void load(Options& o = g) noexcept {
    FieldAmmoEmu::loadConfig(kCfgPath, o, [](FieldAmmoEmu::BaseOptions& base, const char* key, const char* val) {
        auto& so = static_cast<Options&>(base);
        if (std::strcmp(key, "thermo") == 0) so.thermoGovernor = std::atoi(val) != 0;
        else if (std::strcmp(key, "max_burst") == 0) so.maxBurst = std::clamp(std::atoi(val), 1, 8);
        else if (std::strcmp(key, "superfx_fast") == 0) so.superFxFastPath = std::atoi(val) != 0;
    });
}

inline void save(const Options& o = g) noexcept {
    char buf[4096];
    const int n = std::snprintf(buf, sizeof buf,
        "# AmmoSNES config v1\n"
        "sound_on=%d\n"
        "audio_style=%d\n"
        "master_volume=%d\n"
        "last_rom=%s\n"
        "thermo=%d\n"
        "max_burst=%d\n"
        "superfx_fast=%d\n",
        o.soundOn ? 1 : 0,
        static_cast<int>(o.audioStyle),
        o.masterVolume,
        o.lastRom.c_str(),
        o.thermoGovernor ? 1 : 0,
        o.maxBurst,
        o.superFxFastPath ? 1 : 0);
    FieldAmmoVfs::writePath(kCfgPath,
        reinterpret_cast<const std::uint8_t*>(buf), static_cast<std::size_t>(n));
}

} // namespace FieldAmmoSnesConfig

namespace FieldAmmoSnesSetup {

inline bool active = false;
inline bool overlay = false;
inline int page = 0;
inline int sel = 0;

inline const char* kPages[] = { "SuperFX", "Audio", "Help" };
inline constexpr int kPageCount = 3;

inline void paintPage(FieldRtxWidgets::Ui& ui, int pg, int s, FieldAmmoEmu::BaseOptions& o, int& y) noexcept {
    auto& so = static_cast<FieldAmmoSnesConfig::Options&>(o);
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
        row("Thermo governor", so.thermoGovernor ? "On" : "Off", 0);
        std::snprintf(v, sizeof v, "%d", so.maxBurst);
        row("GSU burst frames", v, 1);
        row("SuperFX fast plot/rpix", so.superFxFastPath ? "On" : "Off", 2);
    } else if (pg == 1) {
        FieldAmmoEmu::paintAudioRows(ui, s, o, y);
    } else {
        const char* lines[] = {
            "SNES HELP  SETUP  [.sfc|.smc from C:\\SNES\\]",
            "Super FX (GSU) thermo governor batches RISC steps",
            "F2 save  F3 defaults  Tab next page",
        };
        FieldAmmoEmu::paintHelpRows(ui, lines, 3, y);
    }
}

inline void toggle(int pg, int s, FieldAmmoEmu::BaseOptions& o) noexcept {
    auto& so = static_cast<FieldAmmoSnesConfig::Options&>(o);
    if (pg == 0) {
        if (s == 0) so.thermoGovernor = !so.thermoGovernor;
        else if (s == 2) so.superFxFastPath = !so.superFxFastPath;
    } else if (pg == 1) {
        FieldAmmoEmu::toggleAudio(s, o);
    }
}

inline int maxSel(int pg) noexcept {
    if (pg == 0) return 2;
    if (pg == 1) return 2;
    return 0;
}

inline FieldAmmoEmu::SetupCtx ctx() noexcept {
    return {
        "AmmoSNES Setup", 15u, FieldAosAppIdentity::AppId::SnesSetup,
        active, page, sel, kPageCount, kPages,
        FieldAmmoSnesConfig::g,
        []() { FieldAmmoSnesConfig::load(); },
        []() { FieldAmmoSnesConfig::save(); },
        []() { FieldAmmoSnesConfig::setDefaults(); },
        maxSel, paintPage, toggle
    };
}

inline void open(bool asOverlay = false) noexcept {
    active = true;
    overlay = asOverlay;
    page = sel = 0;
    FieldAmmoSnesConfig::load();
}
inline void paint(std::uint8_t* ram) noexcept {
    auto c = ctx();
    FieldAmmoEmu::paintSetup(c, ram);
}
inline void pump(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept {
    auto c = ctx();
    FieldAmmoEmu::pumpSetup(c, ram, key, keyDown, overlay);
}

} // namespace FieldAmmoSnesSetup

namespace FieldSnes {

constexpr int FB_W = 256;
constexpr int FB_H = 224;

inline AmmoSnes::State chip{};
inline bool active = false;
inline bool optionsOpen = false;
inline bool graphicsMode = false;
inline bool paused = false;
inline float audioLevel = 0.f;
inline std::string romPath;
inline FieldAmmoSnesConfig::Options cfg;
inline std::uint8_t fb[FB_W * FB_H]{};
inline bool vgaMode13 = false;
inline int thermoSteps = 1;

using Audio = FieldAmmoEmu::AudioStream<16, active, audioLevel>;

inline void applyConfig(const FieldAmmoSnesConfig::Options& o) noexcept {
    cfg = o;
    chip.thermoGovernor = o.thermoGovernor;
    chip.thermoSteps = thermoSteps;
    chip.gsu.maxBurst = o.maxBurst;
    if (o.superFxFastPath) {
        chip.gsu.stepBudget = 8192;
        chip.gsu.plotBudget = 16384;
    } else {
        chip.gsu.stepBudget = 4096;
        chip.gsu.plotBudget = 8192;
    }
    AmmoSnes::applyAudioConfig(chip, o.audioStyle, Audio::RATE);
}

inline bool loadRom(const char* path) {
    std::vector<std::uint8_t> data;
    if (!FieldAmmoFat::readFile(path, data) || data.size() < 512) return false;
    if (!AmmoSnes::loadRom(chip, data.data(), data.size())) return false;
    applyConfig(cfg);
    romPath = path;
    vgaMode13 = false;
    return true;
}

inline bool findAnyRom(std::string& out) {
    if (FieldAmmoEmu::findRomInVfsDir("C:\\SNES", ".sfc", out)) return true;
    if (FieldAmmoEmu::findRomInVfsDir("C:\\SNES", ".smc", out)) return true;
    return FieldAmmoEmu::findRomInVfsDir("C:\\SNES", ".SFC", out);
}

inline void renderFrame(std::uint8_t* guestRam) noexcept {
    if (!guestRam) return;
    AmmoSnes::renderFrame(chip, fb, FB_W, FB_H);
    if (!vgaMode13) {
        FieldVga::setMode(FieldVga::MODE_VGA_13, guestRam);
        vgaMode13 = true;
    }
    AmmoSnes::installSnesVgaPalette(guestRam);
    for (int y = 0; y < FB_H; ++y)
        for (int x = 0; x < FB_W; ++x)
            guestRam[FieldVga::VGA_FB + static_cast<std::size_t>(y * 320 + x)] =
                fb[y * FB_W + x];
}

inline void pushFrameAudio() noexcept {
    if (!cfg.soundOn) return;
    constexpr int kN = Audio::RATE / 60;
    float block[kN]{};
    const int n = AmmoSnes::renderAudioFrame(chip, block, kN);
    const float gain = static_cast<float>(cfg.masterVolume) / 512.f;
    for (int i = 0; i < n; ++i) block[i] *= gain;
    Audio::pushSamples(block, static_cast<std::uint32_t>(n));
}

inline void runFrame(std::uint8_t* guestRam) noexcept {
    if (paused) return;
    chip.thermoSteps = thermoSteps;
    const int burst = cfg.thermoGovernor ? std::clamp(1 + thermoSteps, 1, cfg.maxBurst) : 1;
    for (int b = 0; b < burst; ++b)
        AmmoSnes::runFrameCpu(chip);
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
    FieldWinApp::beginEmu(ram, " AmmoSNES ",
        " File>Open ROM  Options>Settings  Esc quit  P pause ");
}

inline bool handleMenu(int action, std::uint8_t* ram) noexcept {
    switch (action) {
    case FieldWinFrame::A_FILE_OPEN:
        FieldAmmoEmu::openRomDialog(FieldAmmoEmu::Kind::Snes, cfg.lastRom.c_str());
        FieldEmuFileDialog::paint(ram);
        paused = true;
        return true;
    case FieldWinFrame::A_OPT_SETTINGS:
        FieldAmmoSnesSetup::open(true);
        optionsOpen = true;
        paused = true;
        FieldAmmoSnesSetup::paint(ram);
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
                    FieldAmmoSnesConfig::save(cfg);
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
    if (optionsOpen && FieldAmmoSnesSetup::active) {
        FieldAmmoSnesSetup::pump(ram, key, keyDown);
        if (!FieldAmmoSnesSetup::active) {
            optionsOpen = false;
            paused = false;
            cfg = FieldAmmoSnesConfig::g;
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
    FieldAmmoSnesConfig::load(cfg);
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
        FieldAmmoSnesSetup::open(true);
        optionsOpen = true;
        paused = true;
        FieldAmmoSnesSetup::paint(ram);
    }
}

inline void close(std::uint8_t* ram) noexcept {
    active = false;
    optionsOpen = false;
    FieldAmmoSnesSetup::active = false;
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

} // namespace FieldSnes