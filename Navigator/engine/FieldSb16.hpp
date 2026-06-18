#pragma once

// Sound Blaster 16 + OPL2/3 FM + DMA digital audio → SDL3_mixer.

#include "FieldDos.hpp"
#include "FieldDma.hpp"
#include "FieldDosConfig.hpp"
#include "FieldMix.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldSb16 {

constexpr int SLOT       = 14;
constexpr int RATE       = 22050;
constexpr int CHUNK      = 1024;

inline MIX_Mixer* mixer  = nullptr;
inline MIX_Track* track  = nullptr;
inline MIX_Audio* audio  = nullptr;
inline bool ready        = false;

inline std::uint8_t dspCmd       = 0;
inline std::uint8_t dspStep      = 0;
inline std::uint8_t dspReadVal   = 0;
inline bool dspResetPending    = false;
inline bool dspWriteReady      = true;
inline bool dspDataAvailable   = false;
inline bool irqPending         = false;
inline std::vector<std::uint8_t> dspParams;

inline std::uint8_t oplIndex     = 0;
inline std::uint8_t oplRegs[256]{};

inline std::vector<std::int16_t> pcmBuf;
inline std::size_t pcmPos = 0;
inline float oplPhase = 0.f;
inline float oplHz    = 110.f;
inline const std::uint8_t* guestRam = nullptr;

inline std::uint16_t sbPort(std::uint8_t off) noexcept {
    return static_cast<std::uint16_t>(FieldDosConfig::cfg.sbBasePort + off);
}

inline void raiseIrq() noexcept { irqPending = true; }

inline void consumeIrq() noexcept { irqPending = false; }

inline bool isIrqPending() noexcept { return irqPending; }

inline std::uint8_t dspVersionMajor() noexcept { return 4u; }
inline std::uint8_t dspVersionMinor() noexcept { return 16u; }

inline void buildPcmFromGuest(const std::uint8_t* guest, std::uint32_t phys,
                              std::uint16_t nbytes, float hz) noexcept {
    pcmBuf.resize(static_cast<std::size_t>(nbytes) * 2u);
    for (std::uint16_t i = 0; i < nbytes; ++i) {
        const std::uint8_t s = guest ? guest[phys + i] : 128u;
        const float t = static_cast<float>(i) / static_cast<float>(nbytes);
        const float env = 0.5f + 0.5f * std::sin(t * 6.2831853f * 4.f);
        const std::int16_t v = static_cast<std::int16_t>(
            (static_cast<int>(s) - 128) * 180 * env);
        pcmBuf[static_cast<std::size_t>(i) * 2u]     = v;
        pcmBuf[static_cast<std::size_t>(i) * 2u + 1u] = v / 2;
    }
    oplHz = hz > 20.f ? hz : 22050.f / static_cast<float>(nbytes > 0 ? nbytes : 1);
    pcmPos = 0;
}

inline void buildSilentChunk() noexcept {
    pcmBuf.assign(static_cast<std::size_t>(CHUNK), 0);
    pcmPos = 0;
    oplPhase = 0.f;
}

inline bool oplKeyOn() noexcept {
    return (oplRegs[0xB0u] & 0x20u) != 0u;
}

inline void buildOplTone() noexcept {
    pcmBuf.resize(static_cast<std::size_t>(CHUNK));
    const float inc = oplHz / static_cast<float>(RATE);
    for (int i = 0; i < CHUNK; ++i) {
        oplPhase += inc;
        if (oplPhase >= 1.f) oplPhase -= 1.f;
        const std::int16_t v = oplPhase < 0.5f ? 4000 : -4000;
        pcmBuf[static_cast<std::size_t>(i)] = v;
    }
    pcmPos = 0;
}

inline void oplWrite(std::uint8_t reg, std::uint8_t val) noexcept {
    oplRegs[reg & 0xFFu] = val;
    if (reg == 0xB0u) {
        const std::uint8_t fnumLo = oplRegs[0xA0u];
        const std::uint8_t octFhi = oplRegs[0xB0u];
        const int octave = (octFhi >> 2) & 7;
        const int fnum = static_cast<int>(fnumLo) | ((octFhi & 3) << 8);
        if (fnum > 0)
            oplHz = 49716.f * static_cast<float>(fnum) / std::pow(2.f, 20.f - static_cast<float>(octave));
    }
}

