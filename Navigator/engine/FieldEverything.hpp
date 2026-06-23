#pragma once

// Everything Everywhere — all CHIPS + DOS hosts simultaneous on one Field canvas.
// No traditional load: systems pre-exist in the fabric; resonance holds state powered off.

#include "FieldStorage.hpp"
#include "FieldAmmoPs1.hpp"
#include "FieldAmmoXbox360.hpp"
#include "FieldAmmoAmiga.hpp"
#include "FieldAmmoN64.hpp"
#include "FieldAmmoDreamcast.hpp"
#include "FieldAmmoPs2.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace FieldEverything {

inline bool active = false;
inline std::uint32_t tickCount = 0u;

inline bool enabled() noexcept {
    return std::getenv("AMOURANTHRTX_EVERYTHING_EVERYWHERE") != nullptr
        || std::getenv("AMOURANTHRTX_END_GAME") != nullptr;
}

inline void seedChips() noexcept {
    std::vector<std::uint8_t> blob(8192, 0xA5u);
    std::vector<std::uint8_t> psx(0x1000, 0);
    std::memcpy(psx.data(), "PS-X EXE", 8);
    const std::uint32_t load = 0x80010000u;
    const std::uint32_t entry = 0x80010100u;
    std::memcpy(psx.data() + 0x10, &load, 4);
    std::memcpy(psx.data() + 0x14, &entry, 4);
    (void)FieldChips::Ps1::loadCart(FieldPs1::chip, psx.data(), psx.size());
    (void)FieldChips::Xbox360::loadImage(FieldXbox360::chip, blob.data(), blob.size());
    (void)FieldChips::N64::loadCart(FieldN64::chip, blob.data(), blob.size());
    (void)FieldChips::Dreamcast::loadCart(FieldDreamcast::chip, blob.data(), blob.size());
    (void)FieldChips::Ps2::loadCart(FieldPs2::chip, blob.data(), blob.size());
    std::vector<std::uint8_t> kick(8192, 0x4Eu);
    (void)FieldChips::Amiga::loadKickstart(FieldAmiga::chip, kick.data(), kick.size());
}

inline void open() noexcept {
    if (active) return;
    FieldStorage::enableEndGameMode(true);
    FieldStorage::restoreFieldState();
    FieldPs1::open();
    FieldXbox360::open();
    FieldAmiga::open(true);
    FieldN64::open();
    FieldDreamcast::open();
    FieldPs2::open();
    seedChips();
    active = true;
    FieldStorage::fabricPersist.everythingActive = true;
    FieldStorage::fabricPersist.everythingTicks = tickCount;
    std::fprintf(stderr, "[FieldEverything] all systems live — no load, fabric resident\n");
}

inline void tick(std::uint8_t* guestRam) noexcept {
    if (!active || !guestRam) return;
    ++tickCount;
    FieldStorage::fabricPersist.everythingTicks = tickCount;
    if (FieldPs1::active) FieldPs1::tick(guestRam, nullptr);
    if (FieldXbox360::active) FieldXbox360::tick(guestRam, nullptr);
    if (FieldN64::active) FieldN64::tick(guestRam, nullptr);
    if (FieldDreamcast::active) FieldDreamcast::tick(guestRam, nullptr);
    if (FieldPs2::active) FieldPs2::tick(guestRam, nullptr);
    if (FieldAmiga::active) FieldAmiga::tick(guestRam, nullptr);
}

inline void powerOff() noexcept {
    if (!active) return;
    FieldStorage::fabricPersist.everythingActive = false;
    FieldStorage::persistFieldState();
    active = false;
}

} // namespace FieldEverything