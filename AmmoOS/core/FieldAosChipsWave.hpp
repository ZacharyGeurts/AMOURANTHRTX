#pragma once

// AmouranthOS — CHIPS wave + Amiga Love launchers (avoids circular include with FieldAmouranthOs).

#include "FieldAmmoAmiga.hpp"
#include "FieldEverything.hpp"

#include <cstdint>
#include <vector>

namespace FieldAosChipsWave {

inline void seedAmigaKick() noexcept {
    std::vector<std::uint8_t> kick(8192, 0x4Eu);
    (void)FieldChips::Amiga::loadKickstart(FieldAmiga::chip, kick.data(), kick.size());
}

inline void openAmigaLove() noexcept {
    FieldAmiga::open(true);
    seedAmigaKick();
}

inline void openLoveOfEverything() noexcept {
    FieldEverything::open();
}

} // namespace FieldAosChipsWave