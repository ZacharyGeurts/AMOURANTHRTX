#pragma once

// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — SDL3 INTEGRATION FLAT
// ONE FILE | ONE VIEWPORT | FULL RAW SDL3 — WINDOW, INPUT, AUDIO, GAMEPADS
// Version 0.81 — February 04, 2026 — Flattened | Raw power | No classes
// - Full SDL3 — audio devices, streams, gamepads, headphone jacks via enumeration
// - All events forwarded to GlobalInputManager (INPUT) with rich hooks
// - AMOURANTH FOREVER 💖
// =============================================================================

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <atomic>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <string>

#include "engine/GLOBAL/ELLIE.hpp"
#include "engine/GLOBAL/AMOURANTHRTX.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/InputManager.hpp"

// ────────────────────────────────────────────────
// Globals — raw and eternal
// ────────────────────────────────────────────────
inline SDL_Window*               g_window = nullptr;
inline std::atomic<int>          g_resize_width{0};
inline std::atomic<int>          g_resize_height{0};
inline std::atomic<bool>         g_resize_requested{false};

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
inline void sdl_window_create(uint32_t width, uint32_t height, const char* title) noexcept {
    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (Options::Window::START_FULLSCREEN) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    g_window = SDL_CreateWindow(title, width, height, flags);
    if (!g_window) {
        LOG_FATAL_CAT("SDL3_window", "Failed to create window: {}", SDL_GetError());
        return;
    }

    LOG_SUCCESS_CAT("SDL3_window", "Window created — {}x{}", width, height);
}

inline bool sdl_poll_events(int& out_w, int& out_h, bool& quit, bool& toggle_fs) noexcept {
    static int last_valid_w = 0;
    static int last_valid_h = 0;
    static bool is_minimized = false;

    SDL_Event ev;
    quit = toggle_fs = false;
    bool event_seen = false;

    while (SDL_PollEvent(&ev)) {
        event_seen = true;

        // Forward every single event to InputManager — core of input supremacy
        INPUT.pumpEvents(ev);

        // Window-level handling (quit, resize, fullscreen toggle)
        switch (ev.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                LOG_INFO_CAT("SDL3_window", "Quit requested");
                quit = true;
            } break;

            case SDL_EVENT_KEY_DOWN: {
                if (ev.key.scancode == SDL_SCANCODE_F11) {
                    LOG_INFO_CAT("INPUT", "F11 pressed — toggling fullscreen");
                    toggle_fs = true;
                }
            } break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                if (!Options::Window::ALLOW_RESIZE) {
                    LOG_INFO_CAT("SDL3_window", "Resize event ignored (ALLOW_RESIZE=false)");
                    continue;
                }

                int w = ev.window.data1;
                int h = ev.window.data2;

                bool currently_minimized = (w <= 0 || h <= 0);

                if (currently_minimized && !is_minimized) {
                    is_minimized = true;
                    LOG_INFO_CAT("SDL3_window", "Window minimized");
                } else if (!currently_minimized && is_minimized) {
                    is_minimized = false;
                    last_valid_w = w;
                    last_valid_h = h;
                    g_resize_width.store(w);
                    g_resize_height.store(h);
                    g_resize_requested.store(true);
                    LOG_INFO_CAT("SDL3_window", "Window restored — resize requested {}x{}", w, h);
                } else if (!currently_minimized && (w != last_valid_w || h != last_valid_h)) {
                    last_valid_w = w;
                    last_valid_h = h;
                    g_resize_width.store(w);
                    g_resize_height.store(h);
                    g_resize_requested.store(true);
                    LOG_INFO_CAT("SDL3_window", "Resize event — new size {}x{}", w, h);
                }
            } break;

            default:
                // All other events (mouse, gamepad, text, focus, etc.) handled by INPUT.pumpEvents
                break;
        }
    }

    if (g_window) {
        int w, h;
        SDL_GetWindowSizeInPixels(g_window, &w, &h);
        out_w = (w > 0) ? w : 1;
        out_h = (h > 0) ? h : 1;

        if (g_resize_requested.load()) {
            int pending_w = g_resize_width.load();
            int pending_h = g_resize_height.load();
            if (pending_w > 0 && pending_h > 0) {
                out_w = pending_w;
                out_h = pending_h;
                last_valid_w = pending_w;
                last_valid_h = pending_h;
            }
            g_resize_requested.store(false);
        }
    }

    return event_seen;
}

inline void sdl_toggle_fullscreen() noexcept {
    if (!g_window) return;

    Uint32 flags = SDL_GetWindowFlags(g_window);
    bool is_fs = (flags & SDL_WINDOW_FULLSCREEN);

    SDL_SetWindowFullscreen(g_window, !is_fs);
    LOG_INFO_CAT("SDL3_window", is_fs ? "Exiting fullscreen" : "Entering fullscreen");
}

