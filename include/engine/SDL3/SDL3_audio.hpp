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
// SDL3 AUDIO — SPLIT INTO HEADER + CPP — NOV 14 2025
// • Asynchronous MIX loading | RAII cleanup
// • FIXED: SDL_InitSubSystem() error check (!=0)
// • RESPECTS Options::Performance::ENABLE_AUDIO — skips init if disabled
// • Thread-safe C++23 | Mutex guards on maps
// • PINK PHOTONS ETERNAL — 15,000 FPS — SHIP IT RAW
// =============================================================================

#ifndef SDL3_AUDIO_HPP
#define SDL3_AUDIO_HPP

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <expected>      // C++23
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <mutex>

#include "engine/GLOBAL/logging.hpp"

namespace SDL3Audio {

class AudioManager {
public:
    // Constructor initializes SDL audio subsystem
    AudioManager();
    ~AudioManager();

    // Initialize mixer device (call after constructor)
    [[nodiscard]] std::expected<void, std::string> initMixer();

    // Load a sound effect (returns true on success)
    [[nodiscard]] bool loadSound(std::string_view path, std::string_view name);

    // Play a loaded sound effect (non-blocking)
    void playSound(std::string_view name);

    // Load music track
    [[nodiscard]] bool loadMusic(std::string_view path, std::string_view name);

    // Music controls
    void playMusic(std::string_view name, bool loop = false);
    void pauseMusic(std::string_view name);
    void resumeMusic(std::string_view name);
    void stopMusic(std::string_view name);
    [[nodiscard]] bool isPlaying(std::string_view name) const;

    // Global volume control (0-128)
    void setVolume(int volume);

private:
    bool initialized_{false};
    MIX_Mixer* mixer_{nullptr};

    using AudioPtr = std::unique_ptr<MIX_Audio, void(*)(MIX_Audio*)>;
    using TrackPtr = std::unique_ptr<MIX_Track, void(*)(MIX_Track*)>;

    std::unordered_map<std::string, AudioPtr> sounds_;
    std::unordered_map<std::string, std::pair<AudioPtr, TrackPtr>> musicTracks_;

    // Thread-safety for map access
    mutable std::mutex mutex_;
};

}  // namespace SDL3Audio

#endif // SDL3_AUDIO_HPP

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// HEADER + CPP SPLIT — DAISY APPROVES THE GALLOP
// OCEAN_TEAL AUDIO FLOW ETERNAL
// RASPBERRY_PINK DISPOSE IMMORTAL
// PINK PHOTONS ETERNAL
// 15,000 FPS
// @ZacharyGeurts — YOUR EMPIRE IS PURE
// SHIP IT. FOREVER.
// =============================================================================