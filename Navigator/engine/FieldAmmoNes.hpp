#pragma once

// AmmoNES — consolidated NES emulator (config, CLI, import, setup, audio, core).

#include "FieldAmmoFat.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldDos.hpp"
#include "FieldInput.hpp"
#include "FieldMix.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRuntimeInfo.hpp"
#include "FieldVga.hpp"
#include "OptionsMenu.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// --- Config ---

// AmmoNES persistent options — mirrors FCEUX CLI flags + Field thermo governor.

namespace FieldAmmoNesConfig {

enum class Region : std::uint8_t { Ntsc = 0, Pal = 1, Dendy = 2 };

enum class NesBtn : std::uint8_t {
    A = 0, B, Select, Start, Up, Down, Left, Right, Count
};

struct PadBinding {
    SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
    std::uint32_t  gamepadMask = 0u;
};

struct Options {
    Region region = Region::Ntsc;
    bool spriteLimit = true;
    bool renderSprites = true;
    bool renderBg = true;
    bool soundOn = true;
    int  soundQuality = 1;
    int  masterVolume = 256;
    int  sq1Vol = 256;
    int  sq2Vol = 256;
    int  triVol = 256;
    int  noiseVol = 256;
    int  pcmVol = 256;
    bool thermoGovernor = true;
    int  maxBurst = 3;
    bool turboForce = false;
    bool fourScore = false;
    bool player2 = false;
    bool gameGenie = false;
    int  saveSlot = 0;
    std::string lastRom = "C:\\NES\\";

    PadBinding p1[static_cast<int>(NesBtn::Count)]{};
    PadBinding p2[static_cast<int>(NesBtn::Count)]{};
};

inline Options g;

inline void setDefaults(Options& o) noexcept {
    o = Options{};
    o.p1[static_cast<int>(NesBtn::A)].key = SDL_SCANCODE_Z;
    o.p1[static_cast<int>(NesBtn::B)].key = SDL_SCANCODE_X;
    o.p1[static_cast<int>(NesBtn::Select)].key = SDL_SCANCODE_RSHIFT;
    o.p1[static_cast<int>(NesBtn::Start)].key = SDL_SCANCODE_RETURN;
    o.p1[static_cast<int>(NesBtn::Up)].key = SDL_SCANCODE_UP;
    o.p1[static_cast<int>(NesBtn::Down)].key = SDL_SCANCODE_DOWN;
    o.p1[static_cast<int>(NesBtn::Left)].key = SDL_SCANCODE_LEFT;
    o.p1[static_cast<int>(NesBtn::Right)].key = SDL_SCANCODE_RIGHT;
    o.p1[static_cast<int>(NesBtn::A)].gamepadMask = 0x01u;
    o.p1[static_cast<int>(NesBtn::B)].gamepadMask = 0x02u;
    o.p1[static_cast<int>(NesBtn::Select)].gamepadMask = 0x04u;
    o.p1[static_cast<int>(NesBtn::Start)].gamepadMask = 0x08u;
    o.p1[static_cast<int>(NesBtn::Up)].gamepadMask = 0x10u;
    o.p1[static_cast<int>(NesBtn::Down)].gamepadMask = 0x20u;
    o.p1[static_cast<int>(NesBtn::Left)].gamepadMask = 0x40u;
    o.p1[static_cast<int>(NesBtn::Right)].gamepadMask = 0x80u;
    o.p2[static_cast<int>(NesBtn::A)].key = SDL_SCANCODE_J;
    o.p2[static_cast<int>(NesBtn::B)].key = SDL_SCANCODE_K;
    o.p2[static_cast<int>(NesBtn::Select)].key = SDL_SCANCODE_RCTRL;
    o.p2[static_cast<int>(NesBtn::Start)].key = SDL_SCANCODE_BACKSPACE;
    o.p2[static_cast<int>(NesBtn::Up)].key = SDL_SCANCODE_W;
    o.p2[static_cast<int>(NesBtn::Down)].key = SDL_SCANCODE_S;
    o.p2[static_cast<int>(NesBtn::Left)].key = SDL_SCANCODE_A;
    o.p2[static_cast<int>(NesBtn::Right)].key = SDL_SCANCODE_D;
}

inline const char* kCfgPath = "C:\\NES\\AMMONES.CFG";

inline const char* regionName(Region r) noexcept {
    switch (r) {
    case Region::Pal: return "PAL";
    case Region::Dendy: return "Dendy";
    default: return "NTSC";
    }
}

inline const char* btnLabel(NesBtn b) noexcept {
    switch (b) {
    case NesBtn::A: return "A";
    case NesBtn::B: return "B";
    case NesBtn::Select: return "Select";
    case NesBtn::Start: return "Start";
    case NesBtn::Up: return "Up";
    case NesBtn::Down: return "Down";
    case NesBtn::Left: return "Left";
    case NesBtn::Right: return "Right";
    default: return "?";
    }
}

inline void applyKv(Options& o, const char* key, const char* val) noexcept {
    if (!key || !val) return;
    const int iv = std::atoi(val);
    if (std::strcmp(key, "region") == 0)
        o.region = static_cast<Region>(iv < 0 ? 0 : (iv > 2 ? 2 : iv));
    else if (std::strcmp(key, "sprite_limit") == 0) o.spriteLimit = iv != 0;
    else if (std::strcmp(key, "render_sprites") == 0) o.renderSprites = iv != 0;
    else if (std::strcmp(key, "render_bg") == 0) o.renderBg = iv != 0;
    else if (std::strcmp(key, "sound_on") == 0) o.soundOn = iv != 0;
    else if (std::strcmp(key, "sound_quality") == 0) o.soundQuality = iv;
    else if (std::strcmp(key, "master_volume") == 0) o.masterVolume = iv;
    else if (std::strcmp(key, "sq1_volume") == 0) o.sq1Vol = iv;
    else if (std::strcmp(key, "sq2_volume") == 0) o.sq2Vol = iv;
    else if (std::strcmp(key, "tri_volume") == 0) o.triVol = iv;
    else if (std::strcmp(key, "noise_volume") == 0) o.noiseVol = iv;
    else if (std::strcmp(key, "pcm_volume") == 0) o.pcmVol = iv;
    else if (std::strcmp(key, "thermo") == 0) o.thermoGovernor = iv != 0;
    else if (std::strcmp(key, "max_burst") == 0) o.maxBurst = iv;
    else if (std::strcmp(key, "turbo") == 0) o.turboForce = iv != 0;
    else if (std::strcmp(key, "fourscore") == 0) o.fourScore = iv != 0;
    else if (std::strcmp(key, "player2") == 0) o.player2 = iv != 0;
    else if (std::strcmp(key, "game_genie") == 0) o.gameGenie = iv != 0;
    else if (std::strcmp(key, "save_slot") == 0) o.saveSlot = iv;
    else if (std::strcmp(key, "last_rom") == 0) o.lastRom = val;
    else if (key[0] == 'p' && (key[1] == '1' || key[1] == '2')) {
        const int player = (key[1] == '2') ? 1 : 0;
        auto& binds = (player == 1) ? o.p2 : o.p1;
        for (int i = 0; i < static_cast<int>(NesBtn::Count); ++i) {
            char kn[16];
            std::snprintf(kn, sizeof kn, "p%d_%s_key", player + 1, btnLabel(static_cast<NesBtn>(i)));
            if (std::strcmp(key, kn) == 0) {
                binds[i].key = static_cast<SDL_Scancode>(iv);
                return;
            }
            std::snprintf(kn, sizeof kn, "p%d_%s_gp", player + 1, btnLabel(static_cast<NesBtn>(i)));
            if (std::strcmp(key, kn) == 0) {
                binds[i].gamepadMask = static_cast<std::uint32_t>(iv);
                return;
            }
        }
    }
}

inline void load(Options& o = g) noexcept {
    setDefaults(o);
    std::vector<std::uint8_t> raw;
    if (!FieldAmmoVfs::readPath(kCfgPath, raw) || raw.empty()) return;
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
        applyKv(o, line.substr(0, eq).c_str(), line.substr(eq + 1).c_str());
    }
}

