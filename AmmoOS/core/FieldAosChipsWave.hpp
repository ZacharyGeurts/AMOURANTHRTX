#pragma once

// AmouranthOS — CHIPS wave + Amiga Love launchers (avoids circular include with FieldAmouranthOs).

#include "FieldAmmoAmiga.hpp"
#include "FieldAmmoPs1.hpp"
#include "FieldAmmoXbox360.hpp"
#include "FieldStorage.hpp"

#include <cstdint>
#include <vector>

namespace FieldAosChipsWave {

inline void seedAmigaKick() noexcept {
    std::vector<std::uint8_t> kick(8192, 0x4Eu);
    (void)FieldChips::Amiga::loadKickstart(FieldAmiga::chip, kick.data(), kick.size());
}

inline void openAmigaLove() noexcept {
    seedAmigaKick();
    FieldAmiga::open(true);
}

inline void openLoveOfEverything() noexcept {
    FieldStorage::enableEndGameMode(true);
    seedAmigaKick();
    FieldAmiga::open(true);
    FieldPs1::open();
    FieldXbox360::open();
}

} // namespace FieldAosChipsWave