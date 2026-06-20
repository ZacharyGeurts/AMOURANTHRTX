#pragma once

// Graceful teardown — close every RTX guest app before AmmoOS / engine exit.

#include "FieldDosViewport.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldAmouranthWm.hpp"
#include "FieldAmouranthFileCmd.hpp"
#include "FieldAosMonitor.hpp"
#include "FieldAmmoBrowser.hpp"
#include "FieldAmmoCode.hpp"
namespace FieldAmmoExec {
bool isActive() noexcept;
void close(std::uint8_t* ram) noexcept;
}
#include "FieldAmmoNes.hpp"
#include "FieldAmmoA2600.hpp"
#include "FieldAmmoSms.hpp"
#include "FieldAmmoGenesis.hpp"
#include "FieldAmmoSnes.hpp"
#include "FieldAmmoText.hpp"
#include "FieldMonacoEdit.hpp"
#include "FieldPadTest.hpp"
#include "FieldRtxBasic.hpp"
#include "FieldRtxConsoleGui.hpp"
#include "FieldRtxThemePicker.hpp"
#include "FieldDevices.hpp"
#include "FieldRtxApp.hpp"
#include "FieldRtxWidgets.hpp"
#include "FieldWinApp.hpp"
#include "FieldX86Emu.hpp"
#include "OptionsMenu.hpp"

#include <cstdio>

namespace FieldRtxShell { extern bool graphicsActive; }

namespace FieldAmouranthShutdown {

inline void closeSetup(bool& active, std::uint8_t* ram) noexcept {
    if (!active) return;
    active = false;
    if (ram) FieldRtxWidgets::clearRam(ram);
    FieldRtxApp::resetUi();
    FieldWinApp::reset();
}

inline void closeAllGuestApps(std::uint8_t* ram) noexcept {
    if (!ram) ram = FieldX86Emu::ramHost;

    FieldAmouranthLaunch::clear();
    FieldAmouranthWm::closeRequested = false;
    FieldAmouranthWm::pendingMenuAction = 0;

    if (ram) {
        if (FieldAosMonitor::active) FieldAosMonitor::close(ram);
        if (FieldPadTest::active) FieldPadTest::close(ram);
        if (FieldNes::active) FieldNes::close(ram);
        if (FieldA2600::active) FieldA2600::close(ram);
        if (FieldSms::active) FieldSms::close(ram);
        if (FieldGenesis::active) FieldGenesis::close(ram);
        if (FieldSnes::active) FieldSnes::close(ram);
        closeSetup(FieldAmmoNesSetup::active, ram);
        closeSetup(FieldAmmoA2600Setup::active, ram);
        closeSetup(FieldAmmoSmsSetup::active, ram);
        closeSetup(FieldAmmoGenesisSetup::active, ram);
        closeSetup(FieldAmmoSnesSetup::active, ram);
    } else {
        FieldAmmoNesSetup::active = false;
        FieldAmmoA2600Setup::active = false;
        FieldAmmoSmsSetup::active = false;
        FieldAmmoGenesisSetup::active = false;
        FieldAmmoSnesSetup::active = false;
        FieldPadTest::active = false;
        FieldAosMonitor::active = false;
    }

    if (FieldAmmoExec::isActive() && ram)
        FieldAmmoExec::close(ram);

    if (ram && FieldMonacoEdit::active)
        FieldMonacoEdit::close(ram);
    FieldAmmoText::active = false;
    FieldAmmoCode::active = false;
    FieldRtxBasic::active = false;
    FieldRtxThemePicker::close();
    FieldRtxShell::graphicsActive = false;
    FieldRtxConsoleGui::close();
    FieldAmmoBrowser::close();
    FieldAmouranthFileCmd::close();

    FieldDevices::reset();

    FieldDosViewport::clearEmuViewport();

    if (ram) {
        FieldRtxWidgets::clearRam(ram);
        FieldRtxApp::resetUi();
        FieldWinApp::reset();
    }
}

} // namespace FieldAmouranthShutdown