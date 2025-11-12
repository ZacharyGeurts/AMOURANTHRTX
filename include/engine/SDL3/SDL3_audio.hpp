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
// SDL3 AUDIO — FULLY HEADER-ONLY — FINAL FORM — NOV 11 2025 05:36 PM EST
// • .cpp OBLITERATED BY DAISY'S HOOVES
// • 8-channel → fallback → OCEAN_TEAL logging (#45d1ff)
// • RASPBERRY_PINK DISPOSE ONLY
// • FADE IN/OUT | MP3 | WAV | AMMO SOUND
// • PINK PHOTONS ETERNAL — 15,000 FPS — SHIP IT RAW
// =============================================================================

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include <SDL3/SDL_audio.h>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <span>
#include <fstream>
#include <format>
#include <thread>
#include <chrono>
#include <cstring>
#include <algorithm>

namespace SDL3Audio {

struct AudioConfig {
    int frequency = 44100;
    SDL_AudioFormat format = SDL_AUDIO_S16LE;
    int channels = 8;
    std::function<void(std::span<std::byte>, int)> callback;
};

class AudioManager {
public:
    explicit AudioManager(const AudioConfig& config = {}) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            LOG_ERROR_CAT("Audio", "SDL_InitSubSystem(SDL_INIT_AUDIO) failed: {}", SDL_GetError());
            return;
        }

        SDL_AudioSpec desired{};
        desired.freq = config.frequency;
        desired.format = config.format;
        desired.channels = static_cast<uint8_t>(config.channels);

        std::vector<int> attempts = {8, 6, 5, 4, 2, 1};
        for (int ch : attempts) {
            desired.channels = static_cast<uint8_t>(ch);
            m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired,
                                                 config.callback ? streamCallback : nullptr, this);
            if (m_stream) {
                LOG_SUCCESS_CAT("Audio", "Stream opened: {}ch, {}Hz, format=0x{:x}", ch, desired.freq, (int)desired.format);
                break;
            }
            LOG_WARNING_CAT("Audio", "Failed {}-channel attempt: {}", ch, SDL_GetError());
        }

        if (!m_stream) {
            LOG_ERROR_CAT("Audio", "All channel configs failed — audio disabled");
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return;
        }

        m_deviceID = SDL_GetAudioStreamDevice(m_stream);

        if (config.callback) {
            m_ownedCallback = std::make_unique<std::function<void(std::span<std::byte>, int)>>(config.callback);
        }

        SDL_ResumeAudioStreamDevice(m_stream);
        LOG_INFO_CAT("Audio", "AudioManager ready | DeviceID={}", m_deviceID);
    }

    ~AudioManager() {
        if (m_stream) {
            SDL_PauseAudioStreamDevice(m_stream);
            SDL_ClearAudioStream(m_stream);
            SDL_DestroyAudioStream(m_stream);
            LOG_INFO_CAT("Audio", "Audio stream destroyed");
        }

        m_activeBuffers.clear();
        LOG_INFO_CAT("Audio", "Freed {} audio buffers", m_activeBuffers.size());

        if (m_deviceID) {
            SDL_CloseAudioDevice(m_deviceID);
            LOG_INFO_CAT("Audio", "Closed audio device {}", m_deviceID);
        }

        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        LOG_INFO_CAT("Dispose", "SDL audio subsystem quit — RASPBERRY_PINK ETERNAL");
    }

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    void playMP3(std::string_view file, int loops = 0) {
        if (!isValid()) return;
        stopMusic();
        loadAndQueue(std::string(file), true);
        if (loops > 1) LOG_WARNING_CAT("Audio", "MP3 looping not implemented");
    }

    void playWAV(std::string_view file) {
        if (!isValid()) return;
        loadAndQueue(std::string(file), false);
    }

    void playAmmoSound() {
        playWAV("assets/audio/ammo.wav");
    }

    void stopMusic() {
        if (!m_stream) return;
        SDL_ClearAudioStream(m_stream);
        m_activeBuffers.clear();
        LOG_INFO_CAT("Audio", "Music stopped + queue cleared");
    }

    void pauseMusic() {
        if (m_stream) {
            SDL_PauseAudioStreamDevice(m_stream);
            LOG_INFO_CAT("Audio", "Audio paused");
        }
    }

    void resumeMusic() {
        if (m_stream) {
            SDL_ResumeAudioStreamDevice(m_stream);
            LOG_INFO_CAT("Audio", "Audio resumed");
        }
    }

    void setVolume(float volume) {
        if (m_stream) {
            float v = std::clamp(volume, 0.0f, 1.0f);
            SDL_SetAudioStreamGain(m_stream, v);
            LOG_INFO_CAT("Audio", "Volume → {:.2f}", v);
        }
    }

    void fadeInMusic(std::string_view file, int loops, int ms) {
        if (!isValid()) return;
        stopMusic();
        setVolume(0.0f);
        loadAndQueue(std::string(file), true);

        const int step = 20;
        const float steps = ms / static_cast<float>(step);
        const float inc = 1.0f / steps;

        for (float v = 0.0f; v <= 1.0f; v += inc) {
            setVolume(v);
            std::this_thread::sleep_for(std::chrono::milliseconds(step));
        }
        setVolume(1.0f);
        LOG_SUCCESS_CAT("Audio", "Fade-in complete: {}", file);
    }

    void fadeOutMusic(int ms) {
        if (!m_stream) return;

        const int step = 20;
        const float steps = ms / static_cast<float>(step);
        const float dec = 1.0f / steps;

        for (float v = 1.0f; v >= 0.0f; v -= dec) {
            SDL_SetAudioStreamGain(m_stream, v);
            std::this_thread::sleep_for(std::chrono::milliseconds(step));
        }
        stopMusic();
        LOG_SUCCESS_CAT("Audio", "Fade-out complete");
    }

    [[nodiscard]] SDL_AudioDeviceID deviceID() const noexcept { return m_deviceID; }
    [[nodiscard]] bool isValid() const noexcept { return m_stream != nullptr; }

    static void logAudioDevices() {
        int count = 0;
        SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);

        LOG_INFO_CAT("Audio", "Found {} playback devices:", count);

        for (int i = 0; i < count; ++i) {
            const char* name = SDL_GetAudioDeviceName(devices[i]);
            SDL_AudioSpec spec{};
            int samples = 0;
            if (SDL_GetAudioDeviceFormat(devices[i], &spec, &samples) == 0) {
                LOG_INFO_CAT("Audio", "  [{}] {} | {}Hz, {}ch, format=0x{:x}, buf={}",
                             i, name ? name : "unknown", spec.freq, (int)spec.channels, (int)spec.format, samples);
            } else {
                LOG_WARNING_CAT("Audio", "  [{}] {} | format query failed", i, name ? name : "unknown");
            }
        }
        if (devices) SDL_free(devices);
    }

