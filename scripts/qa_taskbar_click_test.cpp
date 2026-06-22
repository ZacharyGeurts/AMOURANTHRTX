// QA: AmouranthOS taskbar hit tests — Start, quick-launch, tab slots, clean boot RAM.
#include "FieldAosAppJournal.hpp"
#include "FieldRegistry.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldAmouranthMenu.hpp"
#include "FieldAmouranthOs.hpp"
#include "FieldAmouranthExec.hpp"
#include "FieldAmouranthExitConfirm.hpp"
#include "FieldAmmoFat.hpp"
#include "OptionsMenu.hpp"
#include "FieldDos.hpp"
#include "FieldDosViewport.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxBoot.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static void pump(std::uint8_t* ram, int frames = 3) {
    for (int i = 0; i < frames; ++i)
        FieldAmouranthExec::execPending(ram);
}

static void taskbarStartCenter(float& x, float& y) noexcept {
    float btnY0 = 0.f, btnY1 = 0.f;
    FieldAmouranthOs::taskbarChromeButtonY(btnY0, btnY1);
    x = FieldAmouranthOs::scaledStartW() * 0.5f;
    y = (btnY0 + btnY1) * 0.5f;
}

static void quickLaunchCenter(int qi, float& x, float& y) noexcept {
    float btnY0 = 0.f, btnY1 = 0.f;
    FieldAmouranthOs::taskbarChromeButtonY(btnY0, btnY1);
    const float btnW = FieldAmouranthOs::scaledQuickBtnW();
    x = FieldAmouranthOs::quickLaunchX(qi) + btnW * 0.5f;
    y = (btnY0 + btnY1) * 0.5f;
}

static void tabSlotCenter(int tabSlot, float& x, float& y) noexcept {
    const auto L = FieldAmouranthOs::taskbarLayout();
    const float tx = FieldTaskbarLayout::tabX(L, tabSlot);
    x = tx + L.tabW * 0.5f;
    y = L.tabY + L.tabH * 0.5f;
}

