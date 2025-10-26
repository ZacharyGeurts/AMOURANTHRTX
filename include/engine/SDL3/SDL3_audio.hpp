// AMOURANTH RTX Engine, October 2025 - Configures and manages audio streams and devices.
// Supports 8-channel audio with fallback to stereo or mono if unsupported.
// Dependencies: SDL3, C++20 standard library.

#ifndef SDL3_AUDIO_HPP
#define SDL3_AUDIO_HPP

#include <SDL3/SDL_audio.h>
#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace SDL3Audio {

struct AudioConfig {
    int frequency = 44100;
    SDL_AudioFormat format = SDL_AUDIO_S16LE;
    int channels = 8; // 8-channel audio
    std::function<void(Uint8*, int)> callback;
};

void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream);
void logAudioDevices(); // For debugging
SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice);

class AudioManager {
public:
    AudioManager(const AudioConfig& c);
    ~AudioManager();
    void playMP3(const std::string& file, int loops = 1);
    void playSound(const std::string& file);
    void playAmmoSound();
    void stopMusic();
    void pauseMusic();
    void resumeMusic();
    void setMusicVolume(float volume);
    void fadeInMusic(const std::string& file, int loops, int ms);
    void fadeOutMusic(int ms);
    SDL_AudioDeviceID getAudioDevice() const;

private:
    SDL_AudioDeviceID audioDevice = 0;
    SDL_AudioStream* audioStream = nullptr;
    std::unique_ptr<std::function<void(Uint8*, int)>> ownedCallback;
    void* userdata = nullptr;
    std::vector<std::unique_ptr<Uint8[]>> activeBuffers;

    static void wrappedCallback(void* ud, SDL_AudioStream* stream, int additional_amount, int total_amount);
};

} // namespace SDL3Audio

#endif // SDL3_AUDIO_HPP