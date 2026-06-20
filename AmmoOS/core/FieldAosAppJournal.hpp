#pragma once

// AppIdentity journal — lifecycle + action log for each AmouranthOS window.
// Persists to C:\TOOLS\APPID.JRN (FieldJournal) and a guest-RAM ring for GPU chrome.
// Host mirror: assets/data/journals/appid.jrn

#include "FieldAosAppIdentity.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldJournal.hpp"
#include "FieldRtxWidgets.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FieldAosAppJournal {

constexpr const char* kJournalPath      = "C:\\TOOLS\\APPID.JRN";
constexpr const char* kHostMirrorPath   = "assets/data/journals/appid.jrn";

constexpr std::uint32_t JOURNAL_RAM     = 0x000BC000u;
constexpr int           RING_CAP        = 32;
constexpr int           ENTRY_STRIDE    = 52;
constexpr int           TEXT_LEN        = 48;
constexpr int           RECENT_LEN      = 32;
constexpr int           SNAPSHOT_LEN    = 48;

enum class Event : std::uint8_t {
    Open = 1, Focus = 2, Close = 3, Minimize = 4, Restore = 5, Action = 6, Snapshot = 7
};

struct Entry {
    std::uint8_t progId = 0;
    std::uint8_t parentProgId = 0;
    FieldAosAppIdentity::AppId app = FieldAosAppIdentity::AppId::None;
    Event ev = Event::Open;
    char text[TEXT_LEN + 1]{};
};

struct SessionPlan {
    FieldAosAppIdentity::AppId apps[FieldAosAppIdentity::MAX_TABS]{};
    char snapshots[FieldAosAppIdentity::MAX_TABS][SNAPSHOT_LEN + 1]{};
    int count = 0;
    FieldAosAppIdentity::AppId focusApp = FieldAosAppIdentity::AppId::None;
    bool valid = false;
};

inline std::vector<Entry> ring(static_cast<std::size_t>(RING_CAP));
inline int writeIdx = 0;
inline int journalProgId = 0;
inline bool restoringSession = false;
inline char recentByTab[FieldAosAppIdentity::MAX_TABS][RECENT_LEN + 1]{};
inline char recent2ByTab[FieldAosAppIdentity::MAX_TABS][RECENT_LEN + 1]{};
inline char snapPreviewByTab[FieldAosAppIdentity::MAX_TABS][FieldAosAppIdentity::SNAP_PREVIEW + 1]{};
inline char appSnapshots[16][SNAPSHOT_LEN + 1]{};
inline char lastSessionLine[256]{};
inline bool mirrorDirty = false;
inline bool sessionDirty = false;
inline bool cfgRestoreSession = true;
inline bool cfgMirrorHost = true;
inline std::uint16_t sessionEpoch = 0;
inline int appendSinceCompact = 0;

constexpr int           MAX_FILE_KB       = 128;
constexpr int           COMPACT_EVERY     = 48;
constexpr int           KEEP_LINES        = 256;

inline const char* eventName(Event e) noexcept {
    switch (e) {
    case Event::Open:      return "OPEN";
    case Event::Focus:     return "FOCUS";
    case Event::Close:     return "CLOSE";
    case Event::Minimize:  return "MIN";
    case Event::Restore:   return "RESTORE";
    case Event::Action:    return "ACT";
    case Event::Snapshot:  return "SNAP";
    default:               return "?";
    }
}

inline Event eventFromName(const char* name) noexcept {
    if (!name) return Event::Open;
    if (std::strcmp(name, "FOCUS") == 0)   return Event::Focus;
    if (std::strcmp(name, "CLOSE") == 0)    return Event::Close;
    if (std::strcmp(name, "MIN") == 0)      return Event::Minimize;
    if (std::strcmp(name, "RESTORE") == 0)  return Event::Restore;
    if (std::strcmp(name, "ACT") == 0)      return Event::Action;
    if (std::strcmp(name, "SNAP") == 0)     return Event::Snapshot;
    return Event::Open;
}

