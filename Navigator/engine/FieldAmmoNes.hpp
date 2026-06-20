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

// FULL_FILE_MARKER
