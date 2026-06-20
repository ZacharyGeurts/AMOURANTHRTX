#pragma once

// AmouranthOS — RTX desktop shell: Start menu launches programs on demand.
// American-flag desktop at boot; windows open empty then load content.

#include "FieldAosStatusBar.hpp"
#include "FieldAmouranthFileCmd.hpp"
#include "FieldAmouranthInfo.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldDrives.hpp"
#include "FieldAmouranthHudRam.hpp"
#include "FieldAmouranthFolderView.hpp"
#include "FieldAmouranthMenu.hpp"
#include "FieldAmouranthFilesMenu.hpp"
#include "FieldAmouranthFileIndex.hpp"
#include "FieldAmouranthSearchFlyout.hpp"
#include "FieldAmouranthDnD.hpp"
#include "FieldAmouranthTextures.hpp"
#include "FieldExtensionMap.hpp"
#include "FieldX86Emu.hpp"
#include "FieldAmmoCode.hpp"
#include "FieldDosChrome.hpp"
#include "FieldDosViewport.hpp"
#include "FieldTaskbarLayout.hpp"
#include "FieldRtxWidgets.hpp"

#include "FieldAmmoNes.hpp"
#include "FieldAmmoA2600.hpp"
#include "FieldAmmoSms.hpp"
#include "FieldAmmoGenesis.hpp"
#include "FieldAmmoSnes.hpp"
#include "FieldWebPanel.hpp"
#include "FieldPadTest.hpp"
#include "FieldRuntimeInfo.hpp"
#include "FieldRtxBasic.hpp"
#include "FieldAmmoText.hpp"
#include "FieldMonacoEdit.hpp"
#include "FieldRtxEdit.hpp"
#include "FieldRtxThemePicker.hpp"

namespace FieldRtxShell { extern bool graphicsActive; }
#include "FieldRtxThemes.hpp"
#include "FieldRtxMemory.hpp"
#include "FieldCdRom.hpp"
#include "FieldXms.hpp"
#include "FieldEms.hpp"
#include "FieldMscdex.hpp"
#include "OptionsMenu.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FieldAmmoExec { bool isActive() noexcept; }

namespace FieldAmouranthExitConfirm {
bool isOpen() noexcept;
void show() noexcept;
void dismiss(std::uint8_t* ram) noexcept;
void forceClose() noexcept;
void confirmYes(std::uint8_t* ram) noexcept;
bool onKey(std::uint16_t key, std::uint8_t* ram) noexcept;
bool pumpMouse(std::uint8_t* ram, int& outAction) noexcept;
}
namespace FieldAmouranthShutdown {
void closeAllGuestApps(std::uint8_t* ram) noexcept;
}

#include "FieldAosMonitor.hpp"
#include "FieldAmmoBrowser.hpp"

namespace FieldAmouranthOs {

constexpr std::uint32_t BUS_AOS_FOLDER_VIEW  = 1u << 30u;
