#pragma once

#include "../CHIPS/Nes/FieldNesChip2C02.hpp"

namespace AmmoNes {
using FieldChips::Nes::installNesVgaPalette;
using FieldChips::Nes::nametableEmpty;
using FieldChips::Nes::nesPaletteToVga;
using FieldChips::Nes::renderFrame;
using FieldChips::Nes::sampleBgPixel;
using FieldChips::Nes::sampleSpritePixel;
using FieldChips::Nes::tickPpu;
} // namespace AmmoNes