static bool clickTaskbar(float x, float y) noexcept {
    return FieldAmouranthOs::onTaskbarMouseDown(nullptr, x, y);
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

    const FieldAosAppIdentity::AppId pollutedApps[] = {
        FieldAosAppIdentity::AppId::Shell,
        FieldAosAppIdentity::AppId::AmmoCode,
        FieldAosAppIdentity::AppId::PadTest,
        FieldAosAppIdentity::AppId::Nes,
        FieldAosAppIdentity::AppId::FileCmd };
    const char* pollutedSnaps[] = { "scr=0", "", "raw=0", "", "L=C:\\,R=E:\\,p=0" };
    FieldAosAppJournal::saveSession(pollutedApps, pollutedSnaps, 5,
        FieldAosAppIdentity::AppId::FileCmd);

    FieldRegistry::init();
    FieldRegistry::setValue("HKRTX\\User\\Desktop", "RestoreSession", "0");
    FieldAosAppJournal::loadConfigFromRegistry();

    FieldAmouranthOs::boot();
    FieldAmouranthOs::tick(1920, 1080);

    if (!FieldAmouranthOs::programs.empty()) {
        std::fprintf(stderr, "FAIL boot should not restore taskbar tabs (got %zu)\n",
            FieldAmouranthOs::programs.size());
        return 1;
    }

    FieldAmouranthOs::packChromeRam(ram);
    if (ram[FieldAmouranthMenu::TASKBAR_RAM] != 0u) {
        std::fprintf(stderr, "FAIL TASKBAR_RAM tab count should be 0 at boot, got %u\n",
            ram[FieldAmouranthMenu::TASKBAR_RAM]);
        return 1;
    }
    for (int i = 0; i < FieldAmouranthMenu::MAX_TABS; ++i) {
        const std::uint32_t base = FieldAmouranthMenu::TASKBAR_RAM + 0x10u
            + static_cast<std::uint32_t>(i * FieldAmouranthMenu::TAB_STRIDE);
        if (ram[base + FieldAmouranthMenu::TAB_STRIDE - 1u] != 0u) {
            std::fprintf(stderr, "FAIL ghost tab icon in slot %d (icon=%u)\n",
                i, ram[base + FieldAmouranthMenu::TAB_STRIDE - 1u]);
            return 1;
        }
    }
    std::printf("OK clean boot — zero taskbar tabs in RAM\n");

    float sx = 0.f, sy = 0.f;
    taskbarStartCenter(sx, sy);
    if (!clickTaskbar(sx, sy) || !FieldAmouranthOs::startOpen) {
        std::fprintf(stderr, "FAIL Start click (%.1f,%.1f) startOpen=%d\n",
            sx, sy, FieldAmouranthOs::startOpen ? 1 : 0);
        return 1;
    }
    std::printf("OK Start button opens menu\n");

    clickTaskbar(sx, sy);
    if (FieldAmouranthOs::startOpen) {
        std::fprintf(stderr, "FAIL Start toggle close\n");
        return 1;
    }

    float fx = 0.f, fy = 0.f;
    quickLaunchCenter(0, fx, fy);
    if (!clickTaskbar(fx, fy) || !FieldAmouranthOs::filesOpen) {
        std::fprintf(stderr, "FAIL Files quick-launch (%.1f,%.1f)\n", fx, fy);
        return 1;
    }
    std::printf("OK Files quick-launch opens flyout\n");
    FieldAmouranthOs::filesOpen = false;

    FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Shell);
    pump(ram);
    FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::AmmoCode);
    pump(ram);

    if (FieldAmouranthOs::programs.size() < 2u) {
        std::fprintf(stderr, "FAIL expected 2 running programs, got %zu\n",
            FieldAmouranthOs::programs.size());
        return 1;
    }

    FieldAmouranthOs::programs[0].running = false;
    float tabX = 0.f, tabY = 0.f;
    tabSlotCenter(0, tabX, tabY);
    const int hoverSlot = FieldAmouranthOs::taskBtnIndex(tabX, tabY);
    if (hoverSlot != 0) {
        std::fprintf(stderr, "FAIL tab hover slot with skipped program — got %d expected 0\n",
            hoverSlot);
        return 1;
    }
    const int progIdx = FieldAmouranthOs::programIndexFromTaskTab(hoverSlot);
    if (progIdx != 1) {
        std::fprintf(stderr, "FAIL programIndexFromTaskTab — got %d expected 1\n", progIdx);
        return 1;
    }
    std::printf("OK tab slot maps to program index %d (skipped non-running entry)\n", progIdx);

    float tx = 0.f, ty = 0.f;
    tabSlotCenter(0, tx, ty);
    FieldAmouranthOs::focusedProgId = FieldAmouranthOs::programs[1].id;
    FieldAmouranthOs::focusedApp = FieldAmouranthOs::programs[1].app;
    FieldAmouranthOs::programs[1].minimized = false;
    FieldAmouranthOs::panelVisible = true;
    FieldAmouranthOs::onMouseMotion(nullptr, tx, ty);
    if (FieldAmouranthOs::hover != FieldAmouranthOs::HitZone::TaskBtn) {
        std::fprintf(stderr, "FAIL tab hover zone got %d at (%.1f,%.1f)\n",
            static_cast<int>(FieldAmouranthOs::hover), tx, ty);
        return 1;
    }
    if (!clickTaskbar(tx, ty)) {
        std::fprintf(stderr, "FAIL tab click not handled\n");
        return 1;
    }
    if (!FieldAmouranthOs::programs[1].minimized || FieldAmouranthOs::panelVisible) {
        std::fprintf(stderr, "FAIL focused tab click should minimize (min=%d panel=%d)\n",
            FieldAmouranthOs::programs[1].minimized ? 1 : 0,
            FieldAmouranthOs::panelVisible ? 1 : 0);
        return 1;
    }
    std::printf("OK tab click minimizes focused window\n");

    FieldAmouranthOs::packChromeRam(ram);
    const std::uint32_t tabCount = ram[FieldAmouranthMenu::TASKBAR_RAM];
    if (tabCount != 1u) {
        std::fprintf(stderr, "FAIL packed tab count %u expected 1 (one running program)\n", tabCount);
        return 1;
    }
    std::printf("OK packed tab count=%u after skipping non-running program\n", tabCount);

    Options::SDL3::RequestQuit = false;
    FieldAmouranthExitConfirm::forceClose();
    FieldAmouranthOs::panelVisible = false;

    taskbarStartCenter(sx, sy);
    if (!clickTaskbar(sx, sy) || !FieldAmouranthOs::startOpen) {
        std::fprintf(stderr, "FAIL Start reopen for shutdown test\n");
        return 1;
    }

    FieldAmouranthMenu::rebuildRoot();
    int exitRow = -1;
    for (int i = 0; i < FieldAmouranthMenu::rootCount; ++i) {
        if (FieldAmouranthMenu::rootRows[i].type == FieldAmouranthMenu::RowType::Exit) {
            exitRow = i;
            break;
        }
    }
    if (exitRow < 0) {
        std::fprintf(stderr, "FAIL Shut Down row missing from Start menu\n");
        return 1;
    }

    const float menuX = FieldAmouranthOs::scaledMenuPad() + FieldAmouranthOs::scaledMenuW() * 0.5f;
    const float menuY = FieldAmouranthOs::scaledMenuRootRowY0()
        + static_cast<float>(exitRow) * FieldAmouranthOs::scaledMenuRowH()
        + FieldAmouranthOs::scaledMenuRowH() * 0.5f;
    if (!FieldAmouranthOs::onMouseDown(nullptr, menuX, menuY, 1, 1)) {
        std::fprintf(stderr, "FAIL Shut Down menu click not handled\n");
        return 1;
    }
    if (!FieldAmouranthExitConfirm::isOpen()) {
        std::fprintf(stderr, "FAIL exit confirm not open after Shut Down (row %d at %.1f,%.1f)\n",
            exitRow, menuX, menuY);
        return 1;
    }
    if (FieldAmouranthOs::panelVisible) {
        std::fprintf(stderr, "FAIL exit confirm must not raise DOS panel (hotswap freeze)\n");
        return 1;
    }
    std::printf("OK Start menu Shut Down opens chrome exit dialog\n");

    float yesX = 0.f, yesY = 0.f, pw = 0.f, ph = 0.f;
    float px0 = 0.f, py0 = 0.f;
    FieldAmouranthOs::exitConfirmPanelBounds(px0, py0, pw, ph);
    const float scale = FieldAmouranthOs::uiScale();
    const float btnW = 148.f * scale;
    const float btnH = 40.f * scale;
    const float btnY = py0 + ph - btnH - 24.f * scale;
    const float mid = px0 + pw * 0.5f;
    yesX = mid - 12.f * scale - btnW * 0.5f;
    yesY = btnY + btnH * 0.5f;
    if (FieldAmouranthOs::exitConfirmButtonAt(yesX, yesY) != 1) {
        std::fprintf(stderr, "FAIL exit Yes button hit-test (%.1f,%.1f)\n", yesX, yesY);
        return 1;
    }
    if (!FieldAmouranthOs::onMouseDown(nullptr, yesX, yesY, 1, 1)) {
        std::fprintf(stderr, "FAIL exit Yes click not handled\n");
        return 1;
    }
    if (!Options::SDL3::RequestQuit) {
        std::fprintf(stderr, "FAIL RequestQuit not set after exit Yes\n");
        return 1;
    }
    std::printf("OK exit Yes sets RequestQuit (graceful shutdown)\n");

    std::printf("Taskbar click QA passed\n");
    return 0;
}