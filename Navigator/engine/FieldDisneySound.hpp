#pragma once

// Disney Sound Source — 1-bit DAC on LPT data port @ ~7227 Hz.

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

namespace FieldDisneySound {

constexpr int RATE  = 7227;
constexpr int CHUNK = 512;
constexpr std::size_t RING = 8192u;

inline MIX_Mixer* mixer = nullptr;
inline MIX_Track* track = nullptr;
inline MIX_Audio* audio = nullptr;
inline bool ready = false;

inline std::uint8_t latch = 0x80u;
inline std::deque<float> ring;
inline std::vector<std::int16_t> pcm;

inline void shutdown() noexcept;

inline void pushSample(std::uint8_t val) noexcept {
    const float s = (val & 0x80u) ? 0.45f : -0.45f;
    ring.push_back(s);
    while (ring.size() > RING) ring.pop_front();
}

inline std::vector<std::uint8_t> buildWav() noexcept {
    pcm.resize(static_cast<std::size_t>(CHUNK));
    for (int i = 0; i < CHUNK; ++i) {
        float s = 0.f;
        if (!ring.empty()) {
            s = ring.front();
            ring.pop_front();
        } else {
            s = (latch & 0x80u) ? 0.45f : -0.45f;
        }
        pcm[static_cast<std::size_t>(i)] =
            static_cast<std::int16_t>(std::clamp(s * 28000.f, -32767.f, 32767.f));
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
    if (!mix || !FieldDosConfig::cfg.disneyEnabled) return false;
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
    MIX_SetTrackGain(track, 0.70f);
    FieldMix::playTrack(track, -1);
    ready = true;
    std::fprintf(stderr, "[DSS] Disney Sound Source LPT@%03X (%d Hz 1-bit DAC)\n",
        static_cast<unsigned>(FieldDosConfig::cfg.disneyPort), RATE);
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

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (!FieldDosConfig::cfg.disneyEnabled) return 0xFFu;
    if (port == FieldDosConfig::cfg.disneyPort) return latch;
    return 0xFFu;
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (!FieldDosConfig::cfg.disneyEnabled) return;
    if (port == FieldDosConfig::cfg.disneyPort) {
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

} // namespace FieldDisneySound