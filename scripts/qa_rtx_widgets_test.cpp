// QA: RTX widget apps paint valid widget RAM for each launcher target.
#include "FieldAmouranthExec.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldAmouranthOs.hpp"
#include "FieldAmouranthHudRam.hpp"
#include "FieldRtxWidgets.hpp"
#include "FieldRtxApp.hpp"
#include "FieldDosChrome.hpp"
#include "FieldDosViewport.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxBoot.hpp"
#include "FieldAosAppJournal.hpp"
#include "FieldBrowserHook.hpp"
#include "OptionsMenu.hpp"

#include <cstdio>
#include <vector>

static void pump(std::uint8_t* ram, int n = 3) {
    for (int i = 0; i < n; ++i)
        FieldAmouranthExec::execPending(ram);
}

static bool launchAndCheck(std::uint8_t* ram, FieldAmouranthLaunch::GuiApp app,
                           std::uint8_t expectAppId, const char* name) {
    FieldAmouranthOs::showDosPanelDocked();
    FieldAmouranthLaunch::queueGui(app);
    pump(ram);
    if (!FieldAmouranthOs::panelVisible) {
        std::fprintf(stderr, "FAIL %s panel not visible\n", name);
        return false;
    }
    if (FieldRtxWidgets::g.appId != expectAppId) {
        std::fprintf(stderr, "FAIL %s appId got %u want %u\n",
            name, FieldRtxWidgets::g.appId, expectAppId);
        return false;
    }
    if (FieldRtxWidgets::g.widgets.empty()) {
        std::fprintf(stderr, "FAIL %s no widgets packed\n", name);
        return false;
    }
    std::printf("OK RTX %s widgets=%zu appId=%u\n",
        name, FieldRtxWidgets::g.widgets.size(), expectAppId);
    return true;
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

    std::vector<std::uint8_t> buf(FieldPlatform::GUEST_RAM_BYTES, 0);
    std::uint8_t* ram = buf.data();
    FieldRtxBoot::paintWelcome(ram);
    FieldAosAppJournal::clearSessionJournal();
    FieldAmouranthOs::boot();

    struct Case { FieldAmouranthLaunch::GuiApp app; std::uint8_t id; const char* name; };
    const Case cases[] = {
        {FieldAmouranthLaunch::GuiApp::Shell,    1u,  "Shell"},
        {FieldAmouranthLaunch::GuiApp::FieldC,   9u,  "FieldC"},
        {FieldAmouranthLaunch::GuiApp::Browser,  8u,  "Browser"},
        {FieldAmouranthLaunch::GuiApp::Monitor, 12u, "Monitor"},
        {FieldAmouranthLaunch::GuiApp::FileCmd,  7u,  "FileCmd"},
        {FieldAmouranthLaunch::GuiApp::PadTest,  5u,  "PadTest"},
        {FieldAmouranthLaunch::GuiApp::NesSetup, 11u, "NesSetup"},
    };

    for (const auto& c : cases) {
        if (!launchAndCheck(ram, c.app, c.id, c.name))
            return 1;
    }

    FieldAmouranthOs::onMouseDown(nullptr,
        FieldAmouranthOs::quickLaunchX(0) + 10.f,
        static_cast<float>(FieldAmouranthOs::winH) - 20.f, 1, 1);
    if (!FieldAmouranthOs::filesOpen) {
        std::fprintf(stderr, "FAIL quick-launch folder button\n");
        return 1;
    }
    std::printf("OK quick-launch folder opens files menu\n");

    FieldAmouranthOs::packChromeRam(ram);
    if (ram[FieldAmouranthHudRam::TASKBAR_RAM + 8u] == 0u) {
        std::fprintf(stderr, "FAIL browser icon slot not packed\n");
        return 1;
    }
    std::printf("OK taskbar browser icon slot=%u\n",
        ram[FieldAmouranthHudRam::TASKBAR_RAM + 8u]);

    FieldAmouranthOs::tick(1920, 1080);
    FieldDosViewport::winW = 1920.f;
    FieldDosViewport::winH = 1080.f;
    FieldDosViewport::renderW = FieldDosViewport::winW;
    FieldDosViewport::renderH = FieldDosViewport::winH;
    FieldAmouranthOs::showDosPanelDocked();
    FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Shell);
    pump(ram);
    FieldAmouranthExec::paintRtxShellGui(ram);

    const FieldDosViewport::Rect win = FieldDosViewport::panelRectRender();
    const float titleH = FieldDosViewport::chromeTitleH;
    const float hudH = FieldDosViewport::DOS_HUD_H;
    const float left = 4.f;
    const float top = titleH + 2.f;
    const float cw = std::max(win.w() - 18.f - left, 1.f);
    const float ch = std::max(win.h() - hudH - 2.f - top, 1.f);
    int launchIdx = -1;
    for (int i = 0; i < static_cast<int>(FieldRtxApp::ui.widgets.size()); ++i) {
        const auto& w = FieldRtxApp::ui.widgets[static_cast<std::size_t>(i)];
        if (w.type == FieldRtxWidgets::Type::Button && w.state == 2u) {
            launchIdx = i;
            break;
        }
    }
    if (launchIdx < 0) {
        std::fprintf(stderr, "FAIL shell Launch button widget not found\n");
        return 1;
    }
    const auto& launch = FieldRtxApp::ui.widgets[static_cast<std::size_t>(launchIdx)];
    const float nx = (static_cast<float>(launch.x0 + launch.x1) * 0.5f) / 1024.f;
    const float ny = (static_cast<float>(launch.y0 + launch.y1) * 0.5f) / 1024.f;
    if (FieldRtxApp::hitWidget(nx, ny) != launchIdx) {
        std::fprintf(stderr, "FAIL shell Launch hit-test at button center\n");
        return 1;
    }
    std::printf("OK shell Launch button hit id=%d\n", launchIdx);

    const float u = nx;
    const float v = ny;
    FieldDosChrome::lastPixelX = win.x0 + left + u * cw;
    FieldDosChrome::lastPixelY = win.y0 + top + v * ch;
    FieldDosChrome::prevButtons = 0u;
    FieldDosChrome::lastButtons = 1u;
    int action = 0;
    FieldRtxApp::pumpMouse(ram, action);
    if (action != 2) {
        std::fprintf(stderr, "FAIL shell Launch click action=%d want 2\n", action);
        return 1;
    }
    std::printf("OK shell Launch button click action=%d\n", action);

    FieldAosAppIdentity::packTab(ram, 0, FieldAosAppIdentity::AppId::Shell);
    const char* tag = reinterpret_cast<const char*>(ram + FieldAosAppIdentity::IDENTITY_RAM);
    if (tag[0] == ' ' || tag[0] == '\0') {
        std::fprintf(stderr, "FAIL app identity tagline not packed\n");
        return 1;
    }
    std::printf("OK app identity packed: %.32s\n", tag);

    FieldAosAppJournal::recordOpen(1, FieldAosAppIdentity::AppId::Shell, "RTX Shell");
    FieldAosAppJournal::recordFocus(1, FieldAosAppIdentity::AppId::Shell, "RTX Shell");
    FieldAosAppJournal::recordLinkedAction(1, FieldAosAppIdentity::AppId::FileCmd,
        "AmmoFiles", ">TEST.BAS");
    FieldAosAppJournal::sync(ram);
    const char* recent = reinterpret_cast<const char*>(ram + FieldAosAppIdentity::IDENTITY_RAM
        + FieldAosAppIdentity::RECENT1_OFF);
    if (recent[0] == ' ' && recent[1] == ' ') {
        std::fprintf(stderr, "FAIL journal recent line not packed\n");
        return 1;
    }
    std::printf("OK app journal recent: %.28s\n", recent);

    char recentLines[2][FieldAosAppJournal::RECENT_LEN + 1]{};
    if (FieldAosAppJournal::recentLinesForProg(1, recentLines, 2) < 2) {
        std::fprintf(stderr, "FAIL journal recentLinesForProg\n");
        return 1;
    }
    std::printf("OK journal activity lines: %s | %s\n", recentLines[0], recentLines[1]);

    const FieldAosAppIdentity::AppId sessApps[] = {
        FieldAosAppIdentity::AppId::Shell, FieldAosAppIdentity::AppId::Monitor };
    const char* sessSnaps[] = { "scr=2", "" };
    FieldAosAppJournal::saveSession(sessApps, sessSnaps, 2,
        FieldAosAppIdentity::AppId::Monitor);
    const FieldAosAppJournal::SessionPlan plan = FieldAosAppJournal::lastSession();
    if (!plan.valid || plan.count != 2
            || plan.apps[0] != FieldAosAppIdentity::AppId::Shell
            || plan.focusApp != FieldAosAppIdentity::AppId::Monitor) {
        std::fprintf(stderr, "FAIL session plan parse count=%d focus=%u\n",
            plan.count, static_cast<unsigned>(plan.focusApp));
        return 1;
    }
    if (std::strcmp(plan.snapshots[0], "scr=2") != 0) {
        std::fprintf(stderr, "FAIL session snapshot scr=%s\n", plan.snapshots[0]);
        return 1;
    }
    std::printf("OK session restore plan (%d apps, focus Monitor)\n", plan.count);

    FieldAosAppJournal::clearSessionJournal();
    FieldAosAppJournal::setAppSnapshot(FieldAosAppIdentity::AppId::FileCmd,
        "L=C:\\TOOLS,R=E:\\,p=0");
    const FieldAosAppIdentity::AppId restoreApps[] = {
        FieldAosAppIdentity::AppId::FileCmd };
    const char* restoreSnaps[] = { "L=C:\\TOOLS,R=E:\\,p=0" };
    FieldAosAppJournal::saveSession(restoreApps, restoreSnaps, 1,
        FieldAosAppIdentity::AppId::FileCmd);
    FieldAmouranthOs::programs.clear();
    FieldAmouranthOs::nextProgId = 1;
    FieldAmouranthOs::focusedProgId = 0;
    FieldAmouranthOs::focusedApp = FieldAmouranthOs::AppId::None;
    FieldAmouranthOs::panelVisible = false;
    FieldAmouranthOs::restoreSessionFromJournal();
    if (FieldAmouranthOs::programs.size() != 1u || FieldAmouranthOs::panelVisible) {
        std::fprintf(stderr, "FAIL session restore tabs progs=%zu panel=%d\n",
            FieldAmouranthOs::programs.size(), FieldAmouranthOs::panelVisible ? 1 : 0);
        return 1;
    }
    if (!FieldAmouranthOs::programs[0].minimized) {
        std::fprintf(stderr, "FAIL restored tab should start minimized\n");
        return 1;
    }
    FieldAmouranthOs::focusProgram(FieldAmouranthOs::programs[0].id);
    FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::FileCmd);
    pump(ram);
    if (FieldAmouranthFileCmd::pathL != "C:\\TOOLS") {
        std::fprintf(stderr, "FAIL FileCmd snapshot restore path=%s\n",
            FieldAmouranthFileCmd::pathL.c_str());
        return 1;
    }
    std::printf("OK session tab restore + FileCmd snapshot\n");

    char linkLines[2][FieldAosAppJournal::RECENT_LEN + 1]{};
    FieldAosAppJournal::recordLinkedAction(1, FieldAosAppIdentity::AppId::FileCmd,
        "AmmoFiles", ">LINK.BAS");
    if (FieldAosAppJournal::recentLinesForProg(1, linkLines, 2) < 1
            || linkLines[0][0] != '-' || linkLines[0][1] != '>') {
        std::fprintf(stderr, "FAIL linked journal line for parent prog\n");
        return 1;
    }
    std::printf("OK linked audit chain: %s\n", linkLines[0]);

    FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Browser);
    pump(ram);
    if (FieldBrowserHook::hooked) {
        std::fprintf(stderr, "FAIL browser should not auto-hook on panel open\n");
        return 1;
    }
    std::printf("OK browser panel opens without auto-launch\n");

    std::printf("RTX widgets QA passed\n");
    return 0;
}