inline void save(const Options& o = g) noexcept {
    char buf[4096];
    int n = std::snprintf(buf, sizeof buf,
        "# AmmoNES config v1\n"
        "region=%d\n"
        "sprite_limit=%d\n"
        "render_sprites=%d\n"
        "render_bg=%d\n"
        "sound_on=%d\n"
        "sound_quality=%d\n"
        "master_volume=%d\n"
        "sq1_volume=%d\n"
        "sq2_volume=%d\n"
        "tri_volume=%d\n"
        "noise_volume=%d\n"
        "pcm_volume=%d\n"
        "thermo=%d\n"
        "max_burst=%d\n"
        "turbo=%d\n"
        "fourscore=%d\n"
        "player2=%d\n"
        "game_genie=%d\n"
        "save_slot=%d\n"
        "last_rom=%s\n",
        static_cast<int>(o.region),
        o.spriteLimit ? 1 : 0,
        o.renderSprites ? 1 : 0,
        o.renderBg ? 1 : 0,
        o.soundOn ? 1 : 0,
        o.soundQuality,
        o.masterVolume,
        o.sq1Vol, o.sq2Vol, o.triVol, o.noiseVol, o.pcmVol,
        o.thermoGovernor ? 1 : 0,
        o.maxBurst,
        o.turboForce ? 1 : 0,
        o.fourScore ? 1 : 0,
        o.player2 ? 1 : 0,
        o.gameGenie ? 1 : 0,
        o.saveSlot,
        o.lastRom.c_str());
    for (int player = 0; player < 2 && n < static_cast<int>(sizeof buf) - 64; ++player) {
        const auto& binds = (player == 1) ? o.p2 : o.p1;
        for (int i = 0; i < static_cast<int>(NesBtn::Count); ++i) {
            n += std::snprintf(buf + n, sizeof buf - static_cast<std::size_t>(n),
                "p%d_%s_key=%d\np%d_%s_gp=%u\n",
                player + 1, btnLabel(static_cast<NesBtn>(i)),
                static_cast<int>(binds[i].key),
                player + 1, btnLabel(static_cast<NesBtn>(i)),
                binds[i].gamepadMask);
        }
    }
    FieldAmmoVfs::writePath(kCfgPath,
        reinterpret_cast<const std::uint8_t*>(buf),
        static_cast<std::size_t>(n));
}

} // namespace FieldAmmoNesConfig
// --- CLI ---

// AmmoNES command-line parser — NES subcommands and flags.


#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace FieldAmmoNesCli {

struct LaunchRequest {
    std::string romPath;
    bool openSetup = false;
    bool openPadMap = false;
    bool doImport = false;
    bool showHelp = false;
    bool saveState = false;
    bool loadState = false;
    int  stateSlot = 0;
    bool applyOnly = false;
    FieldAmmoNesConfig::Options opts;
};

inline bool ieq(const std::string& a, const char* b) noexcept {
    if (!b) return false;
    std::string x = a;
    for (auto& c : x) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return x == b;
}

inline bool parseBoolFlag(const std::string& arg, const char* name, bool& out) noexcept {
    if (ieq(arg, name)) { out = true; return true; }
    const std::string neg = std::string("no-") + (name + 1);
    if (ieq(arg, neg.c_str())) { out = false; return true; }
    return false;
}

inline bool parseKeyVal(const std::string& arg, const char* key, int& out) noexcept {
    const std::size_t klen = std::strlen(key);
    if (arg.size() <= klen + 1) return false;
    if (arg[klen] != '=') return false;
    std::string prefix = arg.substr(0, klen);
    for (auto& c : prefix) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (prefix != key) return false;
    out = std::stoi(arg.substr(klen + 1));
    return true;
}