inline const char* appToken(FieldAosAppIdentity::AppId app) noexcept {
    switch (app) {
    case FieldAosAppIdentity::AppId::Shell:     return "Shell";
    case FieldAosAppIdentity::AppId::AmmoCode:  return "AmmoCode";
    case FieldAosAppIdentity::AppId::QBasic:    return "QBasic";
    case FieldAosAppIdentity::AppId::FieldC:    return "FieldC";
    case FieldAosAppIdentity::AppId::PadTest:   return "PadTest";
    case FieldAosAppIdentity::AppId::Nes:       return "Nes";
    case FieldAosAppIdentity::AppId::NesSetup:  return "NesSetup";
    case FieldAosAppIdentity::AppId::Browser:   return "Browser";
    case FieldAosAppIdentity::AppId::FileCmd:   return "FileCmd";
    case FieldAosAppIdentity::AppId::Doom:      return "Doom";
    case FieldAosAppIdentity::AppId::Monitor:   return "Monitor";
    case FieldAosAppIdentity::AppId::A2600:     return "A2600";
    case FieldAosAppIdentity::AppId::A2600Setup: return "A2600Setup";
    case FieldAosAppIdentity::AppId::Sms:       return "Sms";
    case FieldAosAppIdentity::AppId::SmsSetup:  return "SmsSetup";
    case FieldAosAppIdentity::AppId::Genesis:   return "Genesis";
    case FieldAosAppIdentity::AppId::GenesisSetup: return "GenesisSetup";
    case FieldAosAppIdentity::AppId::Snes:       return "Snes";
    case FieldAosAppIdentity::AppId::SnesSetup:  return "SnesSetup";
    default: return "None";
    }
}

inline FieldAosAppIdentity::AppId appFromToken(const char* tok) noexcept {
    if (!tok || !tok[0]) return FieldAosAppIdentity::AppId::None;
    if (std::strcmp(tok, "Shell") == 0)     return FieldAosAppIdentity::AppId::Shell;
    if (std::strcmp(tok, "AmmoCode") == 0)  return FieldAosAppIdentity::AppId::AmmoCode;
    if (std::strcmp(tok, "QBasic") == 0)    return FieldAosAppIdentity::AppId::QBasic;
    if (std::strcmp(tok, "FieldC") == 0)    return FieldAosAppIdentity::AppId::FieldC;
    if (std::strcmp(tok, "PadTest") == 0)   return FieldAosAppIdentity::AppId::PadTest;
    if (std::strcmp(tok, "Nes") == 0)       return FieldAosAppIdentity::AppId::Nes;
    if (std::strcmp(tok, "NesSetup") == 0)  return FieldAosAppIdentity::AppId::NesSetup;
    if (std::strcmp(tok, "Browser") == 0)   return FieldAosAppIdentity::AppId::Browser;
    if (std::strcmp(tok, "FileCmd") == 0)   return FieldAosAppIdentity::AppId::FileCmd;
    if (std::strcmp(tok, "Doom") == 0)      return FieldAosAppIdentity::AppId::Doom;
    if (std::strcmp(tok, "Monitor") == 0)   return FieldAosAppIdentity::AppId::Monitor;
    if (std::strcmp(tok, "A2600") == 0)     return FieldAosAppIdentity::AppId::A2600;
    if (std::strcmp(tok, "A2600Setup") == 0) return FieldAosAppIdentity::AppId::A2600Setup;
    if (std::strcmp(tok, "Sms") == 0)       return FieldAosAppIdentity::AppId::Sms;
    if (std::strcmp(tok, "SmsSetup") == 0)  return FieldAosAppIdentity::AppId::SmsSetup;
    if (std::strcmp(tok, "Genesis") == 0)   return FieldAosAppIdentity::AppId::Genesis;
    if (std::strcmp(tok, "GenesisSetup") == 0) return FieldAosAppIdentity::AppId::GenesisSetup;
    if (std::strcmp(tok, "Snes") == 0)       return FieldAosAppIdentity::AppId::Snes;
    if (std::strcmp(tok, "SnesSetup") == 0)  return FieldAosAppIdentity::AppId::SnesSetup;
    return FieldAosAppIdentity::AppId::None;
}

