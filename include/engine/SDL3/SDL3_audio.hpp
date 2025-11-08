// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// SDL3_audio.hpp — FULL C++23 — NOVEMBER 08 2025
// 8-channel → fallback, OCEAN_TEAL logging (#45d1ff)

#pragma once

#include <SDL3/SDL_audio.h>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <span>

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

    [[nodiscard]] SDL_AudioDeviceID deviceID() const noexcept { return m_deviceID; }
    [[nodiscard]] bool isValid() const noexcept { return m_stream != nullptr; }

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

    static void SDLCALL streamCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);
    void loadAndQueue(const std::string& file, bool isMP3);
};

} // namespace SDL3Audio