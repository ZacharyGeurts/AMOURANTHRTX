#pragma once

#include "../CHIPS/Nes/FieldNesChipBus.hpp"

namespace AmmoNes {
using FieldChips::Nes::mirrorNametableAddr;
using FieldChips::Nes::ppuWriteReg;
using FieldChips::Nes::readCpuMem;
using FieldChips::Nes::readPpuMem;
using FieldChips::Nes::writeCpuMem;
using FieldChips::Nes::writePpuMem;
} // namespace AmmoNes