inline void sdl_window_destroy() noexcept {
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
        LOG_SUCCESS_CAT("SDL3_window", "Window destroyed");
    }
}

// ────────────────────────────────────────────────
// Audio — raw SDL3 (devices, streams, headphone jacks)
// ────────────────────────────────────────────────
inline bool sdl_audio_init() noexcept {
    if (g_audio_device) return true;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        LOG_FATAL_CAT("SDL3_audio", "SDL_InitSubSystem(SDL_INIT_AUDIO) failed: {}", SDL_GetError());
        return false;
    }

    int num_devices = 0;
    SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&num_devices);
    if (devices) {
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
    desired.freq     = Options::Audio::AUDIO_SAMPLE_RATE;
    desired.format   = SDL_AUDIO_F32;
    desired.channels = Options::Audio::AUDIO_CHANNELS;

    g_audio_stream = SDL_CreateAudioStream(&desired, nullptr);
    if (!g_audio_stream) {
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

    if (SDL_LoadWAV(path.c_str(), &spec, &buffer, &length) == 0) {
        LOG_ERROR_CAT("SDL3_audio", "Failed to load WAV {}: {}", path, SDL_GetError());
        return false;
    }

    auto sound = std::make_unique<SoundData>();
    sound->buffer = buffer;
    sound->length = length;
    sound->spec = spec;

    g_sounds[name] = std::move(sound);
    LOG_SUCCESS_CAT("SDL3_audio", "Sound loaded — name='{}' path='{}' ({} bytes, {} Hz, {} ch)", 
                    name, path, length, spec.freq, spec.channels);
    return true;
}

inline void sdl_audio_play_sound(const std::string& name) noexcept {
    auto it = g_sounds.find(name);
    if (it == g_sounds.end()) {
        LOG_WARNING_CAT("SDL3_audio", "Attempted to play unknown sound '{}'", name);
        return;
    }

    if (!g_audio_device || !g_audio_stream) {
        LOG_WARNING_CAT("SDL3_audio", "Audio not initialized — cannot play '{}'", name);
        return;
    }

    SDL_PutAudioStreamData(g_audio_stream, it->second->buffer, it->second->length);
    LOG_INFO_CAT("SDL3_audio", "Playing sound '{}'", name);
}

inline void sdl_audio_cleanup() noexcept {
    LOG_INFO_CAT("SDL3_audio", "Cleaning up audio — {} sounds loaded", g_sounds.size());

    g_sounds.clear();

    if (g_audio_stream) {
        SDL_DestroyAudioStream(g_audio_stream);
        g_audio_stream = nullptr;
        LOG_SUCCESS_CAT("SDL3_audio", "Audio stream destroyed");
    }

    if (g_audio_device) {
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

    if (!ids) {
        LOG_WARNING_CAT("SDL3_input", "SDL_GetGamepads returned null");
        return;
    }

    LOG_INFO_CAT("SDL3_input", "Found {} gamepad(s)", count);

    for (int i = 0; i < count; ++i) {
        SDL_Gamepad* gp = SDL_OpenGamepad(ids[i]);
        if (!gp) {
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

        // Ready for InputManager to track state
        // INPUT.onGamepadConnected(ids[i], gp);
    }

    SDL_free(ids);
}

inline void sdl_gamepads_cleanup() noexcept {
    LOG_INFO_CAT("SDL3_input", "Cleaning up {} gamepad(s)", g_gamepads.size());

    for (auto& [id, gp] : g_gamepads) {
        if (gp) {
            SDL_CloseGamepad(gp);
            LOG_INFO_CAT("SDL3_input", "Gamepad ID={} closed", id);

            // INPUT.onGamepadDisconnected(id);
        }
    }

    g_gamepads.clear();
}

// ────────────────────────────────────────────────
// Full init / cleanup — one call
// ────────────────────────────────────────────────
inline void sdl_init_all(uint32_t w, uint32_t h, const char* title) noexcept {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD) == 0) {
        LOG_FATAL_CAT("SDL3_init", "SDL_Init failed: {}", SDL_GetError());
        return;
    }

    LOG_SUCCESS_CAT("SDL3_init", "SDL subsystems initialized (Video, Audio, Gamepad)");

    sdl_window_create(w, h, title);
    sdl_gamepads_init();
    sdl_audio_init();

    // InputManager startup — call once after SDL init
    INPUT.init();
}

inline void sdl_cleanup_all() noexcept {
    LOG_INFO_CAT("SDL3_init", "Shutting down SDL subsystems");

    sdl_audio_cleanup();
    sdl_gamepads_cleanup();
    sdl_window_destroy();

    SDL_Quit();
    LOG_SUCCESS_CAT("SDL3_init", "SDL fully shut down");
}