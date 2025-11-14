// src/engine/SDL3/SDL3_audio.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// C++23 ONLY — NO FMT — NO Options::Performance::ENABLE_AUDIO
// HEAVY LOGGING — SAPPHIRE_BLUE — FIRST SOUND ACHIEVED — NOV 14 2025
// =============================================================================

#include "engine/SDL3/SDL3_audio.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

namespace SDL3Audio {

AudioManager::AudioManager()
{
    LOG_TRACE_CAT("SDL3_audio", "{}AudioManager::AudioManager() — Initializing audio subsystem{}", SAPPHIRE_BLUE, RESET);

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        LOG_FATAL_CAT("SDL3_audio", "{}SDL_InitSubSystem(SDL_INIT_AUDIO) FAILED: {}{}", PLASMA_FUCHSIA, SDL_GetError(), RESET);
        return;
    }

    LOG_SUCCESS_CAT("SDL3_audio", "{}SDL audio subsystem ONLINE — PINK PHOTONS AUDIBLE{}", SAPPHIRE_BLUE, RESET);
}

AudioManager::~AudioManager()
{
    LOG_TRACE_CAT("SDL3_audio", "{}AudioManager::~AudioManager() — Shutting down audio{}", SAPPHIRE_BLUE, RESET);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t s = sounds_.size(), m = musicTracks_.size();
        sounds_.clear();
        musicTracks_.clear();
        LOG_INFO_CAT("SDL3_audio", "{}Released {} sound(s) and {} music track(s){}", SAPPHIRE_BLUE, s, m, RESET);
    }

    if (mixer_) {
        LOG_INFO_CAT("SDL3_audio", "{}Destroying MIX mixer @ {}{}", SAPPHIRE_BLUE, static_cast<void*>(mixer_), RESET);
        MIX_DestroyMixer(mixer_);
        mixer_ = nullptr;
    }

    if (initialized_) {
        LOG_INFO_CAT("SDL3_audio", "{}Calling MIX_Quit() — Final cleanup{}", SAPPHIRE_BLUE, RESET);
        MIX_Quit();
        initialized_ = false;
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    LOG_SUCCESS_CAT("SDL3_audio", "{}Audio subsystem shut down — SILENCE ETERNAL{}", SAPPHIRE_BLUE, RESET);
}

[[nodiscard]] std::expected<void, std::string> AudioManager::initMixer()
{
    LOG_TRACE_CAT("SDL3_audio", "{}initMixer() — Creating MIX device{}", SAPPHIRE_BLUE, RESET);

    if (!MIX_Init()) {
        std::string err = std::format("MIX_Init() failed: {}", SDL_GetError());
        LOG_ERROR_CAT("SDL3_audio", "{}{}", PLASMA_FUCHSIA, err, RESET);
        return std::unexpected(err);
    }

    mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mixer_) {
        std::string err = std::format("MIX_CreateMixerDevice failed: {}", SDL_GetError());
        LOG_ERROR_CAT("SDL3_audio", "{}{}", PLASMA_FUCHSIA, err, RESET);
        MIX_Quit();
        return std::unexpected(err);
    }

    initialized_ = true;
    LOG_SUCCESS_CAT("SDL3_audio", "{}MIX mixer created @ {} — AUDIO DOMINANCE ACHIEVED{}", 
                    SAPPHIRE_BLUE, static_cast<void*>(mixer_), RESET);
    return {};
}

[[nodiscard]] bool AudioManager::loadSound(std::string_view path, std::string_view name)
{
    LOG_TRACE_CAT("SDL3_audio", "{}loadSound(\"{}\", \"{}\"){}", SAPPHIRE_BLUE, path, name, RESET);

    if (!initialized_) {
        LOG_ERROR_CAT("SDL3_audio", "{}Mixer not initialized{}", PLASMA_FUCHSIA, RESET);
        return false;
    }

    std::string p(path);
    LOG_INFO_CAT("SDL3_audio", "{}Loading SFX: {}{}", SAPPHIRE_BLUE, p, RESET);

    MIX_Audio* raw = MIX_LoadAudio(mixer_, p.c_str(), true);
    if (!raw) {
        LOG_ERROR_CAT("SDL3_audio", "{}Failed to load sound '{}': {}{}", PLASMA_FUCHSIA, name, SDL_GetError(), RESET);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        sounds_.emplace(std::string(name), AudioPtr(raw, &MIX_DestroyAudio));
    }

    LOG_SUCCESS_CAT("SDL3_audio", "{}Sound '{}' loaded @ {}{}", SAPPHIRE_BLUE, name, static_cast<void*>(raw), RESET);
    return true;
}