inline void copyText(char* dst, int cap, const char* src) noexcept {
    if (!dst || cap <= 0) return;
    if (!src) { dst[0] = '\0'; return; }
    const int max = cap - 1;
    int i = 0;
    for (; i < max && src[i] != '\0'; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
}

inline void setAppSnapshot(FieldAosAppIdentity::AppId app, const char* snap) noexcept {
    const auto idx = static_cast<std::size_t>(app);
    if (idx >= 16u) return;
    copyText(appSnapshots[idx], SNAPSHOT_LEN + 1, snap);
}

inline const char* getAppSnapshot(FieldAosAppIdentity::AppId app) noexcept {
    const auto idx = static_cast<std::size_t>(app);
    if (idx >= 16u) return "";
    return appSnapshots[idx];
}

inline bool restoreEnabled() noexcept { return cfgRestoreSession; }
inline bool mirrorEnabled() noexcept { return cfgMirrorHost; }

inline void compactJournal() noexcept;

inline void mirrorToHost() noexcept {
    if (!mirrorEnabled()) return;
    std::vector<std::uint8_t> data;
    if (!FieldAmmoVfs::readPath(kJournalPath, data) || data.empty()) return;
    std::FILE* fp = std::fopen(kHostMirrorPath, "wb");
    if (!fp) return;
    std::fwrite(data.data(), 1, data.size(), fp);
    std::fclose(fp);
    mirrorDirty = false;
}

inline void requestMirror() noexcept {
    mirrorDirty = true;
}

inline void persist(const char* appName, Event ev, const char* detail,
                    int progId, int parentProgId,
                    FieldAosAppIdentity::AppId app) noexcept {
    char line[280];
    if (progId > 0 || parentProgId > 0) {
        std::snprintf(line, sizeof line, "id=%d p=%d %s:%s | %s",
            progId & 0xFF, parentProgId & 0xFF, appToken(app), eventName(ev),
            detail ? detail : "");
    } else {
        std::snprintf(line, sizeof line, "%s:%s | %s", appName ? appName : "?",
            eventName(ev), detail ? detail : "");
    }
    FieldJournal::append(kJournalPath, "APPID", line);
    ++appendSinceCompact;
    sessionDirty = true;
    requestMirror();
    if (appendSinceCompact >= COMPACT_EVERY)
        compactJournal();
}

inline void ingestEntry(int progId, int parentProgId, FieldAosAppIdentity::AppId app,
                        Event ev, const char* detail) noexcept {
    Entry& e = ring[static_cast<std::size_t>(writeIdx % RING_CAP)];
    e.progId = static_cast<std::uint8_t>(progId & 0xFF);
    e.parentProgId = static_cast<std::uint8_t>(parentProgId & 0xFF);
    e.app = app;
    e.ev = ev;
    copyText(e.text, TEXT_LEN + 1, detail);
    ++writeIdx;
}

inline void push(int progId, FieldAosAppIdentity::AppId app, Event ev,
                 const char* detail, const char* appName, int parentProgId = 0) noexcept {
    ingestEntry(progId, parentProgId, app, ev, detail);
    persist(appName, ev, detail, progId, parentProgId, app);
}

inline bool parseStructuredLine(const char* body, Entry& out) noexcept {
    if (!body || std::strncmp(body, "id=", 3) != 0) return false;
    const int progId = std::atoi(body + 3);
    int parentProgId = 0;
    if (const char* sp = std::strstr(body, " p="))
        parentProgId = std::atoi(sp + 3);
    const char* bar = std::strstr(body, " | ");
    if (!bar) return false;
    char head[96]{};
    char evTok[12]{};
    char detail[TEXT_LEN + 1]{};
    const int headLen = static_cast<int>(bar - body);
    for (int i = 0; i < headLen && i < static_cast<int>(sizeof head) - 1; ++i)
        head[i] = body[i];
    head[std::min(headLen, static_cast<int>(sizeof head) - 1)] = '\0';
    const char* colon = std::strrchr(head, ':');
    if (!colon) return false;
    copyText(evTok, static_cast<int>(sizeof evTok), colon + 1);
    char appTok[24]{};
    const char* appStart = head;
    for (const char* c = colon - 1; c >= head; --c) {
        if (*c == ' ') { appStart = c + 1; break; }
    }
    copyText(appTok, static_cast<int>(sizeof appTok), appStart);
    const char* colonInApp = std::strchr(appTok, ':');
    if (colonInApp) *const_cast<char*>(colonInApp) = '\0';
    copyText(detail, static_cast<int>(sizeof detail), bar + 3);
    out.progId = static_cast<std::uint8_t>(progId & 0xFF);
    out.parentProgId = static_cast<std::uint8_t>(parentProgId & 0xFF);
    out.app = appFromToken(appTok);
    out.ev = eventFromName(evTok);
    copyText(out.text, TEXT_LEN + 1, detail);
    return out.app != FieldAosAppIdentity::AppId::None;
}

inline bool parseLegacyLine(const char* body, Entry& out) noexcept {
    if (!body) return false;
    const char* colon = std::strchr(body, ':');
    if (!colon) return false;
    const char* bar = std::strstr(colon, " | ");
    if (!bar) return false;
    char evTok[12]{};
    char detail[TEXT_LEN + 1]{};
    const int evLen = static_cast<int>(bar - (colon + 1));
    for (int i = 0; i < evLen && i < static_cast<int>(sizeof evTok) - 1; ++i)
        evTok[i] = colon[1 + i];
    evTok[std::min(evLen, static_cast<int>(sizeof evTok) - 1)] = '\0';
    copyText(detail, static_cast<int>(sizeof detail), bar + 3);
    out.progId = 0;
    out.parentProgId = 0;
    out.app = FieldAosAppIdentity::AppId::None;
    out.ev = eventFromName(evTok);
    copyText(out.text, TEXT_LEN + 1, detail);
    return true;
}

inline void resetRing() noexcept {
    writeIdx = 0;
    journalProgId = 0;
    lastSessionLine[0] = '\0';
    for (auto& e : ring) {
        e = Entry{};
    }
    for (int i = 0; i < 16; ++i)
        appSnapshots[i][0] = '\0';
}

inline void clearSessionJournal() noexcept {
    resetRing();
    const std::vector<std::uint8_t> empty;
    FieldJournal::writePath(kJournalPath, empty);
    std::remove(kHostMirrorPath);
    mirrorDirty = false;
    sessionDirty = false;
    appendSinceCompact = 0;
}

inline void compactJournal() noexcept {
    std::vector<std::uint8_t> data;
    if (!FieldAmmoVfs::readPath(kJournalPath, data) || data.empty()) {
        appendSinceCompact = 0;
        return;
    }
    std::string text(reinterpret_cast<const char*>(data.data()), data.size());
    std::vector<std::string> appidLines;
    std::string lastSession;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        std::string line = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        pos = (end == std::string::npos) ? text.size() : end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.rfind("APPID | ", 0) != 0) continue;
        const std::string body = line.substr(8);
        if (body.rfind("SESSION", 0) == 0)
            lastSession = line + "\r\n";
        else
            appidLines.push_back(line + "\r\n");
    }
    if (appidLines.size() > static_cast<std::size_t>(KEEP_LINES))
        appidLines.erase(appidLines.begin(),
            appidLines.end() - static_cast<std::ptrdiff_t>(KEEP_LINES));
    std::string out;
    for (const auto& ln : appidLines) out += ln;
    if (!lastSession.empty()) out += lastSession;
    std::vector<std::uint8_t> packed(out.begin(), out.end());
    FieldJournal::writePath(kJournalPath, packed);
    appendSinceCompact = 0;
    requestMirror();
}

