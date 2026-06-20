#pragma once

// Shared Golden Era emulator glue — audio stream, config helpers, options setup UI.

#include "CHIPS/Common/FieldChipAudio.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldDos.hpp"
#include "FieldAosAppIdentity.hpp"
#include "FieldAosAppJournal.hpp"
#include "FieldAosAppSnapshot.hpp"
#include "FieldMix.hpp"
#include "FieldRtxApp.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRuntimeInfo.hpp"
#include "FieldEmuRomPaths.hpp"
#include "FieldEmuFileDialog.hpp"
#include "FieldWinApp.hpp"
#include "OptionsMenu.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace FieldAmmoEmu {

inline void openRomDialog(Kind k, const char* lastRom = nullptr) noexcept {
    const auto& p = profile(k);
    std::string start = p.romDir;
    if (lastRom && lastRom[0]) {
        std::string path = lastRom;
        const auto slash = path.find_last_of('\\');
        if (slash != std::string::npos)
            start = path.substr(0, slash + 1);
        else if (path.back() == '\\')
            start = path;
    }
    FieldEmuFileDialog::open(start.c_str(), p.openTitle, p.exts, p.extCount);
}

struct BaseOptions {
    bool soundOn = true;
    FieldChips::AudioStyle audioStyle = FieldChips::AudioStyle::Modern;
    int masterVolume = 256;
    std::string lastRom;
};

inline const char* audioStyleLabel(FieldChips::AudioStyle s) noexcept {
    return s == FieldChips::AudioStyle::Modern ? "Modern (smooth)" : "Classic (chippy)";
}

inline void applyBaseKv(BaseOptions& o, const char* key, const char* val) noexcept {
    if (!key || !val) return;
    const int iv = std::atoi(val);
    if (std::strcmp(key, "sound_on") == 0) o.soundOn = iv != 0;
    else if (std::strcmp(key, "audio_style") == 0)
        o.audioStyle = iv != 0 ? FieldChips::AudioStyle::Modern : FieldChips::AudioStyle::Classic;
    else if (std::strcmp(key, "master_volume") == 0) o.masterVolume = iv;
    else if (std::strcmp(key, "last_rom") == 0) o.lastRom = val;
}

inline void loadConfig(const char* path, BaseOptions& o,
                       void (*extraKv)(BaseOptions&, const char*, const char*) = nullptr) noexcept {
    o = BaseOptions{};
    if (Options::Emulators::DefaultAudioStyle == 0)
        o.audioStyle = FieldChips::AudioStyle::Classic;
    std::vector<std::uint8_t> raw;
    if (!path || !FieldAmmoVfs::readPath(path, raw) || raw.empty()) return;
    std::string text(raw.begin(), raw.end());
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t end = text.find('\n', pos);
        const std::size_t len = (end == std::string::npos) ? text.size() - pos : end - pos;
        std::string line = text.substr(pos, len);
        pos = (end == std::string::npos) ? text.size() : end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        applyBaseKv(o, line.substr(0, eq).c_str(), line.c_str() + eq + 1);
        if (extraKv) extraKv(o, line.substr(0, eq).c_str(), line.c_str() + eq + 1);
    }
}

inline void saveConfig(const char* path, const BaseOptions& o, const char* header) noexcept {
    char buf[4096];
    const int n = std::snprintf(buf, sizeof buf,
        "%s\n"
        "sound_on=%d\n"
        "audio_style=%d\n"
        "master_volume=%d\n"
        "last_rom=%s\n",
        header ? header : "# emulator config",
        o.soundOn ? 1 : 0,
        static_cast<int>(o.audioStyle),
        o.masterVolume,
        o.lastRom.c_str());
    FieldAmmoVfs::writePath(path,
        reinterpret_cast<const std::uint8_t*>(buf),
        static_cast<std::size_t>(n));
}