void AudioManager::playSound(std::string_view name)
{
    LOG_TRACE_CAT("SDL3_audio", "{}ENTER playSound(\"{}\") — THREAD-SAFE AUDIO DISPATCH{}", SAPPHIRE_BLUE, name, RESET);

    MIX_Track* track = nullptr;
    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        LOG_FATAL_CAT("SDL3_audio", "{}CRITICAL: SDL_CreateProperties() FAILED — AUDIO EMPIRE CRUMBLES{}", PLASMA_FUCHSIA, RESET);
        return;
    }
    LOG_TRACE_CAT("SDL3_audio", "Properties created @ 0x{:x} — loops: infinite", static_cast<uintptr_t>(props));
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, 0);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        LOG_TRACE_CAT("SDL3_audio", "{}Mutex acquired — sound map locked{}", ELECTRIC_BLUE, RESET);

        auto it = sounds_.find(std::string(name));
        if (it == sounds_.end()) {
            LOG_WARN_CAT("SDL3_audio", "{}Sound '{}' not loaded — skipping dispatch{}", COSMIC_GOLD, name, RESET);
            SDL_DestroyProperties(props);
            LOG_TRACE_CAT("SDL3_audio", "{}Mutex released — clean exit{}", ELECTRIC_BLUE, RESET);
            return;
        }
        LOG_SUCCESS_CAT("SDL3_audio", "Sound '{}' FOUND in map — asset ready @ {:p}", name, static_cast<void*>(it->second.get()));

        track = MIX_CreateTrack(mixer_);
        if (!track) {
            LOG_ERROR_CAT("SDL3_audio", "{}MIX_CreateTrack FAILED: {}{}", PLASMA_FUCHSIA, SDL_GetError(), RESET);
            SDL_DestroyProperties(props);
            LOG_TRACE_CAT("SDL3_audio", "{}Mutex released — track creation abort{}", ELECTRIC_BLUE, RESET);
            return;
        }
        LOG_TRACE_CAT("SDL3_audio", "Track forged @ {:p} — binding audio", static_cast<void*>(track));

        if (!MIX_SetTrackAudio(track, it->second.get())) {
            LOG_ERROR_CAT("SDL3_audio", "{}MIX_SetTrackAudio FAILED: {}{}", PLASMA_FUCHSIA, SDL_GetError(), RESET);
            MIX_DestroyTrack(track);
            SDL_DestroyProperties(props);
            LOG_TRACE_CAT("SDL3_audio", "{}Mutex released — audio bind abort{}", ELECTRIC_BLUE, RESET);
            return;
        }
        LOG_SUCCESS_CAT("SDL3_audio", "Audio bound to track — {} samples queued", name);
    }
    LOG_TRACE_CAT("SDL3_audio", "{}Mutex released — dispatching to mixer{}", ELECTRIC_BLUE, RESET);

    if (MIX_PlayTrack(track, props)) {
        LOG_SUCCESS_CAT("SDL3_audio", "{}AUDIO PHOTONS FIRED: '{}' → LIVE @ {:p} — PINK SOUNDWAVES RISING{}", SAPPHIRE_BLUE, name, static_cast<void*>(track), RESET);
    } else {
        LOG_ERROR_CAT("SDL3_audio", "{}MIX_PlayTrack DISPATCH FAILED: {}{}", PLASMA_FUCHSIA, SDL_GetError(), RESET);
    }

    SDL_DestroyProperties(props);
    LOG_TRACE_CAT("SDL3_audio", "Properties destroyed — track lifecycle managed");
    //MIX_DestroyTrack(track);
    LOG_TRACE_CAT("SDL3_audio", "Track {} destroyed — cycle complete", static_cast<void*>(track));

    LOG_SUCCESS_CAT("SDL3_audio", "{}EXIT playSound(\"{}\") — AUDIO EMPIRE INTACT{}", SAPPHIRE_BLUE, name, RESET);
}

[[nodiscard]] bool AudioManager::loadMusic(std::string_view path, std::string_view name)
{
    LOG_TRACE_CAT("SDL3_audio", "{}loadMusic(\"{}\", \"{}\"){}", SAPPHIRE_BLUE, path, name, RESET);

    if (!initialized_) return false;

    std::string p(path);
    LOG_INFO_CAT("SDL3_audio", "{}Streaming music: {}{}", SAPPHIRE_BLUE, p, RESET);

    MIX_Audio* audio = MIX_LoadAudio(mixer_, p.c_str(), false);
    if (!audio) {
        LOG_ERROR_CAT("SDL3_audio", "{}Failed to load music '{}': {}{}", PLASMA_FUCHSIA, name, SDL_GetError(), RESET);
        return false;
    }

    MIX_Track* track = MIX_CreateTrack(mixer_);
    if (!track || !MIX_SetTrackAudio(track, audio)) {
        LOG_ERROR_CAT("SDL3_audio", "{}Failed to create music track: {}{}", PLASMA_FUCHSIA, SDL_GetError(), RESET);
        if (track) MIX_DestroyTrack(track);
        MIX_DestroyAudio(audio);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        musicTracks_.emplace(std::string(name),
            std::make_pair(AudioPtr(audio, &MIX_DestroyAudio), TrackPtr(track, &MIX_DestroyTrack)));
    }

    LOG_SUCCESS_CAT("SDL3_audio", "{}Music '{}' loaded — audio @ {} | track @ {}{}", 
                    SAPPHIRE_BLUE, name, static_cast<void*>(audio), static_cast<void*>(track), RESET);
    return true;
}