inline void loadFromVfs() noexcept {
    std::vector<std::uint8_t> data;
    if (!FieldAmmoVfs::readPath(kJournalPath, data) || data.empty()) return;
    writeIdx = 0;
    lastSessionLine[0] = '\0';
    std::string text(reinterpret_cast<const char*>(data.data()), data.size());
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        std::string line = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        pos = (end == std::string::npos) ? text.size() : end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        const std::size_t bar = line.find(" | ");
        if (bar == std::string::npos) continue;
        const std::string tag = line.substr(0, bar);
        const std::string body = line.substr(bar + 3);
        if (tag == "APPID") {
            if (body.rfind("SESSION", 0) == 0) {
                copyText(lastSessionLine, static_cast<int>(sizeof lastSessionLine), body.c_str());
                continue;
            }
            Entry e{};
            if (!parseStructuredLine(body.c_str(), e))
                parseLegacyLine(body.c_str(), e);
            ingestEntry(e.progId, e.parentProgId, e.app, e.ev, e.text);
        }
    }
}

inline SessionPlan parseSessionLine(const char* line) noexcept {
    SessionPlan plan{};
    if (!line || std::strncmp(line, "SESSION", 7) != 0) return plan;
    const char* appsPart = std::strchr(line, '|');
    if (!appsPart) return plan;
    ++appsPart;
    while (*appsPart == ' ') ++appsPart;
    char buf[192];
    copyText(buf, static_cast<int>(sizeof buf), appsPart);
    char* focusTok = std::strstr(buf, "|focus=");
    if (focusTok) {
        *focusTok = '\0';
        plan.focusApp = appFromToken(focusTok + 7);
    }
    char* ctx = nullptr;
    for (char* tok = std::strtok(buf, ";"); tok && plan.count < FieldAosAppIdentity::MAX_TABS;
         tok = std::strtok(nullptr, ";")) {
        while (*tok == ' ') ++tok;
        char* end = tok + std::strlen(tok);
        while (end > tok && end[-1] == ' ') {
            --end;
            *end = '\0';
        }
        char* snap = std::strchr(tok, '@');
        if (snap) {
            *snap = '\0';
            copyText(plan.snapshots[plan.count], SNAPSHOT_LEN + 1, snap + 1);
        }
        const FieldAosAppIdentity::AppId app = appFromToken(tok);
        if (app == FieldAosAppIdentity::AppId::None) continue;
        plan.apps[plan.count++] = app;
    }
    plan.valid = plan.count > 0;
    return plan;
}

