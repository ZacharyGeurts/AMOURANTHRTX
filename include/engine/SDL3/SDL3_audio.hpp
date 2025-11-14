// include/engine/SDL3/SDL3_audio.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <mutex>

#include "engine/GLOBAL/logging.hpp"

namespace SDL3Audio {

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    [[nodiscard]] std::expected<void, std::string> initMixer();

    [[nodiscard]] bool loadSound(std::string_view path, std::string_view name);
    void playSound(std::string_view name);

    [[nodiscard]] bool loadMusic(std::string_view path, std::string_view name);
    void playMusic(std::string_view name, bool loop = false);
    void pauseMusic(std::string_view name);
    void resumeMusic(std::string_view name);
    void stopMusic(std::string_view name);
    [[nodiscard]] bool isPlaying(std::string_view name) const;

    void setVolume(int volume);

private:
    bool initialized_{false};
    MIX_Mixer* mixer_{nullptr};

    using AudioPtr = std::unique_ptr<MIX_Audio, void(*)(MIX_Audio*)>;
    using TrackPtr = std::unique_ptr<MIX_Track, void(*)(MIX_Track*)>;

    std::unordered_map<std::string, AudioPtr> sounds_;
    std::unordered_map<std::string, std::pair<AudioPtr, TrackPtr>> musicTracks_;

    mutable std::mutex mutex_;
};

}  // namespace SDL3Audio