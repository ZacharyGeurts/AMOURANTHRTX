#pragma once

// AmmoA2600 — re-exports CHIPS/Atari2600 bundle.

#include "../CHIPS/Atari2600/FieldAtari2600.hpp"

namespace AmmoA2600 {

using FieldChips::Atari2600::AudioStyle;
constexpr AudioStyle Authentic = AudioStyle::Classic;
using FieldChips::Atari2600::Cart;
using FieldChips::Atari2600::Cpu;
using FieldChips::Atari2600::State;

using FieldChips::Atari2600::applyAudioConfig;
using FieldChips::Atari2600::loadRom;
using FieldChips::Atari2600::renderAudioFrame;
using FieldChips::Atari2600::renderFrame;
using FieldChips::Atari2600::runFrameCpu;
using FieldChips::Atari2600::stepCpu;

} // namespace AmmoA2600