template<int Slot, bool& ActiveFlag, float& LevelFlag>
struct AudioStream {
    static constexpr int RATE = 44100;
    static constexpr std::size_t RING_CAP = 65536u;
    static constexpr int PUMP_CHUNK = 2048;
    static constexpr std::size_t MIN_PREROLL = static_cast<std::size_t>(RATE / 25u);
    static constexpr int MAX_QUEUED_BYTES = RATE * static_cast<int>(sizeof(float));

    inline static MIX_Mixer* mixer = nullptr;
    inline static MIX_Track* track = nullptr;
    inline static SDL_AudioStream* stream = nullptr;
    inline static bool ready = false;
    inline static std::vector<float> ring;
    inline static std::size_t writePos = 0;
    inline static std::size_t readPos = 0;
    inline static std::size_t fillCount = 0;

    static void shutdown() noexcept {
        ready = false;
        writePos = readPos = fillCount = 0;
        ring.clear();
        if (track) {
            MIX_StopTrack(track, 0);
            MIX_DestroyTrack(track);
            track = nullptr;
        }
        if (stream) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
        }
        mixer = nullptr;
    }

    static void ringPush(const float* samples, std::uint32_t len) noexcept {
        if (!samples || len == 0) return;
        if (ring.empty()) ring.assign(RING_CAP, 0.f);
        for (std::uint32_t i = 0; i < len; ++i) {
            ring[writePos] = samples[i];
            writePos = (writePos + 1u) % ring.size();
            if (fillCount < ring.size()) ++fillCount;
            else readPos = (readPos + 1u) % ring.size();
        }
    }

    static bool init(MIX_Mixer* mix) noexcept {
        shutdown();
        if (!mix || !ActiveFlag) return false;
        mixer = mix;
        SDL_AudioSpec src{};
        src.freq = RATE;
        src.format = SDL_AUDIO_F32;
        src.channels = 1;
        SDL_AudioSpec dst{};
        dst.freq = Options::SDL3::AudioFrequency;
        dst.format = SDL_AUDIO_F32;
        dst.channels = Options::SDL3::AudioChannels;
        stream = SDL_CreateAudioStream(&src, &dst);
        if (!stream) return false;
        ring.assign(RING_CAP, 0.f);
        track = MIX_CreateTrack(mix);
        if (!track || !MIX_SetTrackAudioStream(track, stream)) {
            shutdown();
            return false;
        }
        MIX_SetTrackGain(track, 0.85f);
        ready = true;
        return true;
    }

    static void pushSamples(const float* samples, std::uint32_t len) noexcept {
        if (!samples || len == 0) return;
        ringPush(samples, len);
        float peak = 0.f;
        for (std::uint32_t i = 0; i < len; ++i)
            peak = std::max(peak, std::fabs(samples[i]));
        LevelFlag = std::max(LevelFlag, peak);
    }

    static void pump() noexcept {
        if (!ActiveFlag) {
            if (ready) shutdown();
            return;
        }
        if (!ready && mixer) init(mixer);
        if (!ready || !stream || !track) return;
        std::vector<float> chunk(static_cast<std::size_t>(PUMP_CHUNK));
        while (fillCount > 0 && SDL_GetAudioStreamQueued(stream) < MAX_QUEUED_BYTES) {
            const int n = std::min(PUMP_CHUNK, static_cast<int>(fillCount));
            for (int i = 0; i < n; ++i) {
                chunk[static_cast<std::size_t>(i)] = ring[readPos];
                readPos = (readPos + 1u) % ring.size();
                --fillCount;
            }
            if (!SDL_PutAudioStreamData(stream, chunk.data(), n * static_cast<int>(sizeof(float))))
                break;
        }
        if (!MIX_TrackPlaying(track) && fillCount >= MIN_PREROLL)
            FieldMix::playTrack(track, -1);
    }
};