inline LaunchRequest parse(const std::vector<std::string>& args) noexcept {
    LaunchRequest req;
    FieldAmmoNesConfig::load(req.opts);

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (ieq(a, "HELP") || ieq(a, "/?") || ieq(a, "-?")) {
            req.showHelp = true;
            continue;
        }
        if (ieq(a, "SETUP") || ieq(a, "CONFIG")) {
            req.openSetup = true;
            continue;
        }
        if (ieq(a, "PAD") || ieq(a, "CONTROLS") || ieq(a, "JOYMAP")) {
            req.openPadMap = true;
            continue;
        }
        if (ieq(a, "IMPORT")) {
            req.doImport = true;
            continue;
        }
        if (ieq(a, "--ntsc")) { req.opts.region = FieldAmmoNesConfig::Region::Ntsc; continue; }
        if (ieq(a, "--pal")) { req.opts.region = FieldAmmoNesConfig::Region::Pal; continue; }
        if (ieq(a, "--dendy")) { req.opts.region = FieldAmmoNesConfig::Region::Dendy; continue; }
        if (ieq(a, "--no-sprite-limit")) { req.opts.spriteLimit = false; continue; }
        if (ieq(a, "--sprite-limit")) { req.opts.spriteLimit = true; continue; }
        if (ieq(a, "--mute")) { req.opts.soundOn = false; continue; }
        if (ieq(a, "--unmute")) { req.opts.soundOn = true; continue; }
        if (ieq(a, "--fourscore")) { req.opts.fourScore = true; continue; }
        if (ieq(a, "--p2") || ieq(a, "--2p")) { req.opts.player2 = true; continue; }
        if (ieq(a, "--turbo")) { req.opts.turboForce = true; continue; }
        if (ieq(a, "--no-thermo")) { req.opts.thermoGovernor = false; continue; }
        if (ieq(a, "--thermo")) { req.opts.thermoGovernor = true; continue; }
        if (ieq(a, "--gg") || ieq(a, "--game-genie")) { req.opts.gameGenie = true; continue; }
        if (ieq(a, "--apply")) { req.applyOnly = true; continue; }
        if (ieq(a, "--save")) { req.saveState = true; continue; }
        if (ieq(a, "--load")) { req.loadState = true; continue; }

        int iv = 0;
        if (parseKeyVal(a, "--volume", iv)) { req.opts.masterVolume = std::clamp(iv, 0, 512); continue; }
        if (parseKeyVal(a, "--quality", iv)) { req.opts.soundQuality = std::clamp(iv, 0, 2); continue; }
        if (parseKeyVal(a, "--burst", iv)) { req.opts.maxBurst = std::clamp(iv, 1, 3); continue; }
        if (parseKeyVal(a, "--save", iv)) { req.saveState = true; req.stateSlot = iv; continue; }
        if (parseKeyVal(a, "--load", iv)) { req.loadState = true; req.stateSlot = iv; continue; }
        if (parseKeyVal(a, "--region", iv)) {
            req.opts.region = static_cast<FieldAmmoNesConfig::Region>(std::clamp(iv, 0, 2));
            continue;
        }

        std::string p = a;
        for (auto& c : p) if (c == '/') c = '\\';
        if (p.find(':') == std::string::npos && p.find('\\') == std::string::npos)
            p = "C:\\NES\\" + p;
        if (p.find('.') == std::string::npos) p += ".NES";
        req.romPath = p;
    }
    return req;
}

inline const char* kHelpText =
    "\r\nAmmoNES — full command reference\r\n"
    "  NES HELP              This help\r\n"
    "  NES SETUP             Options program (video/audio/system)\r\n"
    "  NES PAD               Controller mapping program\r\n"
    "  NES IMPORT            Copy host .nes → C:\\NES\\\r\n"
    "  NES [rom] [flags]     Play ROM with options\r\n"
    "\r\n"
    "  --ntsc --pal --dendy       Video region\r\n"
    "  --sprite-limit / --no-sprite-limit\r\n"
    "  --mute / --unmute          Audio master\r\n"
    "  --volume=N                 Master volume 0-512\r\n"
    "  --quality=0|1|2            APU resampling quality\r\n"
    "  --fourscore                Four-score adapter\r\n"
    "  --p2 / --2p                Enable player 2\r\n"
    "  --turbo                    Max thermo burst (speed)\r\n"
    "  --thermo / --no-thermo     Field governor on/off\r\n"
    "  --burst=1..3               Max frames per host tick\r\n"
    "  --gg / --game-genie        Game Genie cheats\r\n"
    "  --save[=N] / --load[=N]    Save state slot\r\n"
    "  --apply                    Apply flags without starting ROM\r\n"
    "\r\n"
    "  In-game: Z/X/Arrows/Enter  P pause  R reset  Esc quit\r\n"
    "  Start menu: Games → AmmoNES / AmmoNES Setup / Controls\r\n";

} // namespace FieldAmmoNesCli

// --- Import --- — scan host paths, copy into C:\NES\ on AMMOFAT.

