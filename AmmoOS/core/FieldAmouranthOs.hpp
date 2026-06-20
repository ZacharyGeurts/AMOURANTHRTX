#pragma once

// AmouranthOS — RTX desktop shell: Start menu launches programs on demand.
// Black desktop backdrop at boot; windows open empty then load content.

#include "FieldAosStatusBar.hpp"
#include "FieldAmouranthFileCmd.hpp"
#include "FieldAmouranthInfo.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldDrives.hpp"
#include "FieldAmouranthHudRam.hpp"
#include "FieldAmouranthMenu.hpp"
#include "FieldAmouranthTextures.hpp"
#include "FieldX86Emu.hpp"
#include "FieldAmmoCode.hpp"
#include "FieldDosChrome.hpp"
#include "FieldDosViewport.hpp"

#include "FieldAmmoNes.hpp"
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
#include "OptionsMenu.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FieldAmmoExec { bool isActive() noexcept; }

#include "FieldAmmoBrowser.hpp"

namespace FieldAmouranthOs {

constexpr float TASKBAR_H   = 52.f;
inline bool active = false;
inline void deactivate() noexcept;
inline void requestGracefulShutdown() noexcept;

} // namespace FieldAmouranthOs

#include "FieldAmouranthWm.hpp"
#include "FieldAmouranthDesktop.hpp"
