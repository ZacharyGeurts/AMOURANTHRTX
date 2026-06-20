#pragma once

// AmmoSNES — re-exports CHIPS/Snes bundle.

#include "../CHIPS/Snes/FieldSnes.hpp"

namespace AmmoSnes {

using FieldChips::Snes::AudioStyle;
constexpr AudioStyle Authentic = AudioStyle::Classic;

using FieldChips::Snes::Cart;
using FieldChips::Snes::Gsu;
using FieldChips::Snes::State;

using FieldChips::Snes::applyAudioConfig;
using FieldChips::Snes::detectSuperFxRom;
using FieldChips::Snes::loadRom;
using FieldChips::Snes::renderAudioFrame;
using FieldChips::Snes::renderFrame;
using FieldChips::Snes::runFrameCpu;
using FieldChips::Snes::runGsuFrame;
using FieldChips::Snes::installSnesVgaPalette;

} // namespace AmmoSnes