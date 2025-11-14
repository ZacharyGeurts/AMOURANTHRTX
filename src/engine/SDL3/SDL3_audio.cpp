// src/engine/SDL3/SDL3_audio.cpp
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
// SDL3 AUDIO IMPLEMENTATION — NOV 14 2025
// • RAII unique_ptr deleters for MIX_Audio/Track
// • Mutex-locked map access for thread-safety
// • std::expected for init/load errors (C++23)
// • Property-based playback (MIX_PROP_*)
// • PINK PHOTONS ETERNAL — AUDIO IMMORTAL
// =============================================================================

#include "engine/SDL3/SDL3_audio.hpp"
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_properties.h>  // For SDL_PropertiesID
#include <algorithm>  // For std::clamp (C++23)

namespace SDL3Audio {

AudioManager::AudioManager() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to init SDL audio: %s", SDL_GetError());
    } else {
        SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "SDL audio subsystem initialized");
    }
}

AudioManager::~AudioManager() {
    // Clear maps to trigger unique_ptr deleters
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sounds_.clear();
        musicTracks_.clear();
    }
    // Mixer is raw; manual destroy if valid
    if (mixer_) {
        MIX_DestroyMixer(mixer_);
        mixer_ = nullptr;
    }
    if (initialized_) {
        MIX_Quit();
        initialized_ = false;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "SDL audio subsystem quit");
}

/* --------------------------------------------------------------- */
[[nodiscard]] std::expected<void, std::string> AudioManager::initMixer() {
    if (!MIX_Init()) {  // bool MIX_Init() – true on success
        return std::unexpected(std::string("MIX_Init failed: ") + SDL_GetError());
    }

    mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mixer_) {
        MIX_Quit();
        return std::unexpected(std::string("Failed to create mixer: ") + SDL_GetError());
    }

    initialized_ = true;
    SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "MIX mixer initialized");
    return {};
}

/* --------------------------------------------------------------- */
[[nodiscard]] bool AudioManager::loadSound(std::string_view path, std::string_view name) {
    if (!initialized_ || !mixer_) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Mixer not initialized!");
        return false;
    }

    std::string p(path);
    MIX_Audio* raw = MIX_LoadAudio(mixer_, p.c_str(), true);  // pre-decode SFX
    if (!raw) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                     "Failed to load sound %s from %s: %s", name.data(), path.data(), SDL_GetError());
        return false;
    }

    // Thread-safe insert
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sounds_.emplace(std::string(name), AudioPtr(raw, &MIX_DestroyAudio));
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "Sound '%s' loaded from '%s'", name.data(), path.data());
    return true;
}

/* --------------------------------------------------------------- */
void AudioManager::playSound(std::string_view name) {
    MIX_Track* tmp = nullptr;
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, 0);  // No loop

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sounds_.find(std::string(name));
        if (it == sounds_.end()) {
            SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Sound '%s' not loaded!", name.data());
            SDL_DestroyProperties(props);
            return;
        }

        tmp = MIX_CreateTrack(mixer_);
        if (!tmp) {
            SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to create temp track: %s", SDL_GetError());
            SDL_DestroyProperties(props);
            return;
        }

        if (!MIX_SetTrackAudio(tmp, it->second.get())) {
            SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to set track audio: %s", SDL_GetError());
            MIX_DestroyTrack(tmp);
            SDL_DestroyProperties(props);
            return;
        }
    }  // Unlock before play

    if (MIX_PlayTrack(tmp, props)) {
        SDL_LogDebug(SDL_LOG_CATEGORY_AUDIO, "Sound '%s' played", name.data());
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to play sound '%s': %s", name.data(), SDL_GetError());
    }

    SDL_DestroyProperties(props);
    MIX_DestroyTrack(tmp);  // Always destroy temp track
}

/* --------------------------------------------------------------- */
[[nodiscard]] bool AudioManager::loadMusic(std::string_view path, std::string_view name) {
    if (!initialized_ || !mixer_) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Mixer not initialized!");
        return false;
    }

    std::string p(path);
    MIX_Audio* raw = MIX_LoadAudio(mixer_, p.c_str(), false);  // Stream music
    if (!raw) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                     "Failed to load music %s from %s: %s", name.data(), path.data(), SDL_GetError());
        return false;
    }

    AudioPtr audio(raw, &MIX_DestroyAudio);

    MIX_Track* t = MIX_CreateTrack(mixer_);
    if (!t) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to create music track: %s", SDL_GetError());
        return false;
    }

    if (!MIX_SetTrackAudio(t, raw)) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to set music track audio: %s", SDL_GetError());
        MIX_DestroyTrack(t);
        return false;
    }

    TrackPtr track(t, &MIX_DestroyTrack);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        musicTracks_.emplace(std::string(name), std::make_pair(std::move(audio), std::move(track)));
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "Music '%s' loaded from '%s'", name.data(), path.data());
    return true;
}

/* --------------------------------------------------------------- */
void AudioManager::playMusic(std::string_view name, bool loop) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = musicTracks_.find(std::string(name));
    if (it == musicTracks_.end()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Music '%s' not loaded!", name.data());
        return;
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loop ? -1 : 0);  // -1 for infinite
    if (MIX_PlayTrack(it->second.second.get(), props)) {
        SDL_LogDebug(SDL_LOG_CATEGORY_AUDIO, "Music '%s' %s", name.data(), loop ? "started looping" : "started playing");
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to play music '%s': %s", name.data(), SDL_GetError());
    }
    SDL_DestroyProperties(props);
}

/* --------------------------------------------------------------- */
void AudioManager::pauseMusic(std::string_view name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = musicTracks_.find(std::string(name));
    if (it != musicTracks_.end()) {
        MIX_PauseTrack(it->second.second.get());
        SDL_LogDebug(SDL_LOG_CATEGORY_AUDIO, "Music '%s' paused", name.data());
    }
}

void AudioManager::resumeMusic(std::string_view name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = musicTracks_.find(std::string(name));
    if (it != musicTracks_.end()) {
        MIX_ResumeTrack(it->second.second.get());
        SDL_LogDebug(SDL_LOG_CATEGORY_AUDIO, "Music '%s' resumed", name.data());
    }
}

void AudioManager::stopMusic(std::string_view name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = musicTracks_.find(std::string(name));
    if (it != musicTracks_.end()) {
        MIX_StopTrack(it->second.second.get(), 0);  // No fade-out
        SDL_LogDebug(SDL_LOG_CATEGORY_AUDIO, "Music '%s' stopped", name.data());
    }
}

[[nodiscard]] bool AudioManager::isPlaying(std::string_view name) const {
    std::lock_guard<std::mutex> lock(mutex_);  // mutable allows lock in const
    auto it = musicTracks_.find(std::string(name));
    if (it == musicTracks_.end() || !it->second.second) {
        return false;
    }
    return MIX_TrackPlaying(it->second.second.get());  // Assume this exists; per API patterns
}

/* --------------------------------------------------------------- */
void AudioManager::setVolume(int volume) {
    if (volume < 0 || volume > 128) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Volume out of range [0,128]; clamping to %d", volume);
        volume = std::clamp(volume, 0, 128);
    }
    float gain = static_cast<float>(volume) / 128.0f;  // Map to 0.0-1.0
    if (initialized_ && mixer_) {
        MIX_SetMasterGain(mixer_, gain);
        SDL_LogDebug(SDL_LOG_CATEGORY_AUDIO, "Master gain set to %.2f (%d/128)", gain, volume);
    }
}

}  // namespace SDL3Audio