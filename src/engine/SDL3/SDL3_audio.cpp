#include "engine/SDL3/SDL3_audio.hpp"
#include <SDL3/SDL_log.h>

namespace SDL3Audio {

AudioManager::AudioManager() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                     "Failed to init SDL audio: %s", SDL_GetError());
    }
}

AudioManager::~AudioManager() {
    if (mixer_) {
        MIX_DestroyMixer(mixer_);
        mixer_ = nullptr;
    }
    if (initialized_) MIX_Quit();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

/* --------------------------------------------------------------- */
std::expected<void, std::string> AudioManager::initMixer() {
    if (!MIX_Init()) {
        return std::unexpected(std::string("MIX_Init failed: ") + SDL_GetError());
    }

    mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mixer_) {
        MIX_Quit();
        return std::unexpected(std::string("Failed to create mixer: ") + SDL_GetError());
    }

    initialized_ = true;
    return {};
}

/* --------------------------------------------------------------- */
bool AudioManager::loadSound(std::string_view path, std::string_view name) {
    if (!initialized_) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Mixer not initialized!");
        return false;
    }

    std::string p(path);
    MIX_Audio* raw = MIX_LoadAudio(mixer_, p.c_str(), true);   // pre‑decode SFX
    if (!raw) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                     "Failed to load sound %s: %s", name.data(), SDL_GetError());
        return false;
    }

    // emplace → no default‑construction of unique_ptr
    sounds_.emplace(std::string(name),
                    AudioPtr(raw, MIX_DestroyAudio));
    return true;
}

/* --------------------------------------------------------------- */
void AudioManager::playSound(std::string_view name) {
    auto it = sounds_.find(std::string(name));
    if (it == sounds_.end()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Sound %s not loaded!", name.data());
        return;
    }

    MIX_Track* tmp = MIX_CreateTrack(mixer_);
    if (!tmp) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                     "Failed to create temp track: %s", SDL_GetError());
        return;
    }

    if (!MIX_SetTrackAudio(tmp, it->second.get())) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                     "Failed to set track audio: %s", SDL_GetError());
        MIX_DestroyTrack(tmp);
        return;
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, "MIX_PROP_PLAY_LOOPS_NUMBER", 0);
    MIX_PlayTrack(tmp, props);
    SDL_DestroyProperties(props);
    MIX_DestroyTrack(tmp);          // non‑blocking
}

/* --------------------------------------------------------------- */
bool AudioManager::loadMusic(std::string_view path, std::string_view name) {
    if (!initialized_) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Mixer not initialized!");
        return false;
    }

    std::string p(path);
    MIX_Audio* raw = MIX_LoadAudio(mixer_, p.c_str(), false); // stream music
    if (!raw) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                     "Failed to load music %s: %s", name.data(), SDL_GetError());
        return false;
    }

    AudioPtr audio(raw, MIX_DestroyAudio);

    MIX_Track* t = MIX_CreateTrack(mixer_);
    if (!t) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                     "Failed to create music track: %s", SDL_GetError());
        return false;
    }

    if (!MIX_SetTrackAudio(t, raw)) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                     "Failed to set music track audio: %s", SDL_GetError());
        MIX_DestroyTrack(t);
        return false;
    }

    TrackPtr track(t, MIX_DestroyTrack);

    // emplace the pair (audio + track)
    musicTracks_.emplace(std::string(name),
                         std::make_pair(std::move(audio), std::move(track)));
    return true;
}

/* --------------------------------------------------------------- */
void AudioManager::playMusic(std::string_view name, bool loop) {
    auto it = musicTracks_.find(std::string(name));
    if (it == musicTracks_.end()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Music %s not loaded!", name.data());
        return;
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, "MIX_PROP_PLAY_LOOPS_NUMBER", loop ? -1 : 0);
    MIX_PlayTrack(it->second.second.get(), props);
    SDL_DestroyProperties(props);
}

/* --------------------------------------------------------------- */
void AudioManager::pauseMusic(std::string_view name) {
    auto it = musicTracks_.find(std::string(name));
    if (it != musicTracks_.end()) MIX_PauseTrack(it->second.second.get());
}
void AudioManager::resumeMusic(std::string_view name) {
    auto it = musicTracks_.find(std::string(name));
    if (it != musicTracks_.end()) MIX_ResumeTrack(it->second.second.get());
}
void AudioManager::stopMusic(std::string_view name) {
    auto it = musicTracks_.find(std::string(name));
    if (it != musicTracks_.end()) MIX_StopTrack(it->second.second.get(), 0);
}
bool AudioManager::isPlaying(std::string_view name) const {
    auto it = musicTracks_.find(std::string(name));
    return it != musicTracks_.end() && MIX_TrackPlaying(it->second.second.get());
}

/* --------------------------------------------------------------- */
void AudioManager::setVolume(int volume) {
    if (volume < 0 || volume > 128) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Volume out of range [0,128]");
        return;
    }
    float gain = static_cast<float>(volume) / 128.0f;
    MIX_SetMasterGain(mixer_, gain);
}

} // namespace SDL3Audio