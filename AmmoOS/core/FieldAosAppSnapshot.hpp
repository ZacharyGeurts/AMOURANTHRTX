#pragma once

// Per-app state snapshots for APPID.JRN session restore.

#include "FieldAosAppIdentity.hpp"
#include "FieldAosAppJournal.hpp"
#include "FieldBrowserHook.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace FieldAmouranthExec { inline int shellGuiScroll = 0; }
namespace FieldAmouranthFileCmd {
void applyJournalSnapshot() noexcept;
void captureSnapshot(char* buf, int cap) noexcept;
}
namespace FieldAmmoNesSetup { extern int page; }
namespace FieldAmmoA2600Setup { extern int page; }
namespace FieldAmmoSmsSetup { extern int page; }
namespace FieldAmmoGenesisSetup { extern int page; }
namespace FieldAmmoSnesSetup { extern int page; }
namespace FieldPadTest { extern bool showRaw; }

namespace FieldAosAppSnapshot {

inline void capture(FieldAosAppIdentity::AppId app, char* buf, int cap) noexcept {
    if (!buf || cap <= 0) return;
    buf[0] = '\0';
    if (const char* cached = FieldAosAppJournal::getAppSnapshot(app)) {
        if (cached[0]) {
            FieldAosAppJournal::copyText(buf, cap, cached);
            return;
        }
    }
    switch (app) {
    case FieldAosAppIdentity::AppId::FileCmd:
        FieldAmouranthFileCmd::captureSnapshot(buf, cap);
        break;
    case FieldAosAppIdentity::AppId::Shell:
        std::snprintf(buf, static_cast<std::size_t>(cap), "scr=%d",
            FieldAmouranthExec::shellGuiScroll);
        break;
    case FieldAosAppIdentity::AppId::NesSetup:
        std::snprintf(buf, static_cast<std::size_t>(cap), "pg=%d",
            FieldAmmoNesSetup::page);
        break;
    case FieldAosAppIdentity::AppId::A2600Setup:
        std::snprintf(buf, static_cast<std::size_t>(cap), "pg=%d",
            FieldAmmoA2600Setup::page);
        break;
    case FieldAosAppIdentity::AppId::SmsSetup:
        std::snprintf(buf, static_cast<std::size_t>(cap), "pg=%d",
            FieldAmmoSmsSetup::page);
        break;
    case FieldAosAppIdentity::AppId::GenesisSetup:
        std::snprintf(buf, static_cast<std::size_t>(cap), "pg=%d",
            FieldAmmoGenesisSetup::page);
        break;
    case FieldAosAppIdentity::AppId::SnesSetup:
        std::snprintf(buf, static_cast<std::size_t>(cap), "pg=%d",
            FieldAmmoSnesSetup::page);
        break;
    case FieldAosAppIdentity::AppId::Browser:
        if (!FieldBrowserHook::currentUrl.empty())
            std::snprintf(buf, static_cast<std::size_t>(cap), "url=%.40s",
                FieldBrowserHook::currentUrl.c_str());
        break;
    case FieldAosAppIdentity::AppId::PadTest:
        std::snprintf(buf, static_cast<std::size_t>(cap), "raw=%d",
            FieldPadTest::showRaw ? 1 : 0);
        break;
    default:
        break;
    }
    if (buf[0]) {
        const char* prev = FieldAosAppJournal::getAppSnapshot(app);
        if (!prev || !prev[0] || app != FieldAosAppIdentity::AppId::FileCmd)
            FieldAosAppJournal::setAppSnapshot(app, buf);
    }
}

inline void apply(FieldAosAppIdentity::AppId app) noexcept {
    const char* snap = FieldAosAppJournal::getAppSnapshot(app);
    if (!snap || !snap[0]) return;
    switch (app) {
    case FieldAosAppIdentity::AppId::FileCmd:
        FieldAmouranthFileCmd::applyJournalSnapshot();
        break;
    case FieldAosAppIdentity::AppId::Shell: {
        int scr = FieldAmouranthExec::shellGuiScroll;
        if (std::sscanf(snap, "scr=%d", &scr) == 1)
            FieldAmouranthExec::shellGuiScroll = scr;
        break;
    }
    case FieldAosAppIdentity::AppId::NesSetup: {
        int pg = FieldAmmoNesSetup::page;
        if (std::sscanf(snap, "pg=%d", &pg) == 1)
            FieldAmmoNesSetup::page = std::clamp(pg, 0, 5);
        break;
    }
    case FieldAosAppIdentity::AppId::A2600Setup: {
        int pg = FieldAmmoA2600Setup::page;
        if (std::sscanf(snap, "pg=%d", &pg) == 1)
            FieldAmmoA2600Setup::page = std::clamp(pg, 0, 2);
        break;
    }
    case FieldAosAppIdentity::AppId::SmsSetup: {
        int pg = FieldAmmoSmsSetup::page;
        if (std::sscanf(snap, "pg=%d", &pg) == 1)
            FieldAmmoSmsSetup::page = std::clamp(pg, 0, 2);
        break;
    }
    case FieldAosAppIdentity::AppId::GenesisSetup: {
        int pg = FieldAmmoGenesisSetup::page;
        if (std::sscanf(snap, "pg=%d", &pg) == 1)
            FieldAmmoGenesisSetup::page = std::clamp(pg, 0, 2);
        break;
    }
    case FieldAosAppIdentity::AppId::SnesSetup: {
        int pg = FieldAmmoSnesSetup::page;
        if (std::sscanf(snap, "pg=%d", &pg) == 1)
            FieldAmmoSnesSetup::page = std::clamp(pg, 0, 2);
        break;
    }
    case FieldAosAppIdentity::AppId::PadTest: {
        int raw = 0;
        if (std::sscanf(snap, "raw=%d", &raw) == 1)
            FieldPadTest::showRaw = raw != 0;
        break;
    }
    default:
        break;
    }
}

inline void refresh(FieldAosAppIdentity::AppId app) noexcept {
    char snap[FieldAosAppJournal::SNAPSHOT_LEN + 1]{};
    capture(app, snap, static_cast<int>(sizeof snap));
}

} // namespace FieldAosAppSnapshot