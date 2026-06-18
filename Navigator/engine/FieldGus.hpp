#pragma once

// Gravis UltraSound — wavetable RAM + voice mixer → SDL3_mixer.

#include "FieldDosConfig.hpp"
#include "FieldMix.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace FieldGus {

constexpr int RATE       = 44100;
constexpr int CHUNK      = 1024;
constexpr int VOICES     = 32;
constexpr std::size_t RAM_BYTES = 1024u * 1024u;

inline MIX_Mixer* mixer = nullptr;
inline MIX_Track* track = nullptr;
inline MIX_Audio* audio = nullptr;
inline bool ready = false;

inline std::uint8_t ctrlReg = 0;
inline std::uint8_t dmaCtrl = 0;
inline std::uint16_t dmaAddr = 0;
inline std::uint16_t dmaLen = 0;
inline std::uint8_t voiceSel = 0;
inline std::uint8_t regSel = 0;

struct Voice {
    bool on = false;
    std::uint32_t pos = 0;
    std::uint32_t start = 0;
    std::uint32_t end = 0;
    std::uint16_t freq = 0x1000u;
    std::uint8_t vol = 0xFFu;
};

inline std::vector<std::uint8_t> ram;
inline Voice voices[VOICES]{};
inline std::vector<std::int16_t> pcm;
inline const std::uint8_t* guestRamPtr = nullptr;

inline void bindGuest(const std::uint8_t* ram) noexcept { guestRamPtr = ram; }

inline void shutdown() noexcept;

inline std::uint16_t basePort() noexcept {
    return FieldDosConfig::cfg.gusBasePort;
}

inline void writeReg(std::uint8_t voice, std::uint8_t reg, std::uint8_t val) noexcept {
    if (voice >= VOICES) return;
    Voice& v = voices[voice];
    switch (reg) {
    case 0x00u: v.freq = static_cast<std::uint16_t>((v.freq & 0xFF00u) | val); break;
    case 0x01u: v.freq = static_cast<std::uint16_t>((v.freq & 0x00FFu) | (static_cast<std::uint16_t>(val) << 8)); break;
    case 0x08u: v.start = (v.start & 0x00FFFFFFu) | (static_cast<std::uint32_t>(val) << 24); break;
    case 0x09u: v.start = (v.start & 0xFF00FFFFu) | (static_cast<std::uint32_t>(val) << 16); break;
    case 0x0Au: v.start = (v.start & 0xFFFF00FFu) | (static_cast<std::uint32_t>(val) << 8); break;
    case 0x0Bu: v.start = (v.start & 0xFFFFFF00u) | val; break;
    case 0x0Cu: v.end = (v.end & 0x00FFFFFFu) | (static_cast<std::uint32_t>(val) << 24); break;
    case 0x0Du: v.end = (v.end & 0xFF00FFFFu) | (static_cast<std::uint32_t>(val) << 16); break;
    case 0x0Eu: v.end = (v.end & 0xFFFF00FFu) | (static_cast<std::uint32_t>(val) << 8); break;
    case 0x0Fu: v.end = (v.end & 0xFFFFFF00u) | val; break;
    case 0x10u: v.pos = (v.pos & 0x00FFFFFFu) | (static_cast<std::uint32_t>(val) << 24); break;
    case 0x11u: v.pos = (v.pos & 0xFF00FFFFu) | (static_cast<std::uint32_t>(val) << 16); break;
    case 0x12u: v.pos = (v.pos & 0xFFFF00FFu) | (static_cast<std::uint32_t>(val) << 8); break;
    case 0x13u: v.pos = (v.pos & 0xFFFFFF00u) | val; break;
    case 0x1Cu: v.vol = val; break;
    case 0x20u: v.on = (val & 0x03u) != 0; break;
    default: break;
    }
}

inline std::uint8_t readReg(std::uint8_t voice, std::uint8_t reg) noexcept {
    if (voice >= VOICES) return 0;
    const Voice& v = voices[voice];
    switch (reg) {
    case 0x00u: return static_cast<std::uint8_t>(v.freq & 0xFFu);
    case 0x01u: return static_cast<std::uint8_t>(v.freq >> 8);
    case 0x1Cu: return v.vol;
    case 0x20u: return v.on ? 0x03u : 0u;
    default: return 0;
    }
}

