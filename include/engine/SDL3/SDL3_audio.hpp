#pragma once

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <expected>      // C++23
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>

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
    bool isPlaying(std::string_view name) const;

    // Global volume control (0-128)
    void setVolume(int volume);

private:
    bool initialized_{false};
    MIX_Mixer* mixer_{nullptr};

    using AudioPtr = std::unique_ptr<MIX_Audio, void(*)(MIX_Audio*)>;
    using TrackPtr = std::unique_ptr<MIX_Track, void(*)(MIX_Track*)>;

    std::unordered_map<std::string, AudioPtr> sounds_;
    std::unordered_map<std::string, std::pair<AudioPtr, TrackPtr>> musicTracks_;
};

}  // namespace SDL3Audio