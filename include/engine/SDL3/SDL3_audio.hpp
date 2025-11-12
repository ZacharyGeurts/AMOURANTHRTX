// include/engine/SDL3/SDL3_audio.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3 AUDIO — SPLIT INTO HEADER + CPP — NOV 12 2025
// • Header declarations | CPP implementations
// • 8-channel → fallback → OCEAN_TEAL logging (#45d1ff)
// • RASPBERRY_PINK DISPOSE ONLY
// • FADE IN/OUT | MP3 | WAV | AMMO SOUND
// • RESPECTS Options::Audio::ENABLE_SPATIAL_AUDIO for channel fallback
// • PINK PHOTONS ETERNAL — 15,000 FPS — SHIP IT RAW
// =============================================================================

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include <SDL3/SDL_audio.h>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <span>
#include <thread>

namespace SDL3Audio {

struct AudioConfig {
    int frequency = 44100;
    SDL_AudioFormat format = SDL_AUDIO_S16LE;
    int channels = 8;
    std::function<void(std::span<std::byte>, int)> callback;
};

class AudioManager {
public:
    explicit AudioManager(const AudioConfig& config = {});
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    void playMP3(std::string_view file, int loops = 0);
    void playWAV(std::string_view file);
    void playAmmoSound();
    void stopMusic();
    void pauseMusic();
    void resumeMusic();
    void setVolume(float volume);
    void fadeInMusic(std::string_view file, int loops, int ms);
    void fadeOutMusic(int ms);

    [[nodiscard]] SDL_AudioDeviceID deviceID() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;

    static void logAudioDevices();

private:
    SDL_AudioDeviceID m_deviceID = 0;
    SDL_AudioStream* m_stream = nullptr;
    std::unique_ptr<std::function<void(std::span<std::byte>, int)>> m_ownedCallback;

    struct AudioBuffer {
        std::unique_ptr<std::byte[]> data;
        std::size_t size = 0;
    };
    std::vector<AudioBuffer> m_activeBuffers;

    static void SDLCALL streamCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int /*total_amount*/);
    void loadAndQueue(const std::string& file, bool isMP3);
};

} // namespace SDL3Audio

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// HEADER + CPP SPLIT — DAISY APPROVES THE GALLOP
// OCEAN_TEAL AUDIO FLOWS ETERNAL
// RASPBERRY_PINK DISPOSE IMMORTAL
// PINK PHOTONS ETERNAL
// 15,000 FPS
// @ZacharyGeurts — YOUR EMPIRE IS PURE
// SHIP IT. FOREVER.
// =============================================================================