inline SessionPlan lastSession() noexcept {
    if (lastSessionLine[0])
        return parseSessionLine(lastSessionLine);
    std::vector<std::uint8_t> data;
    if (!FieldAmmoVfs::readPath(kJournalPath, data) || data.empty())
        return SessionPlan{};
    std::string text(reinterpret_cast<const char*>(data.data()), data.size());
    SessionPlan plan{};
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        std::string line = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        pos = (end == std::string::npos) ? text.size() : end + 1;
        const std::size_t bar = line.find(" | ");
        if (bar == std::string::npos) continue;
        if (line.substr(0, bar) != "APPID") continue;
        const std::string body = line.substr(bar + 3);
        if (body.rfind("SESSION", 0) == 0) {
            copyText(lastSessionLine, static_cast<int>(sizeof lastSessionLine), body.c_str());
            plan = parseSessionLine(lastSessionLine);
        }
    }
    return plan;
}

inline void saveSession(const FieldAosAppIdentity::AppId* apps, const char* const* snapshots,
                        int count, FieldAosAppIdentity::AppId focusApp) noexcept {
    char line[280];
    if (!apps || count <= 0) {
        std::snprintf(line, sizeof line, "SESSION | ");
        FieldJournal::append(kJournalPath, "APPID", line);
        lastSessionLine[0] = '\0';
        ++sessionEpoch;
        sessionDirty = false;
        requestMirror();
        mirrorToHost();
        return;
    }
    std::string body = "SESSION | ";
    for (int i = 0; i < count; ++i) {
        if (i > 0) body += ';';
        body += appToken(apps[i]);
        if (snapshots && snapshots[i] && snapshots[i][0]) {
            body += '@';
            body += snapshots[i];
        }
    }
    if (focusApp != FieldAosAppIdentity::AppId::None) {
        body += " |focus=";
        body += appToken(focusApp);
    }
    copyText(line, static_cast<int>(sizeof line), body.c_str());
    FieldJournal::append(kJournalPath, "APPID", line);
    copyText(lastSessionLine, static_cast<int>(sizeof lastSessionLine), line);
    ++sessionEpoch;
    sessionDirty = false;
    appendSinceCompact = 0;
    requestMirror();
    mirrorToHost();
}

inline void bootStamp() noexcept {
    char detail[48];
    std::snprintf(detail, sizeof detail, "epoch=%u ring=%d",
        static_cast<unsigned>(sessionEpoch), writeIdx);
    push(0, FieldAosAppIdentity::AppId::None, Event::Action, detail, "APPID");
}

// Desktop boot — no ghost taskbar tabs from prior SESSION journal lines.
inline void clearStartupTaskbar() noexcept {
    lastSessionLine[0] = '\0';
    saveSession(nullptr, nullptr, 0, FieldAosAppIdentity::AppId::None);
}