inline void dspCompleteDma8() noexcept {
    const int dmaCh = static_cast<int>(FieldDosConfig::cfg.sbDma8);
    const std::uint32_t phys = FieldDma::physAddr8(dmaCh);
    const std::uint16_t nbytes = FieldDma::transferBytes8(dmaCh);
    buildPcmFromGuest(guestRam ? guestRam : FieldDos::hdGuestRamPtr, phys, nbytes, 8000.f);
    raiseIrq();
    dspDataAvailable = false;
    dspWriteReady = true;
}

inline void dspWrite(std::uint8_t val) noexcept {
    if (dspResetPending) {
        dspResetPending = false;
        dspReadVal = 0xAAu;
        dspDataAvailable = true;
        return;
    }
    if (dspStep == 0) {
        dspCmd = val;
        dspParams.clear();
        if (val == 0xE1u) {
            dspReadVal = dspVersionMajor();
            dspDataAvailable = true;
            dspStep = 0;
            return;
        }
        if (val == 0xE3u) {
            dspReadVal = static_cast<std::uint8_t>(
                (FieldDosConfig::cfg.sbIrq << 4) | FieldDosConfig::cfg.sbDma8);
            dspDataAvailable = true;
            return;
        }
        if (val == 0x14u || val == 0x91u || val == 0x99u) {
            dspStep = 1;
            return;
        }
        if (val == 0xD0u || val == 0xD1u) { consumeIrq(); return; }
        if (val == 0xD3u || val == 0xD4u) { raiseIrq(); return; }
        return;
    }
    dspParams.push_back(val);
    if (dspCmd == 0x14u && dspParams.size() >= 2u) {
        dspCompleteDma8();
        dspStep = 0;
        dspParams.clear();
    } else if (dspCmd == 0xE1u && dspParams.size() >= 1u) {
        dspReadVal = dspVersionMinor();
        dspDataAvailable = true;
        dspStep = 0;
    }
}

inline void shutdown() noexcept {
    if (track) { MIX_StopTrack(track, 0); MIX_DestroyTrack(track); track = nullptr; }
    if (audio) { MIX_DestroyAudio(audio); audio = nullptr; }
    mixer = nullptr;
    ready = false;
    pcmBuf.clear();
    pcmPos = 0;
}