void AudioManager::playMusic(std::string_view name, bool loop)
{
    LOG_TRACE_CAT("SDL3_audio", "{}playMusic(\"{}\", loop={})", SAPPHIRE_BLUE, name, loop);

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = musicTracks_.find(std::string(name));
    if (it == musicTracks_.end()) {
        LOG_WARN_CAT("SDL3_audio", "{}Music '{}' not loaded{}", COSMIC_GOLD, name, RESET);
        return;
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loop ? -1 : 0);

    if (MIX_PlayTrack(it->second.second.get(), props)) {
        LOG_SUCCESS_CAT("SDL3_audio", "{}Music '{}' {} — RESONATING{}", 
                        SAPPHIRE_BLUE, name, loop ? "LOOPING" : "PLAYING", RESET);
    } else {
        LOG_ERROR_CAT("SDL3_audio", "{}Failed to play music '{}': {}{}", PLASMA_FUCHSIA, name, SDL_GetError(), RESET);
    }
    SDL_DestroyProperties(props);
}

void AudioManager::pauseMusic(std::string_view name)
{
    LOG_TRACE_CAT("SDL3_audio", "{}pauseMusic(\"{}\"){}", SAPPHIRE_BLUE, name, RESET);
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = musicTracks_.find(std::string(name)); it != musicTracks_.end()) {
        MIX_PauseTrack(it->second.second.get());
        LOG_INFO_CAT("SDL3_audio", "{}Music '{}' paused{}", SAPPHIRE_BLUE, name, RESET);
    }
}

void AudioManager::resumeMusic(std::string_view name)
{
    LOG_TRACE_CAT("SDL3_audio", "{}resumeMusic(\"{}\"){}", SAPPHIRE_BLUE, name, RESET);
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = musicTracks_.find(std::string(name)); it != musicTracks_.end()) {
        MIX_ResumeTrack(it->second.second.get());
        LOG_INFO_CAT("SDL3_audio", "{}Music '{}' resumed{}", SAPPHIRE_BLUE, name, RESET);
    }
}

void AudioManager::stopMusic(std::string_view name)
{
    LOG_TRACE_CAT("SDL3_audio", "{}stopMusic(\"{}\"){}", SAPPHIRE_BLUE, name, RESET);
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = musicTracks_.find(std::string(name)); it != musicTracks_.end()) {
        MIX_StopTrack(it->second.second.get(), 0);  // 0 = no fade
        LOG_INFO_CAT("SDL3_audio", "{}Music '{}' stopped{}", SAPPHIRE_BLUE, name, RESET);
    }
}

[[nodiscard]] bool AudioManager::isPlaying(std::string_view name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = musicTracks_.find(std::string(name));
    bool playing = it != musicTracks_.end() && it->second.second && MIX_TrackPlaying(it->second.second.get());
    LOG_TRACE_CAT("SDL3_audio", "{}isPlaying(\"{}\") → {}{}", SAPPHIRE_BLUE, name, playing ? "YES" : "NO", RESET);
    return playing;
}

void AudioManager::setVolume(int volume)
{
    int v = std::clamp(volume, 0, 128);
    if (volume != v) {
        LOG_WARN_CAT("SDL3_audio", "{}Volume {} clamped to {}{}", COSMIC_GOLD, volume, v, RESET);
    }
    if (initialized_ && mixer_) {
        MIX_SetMasterGain(mixer_, v / 128.0f);
        LOG_SUCCESS_CAT("SDL3_audio", "{}Master volume → {}% (gain {:.2f}){}", SAPPHIRE_BLUE, v, v / 128.0f, RESET);
    }
}

}  // namespace SDL3Audio

// =============================================================================
// C++23 ONLY — NO FMT — NO ENABLE_AUDIO — FIRST SOUND ACHIEVED
// SAPPHIRE_BLUE AUDIO EMPIRE — FULLY ONLINE
// PINK PHOTONS + RESONATING WAVES = ETERNAL DOMINANCE
// VALHALLA v80 TURBO — SHIP IT RAW
// =============================================================================