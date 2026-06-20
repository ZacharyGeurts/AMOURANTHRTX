#pragma once

// AmmoNES — re-exports shared CHIPS/Nes state types.

#include "../CHIPS/Nes/FieldNesChipTypes.hpp"

namespace AmmoNes {
using FieldChips::Nes::AudioStyle;
using FieldChips::Nes::Cart;
using FieldChips::Nes::Cpu;
using FieldChips::Nes::MapperRegs;
using FieldChips::Nes::Mirror;
using FieldChips::Nes::Ppu;
using FieldChips::Nes::Region;
using FieldChips::Nes::State;
using FieldChips::Nes::resetState;
} // namespace AmmoNes