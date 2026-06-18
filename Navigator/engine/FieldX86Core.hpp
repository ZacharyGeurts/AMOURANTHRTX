#pragma once

// Field RTX x86 core — owned RM/PM32 decode+exec (vendored libx86emu, gutted for Field Die).
// Public include for host CPU / DOS paths. Implementation lives in Navigator/engine/FieldX86Core/.

#include "FieldPlatform.hpp"

#include <cstdint>

extern "C" {
#include <x86emu.h>
}

namespace FieldX86 {

using Cpu = x86emu_t;

inline Cpu* create(std::uint32_t memPerm = X86EMU_PERM_RWX) noexcept {
    return x86emu_new(memPerm, X86EMU_PERM_R | X86EMU_PERM_W);
}

inline void destroy(Cpu* cpu) noexcept {
    if (cpu) x86emu_done(cpu);
}

inline std::uint64_t run(Cpu* cpu, std::uint64_t maxInstr) noexcept {
    if (!cpu) return 0u;
    const u64 t0 = cpu->x86.R_TSC;
    cpu->max_instr = t0 + maxInstr;
    cpu->x86.mode &= ~_MODE_HALTED;
    (void)x86emu_run(cpu, X86EMU_RUN_MAX_INSTR);
    return cpu->x86.R_TSC - t0;
}

} // namespace FieldX86