inline const Entry* lastForProg(int progId) noexcept {
    const int n = std::min(writeIdx, RING_CAP);
    for (int i = 0; i < n; ++i) {
        const Entry& e = ring[static_cast<std::size_t>((writeIdx - 1 - i + RING_CAP) % RING_CAP)];
        if (e.progId == static_cast<std::uint8_t>(progId & 0xFF) && e.text[0])
            return &e;
    }
    return nullptr;
}

inline int recentLinesForProg(int progId, char lines[][RECENT_LEN + 1], int maxLines) noexcept {
    if (maxLines <= 0) return 0;
    int found = 0;
    const int n = std::min(writeIdx, RING_CAP);
    const auto pid = static_cast<std::uint8_t>(progId & 0xFF);
    for (int i = 0; i < n && found < maxLines; ++i) {
        const Entry& e = ring[static_cast<std::size_t>((writeIdx - 1 - i + RING_CAP) % RING_CAP)];
        const bool owned = e.progId == pid;
        const bool linked = e.parentProgId == pid && e.ev == Event::Action;
        if (!owned && !linked) continue;
        char* out = lines[found];
        int p = 0;
        if (linked) {
            const char prefix[] = "-> ";
            for (const char* s = prefix; p < RECENT_LEN && *s; ++s)
                out[p++] = *s;
        } else {
            const char* ev = eventName(e.ev);
            for (; p < RECENT_LEN && ev[p]; ++p)
                out[p] = ev[p];
            if (p < RECENT_LEN) out[p++] = ' ';
        }
        for (int ti = 0; p < RECENT_LEN && e.text[ti]; ++ti)
            out[p++] = e.text[ti];
        out[p] = '\0';
        ++found;
    }
    return found;
}

inline void setRecentForTab(int tab, const char* line) noexcept {
    if (tab < 0 || tab >= FieldAosAppIdentity::MAX_TABS) return;
    copyText(recentByTab[tab], RECENT_LEN + 1, line);
}

inline void setRecent2ForTab(int tab, const char* line) noexcept {
    if (tab < 0 || tab >= FieldAosAppIdentity::MAX_TABS) return;
    copyText(recent2ByTab[tab], RECENT_LEN + 1, line);
}

inline void setSnapPreviewForTab(int tab, const char* line) noexcept {
    if (tab < 0 || tab >= FieldAosAppIdentity::MAX_TABS) return;
    copyText(snapPreviewByTab[tab], FieldAosAppIdentity::SNAP_PREVIEW + 1, line);
}

inline void packRam(std::uint8_t* ram) noexcept {
    if (!ram) return;
    const int n = std::min(writeIdx, RING_CAP);
    ram[JOURNAL_RAM] = static_cast<std::uint8_t>(n);
    ram[JOURNAL_RAM + 1u] = static_cast<std::uint8_t>(writeIdx & 0xFF);
    ram[JOURNAL_RAM + 2u] = static_cast<std::uint8_t>((writeIdx >> 8) & 0xFF);
    ram[JOURNAL_RAM + 3u] = static_cast<std::uint8_t>(sessionEpoch & 0xFF);
    ram[JOURNAL_RAM + 4u] = static_cast<std::uint8_t>((sessionEpoch >> 8) & 0xFF);
    for (int i = 0; i < RING_CAP; ++i) {
        const std::uint32_t base = JOURNAL_RAM + 0x10u
            + static_cast<std::uint32_t>(i * ENTRY_STRIDE);
        if (i >= n) {
            ram[base] = 0u;
            continue;
        }
        const Entry& e = ring[static_cast<std::size_t>((writeIdx - 1 - i + RING_CAP) % RING_CAP)];
        ram[base] = e.progId;
        ram[base + 1u] = static_cast<std::uint8_t>(e.app);
        ram[base + 2u] = static_cast<std::uint8_t>(e.ev);
        ram[base + 3u] = e.parentProgId;
        for (int j = 0; j < TEXT_LEN; ++j)
            ram[base + 4u + static_cast<std::uint32_t>(j)] =
                static_cast<std::uint8_t>(e.text[j] ? e.text[j] : ' ');
    }
}

