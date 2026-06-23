#pragma once

// Headless/visual QA driver for AmouranthOS — env AMOURANTHRTX_AOS_TEST=1

#include "FieldAmouranthMenu.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldAmouranthOs.hpp"
#include "FieldAmouranthExec.hpp"
#include "FieldAmouranthFileCmd.hpp"
#include "FieldAmmoNes.hpp"
#include "FieldDosViewport.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <cstdlib>

namespace FieldAosTest {

inline bool enabled() noexcept {
    return std::getenv("AMOURANTHRTX_AOS_TEST") != nullptr;
}

inline bool nesLaunchTest() noexcept {
    return std::getenv("AMOURANTHRTX_AOS_NES_TEST") != nullptr;
}

inline bool folderLaunchTest() noexcept {
    return std::getenv("AMOURANTHRTX_AOS_FOLDER_TEST") != nullptr;
}

inline bool shellLaunchTest() noexcept {
    return std::getenv("AMOURANTHRTX_AOS_SHELL_TEST") != nullptr;
}

inline bool menuLaunchTest() noexcept {
    return std::getenv("AMOURANTHRTX_AOS_MENU_TEST") != nullptr;
}

inline void clickStartButton() noexcept {
    float btnY0 = 0.f, btnY1 = 0.f;
    FieldAmouranthOs::taskbarChromeButtonY(btnY0, btnY1);
    const float startX = FieldAmouranthOs::scaledStartW() * 0.5f;
    const float startY = (btnY0 + btnY1) * 0.5f;
    FieldAmouranthOs::onTaskbarMouseDown(nullptr, startX, startY);
}

inline void clickFilesButton() noexcept {
    float btnY0 = 0.f, btnY1 = 0.f;
    FieldAmouranthOs::taskbarChromeButtonY(btnY0, btnY1);
    const float x = FieldAmouranthOs::quickLaunchX(0)
        + FieldAmouranthOs::scaledQuickBtnW() * 0.5f;
    const float y = (btnY0 + btnY1) * 0.5f;
    FieldAmouranthOs::onTaskbarMouseDown(nullptr, x, y);
}

inline void clickFolderMenuItem() noexcept {
    const float x = FieldAmouranthOs::scaledFilesPanelX()
        + FieldAmouranthOs::scaledFilesPad()
        + FieldAmouranthOs::scaledFilesCellW() * 0.5f;
    const float y = FieldAmouranthOs::scaledFilesPanelTop()
        + FieldAmouranthOs::scaledFilesHeadH()
        + FieldAmouranthOs::scaledFilesPad()
        + FieldAmouranthOs::scaledFilesCellH() * 0.5f;
    FieldAmouranthOs::onMouseDown(nullptr, x, y, 1, 1);
}

inline void clickDesktop(float x, float y) noexcept {
    FieldAmouranthOs::onMouseDown(nullptr, x, y, 1, 1);
}

inline int categoryRootRow(FieldAmouranthMenu::Category cat) noexcept {
    FieldAmouranthMenu::rebuildRoot();
    for (int i = 0; i < FieldAmouranthMenu::rootCount; ++i) {
        if (FieldAmouranthMenu::rootRows[i].type == FieldAmouranthMenu::RowType::Category
                && FieldAmouranthMenu::rootRows[i].cat == cat)
            return i;
    }
    return 0;
}

inline int gamesRootRow() noexcept {
    return categoryRootRow(FieldAmouranthMenu::Category::Games);
}

inline int programsRootRow() noexcept {
    return categoryRootRow(FieldAmouranthMenu::Category::Programs);
}

inline void tickFrame(std::uint64_t frame, int w, int h) noexcept {
    if (!enabled()) return;
    if (!FieldAmouranthOs::shellChromeActive() && frame < 2u)
        FieldAmouranthOs::boot();
    FieldAmouranthOs::tick(w, h);

    if (frame == 10u) {
        if (shellLaunchTest() && !FieldAmouranthOs::hasShellProgram()) {
            FieldAmouranthOs::openNewWindow(FieldAmouranthOs::AppId::Shell);
            FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Shell, false, 0);
        } else if (FieldX86Emu::ramHost && !FieldAmouranthOs::hasShellProgram())
            FieldAmouranthExec::launchGui(FieldAmouranthOs::AppId::Shell, FieldX86Emu::ramHost);
    }