inline void renderPcm() noexcept {
    pcm.assign(static_cast<std::size_t>(CHUNK), 0);
    for (int vi = 0; vi < VOICES; ++vi) {
        Voice& v = voices[vi];
        if (!v.on || v.end <= v.start) continue;
        const float step = static_cast<float>(v.freq) / 44100.f;
        float phase = 0.f;
        for (int i = 0; i < CHUNK; ++i) {
            const std::uint32_t addr = v.start + static_cast<std::uint32_t>(phase);
            if (addr >= v.end || addr >= RAM_BYTES) { v.on = false; break; }
            const std::int16_t s = static_cast<std::int16_t>(
                (static_cast<int>(ram[addr]) - 128) * static_cast<int>(v.vol) / 2);
            pcm[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(
                std::clamp(pcm[static_cast<std::size_t>(i)] + s, -30000, 30000));
            phase += step;
        }
        v.pos = v.start + static_cast<std::uint32_t>(phase);
    }
}

inline std::vector<std::uint8_t> buildWav() noexcept {
    renderPcm();
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
    if (!mix || !FieldDosConfig::cfg.gusEnabled) return false;
    mixer = mix;
    ram.assign(RAM_BYTES, 0x80u);

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
    std::fprintf(stderr, "[GUS] UltraSound @%03X (%d Hz wavetable)\n",
        static_cast<unsigned>(basePort()), RATE);
    return true;
}

inline void shutdown() noexcept {
    ready = false;
    ctrlReg = dmaCtrl = voiceSel = regSel = 0;
    dmaAddr = dmaLen = 0;
    ram.clear();
    pcm.clear();
    for (auto& v : voices) v = {};
    if (track) { MIX_DestroyTrack(track); track = nullptr; }
    if (audio) { MIX_DestroyAudio(audio); audio = nullptr; }
    mixer = nullptr;
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (!FieldDosConfig::cfg.gusEnabled) return 0xFFu;
    const std::uint16_t base = basePort();
    if (port == static_cast<std::uint16_t>(base + 0x0Bu)) return 0x0Bu;
    if (port == static_cast<std::uint16_t>(base + 0x0Fu))
        return readReg(voiceSel, regSel);
    return 0xFFu;
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (!FieldDosConfig::cfg.gusEnabled) return;
    const std::uint16_t base = basePort();
    if (port == static_cast<std::uint16_t>(base + 0x03u)) {
        ctrlReg = val;
        return;
    }
    if (port == static_cast<std::uint16_t>(base + 0x04u)) {
        voiceSel = val;
        return;
    }
    if (port == static_cast<std::uint16_t>(base + 0x05u)) {
        regSel = val;
        return;
    }
    if (port == static_cast<std::uint16_t>(base + 0x06u)) {
        writeReg(voiceSel, regSel, val);
        return;
    }
    if (port == static_cast<std::uint16_t>(base + 0x07u)) {
        dmaCtrl = val;
        return;
    }
    if (port == static_cast<std::uint16_t>(base + 0x08u)) {
        dmaAddr = static_cast<std::uint16_t>((dmaAddr & 0xFF00u) | val);
        return;
    }
    if (port == static_cast<std::uint16_t>(base + 0x09u)) {
        dmaAddr = static_cast<std::uint16_t>((dmaAddr & 0x00FFu) | (static_cast<std::uint16_t>(val) << 8));
        return;
    }
    if (port == static_cast<std::uint16_t>(base + 0x0Au)) {
        dmaLen = static_cast<std::uint16_t>((dmaLen & 0xFF00u) | val);
        return;
    }
    if (port == static_cast<std::uint16_t>(base + 0x0Bu)) {
        dmaLen = static_cast<std::uint16_t>((dmaLen & 0x00FFu) | (static_cast<std::uint16_t>(val) << 8));
        if ((dmaCtrl & 0x01u) && guestRamPtr) {
            const std::uint32_t guest = static_cast<std::uint32_t>(dmaAddr) << 4;
            const std::size_t n = std::min<std::size_t>(dmaLen, RAM_BYTES);
            for (std::size_t i = 0; i < n; ++i)
                ram[i] = guestRamPtr[guest + i];
        }
        return;
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

} // namespace FieldGus