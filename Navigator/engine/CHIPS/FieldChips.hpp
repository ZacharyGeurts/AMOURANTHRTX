#pragma once

// FieldChips — shared emulator silicon for AmouranthOS.
//
//   Common/     — Golden Era CPUs & audio (6502, Z80, 68000, SN76489, YM2612, SID, …)
//   Nes/        — Nintendo Entertainment System (2A03, 2C02, mappers)
//   Atari2600/  — Atari VCS (6507, TIA, RIOT 6532)
//   Sms/        — Sega Master System (Z80, VDP, SN76489)
//   Genesis/    — Sega Genesis / Mega Drive (68000, Z80, YM2612, SN76489, VDP)
//   Apple2/     — Apple II scaffold
//   C64/        — Commodore 64 scaffold (6581 SID)
//   Spectrum/   — ZX Spectrum scaffold
//   Msx/        — MSX scaffold
//   GameBoy/    — Nintendo Game Boy scaffold
//   Snes/       — Super Nintendo scaffold
//   PS1/        — Sony PlayStation (MIPS R3000, GTE, FieldDie GPU wave)
//   N64/        — Nintendo 64 (VR4300, RSP/RDP FieldDie wave)
//   Dreamcast/  — Sega Dreamcast (SH-4, PowerVR2 FieldDie wave)
//   PS2/        — PlayStation 2 (EE/IOP, GS FieldDie wave)
//   Xbox360/    — Microsoft Xbox 360 (Xenon + Xenos FieldDie wave)
//   Amiga/      — Commodore Amiga 68000 + Paula + Denise/AGA

#include "Common/FieldChipsCommon.hpp"
#include "Nes/FieldNes.hpp"
#include "Atari2600/FieldAtari2600.hpp"
#include "Sms/FieldSms.hpp"
#include "Genesis/FieldGenesis.hpp"
#include "Apple2/FieldApple2.hpp"
#include "C64/FieldC64.hpp"
#include "Spectrum/FieldSpectrum.hpp"
#include "Msx/FieldMsx.hpp"
#include "GameBoy/FieldGameBoy.hpp"
#include "Snes/FieldSnes.hpp"
#include "PS1/FieldPS1.hpp"
#include "N64/FieldN64.hpp"
#include "Dreamcast/FieldDreamcast.hpp"
#include "PS2/FieldPs2.hpp"
#include "Xbox360/FieldXbox360.hpp"
#include "Amiga/FieldAmiga.hpp"