using SetupPaintFn = void(*)(FieldRtxWidgets::Ui&, int page, int sel, BaseOptions&, int& y);
using SetupToggleFn = void(*)(int page, int sel, BaseOptions&);
using SetupMaxSelFn = int(*)(int page);

struct SetupCtx {
    const char* title;
    std::uint8_t widgetAppId;
    FieldAosAppIdentity::AppId identity;
    bool& active;
    int& page;
    int& sel;
    int pageCount;
    const char* const* pageTitles;
    BaseOptions& opts;
    void (*load)();
    void (*save)();
    void (*defaults)();
    SetupMaxSelFn maxSel;
    SetupPaintFn paintPage;
    SetupToggleFn toggle;
};

inline void paintSetup(const SetupCtx& ctx, std::uint8_t* ram) noexcept {
    FieldRtxApp::begin(ram, ctx.widgetAppId);
    auto& ui = FieldRtxApp::ui;
    using namespace FieldRtxWidgets;
    ui.panel(8, 8, 1016, 1016);
    char title[72];
    std::snprintf(title, sizeof title, "%s - %s", ctx.title,
        ctx.pageTitles[ctx.page >= 0 && ctx.page < ctx.pageCount ? ctx.page : 0]);
    ui.label(24, uiRow(0), 640, uiRow(0) + UI_LABEL_H, title, 1);
    ui.dropdown(660, uiRow(0), 1000, uiRow(0) + UI_ROW_H,
        ctx.pageTitles[ctx.page >= 0 && ctx.page < ctx.pageCount ? ctx.page : 0], ctx.page);
    FieldAosAppIdentity::paintAbout(ui, ctx.identity, uiRow(1), uiRow(5));
    FieldAosAppJournal::paintRecent(ui, FieldAosAppJournal::journalProgId, uiRow(5), uiRow(7));
    FieldAosAppSnapshot::apply(ctx.identity);
    char tabs[80];
    std::snprintf(tabs, sizeof tabs, "Page %d/%d  Tab next  Enter toggle  F2 save  F3 defaults",
        ctx.page + 1, ctx.pageCount);
    ui.label(24, uiRow(7), 1000, uiRow(7) + UI_LABEL_H, tabs, 0);
    ui.label(24, uiRow(8), 1000, uiRow(8) + UI_LABEL_H, FieldRuntimeInfo::masterStatusLine(), 0);
    int y = uiRow(9);
    if (ctx.paintPage) ctx.paintPage(ui, ctx.page, ctx.sel, ctx.opts, y);
    FieldAosAppSnapshot::refresh(ctx.identity);
    FieldRtxApp::finish(ram);
}

inline void pumpSetup(SetupCtx& ctx, std::uint8_t* ram,
                      std::uint16_t key, bool keyDown, bool overlay = false) noexcept {
    if (!ctx.active) return;
    if (keyDown) {
        if (key == 0x011Bu) {
            ctx.active = false;
            if (!overlay) {
                FieldRtxWidgets::clearRam(ram);
                FieldWinApp::reset();
            }
            return;
        }
        if (key == 0x0F00u) {
            ctx.page = (ctx.page + 1) % ctx.pageCount;
            ctx.sel = 0;
            paintSetup(ctx, ram);
            return;
        }
        if (key == 0x4900u || key == 0x4800u) {
            if (ctx.sel > 0) --ctx.sel;
            paintSetup(ctx, ram);
            return;
        }
        if (key == 0x5100u || key == 0x5000u) {
            if (ctx.maxSel && ctx.sel < ctx.maxSel(ctx.page)) ++ctx.sel;
            paintSetup(ctx, ram);
            return;
        }
        if (key == 0x3F00u && ctx.defaults) {
            ctx.defaults();
            paintSetup(ctx, ram);
            return;
        }
        if (key == 0x3C00u && ctx.save) {
            ctx.save();
            paintSetup(ctx, ram);
            return;
        }
        if (key == 0x1C0Du && ctx.toggle) {
            ctx.toggle(ctx.page, ctx.sel, ctx.opts);
            paintSetup(ctx, ram);
            return;
        }
    }
    paintSetup(ctx, ram);
}

