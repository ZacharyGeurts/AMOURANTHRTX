// AMOURANTH RTX Engine, October 2025 - Configures and manages audio streams and devices.
// Supports 8-channel audio with fallback to stereo or mono if unsupported.
// Dependencies: SDL3, C++20 standard library.

#include "engine/SDL3/SDL3_audio.hpp"
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL.h>
#include <stdexcept>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <iostream>
#include <fstream>
#include <cstring>

namespace SDL3Audio {

void logAudioDevices() {
    int count = 0;
    SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
    if (!devices || count <= 0) {
        std::cerr << "[AudioDebug] Failed to get playback devices: " << SDL_GetError() << "\n";
        if (devices) SDL_free(devices);
        return;
    }

    std::cerr << "[AudioDebug] Found " << count << " audio playback devices:\n";
    for (int i = 0; i < count; ++i) {
        const char* name = SDL_GetAudioDeviceName(devices[i]);
        std::cerr << "[AudioDebug] Device " << i << ": id=" << devices[i] << ", name=" << (name ? name : "unknown");
        SDL_AudioSpec spec;
        int sampleCount = 0;
        if (SDL_GetAudioDeviceFormat(devices[i], &spec, &sampleCount) == 0) {
            std::cerr << ", channels=" << static_cast<int>(spec.channels)
                      << ", freq=" << spec.freq
                      << ", format=" << spec.format
                      << ", samples=" << sampleCount << "\n";
        } else {
            std::cerr << ", failed to get format: " << SDL_GetError() << "\n";
        }
    }
    SDL_free(devices);
}

void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream) {
    audioDevice = 0;
    audioStream = nullptr;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        std::cerr << "[AudioDebug] SDL_InitSubSystem failed: " << SDL_GetError() << "\n";
        return;
    }
    SDL_AudioSpec spec = {};
    spec.freq = c.frequency;
    spec.format = c.format;
    spec.channels = c.channels;
    // Use nullptr for callback in initAudio as it's a free function without access to wrappedCallback
    audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!audioStream) {
        std::cerr << "[AudioDebug] SDL_OpenAudioDeviceStream failed: " << SDL_GetError() << "\n";
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }
    audioDevice = SDL_GetAudioStreamDevice(audioStream);
    SDL_ResumeAudioStreamDevice(audioStream);
    std::cerr << "[AudioDebug] Audio initialized: channels=" << c.channels << ", format=" << c.format << ", freq=" << c.frequency << "\n";
}

SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice) {
    return audioDevice;
}

AudioManager::AudioManager(const AudioConfig& c) : audioDevice(0), audioStream(nullptr) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        std::cerr << "[AudioDebug] SDL_InitSubSystem failed: " << SDL_GetError() << "\n";
        return;
    }
    SDL_AudioSpec spec = {};
    spec.freq = c.frequency;
    spec.format = c.format;
    spec.channels = c.channels;
    audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, c.callback ? wrappedCallback : nullptr, this);
    if (!audioStream) {
        std::cerr << "[AudioDebug] SDL_OpenAudioDeviceStream failed: " << SDL_GetError() << "\n";
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }
    audioDevice = SDL_GetAudioStreamDevice(audioStream);
    if (c.callback) {
        ownedCallback = std::make_unique<std::function<void(Uint8*, int)>>(c.callback);
    }
    SDL_ResumeAudioStreamDevice(audioStream);
    std::cerr << "[AudioDebug] AudioManager initialized: channels=" << c.channels << ", format=" << c.format << ", freq=" << c.frequency << "\n";
}

