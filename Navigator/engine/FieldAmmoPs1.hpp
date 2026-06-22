#pragma once

#include "AmmoPS1/FieldPs1Core.hpp"
#include "FieldAmmoEmuCommon.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldVga.hpp"

#include <vector>

namespace FieldPs1 {

constexpr int FB_W = 320;
constexpr int FB_H = 240;

inline FieldChips::Ps1::State chip{};
inline bool active = false;
inline std::string romPath;
inline std::uint8_t fb[FB_W * FB_H]{};
inline bool vgaMode13 = false;

inline bool loadRom(const char* path) {
    std::vector<std::uint8_t> data;
    if (!FieldAmmoFat::readFile(path, data) || data.size() < 2048u) return false;
    if (!FieldChips::Ps1::loadCart(chip, data.data(), data.size())) return false;
    romPath = path;
    vgaMode13 = false;
    return true;
}

inline void renderFrame(std::uint8_t* guestRam) noexcept {
    if (!guestRam) return;
    FieldChips::Ps1::gpuCopyFb(chip, fb, FB_W * FB_H);
    if (!vgaMode13) {
        FieldVga::setMode(FieldVga::MODE_VGA_13, guestRam);
        vgaMode13 = true;
    }
    for (int y = 0; y < FB_H; ++y)
        for (int x = 0; x < FB_W; ++x)
            guestRam[FieldVga::VGA_FB + static_cast<std::size_t>(y * 320 + x)] =
                fb[y * FB_W + x];
}

inline void runFrame(std::uint8_t* guestRam) noexcept {
    FieldChips::Ps1::runFrame(chip);
    if (guestRam) renderFrame(guestRam);
}

inline void packDataBus(std::uint32_t* bus, std::size_t off) noexcept {
    if (!bus) return;
    bus[off + 0] = active ? 1u : 0u;
    bus[off + 1] = chip.gpu.dieCycles;
    bus[off + 2] = chip.gpu.waveActive ? 1u : 0u;
    bus[off + 3] = static_cast<std::uint32_t>(chip.frame);
}

inline void open() noexcept {
    active = true;
    FieldChips::Ps1::applyAudioConfig(chip, FieldChips::AudioStyle::Modern);
}

inline void close(std::uint8_t* ram) noexcept {
    (void)ram;
    active = false;
    vgaMode13 = false;
}

inline void tick(std::uint8_t* ram, const bool*) noexcept {
    if (!active) return;
    runFrame(ram);
}

} // namespace FieldPs1