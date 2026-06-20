#pragma once

#include "../Common/FieldChipTypes.hpp"
#include "../Common/FieldChipAudio.hpp"
#include "../Common/FieldChipWdc65816.hpp"

#include <cstdint>
#include <vector>

namespace FieldChips::Snes {

using AudioStyle = FieldChips::AudioStyle;

enum class MapMode : std::uint8_t { LoRom = 0, HiRom = 1 };

struct Cart {
    std::vector<std::uint8_t> rom;
    MapMode map = MapMode::LoRom;
    bool hasSuperFx = false;
    char title[22]{};
};

struct Gsu {
    bool present = false;
    std::uint16_t r[16]{};
    std::uint8_t ram[0x20000]{};
    std::uint8_t sfr = 0;
    std::uint8_t scmr = 0;
    std::uint8_t colr = 0;
    std::uint8_t por = 0;
    std::uint8_t fro = 0;
    std::uint8_t scbr = 0xFC;
    std::uint8_t bramr = 0;
    std::uint8_t vcr = 0;
    std::uint8_t romb = 0;
    std::uint8_t ramb = 0;
    std::uint8_t cbr = 0;
    std::uint16_t pc = 0;
    std::uint8_t sp = 0;
    int cycles = 0;
    int plotOps = 0;
    int stepBudget = 4096;
    int plotBudget = 8192;
    int maxBurst = 4;
    bool irq = false;
};

struct Ppu {
    std::uint8_t vram[0x10000]{};
    std::uint8_t cgram[512]{};
    std::uint8_t oam[544]{};
    int frame = 0;
};

struct Apu {
    AudioStyle style = AudioStyle::Modern;
    int sampleRate = 44100;
    float modernLp = 0.f;
};

struct State {
    Cart cart;
    CpuWdc65816 cpu;
    Gsu gsu;
    Ppu ppu;
    Apu apu;
    std::uint8_t ram[0x20000]{};
    std::uint8_t pad1 = 0;
    AudioStyle audioStyle = AudioStyle::Modern;
    bool thermoGovernor = true;
    int thermoSteps = 1;
};

inline void resetState(State& s) noexcept {
    FieldChips::resetWdc65816(s.cpu);
    s.apu = Apu{};
    s.apu.style = s.audioStyle;
    s.ppu = Ppu{};
    s.gsu = Gsu{};
    s.gsu.present = s.cart.hasSuperFx;
    s.gsu.scbr = 0xFC;
}

} // namespace FieldChips::Snes