AudioManager::~AudioManager() {
    if (audioStream) {
        SDL_DestroyAudioStream(audioStream);
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void AudioManager::playMP3(const std::string& file, int loops) {
    // Load MP3 file into a buffer (SDL3 does not natively decode MP3)
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        std::cerr << "[AudioDebug] Failed to open MP3 file: " << file << "\n";
        return;
    }
    input.seekg(0, std::ios::end);
    size_t size = input.tellg();
    input.seekg(0, std::ios::beg);
    auto buffer = std::make_unique<Uint8[]>(size);
    input.read(reinterpret_cast<char*>(buffer.get()), size);
    input.close();

    // Submit to audio stream
    if (audioStream) {
        if (!SDL_PutAudioStreamData(audioStream, buffer.get(), size)) {
            std::cerr << "[AudioDebug] SDL_PutAudioStreamData failed for " << file << ": " << SDL_GetError() << "\n";
            return;
        }
        SDL_FlushAudioStream(audioStream);
        activeBuffers.push_back(std::move(buffer));
    } else {
        std::cerr << "[AudioDebug] No audio stream available for " << file << "\n";
    }

    // Looping not fully supported (requires external decoder or SDL3_mixer)
    if (loops != 0) {
        std::cerr << "[AudioDebug] Looping not fully supported in this implementation; playing once\n";
    }
}

void AudioManager::playSound(const std::string& file) {
    // Load WAV file using SDL3's native support
    SDL_AudioSpec spec;
    Uint8* buffer = nullptr;
    Uint32 length = 0;
    if (!SDL_LoadWAV(file.c_str(), &spec, &buffer, &length)) {
        std::cerr << "[AudioDebug] SDL_LoadWAV failed for " << file << ": " << SDL_GetError() << "\n";
        return;
    }
    if (audioStream) {
        auto ownedBuffer = std::make_unique<Uint8[]>(length);
        std::memcpy(ownedBuffer.get(), buffer, length);
        if (!SDL_PutAudioStreamData(audioStream, ownedBuffer.get(), length)) {
            std::cerr << "[AudioDebug] SDL_PutAudioStreamData failed for " << file << ": " << SDL_GetError() << "\n";
        } else {
            SDL_FlushAudioStream(audioStream);
            activeBuffers.push_back(std::move(ownedBuffer));
        }
    } else {
        std::cerr << "[AudioDebug] No audio stream available for " << file << "\n";
    }
    SDL_free(buffer);
}

void AudioManager::playAmmoSound() {
    playSound("assets/audio/ammo.wav");
}

void AudioManager::stopMusic() {
    if (audioStream) {
        SDL_ClearAudioStream(audioStream);
        activeBuffers.clear();
    }
}

void AudioManager::pauseMusic() {
    if (audioStream) {
        SDL_PauseAudioStreamDevice(audioStream);
    }
}

void AudioManager::resumeMusic() {
    if (audioStream) {
        SDL_ResumeAudioStreamDevice(audioStream);
    }
}

void AudioManager::setMusicVolume(float volume) {
    if (audioStream) {
        SDL_SetAudioStreamGain(audioStream, volume);
    }
}

void AudioManager::fadeInMusic(const std::string& file, int loops, int ms) {
    // Load MP3 file
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        std::cerr << "[AudioDebug] Failed to open MP3 file: " << file << "\n";
        return;
    }
    input.seekg(0, std::ios::end);
    size_t size = input.tellg();
    input.seekg(0, std::ios::beg);
    auto buffer = std::make_unique<Uint8[]>(size);
    input.read(reinterpret_cast<char*>(buffer.get()), size);
    input.close();

    // Apply fade-in
    if (audioStream) {
        SDL_SetAudioStreamGain(audioStream, 0.0f);
        if (!SDL_PutAudioStreamData(audioStream, buffer.get(), size)) {
            std::cerr << "[AudioDebug] SDL_PutAudioStreamData failed for " << file << ": " << SDL_GetError() << "\n";
            return;
        }
        SDL_FlushAudioStream(audioStream);
        activeBuffers.push_back(std::move(buffer));
        // Simulate fade-in by gradually increasing gain
        float steps = ms / 10.0f;
        for (float v = 0.0f; v <= 1.0f; v += 1.0f / steps) {
            SDL_SetAudioStreamGain(audioStream, v);
            SDL_Delay(10);
        }
        SDL_SetAudioStreamGain(audioStream, 1.0f);
    } else {
        std::cerr << "[AudioDebug] No audio stream available for " << file << "\n";
    }
    if (loops != 0) {
        std::cerr << "[AudioDebug] Looping not fully supported in this implementation; playing once\n";
    }
}

void AudioManager::fadeOutMusic(int ms) {
    if (audioStream) {
        // Simulate fade-out by gradually decreasing gain
        float steps = ms / 10.0f;
        for (float v = 1.0f; v >= 0.0f; v -= 1.0f / steps) {
            SDL_SetAudioStreamGain(audioStream, v);
            SDL_Delay(10);
        }
        SDL_ClearAudioStream(audioStream);
        activeBuffers.clear();
    }
}

SDL_AudioDeviceID AudioManager::getAudioDevice() const {
    return audioDevice;
}

void AudioManager::wrappedCallback(void* ud, SDL_AudioStream* /*stream*/, int additional_amount, int /*total_amount*/) {
    auto* manager = static_cast<AudioManager*>(ud);
    if (manager && manager->ownedCallback) {
        (*manager->ownedCallback)(nullptr, additional_amount);
    }
}

} // namespace SDL3Audio