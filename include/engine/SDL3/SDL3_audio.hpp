// AMOURANTH RTX Engine, October 2025 - Configures and manages audio streams and devices.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library.
// Usage: Call initAudio with AudioConfig to set up audio device and stream.
// Zachary Geurts 2025

#ifndef SDL3_AUDIO_HPP
#define SDL3_AUDIO_HPP

#include <SDL3/SDL_audio.h>
#include <functional>
#include <string>

namespace SDL3Audio {

struct AudioConfig {
    int frequency = 44100;
    SDL_AudioFormat format = SDL_AUDIO_S16LE;
    int channels = 8; // 8-channel audio
    std::function<void(Uint8*, int)> callback;
};

void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream);

SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice);

} // namespace SDL3Audio

#endif // SDL3_AUDIO_HPP