#pragma once

// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <atomic>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <string>

#include "ELLIE.hpp"
#include "AMOURANTHRTX.hpp"
#include "OptionsMenu.hpp"
#include "InputManager.hpp"

// ────────────────────────────────────────────────
// Globals — raw and eternal (no atomics where plain types suffice)
// ────────────────────────────────────────────────
inline SDL_Window*               g_window = nullptr;

inline SDL_AudioDeviceID         g_audio_device = 0;
inline SDL_AudioStream*          g_audio_stream = nullptr;

struct SoundData {
    Uint8* buffer = nullptr;
    Uint32 length = 0;
    SDL_AudioSpec spec{};
};
inline std::unordered_map<std::string, std::unique_ptr<SoundData>> g_sounds;

inline std::unordered_map<SDL_JoystickID, SDL_Gamepad*> g_gamepads;

// ────────────────────────────────────────────────
// Window — raw SDL3
// ────────────────────────────────────────────────
inline void sdl_window_create(int width, int height, const char* title) noexcept {
    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (Options::Window::START_FULLSCREEN) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    g_window = SDL_CreateWindow(title, width, height, flags);
    if (g_window == nullptr) {
        LOG_FATAL_CAT("SDL3_window", "Failed to create window: {}", SDL_GetError());
        return;
    }

    LOG_SUCCESS_CAT("SDL3_window", "Window created — {}x{}", width, height);
}

inline bool sdl_poll_events(int& out_w, int& out_h, bool& quit, bool& toggle_fs) noexcept {
    SDL_Event ev;
    quit = toggle_fs = false;
    bool event_seen = false;

    while (SDL_PollEvent(&ev)) {
        event_seen = true;

        // Forward every event to InputManager first
        INPUT.pumpEvents(ev);

        // Window-level handling only — no resize globals anymore
        switch (ev.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                LOG_INFO_CAT("SDL3_window", "Quit requested");
                quit = true;
                break;
            }

            case SDL_EVENT_KEY_DOWN: {
                if (ev.key.scancode == SDL_SCANCODE_F11) {
                    LOG_INFO_CAT("INPUT", "F11 pressed — toggling fullscreen");
                    toggle_fs = true;
                }
                break;
            }

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                if (!Options::Window::ALLOW_RESIZE) {
                    LOG_INFO_CAT("SDL3_window", "Resize event ignored (ALLOW_RESIZE=false)");
                    break;
                }

                int ew = ev.window.data1;
                int eh = ev.window.data2;

                if (ew <= 0 || eh <= 0) {
                    LOG_INFO_CAT("SDL3_window", "Window minimized (event size {}x{})", ew, eh);
                } else {
                    LOG_INFO_CAT("SDL3_window", "Resize event received — {}x{}", ew, eh);
                    // No globals, no flags here — RayCanvas will detect & handle via SDL_GetWindowSizeInPixels
                }
                break;
            }

            default:
                // Everything else (mouse, gamepad, focus, text, etc.) already pumped to INPUT
                break;
        }
    }

    // Always provide current real pixel size to caller
    // (RayCanvas ignores out_w/out_h now, but we keep them for compatibility)
    if (g_window != nullptr) {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(g_window, &w, &h);

        out_w = (w > 0) ? w : 1;
        out_h = (h > 0) ? h : 1;
    } else {
        out_w = 1;
        out_h = 1;
    }

    return event_seen;
}

inline void sdl_toggle_fullscreen() noexcept {
    if (g_window == nullptr) return;

    Uint64 flags = SDL_GetWindowFlags(g_window);
    bool is_fs = (flags & SDL_WINDOW_FULLSCREEN);

    SDL_SetWindowFullscreen(g_window, !is_fs);
    LOG_INFO_CAT("SDL3_window", is_fs ? "Exiting fullscreen" : "Entering fullscreen");
}

inline void sdl_window_destroy() noexcept {
    if (g_window != nullptr) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
        LOG_SUCCESS_CAT("SDL3_window", "Window destroyed");
    }
}

