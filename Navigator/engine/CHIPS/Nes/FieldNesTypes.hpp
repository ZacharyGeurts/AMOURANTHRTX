#pragma once

// NES console state — Ricoh 2A03 (6502 + APU), 2C02 PPU, cart mappers.

#include "../Common/FieldChipTypes.hpp"
#include "../Common/FieldChipAudio.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldChips::Nes {

enum class Mirror : std::uint8_t { Horizontal = 0, Vertical = 1, SingleLo = 2, SingleHi = 3 };

enum class Region : std::uint8_t { Ntsc = 0, Pal = 1, Dendy = 2 };

using AudioStyle = FieldChips::AudioStyle;

struct Cart {
    std::vector<std::uint8_t> prg;
    std::vector<std::uint8_t> chr;
    int mapper = 0;
    int submapper = 0;
    int prgBanks = 0;
    int chrBanks = 0;
    bool trainer = false;
    bool battery = false;
    Mirror mirror = Mirror::Horizontal;
};

struct MapperRegs {
    int prgBank = 0;
    int chrBank = 0;
    int prgMode = 0;
    int chrMode = 0;
    std::uint8_t mmc1Shift = 0;
    int mmc1WriteCount = 0;
    std::uint8_t mmc3Reg[8]{};
    std::uint8_t mmc3Next = 0;
    bool mmc3PrgRamEnable = false;
    int mmc3PrgBank[2]{0, 1};
    int mmc3ChrBank[8]{0, 1, 2, 3, 4, 5, 6, 7};
    int mmc3IrqLatch = 0;
    int mmc3IrqCounter = 0;
    bool mmc3IrqEnable = false;
    bool mmc3IrqReload = false;
    int axromBank = 0;
    int cnromChrBank = 0;
    int gnromPrgBank = 0;
    int gnromChrBank = 0;
};

struct PulseChan {
    std::uint8_t reg[4]{};
    bool enabled = false;
    int timer = 0;
    int timerPeriod = 0;
    int dutyPos = 0;
    int lengthCounter = 0;
    int envelopeVolume = 0;
    int envelopeDecay = 0;
    bool envelopeLoop = false;
    bool envelopeStart = false;
    bool sweepReload = false;
    int sweepPeriod = 0;
    int sweepShift = 0;
    bool sweepNegate = false;
    bool sweepEnable = false;
    bool sweepMute = false;
    float blipPhase = 0.f;
    float blipLast = 0.f;
};

struct TriangleChan {
    std::uint8_t reg[4]{};
    bool enabled = false;
    int timer = 0;
    int timerPeriod = 0;
    int seqPos = 0;
    int lengthCounter = 0;
    int linearCounter = 0;
    int linearReload = 0;
    bool linearReloadFlag = false;
    bool linearHalt = false;
};

struct NoiseChan {
    std::uint8_t reg[4]{};
    bool enabled = false;
    int timer = 0;
    int timerPeriod = 0;
    int lengthCounter = 0;
    int envelopeVolume = 0;
    int envelopeDecay = 0;
    bool envelopeLoop = false;
    bool envelopeStart = false;
    std::uint16_t lfsr = 1;
    bool mode = false;
};

struct DmcChan {
    std::uint8_t reg[4]{};
    bool enabled = false;
    int timer = 0;
    int timerPeriod = 0;
    int bytesRemaining = 0;
    int sampleAddr = 0;
    int sampleLen = 0;
    int shiftRegister = 0;
    int bitsRemaining = 0;
    bool irqPending = false;
    bool irqEnable = false;
    bool loop = false;
    int outputLevel = 0;
    bool silence = true;
};

struct Apu {
    PulseChan pulse[2]{};
    TriangleChan triangle{};
    NoiseChan noise{};
    DmcChan dmc{};
    std::uint8_t status = 0;
    int frameSeqStep = 0;
    int frameSeqCycle = 0;
    bool frameMode = false; // false=4-step, true=5-step
    bool frameIrqEnable = false;
    bool frameIrqPending = false;
    bool dmcIrq = false;
    int sampleRate = 44100;
    AudioStyle style = AudioStyle::Modern;
    int quality = 1;
    float modernLp = 0.f;
    float sampleAccum = 0.f;
    int sampleAccumCycles = 0;
    std::uint32_t sampleClock = 0;
};

struct Ppu {
    std::uint8_t ctrl = 0;
    std::uint8_t mask = 0;
    std::uint8_t status = 0;
    std::uint8_t oamAddr = 0;
    std::uint16_t vramAddr = 0;
    std::uint16_t scrollT = 0;
    std::uint16_t scrollV = 0;
    std::uint8_t fineX = 0;
    std::uint8_t scrollX = 0;
    std::uint8_t scrollY = 0;
    int addrLatch = 0;
    std::uint8_t oam[256]{};
    std::uint8_t vram[0x800]{};
    std::uint8_t oamCache[8 * 4]{};
    int oamCacheLen = 0;
    int scanline = 0;
    int cycle = 0;
    int frame = 0;
    bool nmiPending = false;
    bool sprite0Hit = false;
    bool spriteOverflow = false;
    int spriteCount = 0;
};

using Cpu = FieldChips::Cpu6502;

struct State {
    Cart cart;
    MapperRegs map;
    Ppu ppu;
    Cpu cpu;
    Apu apu;
    std::uint8_t ram[0x800]{};
    std::uint8_t prgRam[0x2000]{};
    std::uint8_t pad1 = 0;
    std::uint8_t pad1Shift = 0;
    std::uint8_t pad2 = 0;
    std::uint8_t pad2Shift = 0;
    Mirror mirroring = Mirror::Horizontal;
    bool irqLine = false;
    Region region = Region::Ntsc;
    bool spriteLimit = true;
};

inline void resetState(State& s) noexcept {
    s.map = MapperRegs{};
    s.ppu = Ppu{};
    FieldChips::reset6502(s.cpu);
    s.apu = Apu{};
    s.apu.sampleRate = 44100;
    std::memset(s.ram, 0, sizeof s.ram);
    std::memset(s.prgRam, 0, sizeof s.prgRam);
    s.pad1Shift = s.pad2Shift = 0;
    s.irqLine = false;
    s.mirroring = s.cart.mirror;
}

} // namespace FieldChips::Nes