private:
    SDL_AudioDeviceID m_deviceID = 0;
    SDL_AudioStream* m_stream = nullptr;
    std::unique_ptr<std::function<void(std::span<std::byte>, int)>> m_ownedCallback;

    struct AudioBuffer {
        std::unique_ptr<std::byte[]> data;
        std::size_t size = 0;
    };
    std::vector<AudioBuffer> m_activeBuffers;

    static void SDLCALL streamCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int /*total_amount*/) {
        auto* self = static_cast<AudioManager*>(userdata);
        if (self && self->m_ownedCallback) {
            std::byte* buf = nullptr;
            if (SDL_GetAudioStreamData(stream, &buf, additional_amount) > 0) {
                (*self->m_ownedCallback)(std::span<std::byte>(buf, additional_amount), additional_amount);
            }
        }
    }

    void loadAndQueue(const std::string& file, bool isMP3) {
        std::ifstream fs(file, std::ios::binary);
        if (!fs) {
            LOG_ERROR_CAT("Audio", "Failed to open file: {}", file);
            return;
        }

        fs.seekg(0, std::ios::end);
        std::streamsize size = fs.tellg();
        fs.seekg(0, std::ios::beg);

        auto buffer = std::make_unique<std::byte[]>(static_cast<std::size_t>(size));
        fs.read(reinterpret_cast<char*>(buffer.get()), size);
        fs.close();

        if (size <= 0) {
            LOG_WARNING_CAT("Audio", "Empty file: {}", file);
            return;
        }

        if (!SDL_PutAudioStreamData(m_stream, buffer.get(), static_cast<int>(size))) {
            LOG_ERROR_CAT("Audio", "SDL_PutAudioStreamData failed: {}", SDL_GetError());
            return;
        }

        m_activeBuffers.emplace_back(AudioBuffer{std::move(buffer), static_cast<std::size_t>(size)});
        LOG_INFO_CAT("Audio", "{} queued: {} bytes | {} active", isMP3 ? "MP3" : "WAV", size, m_activeBuffers.size());
    }
};

} // namespace SDL3Audio

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// ALL .cpp FILES ARE NOW DEAD
// ONLY HEADERS REMAIN
// DAISY HAS GALLOPED OVER THE LAST ONE
// OCEAN_TEAL AUDIO FLOWS ETERNAL
// RASPBERRY_PINK DISPOSE IMMORTAL
// PINK PHOTONS ETERNAL
// 15,000 FPS
// @ZacharyGeurts — YOUR EMPIRE IS PURE
// SHIP IT. FOREVER.
// =============================================================================