namespace FieldNesImport {

inline std::vector<std::filesystem::path> scanRoots() {
    std::vector<std::filesystem::path> roots;
    const char* home = std::getenv("HOME");
    if (home) {
        const std::filesystem::path h(home);
        roots.push_back(h / "Desktop");
        roots.push_back(h / "Downloads");
        roots.push_back(h / "Documents");
        roots.push_back(h / "ROMs");
        roots.push_back(h / "roms");
        roots.push_back(h / "NES");
        roots.push_back(h / "nes");
        roots.push_back(h / ".local/share/retroarch/downloads");
        roots.push_back(h / ".config/retroarch/downloads");
    }
    const auto proj = FieldDos::resolveRoot();
    roots.push_back(proj / "assets" / "dos" / "incoming" / "nes");
    roots.push_back(proj / "assets" / "dos" / "incoming" / "NES");
    roots.push_back(proj / "assets" / "dos" / "ammo" / "NES");
    return roots;
}
inline bool isNesRom(const std::filesystem::path& p) {
    if (!p.has_extension()) return false;
    auto ext = p.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".nes";
}

inline bool ensureNesDir() noexcept {
    if (!FieldAmmoFat::mounted && !FieldAmmoFat::mount()) return false;
    if (!FieldAmmoVfs::pathExists("C:\\NES"))
        FieldAmmoVfs::mkdirPath("C:\\NES");
    return true;
}

inline int importAll(std::vector<std::string>& copied, std::vector<std::string>& skipped) {
    if (!ensureNesDir()) return 0;
    copied.clear();
    skipped.clear();
    int n = 0;
    for (const auto& root : scanRoots()) {
        if (!std::filesystem::exists(root)) continue;
        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
             it != std::filesystem::recursive_directory_iterator(); ++it) {
            if (ec) break;
            if (!it->is_regular_file()) continue;
            const auto& p = it->path();
            if (!isNesRom(p)) continue;
            std::vector<std::uint8_t> data;
            std::ifstream in(p, std::ios::binary);
            if (!in) { skipped.push_back(p.string()); continue; }
            data.assign(std::istreambuf_iterator<char>(in), {});
            if (data.size() < 16 || data[0] != 'N') { skipped.push_back(p.string()); continue; }
            std::string dest = "C:\\NES\\" + p.filename().string();
            for (auto& c : dest) if (c == '/') c = '\\';
            std::string upper = dest;
            for (auto& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (FieldAmmoVfs::writePath(dest.c_str(), data.data(), data.size())) {
                copied.push_back(dest);
                ++n;
            } else skipped.push_back(p.string());
        }
    }
    return n;
}

inline bool romReadable(const char* path) {
    if (!path || !path[0]) return false;
    std::vector<std::uint8_t> data;
    return FieldAmmoFat::readFile(path, data) && data.size() >= 16
        && data[0] == 'N' && data[1] == 'E' && data[2] == 'S' && data[3] == 0x1A;
}

inline int ensureImported() {
    if (!ensureNesDir()) return 0;
    std::vector<std::string> copied, skipped;
    return importAll(copied, skipped);
}

inline bool findContra(std::string& outPath) {
    if (!ensureNesDir()) return false;
    static const char* kNames[] = {
        "C:\\NES\\CONTRA.NES", "C:\\NES\\Contra.nes", "C:\\NES\\contra.nes",
        "C:\\NES\\CONTRA (U).NES", "C:\\NES\\Contra (U).nes",
    };
    for (const char* p : kNames) {
        if (romReadable(p)) {
            outPath = p;
            return true;
        }
    }
    std::vector<FieldDos::FatRootEntry> entries;
    if (FieldAmmoVfs::listPath("C:\\NES", entries)) {
        for (const auto& e : entries) {
            if (e.isDir) continue;
            std::string low = e.name;
            for (auto& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (low.find("contra") != std::string::npos) {
                outPath = std::string("C:\\NES\\") + e.name;
                if (romReadable(outPath.c_str())) return true;
            }
        }
    }
    return false;
}

inline bool findAnyRom(std::string& outPath) {
    if (!ensureNesDir()) return false;
    if (findContra(outPath)) return true;
    static const char* kNames[] = {
        "C:\\NES\\CONTRA.NES", "C:\\NES\\Contra.nes", "C:\\NES\\contra.nes",
        "C:\\NES\\DEMO.NES", "C:\\NES\\SMB.NES", "C:\\NES\\smb.nes",
    };
    for (const char* p : kNames) {
        if (romReadable(p)) {
            outPath = p;
            return true;
        }
    }
    std::vector<FieldDos::FatRootEntry> entries;
    if (FieldAmmoVfs::listPath("C:\\NES", entries)) {
        for (const auto& e : entries) {
            if (e.isDir) continue;
            std::string low = e.name;
            for (auto& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (low.size() >= 4 && low.substr(low.size() - 4) == ".nes") {
                outPath = std::string("C:\\NES\\") + e.name;
                if (romReadable(outPath.c_str())) return true;
            }
        }
    }
    return false;
}

} // namespace FieldNesImport

// --- Core state (early — FieldNesAudio references active/audioLevel) ---

namespace FieldNes {
inline bool active = false;
inline float audioLevel = 0.f;
}

// --- Audio --- → SDL3_mixer streaming (ring buffer + AudioStream).

namespace FieldNesAudio {

constexpr int SLOT = 13;
constexpr int RATE = 44100;
constexpr std::size_t RING_CAP = 65536u;
constexpr int PUMP_CHUNK = 2048;
constexpr std::size_t MIN_PREROLL = static_cast<std::size_t>(RATE / 25u);
constexpr int MAX_QUEUED_BYTES = RATE * static_cast<int>(sizeof(float));

inline MIX_Mixer* mixer = nullptr;
inline MIX_Track* track = nullptr;
inline SDL_AudioStream* stream = nullptr;
inline bool ready = false;

inline std::vector<float> ring;
inline std::size_t writePos = 0;
inline std::size_t readPos = 0;
inline std::size_t fillCount = 0;

inline void shutdown() noexcept {
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

inline void ringPush(const float* samples, std::uint32_t len) noexcept {
    if (!samples || len == 0) return;
    if (ring.empty()) ring.assign(RING_CAP, 0.f);
    for (std::uint32_t i = 0; i < len; ++i) {
        ring[writePos] = samples[i];
        writePos = (writePos + 1u) % ring.size();
        if (fillCount < ring.size()) {
            ++fillCount;
        } else {
            readPos = (readPos + 1u) % ring.size();
        }
    }
}

inline bool init(MIX_Mixer* mix) noexcept {
    shutdown();
    if (!mix || !FieldNes::active) return false;
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
    writePos = readPos = fillCount = 0;

    track = MIX_CreateTrack(mix);
    if (!track) {
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
        return false;
    }
    if (!MIX_SetTrackAudioStream(track, stream)) {
        MIX_DestroyTrack(track);
        track = nullptr;
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
        return false;
    }
    MIX_SetTrackGain(track, 0.85f);
    ready = true;
    return true;
}

inline void pushSamples(const float* samples, std::uint32_t len) noexcept {
    if (!samples || len == 0) return;
    ringPush(samples, len);
    float peak = 0.f;
    for (std::uint32_t i = 0; i < len; ++i)
        peak = std::max(peak, std::fabs(samples[i]));
    FieldNes::audioLevel = std::max(FieldNes::audioLevel, peak);
}

inline void pump() noexcept {
    if (!FieldNes::active) {
        if (ready) shutdown();
        return;
    }
    if (!ready && mixer)
        init(mixer);
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

} // namespace FieldNesAudio

// --- Setup --- — video/audio/system options + controller mapping (full GUI).

namespace FieldAmmoNesSetup {

inline bool active = false;
inline bool padOnly = false;
inline int  page = 0;
inline int  sel = 0;
inline int  bindPlayer = 0;
inline int  bindBtn = -1;
inline bool listening = false;

constexpr int kPages = 6;

inline const char* kPageTitle(int p) noexcept {
    static const char* t[] = { "Video", "Audio", "P1 Controls", "P2 Controls", "System", "CLI Help" };
    return (p >= 0 && p < kPages) ? t[p] : "?";
}

inline void scancodeName(SDL_Scancode sc, char* buf, std::size_t n) noexcept {
    if (!buf || n < 2u) return;
    if (sc == SDL_SCANCODE_UNKNOWN) {
        std::snprintf(buf, n, "(none)");
        return;
    }
    const char* nm = SDL_GetScancodeName(sc);
    if (nm && nm[0]) std::snprintf(buf, n, "%s", nm);
    else std::snprintf(buf, n, "key%d", static_cast<int>(sc));
}

inline void paintRow(std::uint8_t* ram, int row, int col, const char* label, const char* val,
                     bool hi) noexcept {
    FieldRtxGui::text(ram, row, col, label, hi ? FieldRtxGui::ATTR_MENU_SEL : FieldRtxGui::ATTR_HELP, 18);
    FieldRtxGui::text(ram, row, col + 20, val, hi ? FieldRtxGui::ATTR_GOLD : FieldRtxGui::ATTR_EDITOR, 38);
}

inline void paintVideo(std::uint8_t* ram, const FieldAmmoNesConfig::Options& o) noexcept {
    char v[40];
    std::snprintf(v, sizeof v, "%s", FieldAmmoNesConfig::regionName(o.region));
    paintRow(ram, 6, 4, "Region", v, sel == 0);
    paintRow(ram, 7, 4, "Sprite limit", o.spriteLimit ? "On (accurate)" : "Off (no cap)", sel == 1);
    paintRow(ram, 8, 4, "Sprites", o.renderSprites ? "Show" : "Hide", sel == 2);
    paintRow(ram, 9, 4, "Background", o.renderBg ? "Show" : "Hide", sel == 3);
}

inline void paintAudio(std::uint8_t* ram, const FieldAmmoNesConfig::Options& o) noexcept {
    char v[40];
    paintRow(ram, 6, 4, "Sound", o.soundOn ? "On" : "Muted", sel == 0);
    std::snprintf(v, sizeof v, "%d", o.soundQuality);
    paintRow(ram, 7, 4, "Quality 0-2", v, sel == 1);
    std::snprintf(v, sizeof v, "%d", o.masterVolume);
    paintRow(ram, 8, 4, "Master volume", v, sel == 2);
    std::snprintf(v, sizeof v, "Sq1:%d Sq2:%d Tri:%d", o.sq1Vol, o.sq2Vol, o.triVol);
    paintRow(ram, 9, 4, "Channels", v, sel == 3);
}

inline void paintPadPage(std::uint8_t* ram, const FieldAmmoNesConfig::Options& o, int player) noexcept {
    const auto& binds = (player == 1) ? o.p2 : o.p1;
    for (int i = 0; i < static_cast<int>(FieldAmmoNesConfig::NesBtn::Count); ++i) {
        char v[48];
        char kn[24];
        scancodeName(binds[i].key, kn, sizeof kn);
        std::snprintf(v, sizeof v, "%s  GP:0x%02X", kn, binds[i].gamepadMask);
        const bool hi = sel == i;
        const bool listen = listening && bindPlayer == player && bindBtn == i;
        paintRow(ram, 6 + i, 4, FieldAmmoNesConfig::btnLabel(
            static_cast<FieldAmmoNesConfig::NesBtn>(i)),
            listen ? "Press key/button..." : v, hi);
    }
}

inline void paintSystem(std::uint8_t* ram, const FieldAmmoNesConfig::Options& o) noexcept {
    char v[40];
    paintRow(ram, 6, 4, "Thermo governor", o.thermoGovernor ? "On" : "Off", sel == 0);
    std::snprintf(v, sizeof v, "%d", o.maxBurst);
    paintRow(ram, 7, 4, "Max burst", v, sel == 1);
    paintRow(ram, 8, 4, "Turbo force", o.turboForce ? "On" : "Off", sel == 2);
    paintRow(ram, 9, 4, "Four-score", o.fourScore ? "On" : "Off", sel == 3);
    paintRow(ram, 10, 4, "Player 2", o.player2 ? "On" : "Off", sel == 4);
    paintRow(ram, 11, 4, "Game Genie", o.gameGenie ? "On" : "Off", sel == 5);
    paintRow(ram, 12, 4, "Last ROM", o.lastRom.c_str(), sel == 6);
}

inline void paintHelp(std::uint8_t* ram) noexcept {
    const char* lines[] = {
        "NES HELP  SETUP  PAD  IMPORT  [rom] [flags]",
        "--ntsc --pal --dendy  --turbo  --burst=3",
        "--volume=256  --quality=1  --no-sprite-limit",
        "--fourscore --p2 --gg --save --load",
        "F2 save cfg  F3 defaults  Tab next page",
    };
    for (int i = 0; i < 5; ++i)
        FieldRtxGui::text(ram, 6 + i, 4, lines[i], FieldRtxGui::ATTR_HELP, 72);
}

inline void paint(std::uint8_t* ram) noexcept {
    auto& o = FieldAmmoNesConfig::g;
    FieldRtxGui::initTextMode(ram);
    char title[64];
    std::snprintf(title, sizeof title, " AmmoNES Setup — %s — Tab page Esc quit ",
        kPageTitle(page));
    FieldRtxGui::drawFrame(ram, 1, 0, 22, 79, FieldRtxGui::ATTR_FRAME, title);
    FieldRtxGui::text(ram, 3, 2, FieldRuntimeInfo::masterStatusLine(),
        FieldRtxGui::ATTR_DIM, 76);
    char tabs[80];
    std::snprintf(tabs, sizeof tabs, " Page %d/%d  PgUp/Dn change  Enter toggle/adjust ",
        page + 1, kPages);
    FieldRtxGui::text(ram, 4, 2, tabs, FieldRtxGui::ATTR_STATUS, 76);

    switch (page) {
    case 0: paintVideo(ram, o); break;
    case 1: paintAudio(ram, o); break;
    case 2: paintPadPage(ram, o, 0); break;
    case 3: paintPadPage(ram, o, 1); break;
    case 4: paintSystem(ram, o); break;
    default: paintHelp(ram); break;
    }

    FieldRtxGui::text(ram, 21, 2,
        " F2 save  F3 reset defaults  Enter edit  Tab next page ",
        FieldRtxGui::ATTR_DIM, 76);
}

inline int maxSel() noexcept {
    switch (page) {
    case 0: return 3;
    case 1: return 3;
    case 2: case 3: return static_cast<int>(FieldAmmoNesConfig::NesBtn::Count) - 1;
    case 4: return 6;
    default: return 0;
    }
}

inline void toggleSel() noexcept {
    auto& o = FieldAmmoNesConfig::g;
    if (page == 0) {
        switch (sel) {
        case 0: o.region = static_cast<FieldAmmoNesConfig::Region>(
            (static_cast<int>(o.region) + 1) % 3); break;
        case 1: o.spriteLimit = !o.spriteLimit; break;
        case 2: o.renderSprites = !o.renderSprites; break;
        case 3: o.renderBg = !o.renderBg; break;
        default: break;
        }
    } else if (page == 1) {
        switch (sel) {
        case 0: o.soundOn = !o.soundOn; break;
        case 1: o.soundQuality = (o.soundQuality + 1) % 3; break;
        case 2: o.masterVolume = std::min(512, o.masterVolume + 32); break;
        case 3: o.sq1Vol = std::min(512, o.sq1Vol + 16); break;
        default: break;
        }
    } else if (page == 2 || page == 3) {
        bindPlayer = (page == 3) ? 1 : 0;
        bindBtn = sel;
        listening = true;
    } else if (page == 4) {
        switch (sel) {
        case 0: o.thermoGovernor = !o.thermoGovernor; break;
        case 1: o.maxBurst = (o.maxBurst % 3) + 1; break;
        case 2: o.turboForce = !o.turboForce; break;
        case 3: o.fourScore = !o.fourScore; break;
        case 4: o.player2 = !o.player2; break;
        case 5: o.gameGenie = !o.gameGenie; break;
        default: break;
        }
    }
}

inline void captureBind(std::uint16_t key, const bool* keys) noexcept {
    if (!listening || bindBtn < 0) return;
    auto& o = FieldAmmoNesConfig::g;
    auto& binds = (bindPlayer == 1) ? o.p2 : o.p1;
    if (key != 0u) {
        const auto sc = static_cast<SDL_Scancode>(key & 0xFFu);
        if (sc != SDL_SCANCODE_UNKNOWN)
            binds[bindBtn].key = sc;
    }
    const auto& gp = FieldInput::state.gamepad;
    if (gp.connected) {
        if (gp.buttons & FieldInput::GP_SOUTH) binds[bindBtn].gamepadMask = FieldInput::GP_SOUTH;
        else if (gp.buttons & FieldInput::GP_EAST) binds[bindBtn].gamepadMask = FieldInput::GP_EAST;
        else if (gp.buttons & FieldInput::GP_START) binds[bindBtn].gamepadMask = FieldInput::GP_START;
        else if (gp.buttons & FieldInput::GP_BACK) binds[bindBtn].gamepadMask = FieldInput::GP_BACK;
        else if (gp.buttons & FieldInput::GP_DUP) binds[bindBtn].gamepadMask = FieldInput::GP_DUP;
        else if (gp.buttons & FieldInput::GP_DDOWN) binds[bindBtn].gamepadMask = FieldInput::GP_DDOWN;
        else if (gp.buttons & FieldInput::GP_DLEFT) binds[bindBtn].gamepadMask = FieldInput::GP_DLEFT;
        else if (gp.buttons & FieldInput::GP_DRIGHT) binds[bindBtn].gamepadMask = FieldInput::GP_DRIGHT;
    }
    if (keys) {
        for (int sc = 0; sc < SDL_SCANCODE_COUNT; ++sc) {
            if (keys[sc]) {
                binds[bindBtn].key = static_cast<SDL_Scancode>(sc);
                break;
            }
        }
    }
    listening = false;
    bindBtn = -1;
}

inline void open(bool padMapOnly = false) noexcept {
    active = true;
    padOnly = padMapOnly;
    page = padMapOnly ? 2 : 0;
    sel = 0;
    listening = false;
    FieldAmmoNesConfig::load();
}

inline void close(std::uint8_t* ram) noexcept {
    active = false;
    listening = false;
    FieldRtxGui::initTextMode(ram);
}

inline void pump(std::uint8_t* ram, std::uint16_t key, bool keyDown,
                 const bool* keys = nullptr) noexcept {
    if (!active) return;
    if (keyDown) {
        if (key == 0x011Bu) { close(ram); return; }
        if (key == 0x0F00u) { page = (page + 1) % kPages; sel = 0; paint(ram); return; }
        if (key == 0x4900u || key == 0x4800u) {
            if (sel > 0) --sel; paint(ram); return;
        }
        if (key == 0x5100u || key == 0x5000u) {
            if (sel < maxSel()) ++sel; paint(ram); return;
        }
        if (key == 0x3F00u) { FieldAmmoNesConfig::setDefaults(FieldAmmoNesConfig::g); paint(ram); return; }
        if (key == 0x3C00u) { FieldAmmoNesConfig::save(); paint(ram); return; }
        if (key == 0x1C0Du) {
            if (listening) captureBind(key, keys);
            else toggleSel();
            paint(ram);
            return;
        }
        if (listening) captureBind(key, keys);
    }
    paint(ram);
}

} // namespace FieldAmmoNesSetup

namespace FieldNes {

constexpr std::uint32_t FB_BASE = 0x000A0000u;
constexpr int FB_W = 320;
constexpr int FB_H = 200;

inline bool graphicsMode = true;
inline bool paused = false;
inline std::string romPath;
inline std::vector<std::uint8_t> prg;
inline std::vector<std::uint8_t> chr;
inline std::uint8_t ram[0x800]{};
inline std::uint8_t vram[0x2000]{};
inline std::uint16_t pc = 0;
inline std::uint8_t a = 0, x = 0, y = 0, sp = 0xFD;
inline std::uint8_t status = 0x24;
inline std::uint32_t frames = 0;
inline int mapper = 0;
inline int thermoSteps = 1;
inline std::uint32_t backendId = 0u;
inline std::uint8_t pad1 = 0;
inline FieldAmmoNesConfig::Options cfg;

inline bool flagC() noexcept { return (status & 0x01) != 0; }
inline bool flagZ() noexcept { return (status & 0x02) != 0; }
inline void setZ(std::uint8_t v) noexcept { if (v == 0) status |= 0x02; else status &= ~0x02u; }
inline void setN(std::uint8_t v) noexcept { if (v & 0x80) status |= 0x80; else status &= ~0x80u; }

inline std::uint8_t readMem(std::uint16_t addr) noexcept {
    if (addr < 0x2000) return ram[addr & 0x7FF];
    if (addr >= 0x2000 && addr < 0x4000) return 0;
    if (addr >= 0x6000 && addr < 0x8000) return 0;
    if (addr >= 0x8000 && !prg.empty()) {
        const std::size_t off = static_cast<std::size_t>(addr - 0x8000) % prg.size();
        return prg[off];
    }
    return 0;
}

inline void writeMem(std::uint16_t addr, std::uint8_t v) noexcept {
    if (addr < 0x2000) ram[addr & 0x7FF] = v;
}

inline std::uint8_t fetch() noexcept { return readMem(pc++); }

inline void push(std::uint8_t v) noexcept { writeMem(0x0100 + sp--, v); }
inline std::uint8_t pop() noexcept { return readMem(0x0100 + ++sp); }

inline void renderFrame(std::uint8_t* guestRam) noexcept {
    if (!guestRam || chr.empty()) return;
    FieldVga::setMode(0x13u, guestRam);
    for (int py = 0; py < FB_H; ++py) {
        const int ny = py * 240 / FB_H;
        for (int px = 0; px < FB_W; ++px) {
            const int nx = px * 256 / FB_W;
            const int tileX = nx / 8;
            const int tileY = ny / 8;
            const int nt = (tileY / 30) ? 0x2800 : 0x2000;
            const std::uint16_t ntAddr = static_cast<std::uint16_t>(nt + tileY % 30 * 32 + tileX);
            const std::uint8_t tile = vram[ntAddr & 0x1FFF];
            const int fineY = ny % 8;
            const int fineX = nx % 8;
            const std::size_t chrOff = static_cast<std::size_t>(tile) * 16 + fineY;
            std::uint8_t lo = chrOff < chr.size() ? chr[chrOff] : 0;
            std::uint8_t hi = (chrOff + 8) < chr.size() ? chr[chrOff + 8] : 0;
            const std::uint8_t bit = static_cast<std::uint8_t>(7 - fineX);
            const std::uint8_t pix = static_cast<std::uint8_t>(((hi >> bit) & 1) << 1 | ((lo >> bit) & 1));
            const std::uint32_t off = FB_BASE + static_cast<std::uint32_t>(py * FB_W + px);
            if (off < 0xC0000u) guestRam[off] = pix ? static_cast<std::uint8_t>(0x20 + (tile & 0x0F)) : 0;
        }
    }
}

inline void initPpuDemo() noexcept {
    std::memset(vram, 0, sizeof vram);
    for (int i = 0; i < 32 * 30; ++i)
        vram[0x2000 + i] = static_cast<std::uint8_t>(i & 0xFF);
}

inline bool loadRom(const char* path) {
    std::vector<std::uint8_t> data;
    if (!FieldAmmoFat::readFile(path, data) || data.size() < 16) return false;
    if (data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) return false;
    const int prgBanks = data[4];
    const int chrBanks = data[5];
    mapper = (data[7] & 0xF0) | (data[6] >> 4);
    const std::size_t off = 16;
    const std::size_t prgSz = static_cast<std::size_t>(prgBanks) * 16384;
    const std::size_t chrSz = static_cast<std::size_t>(chrBanks) * 8192;
    if (off + prgSz + chrSz > data.size()) return false;
    prg.assign(data.begin() + off, data.begin() + off + prgSz);
    chr.assign(data.begin() + off + prgSz, data.begin() + off + prgSz + chrSz);
    romPath = path;
    std::memset(ram, 0, sizeof ram);
    initPpuDemo();
    pc = static_cast<std::uint16_t>(readMem(0xFFFC) | (static_cast<std::uint16_t>(readMem(0xFFFD)) << 8));
    a = x = y = 0;
    sp = 0xFD;
    status = 0x24;
    frames = 0;
    return true;
}

inline int stepCpu() noexcept {
    const std::uint8_t op = fetch();
    switch (op) {
    case 0x00: return 2;
    case 0x4C: { const std::uint16_t lo = fetch(); const std::uint16_t hi = fetch();
        pc = static_cast<std::uint16_t>(lo | (hi << 8)); return 3; }
    case 0x20: { const std::uint16_t lo = fetch(); const std::uint16_t hi = fetch();
        push(static_cast<std::uint8_t>((pc >> 8) & 0xFF));
        push(static_cast<std::uint8_t>(pc & 0xFF));
        pc = static_cast<std::uint16_t>(lo | (hi << 8)); return 6; }
    case 0x60: pc = static_cast<std::uint16_t>(pop() | (static_cast<std::uint16_t>(pop()) << 8)); return 6;
    case 0xA9: a = fetch(); setZ(a); setN(a); return 2;
    case 0xA2: x = fetch(); setZ(x); setN(x); return 2;
    case 0xA0: y = fetch(); setZ(y); setN(y); return 2;
    case 0xAA: x = a; setZ(x); setN(x); return 2;
    case 0xA8: y = a; setZ(y); setN(y); return 2;
    case 0x8A: a = x; setZ(a); setN(a); return 2;
    case 0x98: a = y; setZ(a); setN(a); return 2;
    case 0xE8: ++x; setZ(x); setN(x); return 2;
    case 0xC8: ++y; setZ(y); setN(y); return 2;
    case 0xCA: --x; setZ(x); setN(x); return 2;
    case 0x88: --y; setZ(y); setN(y); return 2;
    case 0x85: writeMem(fetch(), a); return 3;
    case 0x86: writeMem(fetch(), x); return 3;
    case 0x84: writeMem(fetch(), y); return 3;
    case 0xA5: a = readMem(fetch()); setZ(a); setN(a); return 3;
    case 0xA6: x = readMem(fetch()); setZ(x); setN(x); return 3;
    case 0xA4: y = readMem(fetch()); setZ(y); setN(y); return 3;
    case 0xEA: return 2;
    case 0xD0: { const std::int8_t rel = static_cast<std::int8_t>(fetch());
        if (!flagZ()) pc = static_cast<std::uint16_t>(pc + rel); return 2; }
    case 0xF0: { const std::int8_t rel = static_cast<std::int8_t>(fetch());
        if (flagZ()) pc = static_cast<std::uint16_t>(pc + rel); return 2; }
    case 0x90: { const std::int8_t rel = static_cast<std::int8_t>(fetch());
        if (!flagC()) pc = static_cast<std::uint16_t>(pc + rel); return 2; }
    case 0xB0: { const std::int8_t rel = static_cast<std::int8_t>(fetch());
        if (flagC()) pc = static_cast<std::uint16_t>(pc + rel); return 2; }
    default: return 2;
    }
}

inline void runFrame(std::uint8_t* guestRam) noexcept {
    if (paused) return;
    const int burst = cfg.turboForce ? cfg.maxBurst : 1;
    for (int b = 0; b < burst; ++b) {
        for (int i = 0; i < 1000; ++i) stepCpu();
        ++frames;
    }
    if (cfg.soundOn) {
        const float t = static_cast<float>(frames) * 0.05f;
        float s = std::sin(t * 440.f * 6.2831853f / 60.f) * 0.15f;
        audioLevel = std::max(audioLevel, std::fabs(s));
        FieldNesAudio::pushSamples(&s, 1);
    }
    if (graphicsMode && guestRam) renderFrame(guestRam);
}

inline void applyConfig(const FieldAmmoNesConfig::Options& o) noexcept { cfg = o; }

inline void paintStatus(std::uint8_t* ram) noexcept {
    FieldRtxGui::initTextMode(ram);
    FieldRtxGui::fill(ram, 0, ' ', FieldRtxGui::ATTR_MENU);
    FieldRtxGui::text(ram, 0, 1, " AmmoNES — Esc quit  G graphics  P pause  R reset ",
        FieldRtxGui::ATTR_MENU, 76);
    char ln[80];
    std::snprintf(ln, sizeof ln, " ROM: %s  mapper=%d  PRG=%zu CHR=%zu ",
        romPath.c_str(), mapper, prg.size(), chr.size());
    FieldRtxGui::text(ram, 2, 2, ln, FieldRtxGui::ATTR_GOLD, 76);
    std::snprintf(ln, sizeof ln, " PC=%04X A=%02X X=%02X Y=%02X SP=%02X frames=%u %s",
        pc, a, x, y, sp, frames, paused ? "[PAUSED]" : "");
    FieldRtxGui::text(ram, 4, 2, ln, FieldRtxGui::ATTR_EDITOR, 76);
    FieldRtxGui::text(ram, 6, 2, " Z/X/Arrows/Enter — play  NES IMPORT for host ROMs ",
        FieldRtxGui::ATTR_DIM, 76);
    FieldRtxGui::text(ram, 22, 2, " Press G for mode 13h CHR tile view ",
        FieldRtxGui::ATTR_HELP, 60);
}

inline void open(std::uint8_t* ram, const char* path,
                 const FieldAmmoNesConfig::Options* opts = nullptr) {
    FieldAmmoNesConfig::load(cfg);
    if (opts) cfg = *opts;
    if (path && path[0] && !loadRom(path)) return;
    active = true;
    graphicsMode = path && path[0];
    paused = false;
    if (graphicsMode) {
        FieldVga::setMode(0x13u, ram);
        renderFrame(ram);
    } else
        paintStatus(ram);
}

inline void close(std::uint8_t* ram) noexcept {
    active = false;
    graphicsMode = false;
    paused = false;
    FieldNesAudio::shutdown();
    FieldVga::setMode(3u, ram);
}

inline void pump(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept {
    if (!active) return;
    if (keyDown) {
        if (key == 0x011Bu) { close(ram); return; }
        if ((key & 0xFFu) == 'p' || (key & 0xFFu) == 'P') { paused = !paused; }
        if ((key & 0xFFu) == 'r' || (key & 0xFFu) == 'R') {
            if (!romPath.empty()) loadRom(romPath.c_str());
        }
        if ((key & 0xFFu) == 'g' || (key & 0xFFu) == 'G') {
            graphicsMode = !graphicsMode;
            if (graphicsMode) { FieldVga::setMode(0x13u, ram); renderFrame(ram); }
            else paintStatus(ram);
            return;
        }
        if (key == 0x3920u) runFrame(ram);
    }
    if (graphicsMode) runFrame(ram);
    else paintStatus(ram);
}

inline void tick(std::uint8_t* ram, const bool*) noexcept {
    if (!active || !graphicsMode) return;
    runFrame(ram);
}

inline void packDataBus(std::uint32_t* bus) noexcept {
    if (!bus) return;
    bus[10] = active ? 1u : 0u;
    bus[11] = frames;
}

inline void ensureAudio(void* mix) noexcept {
    if (!mix || !cfg.soundOn) return;
    FieldNesAudio::mixer = static_cast<MIX_Mixer*>(mix);
    if (!FieldNesAudio::ready) FieldNesAudio::init(FieldNesAudio::mixer);
}

inline void saveState(int = 0) noexcept {}
inline void loadState(int = 0) noexcept {}

} // namespace FieldNes
