#pragma once

// Covox Speech Thing — 8-bit DAC on LPT data port.

#include "FieldDosConfig.hpp"
#include "FieldMix.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

namespace FieldCovox {

constexpr int RATE  = 22050;
constexpr int CHUNK = 1024;
constexpr std::size_t RING = 16384u;

inline MIX_Mixer* mixer = nullptr;
inline MIX_Track* track = nullptr;
inline MIX_Audio* audio = nullptr;
inline bool ready = false;

inline std::uint8_t latch = 0x80u;
inline std::deque<std::uint8_t> ring;
inline std::vector<std::int16_t> pcm;

inline void shutdown() noexcept;

inline void pushSample(std::uint8_t val) noexcept {
    ring.push_back(val);
    while (ring.size() > RING) ring.pop_front();
}

inline std::vector<std::uint8_t> buildWav() noexcept {
    pcm.resize(static_cast<std::size_t>(CHUNK));
    for (int i = 0; i < CHUNK; ++i) {
        const std::uint8_t s = ring.empty() ? latch : ring.front();
        if (!ring.empty()) ring.pop_front();
        pcm[static_cast<std::size_t>(i)] =
            static_cast<std::int16_t>((static_cast<int>(s) - 128) * 256);
    }
    const std::size_t bytes = pcm.size() * sizeof(std::int16_t);
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
    std::memcpy(wav.data() + 44, pcm.data(), bytes);
    return wav;
}

inline bool init(MIX_Mixer* mix) noexcept {
    shutdown();
    if (!mix || !FieldDosConfig::cfg.covoxEnabled) return false;
    mixer = mix;
    latch = 0x80u;
    for (int i = 0; i < CHUNK; ++i) pushSample(latch);

    auto wav = buildWav();
    SDL_IOStream* io = SDL_IOFromConstMem(wav.data(), static_cast<int>(wav.size()));
    if (!io) return false;
    audio = MIX_LoadAudio_IO(mix, io, true, true);
    if (!audio) return false;
    track = MIX_CreateTrack(mix);
    if (!track) return false;
    MIX_SetTrackAudio(track, audio);
    MIX_SetTrackGain(track, 0.65f);
    FieldMix::playTrack(track, -1);
    ready = true;
    std::fprintf(stderr, "[Covox] Speech Thing LPT@%03X (%d Hz 8-bit DAC)\n",
        static_cast<unsigned>(FieldDosConfig::cfg.covoxPort), RATE);
    return true;
}

inline void shutdown() noexcept {
    ready = false;
    latch = 0x80u;
    ring.clear();
    pcm.clear();
    if (track) { MIX_DestroyTrack(track); track = nullptr; }
    if (audio) { MIX_DestroyAudio(audio); audio = nullptr; }
    mixer = nullptr;
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (!FieldDosConfig::cfg.covoxEnabled || !ready) return;
    if (port == FieldDosConfig::cfg.covoxPort) {
        latch = val;
        pushSample(val);
    }
}

inline void pump() noexcept {
    if (!ready || !track) return;
    auto wav = buildWav();
    SDL_IOStream* io = SDL_IOFromConstMem(wav.data(), static_cast<int>(wav.size()));
    if (!io) return;
    if (MIX_Audio* next = MIX_LoadAudio_IO(mixer, io, true, true)) {
        MIX_DestroyAudio(audio);
        audio = next;
        MIX_SetTrackAudio(track, audio);
        if (!MIX_TrackPlaying(track))
            FieldMix::playTrack(track, -1);
    }
}

} // namespace FieldCovox