    if (frame == 24u) {
        if (folderLaunchTest())
            clickFilesButton();
        else if (nesLaunchTest() || menuLaunchTest())
            clickStartButton();
        return;
    }

    if (!nesLaunchTest() && !folderLaunchTest() && !menuLaunchTest() && !shellLaunchTest()) {
        if (frame >= 32u && (frame % 8u) == 0u) {
            std::fprintf(stderr,
                "[AOS_TEST] frame=%llu panel=%d shell=%d prog=%d ox=%.0f oy=%.0f\n",
                static_cast<unsigned long long>(frame),
                FieldAmouranthOs::panelVisible ? 1 : 0,
                FieldAmouranthOs::hasShellProgram() ? 1 : 0,
                FieldAmouranthOs::focusedProgId,
                FieldDosViewport::panelOx, FieldDosViewport::panelOy);
        }
        return;
    }

    if (folderLaunchTest()) {
        if (frame == 26u && FieldAmouranthOs::filesOpen) {
            clickFolderMenuItem();
            return;
        }
        if (frame >= 32u && (frame % 8u) == 0u) {
            std::fprintf(stderr,
                "[AOS_TEST] frame=%llu panel=%d filecmd=%d prog=%d ox=%.0f oy=%.0f\n",
                static_cast<unsigned long long>(frame),
                FieldAmouranthOs::panelVisible ? 1 : 0,
                FieldAmouranthFileCmd::active ? 1 : 0,
                FieldAmouranthOs::focusedProgId,
                FieldDosViewport::panelOx, FieldDosViewport::panelOy);
        }
        return;
    }

    if (shellLaunchTest()) {
        if (frame >= 32u && (frame % 8u) == 0u) {
            std::fprintf(stderr,
                "[AOS_TEST] frame=%llu panel=%d shell=%d prog=%d\n",
                static_cast<unsigned long long>(frame),
                FieldAmouranthOs::panelVisible ? 1 : 0,
                FieldAmouranthOs::hasShellProgram() ? 1 : 0,
                FieldAmouranthOs::focusedProgId);
        }
        return;
    }

    if (!nesLaunchTest()) return;

    const float rowH = FieldAmouranthOs::scaledMenuRowH();
    const float menuX = FieldAmouranthOs::scaledMenuPad()
        + FieldAmouranthOs::scaledMenuW() * 0.5f;

    if (frame == 26u && FieldAmouranthOs::startOpen) {
        const int gamesRow = gamesRootRow();
        const float rowY = FieldAmouranthOs::scaledMenuRootRowY0()
            + rowH * static_cast<float>(gamesRow) + rowH * 0.5f;
        clickDesktop(menuX, rowY);
        return;
    }

    if (frame == 28u && FieldAmouranthMenu::flyoutOpen()) {
        const float flyX = FieldAmouranthOs::scaledMenuPad()
            + FieldAmouranthOs::scaledMenuW()
            + FieldAmouranthOs::scaledMenuFlyoutGap()
            + FieldAmouranthOs::scaledMenuFlyoutW() * 0.5f;
        const float rowY = FieldAmouranthOs::scaledMenuFlyoutTop()
            + FieldAmouranthOs::scaledMenuPad() + rowH * 0.5f;
        clickDesktop(flyX, rowY);
        return;
    }

    if (frame >= 32u && (frame % 8u) == 0u) {
        std::fprintf(stderr,
            "[AOS_TEST] frame=%llu panel=%d nes=%d prog=%d ox=%.0f oy=%.0f\n",
            static_cast<unsigned long long>(frame),
            FieldAmouranthOs::panelVisible ? 1 : 0,
            FieldNes::active ? 1 : 0,
            FieldAmouranthOs::focusedProgId,
            FieldDosViewport::panelOx, FieldDosViewport::panelOy);
    }
}

} // namespace FieldAosTest