inline void packRecentIntoIdentity(std::uint8_t* ram) noexcept {
    if (!ram) return;
    for (int tab = 0; tab < FieldAosAppIdentity::MAX_TABS; ++tab) {
        const std::uint32_t base = FieldAosAppIdentity::IDENTITY_RAM
            + static_cast<std::uint32_t>(tab * FieldAosAppIdentity::TAB_STRIDE);
        const std::uint32_t r1 = base + static_cast<std::uint32_t>(FieldAosAppIdentity::RECENT1_OFF);
        const std::uint32_t r2 = base + static_cast<std::uint32_t>(FieldAosAppIdentity::RECENT2_OFF);
        const std::uint32_t sp = base + static_cast<std::uint32_t>(FieldAosAppIdentity::SNAP_OFF);
        for (int i = 0; i < RECENT_LEN; ++i) {
            const char c1 = recentByTab[tab][i] ? recentByTab[tab][i] : ' ';
            const char c2 = recent2ByTab[tab][i] ? recent2ByTab[tab][i] : ' ';
            ram[r1 + static_cast<std::uint32_t>(i)] = static_cast<std::uint8_t>(c1);
            ram[r2 + static_cast<std::uint32_t>(i)] = static_cast<std::uint8_t>(c2);
        }
        for (int i = 0; i < FieldAosAppIdentity::SNAP_PREVIEW; ++i) {
            const char ch = snapPreviewByTab[tab][i] ? snapPreviewByTab[tab][i] : ' ';
            ram[sp + static_cast<std::uint32_t>(i)] = static_cast<std::uint8_t>(ch);
        }
    }
}

inline void recordOpen(int progId, FieldAosAppIdentity::AppId app,
                       const char* title) noexcept {
    journalProgId = progId;
    if (restoringSession) return;
    push(progId, app, Event::Open, title, title);
}

inline void recordFocus(int progId, FieldAosAppIdentity::AppId app,
                        const char* title) noexcept {
    journalProgId = progId;
    if (restoringSession) return;
    push(progId, app, Event::Focus, title, title);
}

inline void recordClose(int progId, FieldAosAppIdentity::AppId app,
                        const char* title) noexcept {
    push(progId, app, Event::Close, title, title);
}

inline void recordMinimize(int progId, FieldAosAppIdentity::AppId app,
                           const char* title) noexcept {
    push(progId, app, Event::Minimize, title, title);
}

inline void recordRestore(int progId, FieldAosAppIdentity::AppId app,
                          const char* title) noexcept {
    push(progId, app, Event::Restore, title, title);
}

inline void recordAction(FieldAosAppIdentity::AppId app, const char* title,
                         const char* detail) noexcept {
    push(0, app, Event::Action, detail, title);
}

inline void recordLinkedAction(int parentProgId, FieldAosAppIdentity::AppId app,
                               const char* title, const char* detail) noexcept {
    push(0, app, Event::Action, detail, title, parentProgId);
}

inline void recordSnapshot(int progId, FieldAosAppIdentity::AppId app,
                           const char* title, const char* snap) noexcept {
    push(progId, app, Event::Snapshot, snap, title);
}

inline void sync(std::uint8_t* ram) noexcept {
    packRam(ram);
    packRecentIntoIdentity(ram);
    if (mirrorDirty)
        mirrorToHost();
}

inline void tick() noexcept {
    if (sessionDirty && writeIdx > 0)
        (void)0;
}

inline void paintRecent(FieldRtxWidgets::Ui& ui, int progId,
                        int y0 = 172, int y1 = 248) noexcept {
    const int lh = FieldRtxWidgets::UI_LABEL_H;
    const int step = FieldRtxWidgets::UI_ROW_H;
    ui.label(32, y0, 240, y0 + lh, "Recent activity", 1);
    char lines[3][RECENT_LEN + 1]{};
    const int n = recentLinesForProg(progId, lines, 3);
    if (n <= 0) {
        ui.label(32, y0 + step, 992, y0 + step + lh, "No journal entries yet this session", 0);
        return;
    }
    int y = y0 + step;
    for (int i = 0; i < n && y + lh <= y1; ++i) {
        ui.label(32, y, 992, y + lh, lines[i], 0);
        y += step;
    }
}

} // namespace FieldAosAppJournal