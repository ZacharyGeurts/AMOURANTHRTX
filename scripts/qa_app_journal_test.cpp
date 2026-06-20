// QA: APPID.JRN journal — compaction, session, snapshots, identity RAM layout.
#include "FieldAosAppIdentity.hpp"
#include "FieldAosAppJournal.hpp"
#include "FieldAosAppSnapshot.hpp"
#include "FieldAmouranthOs.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldAmouranthExec.hpp"
#include "FieldAmouranthFileCmd.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldRegistry.hpp"
#include "FieldJournal.hpp"
#include "FieldAosAppJournalCfg.hpp"
#include "FieldRtxBoot.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static void pump(std::uint8_t* ram) {
    for (int i = 0; i < 3; ++i)
        FieldAmouranthExec::execPending(ram);
}

int main() {
    if (!FieldDos::loadHdImage(FieldDos::defaultHdPath("."))) {
        std::fprintf(stderr, "FAIL hd load\n");
        return 1;
    }
    if (!FieldAmmoFat::mount()) {
        std::fprintf(stderr, "FAIL fat mount\n");
        return 1;
    }
    FieldRegistry::init();
    FieldAosAppJournal::loadConfigFromRegistry();

    std::vector<std::uint8_t> buf(FieldPlatform::GUEST_RAM_BYTES, 0);
    std::uint8_t* ram = buf.data();
    FieldAosAppJournal::clearSessionJournal();

    for (int i = 0; i < 60; ++i)
        FieldAosAppJournal::recordAction(FieldAosAppIdentity::AppId::Shell,
            "RTX Shell", "stress");
    FieldAosAppJournal::compactJournal();
    const std::size_t sz = FieldJournal::pathSize(FieldAosAppJournal::kJournalPath);
    if (sz == 0u) {
        std::fprintf(stderr, "FAIL journal empty after stress\n");
        return 1;
    }
    std::printf("OK journal compact size=%zu bytes\n", sz);

    FieldAosAppJournal::recordOpen(7, FieldAosAppIdentity::AppId::FileCmd, "AmmoFiles");
    FieldAosAppJournal::recordLinkedAction(7, FieldAosAppIdentity::AppId::FileCmd,
        "AmmoFiles", ">ROM.NES");
    char lines[3][FieldAosAppJournal::RECENT_LEN + 1]{};
    if (FieldAosAppJournal::recentLinesForProg(7, lines, 3) < 2) {
        std::fprintf(stderr, "FAIL linked+owned recent lines\n");
        return 1;
    }
    std::printf("OK audit chain lines: %s | %s\n", lines[0], lines[1]);

    FieldAosAppJournal::setAppSnapshot(FieldAosAppIdentity::AppId::Shell, "scr=4");
    char snap[FieldAosAppJournal::SNAPSHOT_LEN + 1]{};
    FieldAosAppSnapshot::capture(FieldAosAppIdentity::AppId::Shell, snap, sizeof snap);
    if (std::strcmp(snap, "scr=4") != 0) {
        std::fprintf(stderr, "FAIL snapshot capture got %s\n", snap);
        return 1;
    }
    FieldAmouranthExec::shellGuiScroll = 0;
    FieldAosAppSnapshot::apply(FieldAosAppIdentity::AppId::Shell);
    if (FieldAmouranthExec::shellGuiScroll != 4) {
        std::fprintf(stderr, "FAIL snapshot apply scr=%d\n", FieldAmouranthExec::shellGuiScroll);
        return 1;
    }
    std::printf("OK snapshot capture/apply Shell scr=4\n");

    const FieldAosAppIdentity::AppId apps[] = { FieldAosAppIdentity::AppId::FileCmd };
    const char* snaps[] = { "L=C:\\TOOLS,R=E:\\,p=1" };
    FieldAosAppJournal::saveSession(apps, snaps, 1, FieldAosAppIdentity::AppId::FileCmd);

    FieldRegistry::setValue("HKRTX\\User\\Desktop", "RestoreSession", "1");
    FieldAosAppJournal::loadConfigFromRegistry();
    FieldRtxBoot::paintWelcome(ram);
    FieldAmouranthOs::boot();
    if (FieldAmouranthOs::programs.size() != 1u || FieldAmouranthOs::panelVisible) {
        std::fprintf(stderr, "FAIL boot restore progs=%zu panel=%d\n",
            FieldAmouranthOs::programs.size(), FieldAmouranthOs::panelVisible ? 1 : 0);
        return 1;
    }
    FieldAmouranthOs::focusProgram(FieldAmouranthOs::programs[0].id);
    FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::FileCmd);
    pump(ram);
    if (FieldAmouranthFileCmd::pathL != "C:\\TOOLS" || FieldAmouranthFileCmd::pane != 1) {
        std::fprintf(stderr, "FAIL boot FileCmd snap L=%s p=%d\n",
            FieldAmouranthFileCmd::pathL.c_str(), FieldAmouranthFileCmd::pane);
        return 1;
    }
    std::printf("OK boot session restore FileCmd\n");

    FieldAmouranthOs::packChromeRam(ram);
    const std::uint32_t idBase = FieldAosAppIdentity::IDENTITY_RAM;
    if (ram[idBase + FieldAosAppIdentity::PROG_ID_OFF] == 0u) {
        std::fprintf(stderr, "FAIL identity progId not packed\n");
        return 1;
    }
    const char* r1 = reinterpret_cast<const char*>(ram + idBase
        + FieldAosAppIdentity::RECENT1_OFF);
    if (r1[0] == ' ' && r1[1] == ' ') {
        std::fprintf(stderr, "FAIL identity recent1 empty\n");
        return 1;
    }
    const char* sp = reinterpret_cast<const char*>(ram + idBase
        + FieldAosAppIdentity::SNAP_OFF);
    if (sp[0] == ' ' || sp[0] == 'L') { /* expect L= path snap */ }
    if (sp[0] != 'L') {
        std::fprintf(stderr, "FAIL identity snap preview %.16s\n", sp);
        return 1;
    }
    if (FieldAosAppJournal::sessionEpoch == 0u) {
        std::fprintf(stderr, "FAIL journal session epoch zero\n");
        return 1;
    }
    std::printf("OK identity RAM layout stride=%d epoch=%u\n",
        FieldAosAppIdentity::TAB_STRIDE,
        static_cast<unsigned>(FieldAosAppJournal::sessionEpoch));

    FieldRegistry::setValue("HKRTX\\User\\Desktop", "RestoreSession", "0");
    FieldAmouranthOs::programs.clear();
    FieldAmouranthOs::restoreSessionFromJournal();
    if (!FieldAmouranthOs::programs.empty()) {
        std::fprintf(stderr, "FAIL restore disabled should skip\n");
        return 1;
    }
    FieldRegistry::setValue("HKRTX\\User\\Desktop", "RestoreSession", "1");
    std::printf("OK registry RestoreSession gate\n");

    std::printf("APPID journal QA passed\n");
    return 0;
}