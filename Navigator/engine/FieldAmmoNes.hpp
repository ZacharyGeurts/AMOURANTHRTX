#pragma once

// AmmoNES — consolidated NES emulator (config, CLI, import, setup, audio, core).
// Silicon: CHIPS/Common (6502) + CHIPS/Nes (2C02, 2A03, mappers).

#include "AmmoNES/FieldNesCore.hpp"
#include "AmmoNES/FieldNesRomQuality.hpp"
#include "AmmoNES/FieldNesRomFixDialog.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldDos.hpp"
#include "FieldInput.hpp"
#include "FieldMix.hpp"
#include "FieldAosAppIdentity.hpp"
#include "FieldAosAppJournal.hpp"
#include "FieldAosAppSnapshot.hpp"

#include "FieldRtxApp.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRuntimeInfo.hpp"
#include "FieldVga.hpp"
#include "FieldAmmoEmuCommon.hpp"
#include "FieldEmuFileDialog.hpp"
#include "FieldWinApp.hpp"
#include "FieldWmNesMenu.hpp"
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

constexpr int kRecentMax = FieldWinFrame::A_FILE_RECENT_MAX;

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
    AmmoNes::AudioStyle audioStyle = AmmoNes::AudioStyle::Modern;
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
    int  romFixPrompt = 0;
    std::string lastRom = "C:\\NES\\";
    std::string recentRoms[kRecentMax]{};

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
    o.p1[static_cast<int>(NesBtn::A)].gamepadMask = FieldInput::GP_SOUTH;
    o.p1[static_cast<int>(NesBtn::B)].gamepadMask = FieldInput::GP_EAST;
    o.p1[static_cast<int>(NesBtn::Select)].gamepadMask = FieldInput::GP_BACK;
    o.p1[static_cast<int>(NesBtn::Start)].gamepadMask = FieldInput::GP_START;
    o.p1[static_cast<int>(NesBtn::Up)].gamepadMask = FieldInput::GP_DUP;
    o.p1[static_cast<int>(NesBtn::Down)].gamepadMask = FieldInput::GP_DDOWN;
    o.p1[static_cast<int>(NesBtn::Left)].gamepadMask = FieldInput::GP_DLEFT;
    o.p1[static_cast<int>(NesBtn::Right)].gamepadMask = FieldInput::GP_DRIGHT;
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
    else if (std::strcmp(key, "audio_style") == 0)
        o.audioStyle = iv != 0 ? AmmoNes::AudioStyle::Modern : AmmoNes::Authentic;
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
    else if (std::strcmp(key, "rom_fix_prompt") == 0) o.romFixPrompt = std::clamp(iv, 0, 2);
    else if (std::strcmp(key, "last_rom") == 0) o.lastRom = val;
    else if (std::strncmp(key, "recent_rom_", 11) == 0) {
        const int idx = std::atoi(key + 11);
        if (idx >= 0 && idx < kRecentMax) o.recentRoms[idx] = val;
    } else if (key[0] == 'p' && (key[1] == '1' || key[1] == '2')) {
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
        "audio_style=%d\n"
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
        "rom_fix_prompt=%d\n"
        "last_rom=%s\n",
        static_cast<int>(o.region),
        o.spriteLimit ? 1 : 0,
        o.renderSprites ? 1 : 0,
        o.renderBg ? 1 : 0,
        o.soundOn ? 1 : 0,
        o.soundQuality,
        static_cast<int>(o.audioStyle),
        o.masterVolume,
        o.sq1Vol, o.sq2Vol, o.triVol, o.noiseVol, o.pcmVol,
        o.thermoGovernor ? 1 : 0,
        o.maxBurst,
        o.turboForce ? 1 : 0,
        o.fourScore ? 1 : 0,
        o.player2 ? 1 : 0,
        o.gameGenie ? 1 : 0,
        o.saveSlot,
        o.romFixPrompt,
        o.lastRom.c_str());
    for (int i = 0; i < kRecentMax && n < static_cast<int>(sizeof buf) - 96; ++i) {
        n += std::snprintf(buf + n, sizeof buf - static_cast<std::size_t>(n),
            "recent_rom_%d=%s\n", i, o.recentRoms[i].c_str());
    }
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

