#pragma once

// AmmoNES — re-exports full shared CHIPS/Nes bundle.

#include "../CHIPS/Nes/FieldNes.hpp"

namespace AmmoNes {

using FieldChips::Nes::AudioStyle;
constexpr AudioStyle Authentic = AudioStyle::Classic;
using FieldChips::Nes::Cart;
using FieldChips::Nes::Cpu;
using FieldChips::Nes::Mirror;
using FieldChips::Nes::Ppu;
using FieldChips::Nes::Region;
using FieldChips::Nes::State;

using FieldChips::Nes::applyApuConfig;
using FieldChips::Nes::loadInes;
using FieldChips::Nes::renderAudioFrame;
using FieldChips::Nes::seedChrFromPrg;
using FieldChips::Nes::renderFrame;
using FieldChips::Nes::stepCpu;
using FieldChips::Nes::runFrameCpu;
using FieldChips::Nes::AccuracyTier;
using FieldChips::Nes::runAccuracyFrame;
using FieldChips::Nes::tierFromEnv;
using FieldChips::Nes::ntscFrameCpuCycles;
using FieldChips::Nes::mapperName;
using FieldChips::Nes::installNesVgaPalette;

} // namespace AmmoNes