inline bool init(MIX_Mixer* mix) noexcept {
    shutdown();
    if (!mix || !FieldDosConfig::cfg.sb16Enabled) return false;
    mixer = mix;
    buildSilentChunk();
    const std::size_t bytes = pcmBuf.size() * sizeof(std::int16_t);
    std::vector<std::uint8_t> wav(44u + bytes);
    auto w32 = [&](std::size_t o, std::uint32_t v) { std::memcpy(wav.data() + o, &v, 4); };
    auto w16 = [&](std::size_t o, std::uint16_t v) { std::memcpy(wav.data() + o, &v, 2); };
    std::memcpy(wav.data(), "RIFF", 4);
    w32(4, static_cast<std::uint32_t>(36u + bytes));
    std::memcpy(wav.data() + 8, "WAVE", 4);
    std::memcpy(wav.data() + 12, "fmt ", 4);
    w32(16, 16u);
    w16(20, 1u);
    w16(22, 1u);
    w32(24, static_cast<std::uint32_t>(RATE));
    w32(28, static_cast<std::uint32_t>(RATE * 2));
    w16(32, 2u);
    w16(34, 16u);
    std::memcpy(wav.data() + 36, "data", 4);
    w32(40, static_cast<std::uint32_t>(bytes));
    std::memcpy(wav.data() + 44, pcmBuf.data(), bytes);
    SDL_IOStream* io = SDL_IOFromConstMem(wav.data(), static_cast<int>(wav.size()));
    if (!io) return false;
    audio = MIX_LoadAudio_IO(mixer, io, true, true);
    if (!audio) return false;
    track = MIX_CreateTrack(mixer);
    if (!track) { MIX_DestroyAudio(audio); audio = nullptr; return false; }
    MIX_SetTrackAudio(track, audio);
    MIX_SetTrackGain(track, 0.45f);
    ready = true;
    return true;
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (!FieldDosConfig::cfg.sb16Enabled) return;
    const std::uint16_t b = FieldDosConfig::cfg.sbBasePort;
    if (port == b + 6u) {
        if (val == 1u) {
            dspResetPending = false;
            dspReadVal = 0xAAu;
            dspDataAvailable = true;
            dspWriteReady = true;
        }
        return;
    }
    if (port == b + 0xCu) {
        if (dspWriteReady) dspWrite(val);
        return;
    }
    if (FieldDosConfig::portInOplRange(port)) {
        if (!FieldDosConfig::cfg.oplEnabled) return;
        if (port == FieldDosConfig::cfg.oplPort || port == b + 8u)
            oplIndex = val;
        else
            oplWrite(oplIndex, val);
        return;
    }
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (!FieldDosConfig::cfg.sb16Enabled) return 0xFFu;
    const std::uint16_t b = FieldDosConfig::cfg.sbBasePort;
    if (port == b + 0xEu) {
        std::uint8_t st = 0x7Fu;
        if (dspDataAvailable) st |= 0x80u;
        if (irqPending) st |= 0x80u;
        return st;
    }
    if (port == b + 0xAu) {
        if (dspDataAvailable) {
            dspDataAvailable = false;
            return dspReadVal;
        }
        return 0xAAu;
    }
    if (FieldDosConfig::portInOplRange(port)) {
        if (port == FieldDosConfig::cfg.oplPort || port == b + 8u)
            return oplIndex;
        return oplRegs[oplIndex];
    }
    return 0xFFu;
}

inline bool hasPcmReady() noexcept {
    return !pcmBuf.empty() && pcmPos < pcmBuf.size();
}

inline void pump() noexcept {
    if (!ready || !track) return;
    const bool playDma = hasPcmReady();
    const bool playOpl = oplKeyOn();
    if (!playDma && !playOpl) {
        if (MIX_TrackPlaying(track))
            MIX_StopTrack(track, 0);
        return;
    }
    if (!playDma)
        buildOplTone();
    const std::size_t bytes = pcmBuf.size() * sizeof(std::int16_t);
    std::vector<std::uint8_t> wav(44u + bytes);
    auto w32 = [&](std::size_t o, std::uint32_t v) { std::memcpy(wav.data() + o, &v, 4); };
    auto w16 = [&](std::size_t o, std::uint16_t v) { std::memcpy(wav.data() + o, &v, 2); };
    std::memcpy(wav.data(), "RIFF", 4);
    w32(4, static_cast<std::uint32_t>(36u + bytes));
    std::memcpy(wav.data() + 8, "WAVE", 4);
    std::memcpy(wav.data() + 12, "fmt ", 4);
    w32(16, 16u);
    w16(20, 1u);
    w16(22, 1u);
    w32(24, static_cast<std::uint32_t>(RATE));
    w32(28, static_cast<std::uint32_t>(RATE * 2));
    w16(32, 2u);
    w16(34, 16u);
    std::memcpy(wav.data() + 36, "data", 4);
    w32(40, static_cast<std::uint32_t>(bytes));
    std::memcpy(wav.data() + 44, pcmBuf.data(), bytes);
    SDL_IOStream* io = SDL_IOFromConstMem(wav.data(), static_cast<int>(wav.size()));
    if (!io) return;
    MIX_Audio* next = MIX_LoadAudio_IO(mixer, io, true, true);
    if (!next) return;
    if (audio) MIX_DestroyAudio(audio);
    audio = next;
    MIX_SetTrackAudio(track, audio);
    if (!MIX_TrackPlaying(track))
        FieldMix::playTrack(track, -1);
}

inline void setGuestRam(const std::uint8_t* ram) noexcept {
    guestRam = ram;
}

} // namespace FieldSb16