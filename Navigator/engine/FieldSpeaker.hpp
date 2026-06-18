#pragma once

// PC speaker (PIT channel 2 + port 0x61) → SDL3_mixer procedural square wave.

#include "FieldMix.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldSpeaker {

constexpr int SLOT            = 15;
constexpr int SAMPLE_RATE     = 44100;
constexpr int BUFFER_SAMPLES  = 2048;

inline uint8_t  pitMode       = 0;
inline uint16_t pitLatch      = 0;
inline bool     pitHiLo       = false;
inline bool     speakerGate   = false;
inline float    phase         = 0.f;
inline uint16_t lastLatch     = 0;
inline bool     lastGate      = false;

inline MIX_Mixer* mixer       = nullptr;
inline MIX_Track* track       = nullptr;
inline MIX_Audio* audio       = nullptr;
inline std::vector<std::uint8_t> wavBlob;
inline bool       ready       = false;

inline void resetPit() noexcept {
    pitMode = 0;
    pitLatch = 0;
    pitHiLo = false;
    speakerGate = false;
    phase = 0.f;
    lastLatch = 0;
    lastGate = false;
}

inline void portOut(uint16_t port, uint8_t val) noexcept {
    if (port == 0x43u) {
        pitMode = val;
        pitHiLo = false;
        return;
    }
    if (port == 0x42u || port == 0x40u) {
        if (!pitHiLo) {
            pitLatch = static_cast<uint16_t>(val);
            pitHiLo = true;
        } else {
            pitLatch |= static_cast<uint16_t>(val) << 8;
            pitHiLo = false;
        }
        return;
    }
    if (port == 0x61u) {
        speakerGate = (val & 0x02u) != 0u;
        return;
    }
}

inline uint8_t portIn(uint16_t port) noexcept {
    if (port == 0x61u)
        return speakerGate ? 0x03u : 0x01u;
    if (port == 0x40u || port == 0x42u)
        return static_cast<uint8_t>(pitLatch & 0xFFu);
    return 0xFFu;
}

inline float currentHz() noexcept {
    if (pitLatch == 0u) return 0.f;
    return 1193181.67f / static_cast<float>(pitLatch);
}

inline void buildWav() {
    const std::size_t dataBytes = static_cast<std::size_t>(BUFFER_SAMPLES) * 2u;
    wavBlob.resize(44u + dataBytes);
    auto w32 = [&](std::size_t off, std::uint32_t v) {
        std::memcpy(wavBlob.data() + off, &v, 4);
    };
    auto w16 = [&](std::size_t off, std::uint16_t v) {
        std::memcpy(wavBlob.data() + off, &v, 2);
    };
    std::memcpy(wavBlob.data(), "RIFF", 4);
    w32(4, static_cast<std::uint32_t>(36u + dataBytes));
    std::memcpy(wavBlob.data() + 8, "WAVE", 4);
    std::memcpy(wavBlob.data() + 12, "fmt ", 4);
    w32(16, 16u);
    w16(20, 1u);
    w16(22, 1u);
    w32(24, static_cast<std::uint32_t>(SAMPLE_RATE));
    w32(28, static_cast<std::uint32_t>(SAMPLE_RATE * 2));
    w16(32, 2u);
    w16(34, 16u);
    std::memcpy(wavBlob.data() + 36, "data", 4);
    w32(40, static_cast<std::uint32_t>(dataBytes));

    const float hz = currentHz();
    auto* pcm = reinterpret_cast<std::int16_t*>(wavBlob.data() + 44);
    if (!speakerGate || hz < 20.f) {
        std::fill(pcm, pcm + BUFFER_SAMPLES, 0);
        return;
    }
    const float inc = hz / static_cast<float>(SAMPLE_RATE);
    for (int i = 0; i < BUFFER_SAMPLES; ++i) {
        phase += inc;
        if (phase >= 1.f) phase -= 1.f;
        pcm[i] = phase < 0.5f ? static_cast<std::int16_t>(9000)
                              : static_cast<std::int16_t>(-9000);
    }
}

inline void shutdown() noexcept {
    if (track) { MIX_StopTrack(track, 0); MIX_DestroyTrack(track); track = nullptr; }
    if (audio) { MIX_DestroyAudio(audio); audio = nullptr; }
    mixer = nullptr;
    ready = false;
    lastLatch = 0;
    lastGate = false;
}

inline bool init(MIX_Mixer* mix) noexcept {
    shutdown();
    if (!mix) return false;
    mixer = mix;
    buildWav();
    SDL_IOStream* io = SDL_IOFromConstMem(wavBlob.data(), static_cast<int>(wavBlob.size()));
    if (!io) return false;
    audio = MIX_LoadAudio_IO(mixer, io, true, true);
    if (!audio) return false;
    track = MIX_CreateTrack(mixer);
    if (!track) { MIX_DestroyAudio(audio); audio = nullptr; return false; }
    MIX_SetTrackAudio(track, audio);
    MIX_SetTrackGain(track, 0.35f);
    ready = true;
    return true;
}

inline void reloadIfNeeded() {
    if (!ready || !mixer) return;
    if (pitLatch == lastLatch && speakerGate == lastGate) return;
    lastLatch = pitLatch;
    lastGate = speakerGate;
    buildWav();
    if (audio) MIX_DestroyAudio(audio);
    SDL_IOStream* io = SDL_IOFromConstMem(wavBlob.data(), static_cast<int>(wavBlob.size()));
    if (!io) return;
    audio = MIX_LoadAudio_IO(mixer, io, true, true);
    if (!audio || !track) return;
    MIX_SetTrackAudio(track, audio);
}

inline void pump() noexcept {
    if (!ready || !track) return;
    reloadIfNeeded();
    if (speakerGate && currentHz() >= 20.f) {
        if (!MIX_TrackPlaying(track))
            FieldMix::playTrack(track, -1);
    } else if (MIX_TrackPlaying(track)) {
        MIX_StopTrack(track, 0);
    }
}

} // namespace FieldSpeaker