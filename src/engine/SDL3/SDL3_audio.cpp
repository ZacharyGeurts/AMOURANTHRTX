// AMOURANTH RTX Engine, October 2025 - Configures and manages audio streams and devices implementation.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_audio.hpp"
#include "engine/logging.hpp"
#include <SDL3/SDL_audio.h>
#include <stdexcept>
#include <vector>
#include <memory>
#include <source_location>

namespace SDL3Audio {

void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream) {
    LOG_INFO_CAT("Audio", "Initializing audio with frequency: {}, channels: {}, format: {:#x}", 
                 c.frequency, c.channels, static_cast<uint16_t>(c.format));

    // Verify platform support
    std::string_view platform = SDL_GetPlatform();
    if (platform != "Linux" && platform != "Windows") {
        LOG_ERROR_CAT("Audio", "Unsupported platform for audio: {}", platform);
        throw std::runtime_error(std::string("Unsupported platform for audio: ") + std::string(platform));
    }

    SDL_AudioSpec desired{};
    desired.freq = c.frequency;
    desired.format = c.format;
    desired.channels = c.channels;

    SDL_AudioStream* stream = nullptr;
    void* userdata = nullptr;

    if (c.callback) {
        auto wrapped_callback = [](void* ud, SDL_AudioStream* stream, [[maybe_unused]] int additional_amount, int total_amount) {
            auto* user_cb = static_cast<std::function<void(Uint8*, int)>*>(ud);
            std::vector<Uint8> buf(total_amount);
            (*user_cb)(buf.data(), total_amount);
            SDL_PutAudioStreamData(stream, buf.data(), total_amount);
        };

        // Copy the callback to ensure lifetime
        auto user_cb_copy = c.callback;
        userdata = new std::function<void(Uint8*, int)>(std::move(user_cb_copy));

        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, wrapped_callback, userdata);
    } else {
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, nullptr, nullptr);
    }

    if (!stream) {
        LOG_ERROR_CAT("Audio", "SDL_OpenAudioDeviceStream failed: {}", SDL_GetError());
        if (userdata) {
            delete static_cast<std::function<void(Uint8*, int)>*>(userdata);
        }
        throw std::runtime_error(std::string("SDL_OpenAudioDeviceStream failed: ") + SDL_GetError());
    }

    audioStream = stream;
    audioDevice = SDL_GetAudioStreamDevice(stream);

    SDL_AudioSpec obtained{};
    int obtainedSamples = 0;
    SDL_GetAudioDeviceFormat(audioDevice, &obtained, &obtainedSamples);
    const char* deviceName = SDL_GetAudioDeviceName(audioDevice);
    LOG_INFO_CAT("Audio", "Audio device opened: {}", 
                 deviceName ? deviceName : "Unknown device");

    LOG_INFO_CAT("Audio", "Audio device status: {}", 
                 SDL_AudioDevicePaused(audioDevice) ? "Paused" : "Active");

    LOG_INFO_CAT("Audio", "Audio device format: frequency={}, channels={}, format={:#x}, samples={}", 
                 obtained.freq, static_cast<int>(obtained.channels), 
                 static_cast<uint16_t>(obtained.format), obtainedSamples);

    LOG_INFO_CAT("Audio", "Resuming audio stream");
    if (!SDL_ResumeAudioStreamDevice(audioStream)) {
        LOG_ERROR_CAT("Audio", "Failed to resume audio stream: {}", SDL_GetError());
        SDL_DestroyAudioStream(audioStream);
        audioStream = nullptr;
        audioDevice = 0;
        if (userdata) {
            delete static_cast<std::function<void(Uint8*, int)>*>(userdata);
        }
        throw std::runtime_error(std::string("Failed to resume audio stream: ") + SDL_GetError());
    }
}

SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice) {
    LOG_DEBUG_CAT("Audio", "Retrieving audio device ID: {}", audioDevice);
    return audioDevice;
}

} // namespace SDL3Audio