// ────────────────────────────────────────────────
// Audio — raw SDL3 (devices, streams, headphone jacks)
// ────────────────────────────────────────────────
inline bool sdl_audio_init() noexcept {
    if (g_audio_device != 0) return true;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        LOG_FATAL_CAT("SDL3_audio", "SDL_InitSubSystem(SDL_INIT_AUDIO) failed: {}", SDL_GetError());
        return false;
    }

    int num_devices = 0;
    SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&num_devices);
    if (devices != nullptr) {
        LOG_INFO_CAT("SDL3_audio", "Found {} audio playback devices:", num_devices);
        for (int i = 0; i < num_devices; ++i) {
            const char* name = SDL_GetAudioDeviceName(devices[i]);
            LOG_INFO_CAT("SDL3_audio", "  [{}] {}", i, name ? name : "Unknown");
        }
        SDL_free(devices);
    } else {
        LOG_WARNING_CAT("SDL3_audio", "No audio playback devices enumerated");
    }

    g_audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (g_audio_device == 0) {
        LOG_FATAL_CAT("SDL3_audio", "Failed to open default playback device: {}", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    SDL_AudioSpec desired{};
    desired.freq     = Options::Audio::SAMPLE_RATE;
    desired.format   = SDL_AUDIO_F32;
    desired.channels = Options::Audio::CHANNELS;

    g_audio_stream = SDL_CreateAudioStream(&desired, nullptr);
    if (g_audio_stream == nullptr) {
        LOG_FATAL_CAT("SDL3_audio", "Failed to create audio stream: {}", SDL_GetError());
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    if (SDL_BindAudioStream(g_audio_device, g_audio_stream) == 0) {
        LOG_FATAL_CAT("SDL3_audio", "Failed to bind audio stream to device: {}", SDL_GetError());
        SDL_DestroyAudioStream(g_audio_stream);
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_stream = nullptr;
        g_audio_device = 0;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    SDL_ResumeAudioDevice(g_audio_device);
    LOG_SUCCESS_CAT("SDL3_audio", "Audio initialized — device ID {} | stream ready", g_audio_device);
    return true;
}

inline bool sdl_audio_load_sound(const std::string& path, const std::string& name) noexcept {
    SDL_AudioSpec spec{};
    Uint8* buffer = nullptr;
    Uint32 length = 0;

    if (SDL_LoadWAV(path.c_str(), &spec, &buffer, &length) == false) {
        LOG_ERROR_CAT("SDL3_audio", "Failed to load WAV {}: {}", path, SDL_GetError());
        return false;
    }

    std::unique_ptr<SoundData> sound = std::make_unique<SoundData>();
    sound->buffer = buffer;
    sound->length = length;
    sound->spec = spec;

    g_sounds[name] = std::move(sound);
    LOG_SUCCESS_CAT("SDL3_audio", "Sound loaded — name='{}' path='{}' ({} bytes, {} Hz, {} ch)", 
                    name, path, length, spec.freq, spec.channels);
    return true;
}

inline void sdl_audio_play_sound(const std::string& name) noexcept {
    std::unordered_map<std::string, std::unique_ptr<SoundData>>::const_iterator it = g_sounds.find(name);
    if (it == g_sounds.cend()) {
        LOG_WARNING_CAT("SDL3_audio", "Attempted to play unknown sound '{}'", name);
        return;
    }

    if (g_audio_device == 0 || g_audio_stream == nullptr) {
        LOG_WARNING_CAT("SDL3_audio", "Audio not initialized — cannot play '{}'", name);
        return;
    }

    const SoundData* sound = it->second.get();
    if (sound == nullptr || sound->buffer == nullptr || sound->length == 0) {
        LOG_WARNING_CAT("SDL3_audio", "Invalid sound data for '{}'", name);
        return;
    }

    if (sound->length > INT32_MAX) {  // theoretically impossible for audio, but defensive
        LOG_ERROR_CAT("SDL3_audio", "Absurdly large sound length for '{}' — {} bytes (skipping)", 
                      name, sound->length);
        return;
    }

    SDL_PutAudioStreamData(g_audio_stream, sound->buffer, static_cast<Sint32>(sound->length));
    LOG_INFO_CAT("SDL3_audio", "Playing sound '{}' ({} bytes)", name, sound->length);
}

inline void sdl_audio_cleanup() noexcept {
    LOG_INFO_CAT("SDL3_audio", "Cleaning up audio — {} sounds loaded", g_sounds.size());

    g_sounds.clear();

    if (g_audio_stream != nullptr) {
        SDL_DestroyAudioStream(g_audio_stream);
        g_audio_stream = nullptr;
        LOG_SUCCESS_CAT("SDL3_audio", "Audio stream destroyed");
    }

    if (g_audio_device != 0) {
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
        LOG_SUCCESS_CAT("SDL3_audio", "Audio device closed");
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

// ────────────────────────────────────────────────
// Gamepads — raw SDL3 (events pumped to InputManager)
// ────────────────────────────────────────────────
inline void sdl_gamepads_init() noexcept {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);

    if (ids == nullptr) {
        LOG_WARNING_CAT("SDL3_input", "SDL_GetGamepads returned null");
        return;
    }

    LOG_INFO_CAT("SDL3_input", "Found {} gamepad(s)", count);

    for (int i = 0; i < count; ++i) {
        SDL_Gamepad* gp = SDL_OpenGamepad(ids[i]);
        if (gp == nullptr) {
            LOG_WARNING_CAT("SDL3_input", "Failed to open gamepad ID={}: {}", ids[i], SDL_GetError());
            continue;
        }

        g_gamepads[ids[i]] = gp;

        if (Options::Audio::ENABLE_HAPTICS_FEEDBACK) {
            Uint16 intensity = 32768;
            SDL_RumbleGamepad(gp, intensity, intensity, 500);
        }

        const char* name = SDL_GetGamepadName(gp);
        LOG_SUCCESS_CAT("SDL3_input", "Gamepad connected — ID={} name='{}'", ids[i], name ? name : "Unknown");
    }

    SDL_free(ids);
}

inline void sdl_gamepads_cleanup() noexcept {
    LOG_INFO_CAT("SDL3_input", "Cleaning up {} gamepad(s)", g_gamepads.size());

    for (std::unordered_map<SDL_JoystickID, SDL_Gamepad*>::iterator it = g_gamepads.begin(); it != g_gamepads.end(); ++it) {
        SDL_Gamepad* gp = it->second;
        if (gp != nullptr) {
            SDL_CloseGamepad(gp);
            LOG_INFO_CAT("SDL3_input", "Gamepad ID={} closed", it->first);
        }
    }

    g_gamepads.clear();
}

// ────────────────────────────────────────────────
// Full init / cleanup — one call
// ────────────────────────────────────────────────
inline void sdl_init_all(int w, int h, const char* title) noexcept {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD) == 0) {
        LOG_FATAL_CAT("SDL3_init", "SDL_Init failed: {}", SDL_GetError());
        return;
    }

    LOG_SUCCESS_CAT("SDL3_init", "SDL subsystems initialized (Video, Audio, Gamepad)");

    sdl_window_create(w, h, title);

    // Very important: check that window actually exists before giving it to input manager
    if (g_window == nullptr) {
        LOG_FATAL_CAT("SDL3_init", "Cannot initialize input — window creation failed");
        return;
    }

    sdl_gamepads_init();
    sdl_audio_init();

    // Now safe to init input
    GlobalInputManager::get().init(g_window);
}

inline void sdl_cleanup_all() noexcept {
    LOG_INFO_CAT("SDL3_init", "Shutting down SDL subsystems");

    sdl_audio_cleanup();
    sdl_gamepads_cleanup();
    sdl_window_destroy();

    SDL_Quit();
    LOG_SUCCESS_CAT("SDL3_init", "SDL fully shut down");
}