#pragma once

#include "FieldPs1Types.hpp"

#include <cstdio>
#include <cstring>

namespace FieldChips::Ps1 {

inline bool detectPsxExe(const std::uint8_t* data, std::size_t size) noexcept {
    return data && size >= 0x800u
        && std::memcmp(data, "PS-X EXE", 8) == 0;
}

inline bool loadCart(State& s, const std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size < 2048u) return false;
    resetState(s);
    s.cart.rom.assign(data, data + size);
    if (detectPsxExe(data, size)) {
        s.cart.psxExe = true;
        s.cart.loadAddr = *reinterpret_cast<const std::uint32_t*>(data + 0x10);
        s.cart.entryPc = *reinterpret_cast<const std::uint32_t*>(data + 0x14);
        std::memcpy(s.cart.title, data + 0x4A, 60);
        s.cart.title[31] = '\0';
        const std::size_t copyLen = std::min<std::size_t>(size - 0x800u, sizeof s.ram);
        std::memcpy(s.ram, data + 0x800u, copyLen);
        s.cpu.pc = s.cart.entryPc;
        return true;
    }
    if (size >= 2352u && std::memcmp(data + 1, "CD001", 5) == 0) {
        s.cart.cdImage = true;
        std::snprintf(s.cart.title, sizeof s.cart.title, "PS1 CD IMAGE");
        s.cpu.pc = 0x80030000u;
        return true;
    }
    std::snprintf(s.cart.title, sizeof s.cart.title, "PS1 RAW ROM");
    std::memcpy(s.ram, data, std::min(size, sizeof s.ram));
    s.cpu.pc = 0x80010000u;
    s.cart.entryPc = s.cpu.pc;
    return true;
}

} // namespace FieldChips::Ps1