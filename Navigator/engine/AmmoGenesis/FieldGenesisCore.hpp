#pragma once

// AmmoGenesis — re-exports CHIPS/Genesis bundle.

#include "../CHIPS/Genesis/FieldGenesis.hpp"

namespace AmmoGenesis {

using FieldChips::Genesis::AudioStyle;
constexpr AudioStyle Authentic = AudioStyle::Classic;

using FieldChips::Genesis::Cart;
using FieldChips::Genesis::State;
using FieldChips::Genesis::Vdp;

using FieldChips::Genesis::applyAudioConfig;
using FieldChips::Genesis::loadRom;
using FieldChips::Genesis::renderAudioFrame;
using FieldChips::Genesis::renderFrame;
using FieldChips::Genesis::runFrameCpu;
using FieldChips::Genesis::stepCpu;

} // namespace AmmoGenesis