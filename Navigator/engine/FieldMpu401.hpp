#pragma once

// Roland MPU-401 UART + LAPC-1 / MT-32 MIDI — FluidSynth synthesis → SDL3_mixer.

#include "FieldDosConfig.hpp"
#include "FieldMix.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <fluidsynth.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

namespace FieldMpu401 {

constexpr int RATE  = 44100;
constexpr int CHUNK = 1024;

inline MIX_Mixer* mixer = nullptr;
inline MIX_Track* track = nullptr;
inline MIX_Audio* audio = nullptr;
inline fluid_settings_t* fsSettings = nullptr;
inline fluid_synth_t* synth = nullptr;
inline bool ready = false;
inline std::uint8_t stat = 0xFEu;
inline bool uartMode = false;

inline std::vector<std::int16_t> pcm;
inline std::uint8_t midiSt = 0;
inline std::uint8_t midiD1 = 0;

inline void shutdown() noexcept;

inline bool loadSoundFont() noexcept {
    if (!synth) return false;
    static const char* kPaths[] = {
        "/usr/share/sounds/sf2/default-GM.sf2",
        "/usr/share/sounds/sf2/TimGM6mb.sf2",
        "/usr/share/soundfonts/FluidR3_GM.sf2",
        "/usr/share/sounds/sf2/FluidR3_GM.sf2",
    };
    for (const char* path : kPaths) {
        if (!std::filesystem::exists(path)) continue;
        if (fluid_synth_sfload(synth, path, 1) != FLUID_FAILED) {
            std::fprintf(stderr, "[MPU-401] SoundFont %s\n", path);
            return true;
        }
    }
    std::fprintf(stderr, "[MPU-401] WARN no SoundFont found — MIDI silent until .sf2 installed\n");
    return false;
}

inline void sendMidiByte(std::uint8_t b) noexcept {
    if (!synth) return;
    if (b >= 0xF8u) return;
    if (b >= 0x80u) {
        midiSt = b;
        midiD1 = 0;
        return;
    }
    if (!midiSt) return;
    const int cmd = midiSt & 0xF0;
    const int ch  = midiSt & 0x0F;
    if (cmd == 0x90u) {
        if (b == 0) fluid_synth_noteoff(synth, ch, midiD1);
        else fluid_synth_noteon(synth, ch, midiD1, b);
        midiSt = 0;
        return;
    }
    if (cmd == 0x80u) {
        fluid_synth_noteoff(synth, ch, midiD1);
        midiSt = 0;
        return;
    }
    if (cmd == 0xC0u) {
        fluid_synth_program_change(synth, ch, b);
        midiSt = 0;
        return;
    }
    if (cmd == 0xB0u) {
        fluid_synth_cc(synth, ch, midiD1, b);
        midiSt = 0;
        return;
    }
    if (!midiD1) {
        midiD1 = b;
        return;
    }
    if (cmd == 0xB0u)
        fluid_synth_cc(synth, ch, midiD1, b);
    midiSt = 0;
    midiD1 = 0;
}

inline void renderPcm() noexcept {
    if (!synth) return;
    pcm.resize(static_cast<std::size_t>(CHUNK) * 2u);
    fluid_synth_write_s16(synth, CHUNK, pcm.data(), 0, 2, pcm.data(), 1, 2);
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
    w16(22, 2u);
    w32(24, static_cast<std::uint32_t>(RATE));
    w32(28, static_cast<std::uint32_t>(RATE * 4));
    w16(32, 4u);
    w16(34, 16u);
    std::memcpy(wav.data() + 36, "data", 4);
    w32(40, static_cast<std::uint32_t>(bytes));
    std::memcpy(wav.data() + 44, pcm.data(), bytes);
    return wav;
}

inline bool init(MIX_Mixer* mix) noexcept {
    shutdown();
    if (!mix || !FieldDosConfig::cfg.mpu401Enabled || !FieldDosConfig::cfg.lapc1Enabled)
        return false;
    mixer = mix;

    fsSettings = new_fluid_settings();
    if (!fsSettings) return false;
    fluid_settings_setstr(fsSettings, "audio.driver", "off");
    synth = new_fluid_synth(fsSettings);
    if (!synth) return false;

    fluid_synth_set_gain(synth, 0.65f);
    fluid_synth_set_polyphony(synth, 96);
    loadSoundFont();

    auto wav = buildWav();
    SDL_IOStream* io = SDL_IOFromConstMem(wav.data(), static_cast<int>(wav.size()));
    if (!io) return false;
    audio = MIX_LoadAudio_IO(mixer, io, true, true);
    if (!audio) return false;
    track = MIX_CreateTrack(mixer);
    if (!track) return false;
    MIX_SetTrackAudio(track, audio);
    MIX_SetTrackGain(track, 0.85f);
    FieldMix::playTrack(track, -1);

    stat = 0xFEu;
    uartMode = false;
    ready = true;
    std::fprintf(stderr, "[MPU-401] Roland LAPC-1 / MT-32 @%03X (%d Hz FluidSynth)\n",
        static_cast<unsigned>(FieldDosConfig::cfg.mpuPort), RATE);
    return true;
}

inline void shutdown() noexcept {
    ready = false;
    uartMode = false;
    stat = 0xFFu;
    midiSt = midiD1 = 0;
    pcm.clear();
    if (track) { MIX_DestroyTrack(track); track = nullptr; }
    if (audio) { MIX_DestroyAudio(audio); audio = nullptr; }
    if (synth) { delete_fluid_synth(synth); synth = nullptr; }
    if (fsSettings) { delete_fluid_settings(fsSettings); fsSettings = nullptr; }
    mixer = nullptr;
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (!FieldDosConfig::cfg.mpu401Enabled) return 0xFFu;
    const std::uint16_t base = FieldDosConfig::cfg.mpuPort;
    if (port == static_cast<std::uint16_t>(base + 1u)) return stat;
    return 0xFFu;
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (!FieldDosConfig::cfg.mpu401Enabled) return;
    const std::uint16_t base = FieldDosConfig::cfg.mpuPort;
    if (port == base) {
        if (uartMode && ready) sendMidiByte(val);
        return;
    }
    if (port == static_cast<std::uint16_t>(base + 1u)) {
        if (val == 0xFFu) { uartMode = false; stat = 0xFFu; }
        else if (val == 0x3Fu) { uartMode = true; stat = 0xFEu; }
    }
}

inline void pump() noexcept {
    if (!ready || !track || !synth) return;
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

} // namespace FieldMpu401