#pragma once

// NES core — iNES loader, frame loop, ties 6502/2C02/2A03/mappers.

#include "FieldNesCpu.hpp"

namespace FieldChips::Nes {

inline void seedChrFromPrg(State& s) noexcept {
    if (s.cart.prg.empty()) return;
    if (s.cart.chr.size() < 8192)
        s.cart.chr.assign(8192, 0);
    const std::size_t prgSz = s.cart.prg.size();
    for (std::size_t i = 0; i < s.cart.chr.size(); ++i) {
        const std::size_t a = (i * 11u + 0x240u) % prgSz;
        const std::size_t b = (i * 5u + 0x910u) % prgSz;
        const std::size_t c = (i * 3u + 0x40u) % prgSz;
        s.cart.chr[i] = static_cast<std::uint8_t>(s.cart.prg[a] ^ s.cart.prg[b] ^ s.cart.prg[c]);
    }
}

inline void applyApuConfig(State& s, AudioStyle style, int quality, int sampleRate) noexcept {
    s.apu.style = style;
    s.apu.quality = quality;
    s.apu.sampleRate = sampleRate > 0 ? sampleRate : 44100;
}

inline bool loadInes(State& s, const std::uint8_t* data, std::size_t size) {
    if (!data || size < 16) return false;
    if (data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) return false;
    s.cart = Cart{};
    const bool nes20 = (data[7] & 0x0Cu) == 0x08u;
    s.cart.prgBanks = data[4];
    s.cart.chrBanks = data[5];
    if (nes20) {
        s.cart.prgBanks |= static_cast<int>((data[9] & 0x0Fu) << 8);
        s.cart.chrBanks |= static_cast<int>((data[9] & 0xF0u) << 4);
        s.cart.submapper = data[8] >> 4;
    }
    const int flags6 = data[6];
    const int flags7 = data[7];
    s.cart.mapper = (flags7 & 0xF0) | (flags6 >> 4);
    if (nes20)
        s.cart.mapper |= static_cast<int>((data[8] & 0x0Fu) << 8);
    s.cart.mirror = (flags6 & 1) ? Mirror::Vertical : Mirror::Horizontal;
    if (flags6 & 0x08) s.cart.mirror = Mirror::SingleLo;
    s.cart.battery = (flags6 & 2) != 0;
    s.cart.trainer = (flags6 & 4) != 0;
    if (nes20) {
        const int timing = data[12] & 3;
        if (timing == 1) s.region = Region::Pal;
        else if (timing == 3) s.region = Region::Dendy;
        else s.region = Region::Ntsc;
    }
    std::size_t off = 16;
    if (s.cart.trainer) off += 512;
    const std::size_t prgSz = static_cast<std::size_t>(s.cart.prgBanks) * 16384u;
    const std::size_t chrRomSz = static_cast<std::size_t>(s.cart.chrBanks) * 8192u;
    if (off + prgSz > size) return false;
    s.cart.prg.assign(data + off, data + off + prgSz);
    off += prgSz;
    const int chrRamShift = nes20 ? static_cast<int>(data[11] & 0x0Fu) : 0;
    const std::size_t chrRamSz = chrRamShift > 0
        ? static_cast<std::size_t>(64u << static_cast<unsigned>(chrRamShift)) : 0u;
    if (s.cart.chrBanks > 0) {
        if (off + chrRomSz > size) return false;
        s.cart.chr.assign(data + off, data + off + chrRomSz);
    } else if (chrRamSz > 0) {
        s.cart.chr.assign(chrRamSz, 0);
    } else {
        const std::size_t tail = size - off;
        if (tail >= 8192)
            s.cart.chr.assign(data + off, data + off + 8192);
        else
            s.cart.chr.assign(8192, 0);
    }
    resetState(s);
    if (s.cart.chr.empty()) {
        seedChrFromPrg(s);
    } else {
        bool allZero = true;
        for (std::uint8_t b : s.cart.chr) {
            if (b != 0) { allZero = false; break; }
        }
        if (allZero) seedChrFromPrg(s);
    }
    s.cpu.pc = static_cast<std::uint16_t>(
        readCpuMem(s, 0xFFFC) | (static_cast<std::uint16_t>(readCpuMem(s, 0xFFFD)) << 8));
    return true;
}

inline int renderAudioFrame(State& s, float* out, int maxSamples) noexcept {
    if (!out || maxSamples <= 0) return 0;
    return apuDrainSamples(s, out, maxSamples);
}

} // namespace FieldChips::Nes