inline void paintAudioRows(FieldRtxWidgets::Ui& ui, int sel, const BaseOptions& o, int& y) noexcept {
    char v[64];
    auto row = [&](const char* label, const char* val, int idx) {
        const bool hi = sel == idx;
        char line[96];
        std::snprintf(line, sizeof line, "%s%s", hi ? "> " : "  ", label);
        ui.label(32, y, 360, y + 32, line, hi ? 1 : 0);
        ui.label(380, y, 960, y + 32, val, hi ? 1 : 0);
        y += 40;
    };
    row("Sound", o.soundOn ? "On" : "Muted", 0);
    row("Audio style", audioStyleLabel(o.audioStyle), 1);
    std::snprintf(v, sizeof v, "%d", o.masterVolume);
    row("Master volume", v, 2);
}

inline void paintSystemRows(FieldRtxWidgets::Ui& ui, int sel, const BaseOptions& o, int& y) noexcept {
    char v[64];
    auto row = [&](const char* label, const char* val, int idx) {
        const bool hi = sel == idx;
        char line[96];
        std::snprintf(line, sizeof line, "%s%s", hi ? "> " : "  ", label);
        ui.label(32, y, 360, y + 32, line, hi ? 1 : 0);
        ui.label(380, y, 960, y + 32, val, hi ? 1 : 0);
        y += 40;
    };
    std::snprintf(v, sizeof v, "%.48s", o.lastRom.c_str());
    row("Last ROM path", v, 0);
}

inline void toggleAudio(int sel, BaseOptions& o) noexcept {
    switch (sel) {
    case 0: o.soundOn = !o.soundOn; break;
    case 1: o.audioStyle = (o.audioStyle == FieldChips::AudioStyle::Modern)
        ? FieldChips::AudioStyle::Classic : FieldChips::AudioStyle::Modern; break;
    case 2: o.masterVolume = std::min(512, o.masterVolume + 32); break;
    default: break;
    }
}

inline int maxSelAudio(int) noexcept { return 2; }
inline int maxSelSystem(int) noexcept { return 0; }
inline int maxSelHelp(int) noexcept { return 0; }

inline void paintHelpRows(FieldRtxWidgets::Ui& ui, const char* const* lines, int n, int& y) noexcept {
    for (int i = 0; i < n; ++i)
        ui.label(32, y + i * 40, 980, y + i * 40 + 32, lines[i], 0);
    y += n * 40;
}

inline bool pumpFileDialog(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept {
    if (!FieldEmuFileDialog::active) return false;
    return FieldEmuFileDialog::pumpKey(ram, key, keyDown);
}

inline bool pumpFileDialogMouse(std::uint8_t* ram, int action) noexcept {
    if (!FieldEmuFileDialog::active) return false;
    return FieldEmuFileDialog::pumpMouse(ram, action);
}

inline bool findRomInVfsDir(const char* vfsDir, const char* ext, std::string& out) noexcept {
    if (!vfsDir || !ext) return false;
    const std::size_t extLen = std::strlen(ext);
    std::vector<FieldDos::FatRootEntry> entries;
    if (!FieldAmmoVfs::listPath(vfsDir, entries)) return false;
    for (const auto& e : entries) {
        if (e.isDir) continue;
        std::string low = e.name;
        for (auto& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (low.size() >= extLen && low.substr(low.size() - extLen) == ext) {
            out = std::string(vfsDir);
            if (out.back() != '\\') out += '\\';
            out += e.name;
            std::vector<std::uint8_t> data;
            if (FieldAmmoFat::readFile(out.c_str(), data) && data.size() >= 256)
                return true;
        }
    }
    return false;
}

} // namespace FieldAmmoEmu