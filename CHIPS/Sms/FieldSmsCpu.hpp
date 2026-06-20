#pragma once

#include "../Common/FieldChipZ80.hpp"
#include "../Common/FieldChipFieldExec.hpp"
#include "FieldSmsBus.hpp"

namespace FieldChips::Sms {

struct SmsCpuTraits {
    static std::uint8_t read(State& s, std::uint16_t addr) noexcept {
        return readMem(s, addr);
    }
    static void write(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
        writeMem(s, addr, v);
    }
    static std::uint8_t inPort(State& s, std::uint8_t port) noexcept {
        return readIo(s, port);
    }
    static void outPort(State& s, std::uint8_t port, std::uint8_t v) noexcept {
        writeIo(s, port, v);
    }
};

struct SmsDevices {
    static void endCpuFrame(State& s, int cpuCycles) noexcept {
        tickVdp(s, cpuCycles);
        sn76489Clock(s.psg, cpuCycles);
    }
};

inline int stepCpu(State& s) noexcept {
    return stepZ80<State, SmsCpuTraits>(s);
}

inline constexpr int kFrameCycles = 3579545 / 60;

inline void runFrameCpu(State& s, int targetCycles = kFrameCycles) noexcept {
    runZ80FieldFrame<State, SmsCpuTraits, SmsDevices>(s, targetCycles);
}

} // namespace FieldChips::Sms