inline std::string basename(const std::string& path) {
    const std::size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

inline void addRecentRom(Options& o, const std::string& path) {
    if (path.empty()) return;
    std::string slots[kRecentMax]{};
    int n = 0;
    slots[n++] = path;
    for (int i = 0; i < kRecentMax; ++i) {
        if (o.recentRoms[i].empty() || o.recentRoms[i] == path) continue;
        if (n < kRecentMax) slots[n++] = o.recentRoms[i];
    }
    for (int i = 0; i < kRecentMax; ++i)
        o.recentRoms[i] = (i < n) ? slots[i] : "";
}

inline void clearRecentRoms(Options& o) noexcept {
    for (int i = 0; i < kRecentMax; ++i) o.recentRoms[i].clear();
}

inline int recentRomCount(const Options& o) noexcept {
    int n = 0;
    for (int i = 0; i < kRecentMax; ++i)
        if (!o.recentRoms[i].empty()) ++n;
    return n;
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
        if (ieq(a, "--chippy") || ieq(a, "--authentic")) {
            req.opts.audioStyle = AmmoNes::Authentic; continue;
        }
        if (ieq(a, "--modern")) {
            req.opts.audioStyle = AmmoNes::AudioStyle::Modern; continue;
        }
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
    "  --chippy / --modern        APU style (authentic vs smooth)\r\n"
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

inline bool inesPayloadValid(const std::vector<std::uint8_t>& data) noexcept {
    if (data.size() < 16) return false;
    if (data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) return false;
    std::size_t off = 16u;
    if (data[6] & 4u) off += 512u;
    const std::size_t prgSz = static_cast<std::size_t>(data[4]) * 16384u;
    if (prgSz < 4u || off + prgSz > data.size()) return false;
    const std::uint8_t lo = data[off + prgSz - 4u];
    const std::uint8_t hi = data[off + prgSz - 3u];
    const std::uint16_t reset = static_cast<std::uint16_t>(
        lo | (static_cast<std::uint16_t>(hi) << 8));
    return reset >= 0x8000u;
}

inline bool sameRomLeaf(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

inline bool readHostRomByName(const char* leaf, std::vector<std::uint8_t>& out) {
    if (!leaf || !leaf[0]) return false;
    const std::string want(leaf);
    for (const auto& root : scanRoots()) {
        if (!std::filesystem::exists(root)) continue;
        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
             it != std::filesystem::recursive_directory_iterator(); ++it) {
            if (ec) break;
            if (!it->is_regular_file() || !isNesRom(it->path())) continue;
            if (!sameRomLeaf(it->path().filename().string(), want)) continue;
            std::ifstream in(it->path(), std::ios::binary);
            if (!in) continue;
            out.assign(std::istreambuf_iterator<char>(in), {});
            if (inesPayloadValid(out)) return true;
            out.clear();
        }
    }
    return false;
}

inline bool romMagicValid(const std::vector<std::uint8_t>& data) noexcept {
    return data.size() >= 16 && data[0] == 'N' && data[1] == 'E' && data[2] == 'S' && data[3] == 0x1A;
}

inline bool readRomBytesRaw(const char* path, std::vector<std::uint8_t>& out) {
    out.clear();
    if (!path || !path[0]) return false;
    const char* leaf = std::strrchr(path, '\\');
    if (!leaf) leaf = std::strrchr(path, '/');
    for (const auto& root : scanRoots()) {
        if (!std::filesystem::exists(root)) continue;
        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
             it != std::filesystem::recursive_directory_iterator(); ++it) {
            if (ec) break;
            if (!it->is_regular_file() || !isNesRom(it->path())) continue;
            if (!sameRomLeaf(it->path().filename().string(), leaf ? leaf + 1 : path)) continue;
            std::ifstream in(it->path(), std::ios::binary);
            if (!in) continue;
            out.assign(std::istreambuf_iterator<char>(in), {});
            if (romMagicValid(out)) {
                FieldAmmoVfs::writePath(path, out.data(), out.size());
                return true;
            }
            out.clear();
        }
    }
    out.clear();
    if (FieldAmmoVfs::readPath(path, out) && romMagicValid(out)) return true;
    out.clear();
    return false;
}

inline bool readRomBytes(const char* path, std::vector<std::uint8_t>& out) {
    if (!readRomBytesRaw(path, out)) return false;
    if (inesPayloadValid(out)) return true;
    out.clear();
    return false;
}

inline bool romReadable(const char* path) {
    if (!path || !path[0]) return false;
    std::vector<std::uint8_t> data;
    return readRomBytes(path, data);
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

inline int ensureImported() {
    if (!ensureNesDir()) return 0;
    std::string existing;
    if (findAnyRom(existing)) {
        std::vector<std::uint8_t> probe;
        if (FieldAmmoVfs::readPath(existing.c_str(), probe) && inesPayloadValid(probe))
            return 0;
        FieldAmmoVfs::deletePath(existing.c_str());
    }
    std::vector<std::string> copied, skipped;
    return importAll(copied, skipped);
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
inline bool overlay = false;
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

inline const char* audioStyleLabel(AmmoNes::AudioStyle s) noexcept {
    return s == AmmoNes::AudioStyle::Modern ? "Modern (smooth)" : "Chippy (authentic)";
}

inline void paintAudio(std::uint8_t* ram, const FieldAmmoNesConfig::Options& o) noexcept {
    char v[40];
    paintRow(ram, 6, 4, "Sound", o.soundOn ? "On" : "Muted", sel == 0);
    paintRow(ram, 7, 4, "APU style", audioStyleLabel(o.audioStyle), sel == 1);
    std::snprintf(v, sizeof v, "%d", o.soundQuality);
    paintRow(ram, 8, 4, "Quality 0-2", v, sel == 2);
    std::snprintf(v, sizeof v, "%d", o.masterVolume);
    paintRow(ram, 9, 4, "Master volume", v, sel == 3);
    std::snprintf(v, sizeof v, "Sq1:%d Sq2:%d Tri:%d", o.sq1Vol, o.sq2Vol, o.triVol);
    paintRow(ram, 10, 4, "Channels", v, sel == 4);
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
    FieldRtxApp::begin(ram, 11u);
    auto& ui = FieldRtxApp::ui;
    ui.panel(8, 8, 1016, 1016);
    char title[72];
    std::snprintf(title, sizeof title, "AmmoNES Setup — %s", kPageTitle(page));
    ui.label(24, 20, 640, 52, title, 1);
    ui.dropdown(660, 18, 1000, 56, kPageTitle(page), page);
    FieldAosAppIdentity::paintAbout(ui, FieldAosAppIdentity::AppId::NesSetup, 52, 164);
    FieldAosAppJournal::paintRecent(ui, FieldAosAppJournal::journalProgId, 172, 240);
    FieldAosAppSnapshot::apply(FieldAosAppIdentity::AppId::NesSetup);
    char tabs[80];
    std::snprintf(tabs, sizeof tabs, "Page %d/%d  Tab next  Enter toggle  F2 save  F3 defaults",
        page + 1, kPages);
    ui.label(24, 176, 1000, 204, tabs, 0);
    ui.label(24, 208, 1000, 236, FieldRuntimeInfo::masterStatusLine(), 0);

    char v[64];
    int y = 248;
    auto row = [&](const char* label, const char* val, int idx) {
        const bool hi = sel == idx;
        char line[96];
        std::snprintf(line, sizeof line, "%s%s", hi ? "> " : "  ", label);
        ui.label(32, y, 360, y + 32, line, hi ? 1 : 0);
        ui.label(380, y, 960, y + 32, val, hi ? 1 : 0);
        y += 40;
    };

    switch (page) {
    case 0:
        std::snprintf(v, sizeof v, "%s", FieldAmmoNesConfig::regionName(o.region));
        row("Region", v, 0);
        row("Sprite limit", o.spriteLimit ? "On (accurate)" : "Off (no cap)", 1);
        row("Sprites", o.renderSprites ? "Show" : "Hide", 2);
        row("Background", o.renderBg ? "Show" : "Hide", 3);
        break;
    case 1:
        row("Sound", o.soundOn ? "On" : "Muted", 0);
        row("APU style", audioStyleLabel(o.audioStyle), 1);
        std::snprintf(v, sizeof v, "%d", o.soundQuality);
        row("Quality 0-2", v, 2);
        std::snprintf(v, sizeof v, "%d", o.masterVolume);
        row("Master volume", v, 3);
        std::snprintf(v, sizeof v, "Sq1:%d Sq2:%d Tri:%d", o.sq1Vol, o.sq2Vol, o.triVol);
        row("Channels", v, 4);
        break;
    case 2: case 3: {
        const auto& binds = (page == 3) ? o.p2 : o.p1;
        for (int i = 0; i < static_cast<int>(FieldAmmoNesConfig::NesBtn::Count); ++i) {
            char kn[24];
            scancodeName(binds[i].key, kn, sizeof kn);
            std::snprintf(v, sizeof v, "%s  GP:0x%02X", kn, binds[i].gamepadMask);
            const bool listen = listening && bindPlayer == (page == 3 ? 1 : 0) && bindBtn == i;
            row(FieldAmmoNesConfig::btnLabel(static_cast<FieldAmmoNesConfig::NesBtn>(i)),
                listen ? "Press key/button..." : v, i);
        }
        break;
    }
    case 4:
        row("Thermo governor", o.thermoGovernor ? "On" : "Off", 0);
        std::snprintf(v, sizeof v, "%d", o.maxBurst);
        row("Max burst", v, 1);
        row("Turbo force", o.turboForce ? "On" : "Off", 2);
        row("Four-score", o.fourScore ? "On" : "Off", 3);
        row("Player 2", o.player2 ? "On" : "Off", 4);
        row("Game Genie", o.gameGenie ? "On" : "Off", 5);
        std::snprintf(v, sizeof v, "%.48s", o.lastRom.c_str());
        row("Last ROM", v, 6);
        break;
    default:
        ui.label(32, 128, 980, 156,
            "NES HELP  SETUP  PAD  IMPORT  [rom] [flags]", 0);
        ui.label(32, 168, 980, 196, "--ntsc --pal --dendy  --turbo  --burst=3", 0);
        ui.label(32, 208, 980, 236, "--volume=256  --quality=1  --no-sprite-limit", 0);
        break;
    }

    FieldAosAppSnapshot::refresh(FieldAosAppIdentity::AppId::NesSetup);
    FieldRtxApp::finish(ram);
}

inline int maxSel() noexcept {
    switch (page) {
    case 0: return 3;
    case 1: return 4;
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
        case 1: o.audioStyle = (o.audioStyle == AmmoNes::AudioStyle::Modern)
            ? AmmoNes::Authentic : AmmoNes::AudioStyle::Modern; break;
        case 2: o.soundQuality = (o.soundQuality + 1) % 3; break;
        case 3: o.masterVolume = std::min(512, o.masterVolume + 32); break;
        case 4: o.sq1Vol = std::min(512, o.sq1Vol + 16); break;
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

inline void open(bool padMapOnly = false, bool asOverlay = false) noexcept {
    active = true;
    overlay = asOverlay;
    padOnly = padMapOnly;
    page = padMapOnly ? 2 : 0;
    sel = 0;
    listening = false;
    FieldAmmoNesConfig::load();
}

inline void close(std::uint8_t* ram) noexcept {
    const bool wasOverlay = overlay;
    active = false;
    overlay = false;
    listening = false;
    if (!wasOverlay) {
        FieldRtxWidgets::clearRam(ram);
        if (!FieldNes::active) FieldWinApp::reset();
    }
}

inline void pump(std::uint8_t* ram, std::uint16_t key, bool keyDown,
                 const bool* keys = nullptr) noexcept {
    if (!active) return;
    if (keyDown) {
        if (key == 0x011Bu) {
            const bool wasOverlay = overlay;
            active = false;
            overlay = false;
            listening = false;
            if (!wasOverlay) {
                FieldRtxWidgets::clearRam(ram);
                if (!FieldNes::active) FieldWinApp::reset();
            }
            return;
        }
        if (key == 0x0F00u) { page = (page + 1) % kPages; sel = 0; paint(ram); return; }
        if (key == 0x4900u || key == 0x4800u) {
            if (sel > 0) --sel;
            paint(ram);
            return;
        }
        if (key == 0x5100u || key == 0x5000u) {
            if (sel < maxSel()) ++sel;
            paint(ram);
            return;
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

namespace FieldNesMenu {

inline FieldRtxGui::MenuItem recentItems[FieldWinFrame::A_FILE_RECENT_MAX + 1]{};
inline char recentLabels[FieldWinFrame::A_FILE_RECENT_MAX + 1][52]{};
inline int recentMenuCount = 1;

inline FieldRtxGui::MenuItem optionsItems[2] = {
    {FieldRtxGui::BOX_X, "Settings...", 'S', FieldWinFrame::A_OPT_SETTINGS},
    {FieldRtxGui::BOX_X, "Controls...", 'C', FieldWinFrame::A_OPT_CONTROLS},
};

inline FieldRtxGui::Menu menus[5]{};
constexpr int kMenuCount = 5;

inline void syncRecent(const FieldAmmoNesConfig::Options& o) noexcept {
    recentMenuCount = 0;
    for (int i = 0; i < FieldAmmoNesConfig::kRecentMax; ++i) {
        if (o.recentRoms[i].empty()) continue;
        const std::string base = FieldAmmoNesConfig::basename(o.recentRoms[i]);
        std::snprintf(recentLabels[recentMenuCount], sizeof recentLabels[0], "%.48s",
            base.c_str());
        recentItems[recentMenuCount] = {
            FieldRtxGui::ICO_DISK,
            recentLabels[recentMenuCount],
            static_cast<char>(recentMenuCount < 9 ? '1' + recentMenuCount : 0),
            FieldWinFrame::A_FILE_RECENT_BASE + recentMenuCount
        };
        ++recentMenuCount;
    }
    if (recentMenuCount == 0) {
        std::snprintf(recentLabels[0], sizeof recentLabels[0], "(No recent ROMs)");
        recentItems[0] = {FieldRtxGui::BOX_X, recentLabels[0], 0, 0};
        recentMenuCount = 1;
    } else if (recentMenuCount < FieldWinFrame::A_FILE_RECENT_MAX + 1) {
        std::snprintf(recentLabels[recentMenuCount], sizeof recentLabels[0], "Clear Recent");
        recentItems[recentMenuCount] = {
            FieldRtxGui::BOX_X, recentLabels[recentMenuCount], 'C',
            FieldWinFrame::A_FILE_CLEAR_RECENT
        };
        ++recentMenuCount;
    }
}

inline void sync(const FieldAmmoNesConfig::Options& o) noexcept {
    syncRecent(o);
    menus[0] = {FieldRtxGui::ICO_FOLDER, "File", FieldWinFrame::kNesFileStatic,
        FieldWinFrame::kNesFileStaticCount};
    menus[1] = {FieldRtxGui::ICO_DISK, "Recent", recentItems, recentMenuCount};
    menus[2] = {FieldRtxGui::BOX_X, "Emulation", FieldWinFrame::kNesEmuStatic,
        FieldWinFrame::kNesEmuStaticCount};
    menus[3] = {FieldRtxGui::BOX_X, "Options", optionsItems, 2};
    menus[4] = {FieldRtxGui::ICO_BOOK, "Help", FieldWinFrame::kNesHelpStatic,
        FieldWinFrame::kNesHelpStaticCount};
}

inline void packWmRecent(std::uint8_t* ram, const FieldAmmoNesConfig::Options& o) noexcept {
    char labelBuf[FieldAmouranthHudRam::WM_DROPDOWN_MAX][48]{};
    const char* labels[FieldAmouranthHudRam::WM_DROPDOWN_MAX + 1]{};
    int actions[FieldAmouranthHudRam::WM_DROPDOWN_MAX + 1]{};
    int n = 0;
    for (int i = 0; i < FieldAmmoNesConfig::kRecentMax; ++i) {
        if (o.recentRoms[i].empty()) continue;
        std::snprintf(labelBuf[n], sizeof labelBuf[0], "%.46s",
            FieldAmmoNesConfig::basename(o.recentRoms[i]).c_str());
        labels[n] = labelBuf[n];
        actions[n] = FieldWinFrame::A_FILE_RECENT_BASE + n;
        ++n;
    }
    if (n == 0) {
        FieldWmNesMenu::packEmptyRecent(ram);
        return;
    }
    if (n < FieldAmouranthHudRam::WM_DROPDOWN_MAX) {
        labels[n] = "Clear Recent";
        actions[n] = FieldWinFrame::A_FILE_CLEAR_RECENT;
        ++n;
    }
    FieldWmNesMenu::packDropdown(ram, labels, actions, n);
}

} // namespace FieldNesMenu

namespace FieldNes {

constexpr std::uint32_t FB_BASE = 0x000A0000u;
constexpr int FB_W = 320;
constexpr int FB_H = 200;

inline AmmoNes::State chip{};

inline bool graphicsMode = true;
inline bool optionsOpen = false;
inline bool aboutOpen = false;
inline bool paused = false;
inline bool menuTurbo = false;
inline bool menuUnlimited = false;
inline std::string romPath;
inline std::vector<std::uint8_t>& prg = chip.cart.prg;
inline std::vector<std::uint8_t>& chr = chip.cart.chr;
inline std::uint8_t* ram = chip.ram;
inline int& mapper = chip.cart.mapper;
inline int& prgBank = chip.map.prgBank;
inline int thermoSteps = 1;
inline std::uint32_t backendId = 0u;
inline std::uint8_t& pad1 = chip.pad1;
inline std::uint8_t& pad2 = chip.pad2;
inline bool vgaMode13 = false;
inline bool romLoaded = false;
inline bool romFixPending = false;
inline FieldAmmoNesConfig::Options cfg;

inline std::uint16_t& pc = chip.cpu.pc;
inline std::uint8_t& a = chip.cpu.a;
inline std::uint8_t& x = chip.cpu.x;
inline std::uint8_t& y = chip.cpu.y;
inline std::uint8_t& sp = chip.cpu.sp;
inline std::uint8_t& status = chip.cpu.status;
inline std::uint32_t frames = 0;

inline void ensurePpuDisplay() noexcept {
    if ((chip.ppu.mask & 0x18) == 0) {
        chip.ppu.ctrl = 0x88;
        chip.ppu.mask = 0x1Eu;
    }
}

inline void applyConfig(const FieldAmmoNesConfig::Options& o) noexcept {
    cfg = o;
    chip.spriteLimit = o.spriteLimit;
    chip.region = static_cast<FieldChips::Nes::Region>(static_cast<int>(o.region));
    AmmoNes::applyApuConfig(chip, o.audioStyle, o.soundQuality, FieldNesAudio::RATE);
}

inline void renderFrame(std::uint8_t* guestRam) noexcept {
    if (!guestRam) return;
    if (chr.empty())
        AmmoNes::seedChrFromPrg(chip);
    if (chr.empty()) return;
    ensurePpuDisplay();
    if (!vgaMode13) {
        FieldVga::setMode(FieldVga::MODE_VGA_13, guestRam);
        vgaMode13 = true;
    }
    guestRam[0x449u] = FieldVga::MODE_VGA_13;
    FieldVga::mode = FieldVga::MODE_VGA_13;
    AmmoNes::installNesVgaPalette(guestRam);
    AmmoNes::renderFrame(chip, guestRam, FieldVga::VGA_FB, FB_W, FB_H,
        cfg.renderBg, cfg.renderSprites);
}

inline bool loadInesFromData(const std::vector<std::uint8_t>& data) noexcept {
    if (!AmmoNes::loadInes(chip, data.data(), data.size())) return false;
    if (chip.cart.chr.empty())
        AmmoNes::seedChrFromPrg(chip);
    applyConfig(cfg);
    vgaMode13 = false;
    chip.ppu.frame = 0;
    romLoaded = true;
    return true;
}

inline void warmupAfterLoad(std::uint8_t* guestRam) noexcept;

inline bool loadRom(const char* path, std::uint8_t* guestRam = nullptr) {
    std::vector<std::uint8_t> data;
    if (!FieldNesImport::readRomBytesRaw(path, data)) return false;
    romPath = path;
    if (cfg.romFixPrompt != 2) {
        const auto report = FieldNesRomQuality::analyzeVector(data);
        if (report.hasFixable() && !report.blocking()) {
            const bool ask = cfg.romFixPrompt == 0 && report.needsConfirm();
            const bool autoFix = cfg.romFixPrompt == 1
                || (cfg.romFixPrompt == 0 && !report.needsConfirm());
            if (autoFix) {
                std::string summary;
                FieldNesRomQuality::applyFixes(data, report, summary);
                FieldAmmoVfs::writePath(path, data.data(), data.size());
            } else if (ask) {
                FieldNesRomFix::open(path, std::move(data), report);
                romFixPending = true;
                paused = true;
                if (guestRam) FieldNesRomFix::paint(guestRam);
                return true;
            }
        }
    }
    if (!loadInesFromData(data)) return false;
    FieldAmmoNesConfig::addRecentRom(cfg, romPath);
    FieldAmmoNesConfig::save(cfg);
    FieldNesMenu::sync(cfg);
    warmupAfterLoad(guestRam);
    return true;
}

inline int stepCpu() noexcept { return AmmoNes::stepCpu(chip); }

inline void pushFrameAudio() noexcept {
    if (!cfg.soundOn) return;
    constexpr int kSamplesPerFrame = 44100 / 60;
    float block[kSamplesPerFrame]{};
    const int n = AmmoNes::renderAudioFrame(chip, block, kSamplesPerFrame);
    const float gain = static_cast<float>(cfg.masterVolume) / 512.f;
    for (int i = 0; i < n; ++i)
        block[i] *= gain;
    FieldNesAudio::pushSamples(block, static_cast<std::uint32_t>(n));
}

inline std::uint8_t buildPad(const FieldAmmoNesConfig::PadBinding* binds,
        const bool* keys) noexcept {
    std::uint8_t pad = 0;
    const auto& gp = FieldInput::state.gamepad;
    for (int i = 0; i < static_cast<int>(FieldAmmoNesConfig::NesBtn::Count); ++i) {
        bool pressed = false;
        if (keys && binds[i].key != SDL_SCANCODE_UNKNOWN)
            pressed = keys[binds[i].key];
        if (!pressed && gp.connected && binds[i].gamepadMask != 0u)
            pressed = (gp.buttons & binds[i].gamepadMask) != 0;
        if (pressed)
            pad |= static_cast<std::uint8_t>(1u << i);
    }
    return pad;
}

inline void applyPadFromInput(const bool* keys) noexcept {
    pad1 = buildPad(cfg.p1, keys);
    if (cfg.player2 || cfg.fourScore)
        pad2 = buildPad(cfg.p2, keys);
    else
        pad2 = 0;
}

inline int frameBurst() noexcept {
    if (menuUnlimited) return 12;
    if (menuTurbo || cfg.turboForce) return std::max(1, cfg.maxBurst);
    return 1;
}

inline void runFrame(std::uint8_t* guestRam) noexcept {
    if (paused) return;
    const int burst = frameBurst();
    const int thermoExtra = (!menuUnlimited && cfg.thermoGovernor) ? thermoSteps * 4 : 0;
    const auto tier = AmmoNes::tierFromEnv();
    for (int b = 0; b < burst; ++b)
        AmmoNes::runAccuracyFrame(chip, tier, thermoExtra);
    frames = static_cast<std::uint32_t>(chip.ppu.frame);
    pushFrameAudio();
    if (graphicsMode && guestRam) renderFrame(guestRam);
}

inline void advanceOneFrame(std::uint8_t* guestRam) noexcept {
    const bool wasPaused = paused;
    paused = false;
    AmmoNes::runFrameCpu(chip, 29780);
    frames = static_cast<std::uint32_t>(chip.ppu.frame);
    pushFrameAudio();
    if (graphicsMode && guestRam) renderFrame(guestRam);
    paused = wasPaused;
}

inline void ensureAudio(void* mix) noexcept {
    if (!mix || !cfg.soundOn) return;
    FieldNesAudio::mixer = static_cast<MIX_Mixer*>(mix);
    if (!FieldNesAudio::ready) FieldNesAudio::init(FieldNesAudio::mixer);
}

inline void warmupAfterLoad(std::uint8_t* guestRam) noexcept {
    if (!graphicsMode || !guestRam) return;
    if (FieldWinApp::useGpuChrome()) {
        FieldRtxApp::ui.clear();
        FieldRtxApp::finish(guestRam);
    }
    FieldVga::setMode(FieldVga::MODE_VGA_13, guestRam);
    ensurePpuDisplay();
    for (int f = 0; f < 90; ++f) runFrame(guestRam);
    if (cfg.soundOn) ensureAudio(FieldNesAudio::mixer);
}

inline void buildStatusLine(char* buf, std::size_t n) noexcept {
    if (!buf || n < 2u) return;
    if (!romLoaded) {
        std::snprintf(buf, n, " No ROM loaded — File>Open or pick Recent ");
        return;
    }
    const char* run = paused ? "Paused" : "Running";
    const char* spd = menuUnlimited ? "Unlimited"
        : (menuTurbo || cfg.turboForce) ? "Turbo" : "1x";
    const char* snd = cfg.soundOn ? "Sound" : "Muted";
    const std::string name = FieldAmmoNesConfig::basename(romPath);
    std::snprintf(buf, n, " %s  %s  %s  %s  %s(%d) ",
        name.c_str(), run, spd, snd, AmmoNes::mapperName(mapper), mapper);
}

inline void refreshChrome(std::uint8_t* ram) noexcept {
    char status[80];
    buildStatusLine(status, sizeof status);
    FieldNesMenu::sync(cfg);
    FieldNesMenu::packWmRecent(ram, cfg);
    FieldWinApp::menus = FieldNesMenu::menus;
    FieldWinApp::menuCount = FieldNesMenu::kMenuCount;
    FieldWinApp::repaintChrome(ram, " AmmoNES ", status);
}

inline void paintBlackClient(std::uint8_t* ram) noexcept {
    if (!ram) return;
    if (FieldWinApp::useGpuChrome()) {
        FieldRtxApp::ui.clear();
        FieldRtxApp::ui.appId = static_cast<std::uint8_t>(FieldAosAppIdentity::AppId::Nes);
        FieldRtxApp::finish(ram);
        return;
    }
    FieldWinFrame::paintClientClear(ram, FieldWinApp::layout, 0x07u);
}

inline void paintWelcome(std::uint8_t* ram) noexcept {
    if (!ram) return;
    if (aboutOpen && !FieldWinApp::useGpuChrome()) {
        const FieldWinFrame::Layout& L = FieldWinApp::layout;
        FieldWinFrame::paintClientClear(ram, L, 0x07u);
        const int c = L.clientC0 + 2;
        int r = L.clientR0 + 2;
        FieldRtxGui::text(ram, r, c, " AmmoNES — Nintendo Entertainment System Emulator ",
            FieldRtxGui::ATTR_GOLD, L.clientCols - 4);
        FieldRtxGui::text(ram, r + 2, c, " Accurate 6502 / 2C02 / 2A03 emulation with ROM quality repair. ",
            FieldRtxGui::ATTR_EDITOR, L.clientCols - 4);
        FieldRtxGui::text(ram, r + 4, c, " File>Open loads .nes ROMs. Recent lists your last picks. ",
            FieldRtxGui::ATTR_HELP, L.clientCols - 4);
        FieldRtxGui::text(ram, r + 6, c, " Emulation>Pause/Turbo/Unlimited  Options>Settings/Controls ",
            FieldRtxGui::ATTR_HELP, L.clientCols - 4);
        FieldRtxGui::text(ram, r + 9, c, " Press any key to return... ",
            FieldRtxGui::ATTR_DIM, L.clientCols - 4);
        return;
    }
    paintBlackClient(ram);
}

inline void paintIdle(std::uint8_t* ram) noexcept {
    refreshChrome(ram);
    if (!graphicsMode || !romLoaded) paintWelcome(ram);
}

inline void close(std::uint8_t* ram) noexcept;

inline void beginChrome(std::uint8_t* ram) noexcept {
    FieldNesMenu::sync(cfg);
    char status[80];
    buildStatusLine(status, sizeof status);
    FieldWinApp::beginEmuMenus(ram, " AmmoNES ", status,
        FieldNesMenu::menus, FieldNesMenu::kMenuCount);
}

inline bool loadRecent(int slot, std::uint8_t* ram) noexcept {
    if (slot < 0 || slot >= FieldAmmoNesConfig::kRecentMax) return false;
    const std::string& path = cfg.recentRoms[slot];
    if (path.empty()) return false;
    if (!loadRom(path.c_str(), ram)) {
        paintIdle(ram);
        return true;
    }
    cfg.lastRom = path;
    FieldAmmoNesConfig::save(cfg);
    graphicsMode = !romFixPending && !chr.empty();
    if (!romFixPending) paused = false;
    refreshChrome(ram);
    return true;
}

inline bool handleMenu(int action, std::uint8_t* ram) noexcept {
    if (action >= FieldWinFrame::A_FILE_RECENT_BASE
            && action < FieldWinFrame::A_FILE_RECENT_BASE + FieldWinFrame::A_FILE_RECENT_MAX)
        return loadRecent(action - FieldWinFrame::A_FILE_RECENT_BASE, ram);
    switch (action) {
    case 0:
        return true;
    case FieldWinFrame::A_FILE_OPEN:
        FieldAmmoEmu::openRomDialog(FieldAmmoEmu::Kind::Nes, cfg.lastRom.c_str());
        FieldEmuFileDialog::paint(ram);
        paused = true;
        return true;
    case FieldWinFrame::A_FILE_CLEAR_RECENT:
        FieldAmmoNesConfig::clearRecentRoms(cfg);
        FieldAmmoNesConfig::save(cfg);
        refreshChrome(ram);
        paintWelcome(ram);
        return true;
    case FieldWinFrame::A_EMU_PAUSE:
        paused = true;
        refreshChrome(ram);
        return true;
    case FieldWinFrame::A_EMU_RESUME:
        paused = false;
        aboutOpen = false;
        refreshChrome(ram);
        return true;
    case FieldWinFrame::A_EMU_RESET:
        if (!romPath.empty()) {
            loadRom(romPath.c_str(), ram);
            graphicsMode = !romFixPending && !chr.empty();
            if (!romFixPending) paused = false;
        }
        refreshChrome(ram);
        return true;
    case FieldWinFrame::A_EMU_FRAME:
        if (romLoaded) advanceOneFrame(ram);
        refreshChrome(ram);
        return true;
    case FieldWinFrame::A_EMU_TURBO:
        menuTurbo = !menuTurbo;
        if (menuTurbo) menuUnlimited = false;
        refreshChrome(ram);
        return true;
    case FieldWinFrame::A_EMU_UNLIMITED:
        menuUnlimited = !menuUnlimited;
        if (menuUnlimited) menuTurbo = false;
        refreshChrome(ram);
        return true;
    case FieldWinFrame::A_EMU_MUTE:
        cfg.soundOn = !cfg.soundOn;
        FieldAmmoNesConfig::g = cfg;
        FieldAmmoNesConfig::save(cfg);
        if (!cfg.soundOn) FieldNesAudio::shutdown();
        refreshChrome(ram);
        return true;
    case FieldWinFrame::A_OPT_SETTINGS:
        FieldAmmoNesSetup::open(false, true);
        optionsOpen = true;
        paused = true;
        FieldAmmoNesSetup::paint(ram);
        return true;
    case FieldWinFrame::A_OPT_CONTROLS:
        FieldAmmoNesSetup::open(true, true);
        optionsOpen = true;
        paused = true;
        FieldAmmoNesSetup::paint(ram);
        return true;
    case FieldWinFrame::A_HELP_ABOUT:
        aboutOpen = true;
        paused = true;
        paintWelcome(ram);
        return true;
    case FieldWinFrame::A_FILE_EXIT:
        close(ram);
        return true;
    default:
        return false;
    }
}

inline bool pumpOverlays(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept {
    if (FieldNesRomFix::active) {
        bool choseFix = false;
        std::vector<std::uint8_t> fixed;
        std::string path;
        if (FieldNesRomFix::pump(ram, key, keyDown, choseFix, fixed, path)) {
            romFixPending = false;
            if (!path.empty() && !fixed.empty()) {
                if (choseFix)
                    FieldAmmoVfs::writePath(path.c_str(), fixed.data(), fixed.size());
                romPath = path;
                if (loadInesFromData(fixed)) {
                    FieldAmmoNesConfig::addRecentRom(cfg, romPath);
                    FieldAmmoNesConfig::save(cfg);
                    FieldNesMenu::sync(cfg);
                    graphicsMode = !chr.empty();
                    paused = false;
                    warmupAfterLoad(ram);
                    refreshChrome(ram);
                } else {
                    paused = false;
                    paintIdle(ram);
                }
            } else
                paused = false;
            return true;
        }
        return true;
    }
    if (FieldEmuFileDialog::active) {
        if (FieldAmmoEmu::pumpFileDialog(ram, key, keyDown)) {
            if (!FieldEmuFileDialog::active && !FieldEmuFileDialog::result.empty()) {
                if (loadRom(FieldEmuFileDialog::result.c_str(), ram)) {
                    cfg.lastRom = FieldEmuFileDialog::result;
                    FieldAmmoNesConfig::save(cfg);
                    graphicsMode = !romFixPending && !chr.empty();
                    refreshChrome(ram);
                }
                if (!romFixPending) paused = false;
            } else if (!FieldEmuFileDialog::active)
                paused = false;
            return true;
        }
    }
    if (optionsOpen && FieldAmmoNesSetup::active) {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        FieldAmmoNesSetup::pump(ram, key, keyDown, keys);
        if (!FieldAmmoNesSetup::active) {
            optionsOpen = false;
            paused = false;
            cfg = FieldAmmoNesConfig::g;
            applyConfig(cfg);
            if (graphicsMode) renderFrame(ram);
            else paintIdle(ram);
            refreshChrome(ram);
        }
        return true;
    }
    return false;
}

inline void open(std::uint8_t* ram, const char* path,
                 const FieldAmmoNesConfig::Options* opts = nullptr,
                 bool openOptions = false, bool padOnly = false) {
    FieldWinApp::reset();
    menuTurbo = false;
    menuUnlimited = false;
    aboutOpen = false;
    FieldAmmoNesConfig::load(cfg);
    if (opts) cfg = *opts;
    beginChrome(ram);
    active = true;
    graphicsMode = false;
    romLoaded = false;
    romPath.clear();
    if (path && path[0]) {
        if (!loadRom(path, ram)) {
            paintIdle(ram);
            return;
        }
        graphicsMode = !romFixPending && !chr.empty();
    }
    if (!romFixPending) paused = false;
    if (graphicsMode)
        warmupAfterLoad(ram);
    else if (!romFixPending)
        paintIdle(ram);
    if (openOptions) {
        FieldAmmoNesSetup::open(padOnly, true);
        optionsOpen = true;
        paused = true;
        FieldAmmoNesSetup::paint(ram);
    }
}

inline void close(std::uint8_t* ram) noexcept {
    active = false;
    optionsOpen = false;
    aboutOpen = false;
    menuTurbo = false;
    menuUnlimited = false;
    FieldAmmoNesSetup::active = false;
    FieldEmuFileDialog::close();
    FieldNesRomFix::close(ram);
    romFixPending = false;
    graphicsMode = false;
    paused = false;
    romLoaded = false;
    vgaMode13 = false;
    FieldNesAudio::shutdown();
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
    if (aboutOpen && keyDown) {
        aboutOpen = false;
        paintIdle(ram);
        return;
    }
    if (keyDown) {
        if (key == 0x011Bu) { close(ram); return; }
        if ((key & 0xFFu) == 'p' || (key & 0xFFu) == 'P') {
            paused = !paused;
            refreshChrome(ram);
        }
        if ((key & 0xFFu) == 'r' || (key & 0xFFu) == 'R') {
            if (!romPath.empty()) {
                loadRom(romPath.c_str(), ram);
                graphicsMode = !romFixPending && !chr.empty();
                refreshChrome(ram);
            }
        }
        if (key == 0x3920u && romLoaded) {
            applyPadFromInput(nullptr);
            advanceOneFrame(ram);
        }
    }
    if (!graphicsMode && !FieldEmuFileDialog::active && !optionsOpen && !FieldNesRomFix::active
            && aboutOpen)
        paintWelcome(ram);
}

inline void tick(std::uint8_t* ram, const bool* keys) noexcept {
    if (!active || !graphicsMode || !romLoaded || paused || optionsOpen
            || FieldEmuFileDialog::active || FieldNesRomFix::active)
        return;
    applyPadFromInput(keys);
    runFrame(ram);
}

inline void packDataBus(std::uint32_t* bus) noexcept {
    if (!bus) return;
    bus[10] = active ? 1u : 0u;
    bus[11] = frames;
}

inline void saveState(int = 0) noexcept {}
inline void loadState(int = 0) noexcept {}

} // namespace FieldNes
