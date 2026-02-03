#pragma once

// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — SDL3 INTEGRATION FLAT
// ONE FILE | ONE VIEWPORT | FULL RAW SDL3 — WINDOW, INPUT, AUDIO, GAMEPADS
// Version 0.81 — February 03, 2026 — Flattened | Raw power | No classes
// - Full SDL3 — audio devices, streams, gamepads, headphone jacks via device enumeration
// - Events forwarded to GlobalInputManager
// AMOURANTH FOREVER 💖
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
        LOG_FATAL_CAT("SDL", "Failed to create window: {}", SDL_GetError());
        return;
    }

    LOG_SUCCESS_CAT("SDL", "Window created — {}x{}", width, height);
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

        INPUT.pumpEvents();  // Forward all events to GlobalInputManager

        if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            quit = true;
            return event_seen;
        }

        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.scancode == SDL_SCANCODE_F11) {
            toggle_fs = true;
        }

        if (ev.type == SDL_EVENT_WINDOW_RESIZED || ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            if (!Options::Window::ALLOW_RESIZE) continue;

            int w = ev.window.data1;
            int h = ev.window.data2;

            bool currently_minimized = (w <= 0 || h <= 0);

            if (currently_minimized && !is_minimized) {
                is_minimized = true;
            } else if (!currently_minimized && is_minimized) {
                is_minimized = false;
                last_valid_w = w;
                last_valid_h = h;
                g_resize_width.store(w);
                g_resize_height.store(h);
                g_resize_requested.store(true);
            } else if (!currently_minimized && (w != last_valid_w || h != last_valid_h)) {
                last_valid_w = w;
                last_valid_h = h;
                g_resize_width.store(w);
                g_resize_height.store(h);
                g_resize_requested.store(true);
            }
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
}

inline void sdl_window_destroy() noexcept {
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
    }
}

// ────────────────────────────────────────────────
// Audio — raw SDL3 audio (devices, streams, headphone jacks via device list)
// ────────────────────────────────────────────────
inline bool sdl_audio_init() noexcept {
    if (g_audio_device) return true;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) return false;

    // List available audio devices (including headphone jacks)
    int num_devices = 0;
    SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&num_devices);
    if (devices) {
        LOG_INFO_CAT("AUDIO", "Found {} playback devices:", num_devices);
        for (int i = 0; i < num_devices; ++i) {
            const char* name = SDL_GetAudioDeviceName(devices[i]);
            LOG_INFO_CAT("AUDIO", "  [{}] {}", i, name ? name : "Unknown");
        }
        SDL_free(devices);
    }

    g_audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (g_audio_device == 0) {
        LOG_FATAL_CAT("AUDIO", "Failed to open audio device: {}", SDL_GetError());
        return false;
    }

    SDL_AudioSpec desired{};
    desired.freq     = Options::Audio::AUDIO_SAMPLE_RATE;
    desired.format   = SDL_AUDIO_F32;
    desired.channels = Options::Audio::AUDIO_CHANNELS;

    g_audio_stream = SDL_CreateAudioStream(&desired, nullptr);
    if (!g_audio_stream) {
        LOG_FATAL_CAT("AUDIO", "Failed to create audio stream: {}", SDL_GetError());
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
        return false;
    }

    if (SDL_BindAudioStream(g_audio_device, g_audio_stream) == 0) {
        LOG_FATAL_CAT("AUDIO", "Failed to bind audio stream: {}", SDL_GetError());
        SDL_DestroyAudioStream(g_audio_stream);
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_stream = nullptr;
        g_audio_device = 0;
        return false;
    }

    SDL_ResumeAudioDevice(g_audio_device);
    LOG_SUCCESS_CAT("AUDIO", "Audio initialized — device {} | stream ready", g_audio_device);
    return true;
}

inline bool sdl_audio_load_sound(const std::string& path, const std::string& name) noexcept {
    SDL_AudioSpec spec{};
    Uint8* buffer = nullptr;
    Uint32 length = 0;

    if (SDL_LoadWAV(path.c_str(), &spec, &buffer, &length) == 0) {
        LOG_ERROR_CAT("AUDIO", "Failed to load WAV {}: {}", path, SDL_GetError());
        return false;
    }

    auto sound = std::make_unique<SoundData>();
    sound->buffer = buffer;
    sound->length = length;
    sound->spec = spec;

    g_sounds[name] = std::move(sound);
    LOG_SUCCESS_CAT("AUDIO", "Sound loaded — {} ({})", name, path);
    return true;
}

inline void sdl_audio_play_sound(const std::string& name) noexcept {
    auto it = g_sounds.find(name);
    if (it == g_sounds.end()) return;

    if (!g_audio_device || !g_audio_stream) return;

    SDL_PutAudioStreamData(g_audio_stream, it->second->buffer, it->second->length);
}

inline void sdl_audio_cleanup() noexcept {
    g_sounds.clear();

    if (g_audio_stream) {
        SDL_DestroyAudioStream(g_audio_stream);
        g_audio_stream = nullptr;
    }

    if (g_audio_device) {
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
    }
}

// ────────────────────────────────────────────────
// Gamepads — raw SDL3
// ────────────────────────────────────────────────
inline void sdl_gamepads_init() noexcept {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);

    if (!ids) return;

    for (int i = 0; i < count; ++i) {
        SDL_Gamepad* gp = SDL_OpenGamepad(ids[i]);
        if (gp) {
            g_gamepads[ids[i]] = gp;

            if (Options::Audio::ENABLE_HAPTICS_FEEDBACK) {
                Uint16 intensity = 32768;
                SDL_RumbleGamepad(gp, intensity, intensity, 500);
            }

            LOG_SUCCESS_CAT("INPUT", "Gamepad connected — {}", SDL_GetGamepadName(gp));
        }
    }

    SDL_free(ids);
}

inline void sdl_gamepads_cleanup() noexcept {
    for (auto& [id, gp] : g_gamepads) {
        if (gp) SDL_CloseGamepad(gp);
    }
    g_gamepads.clear();
}

// ────────────────────────────────────────────────
// Full init / cleanup — one call
// ────────────────────────────────────────────────
inline void sdl_init_all(uint32_t w, uint32_t h, const char* title) noexcept {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD) == 0) {
        LOG_FATAL_CAT("SDL", "SDL_Init failed: {}", SDL_GetError());
        return;
    }

    sdl_window_create(w, h, title);
    sdl_gamepads_init();
    sdl_audio_init();
}

inline void sdl_cleanup_all() noexcept {
    sdl_audio_cleanup();
    sdl_gamepads_cleanup();
    sdl_window_destroy();

    SDL_Quit();
}