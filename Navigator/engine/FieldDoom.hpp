#pragma once

// Shareware Doom — AmmoDOS launcher alias.

#include "FieldAmmoExec.hpp"
#include "FieldDos.hpp"

#include <vector>

namespace FieldDoom {

inline bool sharewareReady() noexcept {
    std::vector<std::uint8_t> exe;
    return FieldDos::readHostFile("C:\\DOOM.EXE", exe) && exe.size() >= 2u
        && exe[0] == 'M' && exe[1] == 'Z';
}

inline bool launch(void* mapped, std::size_t offset, std::uint8_t* ram) noexcept {
    return FieldAmmoExec::launch(mapped, offset, ram, "C:\\GAMES\\DOOM\\DOOM.EXE");
}

inline bool launchFromGuest(std::uint8_t* ram) noexcept {
    return FieldAmmoExec::launchFromGuest(ram, "C:\\GAMES\\DOOM\\DOOM.EXE");
}

inline bool isActive() noexcept { return FieldAmmoExec::isActive(); }
inline void close(std::uint8_t* ram) noexcept { FieldAmmoExec::close(ram); }

inline void pump(std::uint8_t* ram, void* mapped, std::size_t offset,
                 std::uint32_t hostKey, bool keyDown, std::uint32_t cycles) noexcept {
    FieldAmmoExec::pump(ram, mapped, offset, hostKey, keyDown, cycles);
}

inline std::uint32_t hostCyclesPerPump() noexcept {
    return FieldAmmoExec::gpuCyclesPerFrame();
}

} // namespace FieldDoom