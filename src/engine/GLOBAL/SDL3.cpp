// src/engine/GLOBAL/SDL3.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// SDL_EVENT_WINDOW_CLOSE_REQUESTED handled early + forced quit event
// FULLY COMPATIBLE WITH CURRENT ENGINE STATE — PINK PHOTONS ETERNAL
// EMPIRE UNBROKEN — DECEMBER 22, 2025
// =============================================================================

#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <memory>
#include <source_location>
#include <functional>

using namespace Logging::Color;
using namespace std::chrono;
using StoneKey::stone_window;

// =============================================================================
// Global state
// =============================================================================
std::unique_ptr<VulkanRenderer> g_vulkanRenderer;
SDLWindowPtr                    g_sdl_window;
std::atomic<int>  g_resizeWidth{0};
std::atomic<int>  g_resizeHeight{0};
std::atomic<bool> g_resizeRequested{false};

// =============================================================================
// SDLWindowDeleter — RAII eternal
// =============================================================================
void SDLWindowDeleter::operator()(SDL_Window* w) const noexcept
{
    if (w) SDL_DestroyWindow(w);
}

// =============================================================================
// Namespace: SDL3Initializer — Input System
// =============================================================================
namespace SDL3Initializer {

using KeyboardCallback       = std::function<void(const SDL_KeyboardEvent&)>;
using MouseButtonCallback    = std::function<void(const SDL_MouseButtonEvent&)>;
using MouseMotionCallback    = std::function<void(const SDL_MouseMotionEvent&)>;
using MouseWheelCallback     = std::function<void(const SDL_MouseWheelEvent&)>;
using TextInputCallback      = std::function<void(const SDL_TextInputEvent&)>;
using TouchCallback          = std::function<void(const SDL_TouchFingerEvent&)>;
using GamepadButtonCallback  = std::function<void(const SDL_GamepadButtonEvent&)>;
using GamepadAxisCallback    = std::function<void(const SDL_GamepadAxisEvent&)>;
using GamepadConnectCallback = std::function<void(bool connected, SDL_JoystickID id, SDL_Gamepad* gp)>;

std::string SDL3Input::locationString(const std::source_location& loc)
{
    return std::format("{}:{}:{}", loc.file_name(), loc.line(), loc.function_name());
}

SDL3Input::SDL3Input() = default;

SDL3Input::~SDL3Input()
{
    const size_t count = m_gamepads.size();
    if (count > 0) {
        LOG_INFO_CAT("Dispose", "{}SDL3Input destroyed — {} gamepad(s) auto-closed by RAII{}", SAPPHIRE_BLUE, count, RESET);
    } else {
        LOG_INFO_CAT("Dispose", "{}SDL3Input destroyed — no gamepads to close{}", SAPPHIRE_BLUE, RESET);
    }
    m_gamepads.clear();
}

void SDL3Input::initialize()
{
    const std::string loc = locationString();
    const std::string_view platform = SDL_GetPlatform();

    if (platform != "Linux" && platform != "Windows") {
        LOG_ERROR_CAT("Input", "{}Unsupported platform: {} | {}{}", 
                  OCEAN_TEAL, platform, loc, RESET);
        LOG_FATAL_CAT("FATAL", "Unsupported platform: {}", platform);
        return;
    }

    LOG_SUCCESS_CAT("Input", "{}Initializing SDL3Input | {}{}", OCEAN_TEAL, loc, RESET);

    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");

    int num = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&num);
    LOG_INFO_CAT("Input", "{}Found {} joystick(s) at startup | {}{}", OCEAN_TEAL, num, loc, RESET);

    if (joysticks) {
        for (int i = 0; i < num; ++i) {
            if (SDL_IsGamepad(joysticks[i])) {
                if (SDL_Gamepad* gp = SDL_OpenGamepad(joysticks[i])) {
                    m_gamepads[joysticks[i]] = GamepadPtr(gp);

                    if (m_gamepadConnectCallback) {
                        m_gamepadConnectCallback(true, joysticks[i], gp);
                    }

                    if (Options::Audio::ENABLE_HAPTICS_FEEDBACK) {
                        constexpr Uint16 intensity = 32768;
                        SDL_RumbleGamepad(gp, intensity, intensity, 500);
                        LOG_INFO_CAT("Input", "{}HAPTICS: Connect rumble → ID {}{}", OCEAN_TEAL, joysticks[i], RESET);
                    }
                }
            }
        }
        SDL_free(joysticks);
    }
}

bool SDL3Input::pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        // === CRITICAL: Immediate quit on window close or X button ===
        if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            LOG_INFO_CAT("Input", "{}Window close requested (X icon or ALT+F4) — immediate quit{}", RASPBERRY_PINK, RESET);
            SDL_Event quit{.type = SDL_EVENT_QUIT};
            SDL_PushEvent(&quit);
            return !exitOnClose;
        }

        switch (ev.type) {
            case SDL_EVENT_KEY_DOWN:
                handleKeyboard(ev.key, window, audioDevice, consoleOpen);
                if (m_keyboardCallback) m_keyboardCallback(ev.key);
                break;

            case SDL_EVENT_KEY_UP:
                if (m_keyboardCallback) m_keyboardCallback(ev.key);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                handleMouseButton(ev.button, window);
                if (m_mouseButtonCallback) m_mouseButtonCallback(ev.button);
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (m_mouseMotionCallback) m_mouseMotionCallback(ev.motion);
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                if (m_mouseWheelCallback) m_mouseWheelCallback(ev.wheel);
                break;

            case SDL_EVENT_TEXT_INPUT:
                if (m_textInputCallback) m_textInputCallback(ev.text);
                break;

            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
                if (m_touchCallback) m_touchCallback(ev.tfinger);
                break;

            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                handleGamepadButton(ev.gbutton, audioDevice);
                if (m_gamepadButtonCallback) m_gamepadButtonCallback(ev.gbutton);
                break;

            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if (m_gamepadAxisCallback) m_gamepadAxisCallback(ev.gaxis);
                break;

            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                handleGamepadConnection(ev.gdevice);
                break;
        }
    }
    return true;
}

void SDL3Input::enableTextInput(SDL_Window* window, bool enable)
{
    if (enable) {
        SDL_StartTextInput(window);
        LOG_INFO_CAT("Input", "{}Text input ENABLED{}", OCEAN_TEAL, RESET);
    } else {
        SDL_StopTextInput(window);
        LOG_INFO_CAT("Input", "{}Text input DISABLED{}", OCEAN_TEAL, RESET);
    }
}

void SDL3Input::exportLog(std::string_view filename) const
{
    const std::string loc = locationString();
    LOG_INFO_CAT("Input", "{}Exporting input log → {} | {}{}", OCEAN_TEAL, filename, loc, RESET);

    std::ofstream f(filename.data(), std::ios::app);
    if (f.is_open()) {
        auto now = system_clock::now();
        auto secs = duration_cast<seconds>(now.time_since_epoch()).count();
        f << "[INPUT LOG] " << secs << " | Gamepads: " << m_gamepads.size() << "\n";
        LOG_SUCCESS_CAT("Input", "{}Log exported → {}{}", OCEAN_TEAL, filename, RESET);
    } else {
        LOG_ERROR_CAT("Input", "{}Failed to export log → {}{}", OCEAN_TEAL, filename, RESET);
    }
}

void SDL3Input::handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen)
{
    if (!k.down) return;

    switch (k.key) {
        case SDLK_F: {
            bool fs = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0;
            SDL_SetWindowFullscreen(window, !fs);
            LOG_INFO_CAT("Input", "{}Fullscreen → {}{}", OCEAN_TEAL, !fs ? "ENABLED" : "DISABLED", RESET);
        } break;

        case SDLK_ESCAPE: {
            SDL_Event quit{.type = SDL_EVENT_QUIT};
            SDL_PushEvent(&quit);
        } break;

        case SDLK_SPACE:
            if (audioDevice) {
                bool paused = SDL_AudioDevicePaused(audioDevice);
                paused ? SDL_ResumeAudioDevice(audioDevice) : SDL_PauseAudioDevice(audioDevice);
                LOG_INFO_CAT("Input", "{}Audio {} via SPACE{}", OCEAN_TEAL, paused ? "RESUMED" : "PAUSED", RESET);
            }
            break;

        case SDLK_M:
            if (audioDevice) {
                float gain = SDL_GetAudioDeviceGain(audioDevice);
                SDL_SetAudioDeviceGain(audioDevice, gain > 0.5f ? 0.0f : 1.0f);
                LOG_INFO_CAT("Input", "{}Audio MUTE toggle{}", OCEAN_TEAL, RESET);
            }
            break;

        case SDLK_GRAVE:
            if (Options::ENABLE_CONSOLE_LOG) {
                consoleOpen = !consoleOpen;
                LOG_INFO_CAT("Input", "{}Console → {}{}", OCEAN_TEAL, consoleOpen ? "OPEN" : "CLOSED", RESET);
            }
            break;
    }
}

void SDL3Input::handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window)
{
    if (b.down && b.button == SDL_BUTTON_RIGHT) {
        bool relative = SDL_GetWindowRelativeMouseMode(window);
        SDL_SetWindowRelativeMouseMode(window, !relative);
        LOG_INFO_CAT("Input", "{}Relative mouse → {}{}", OCEAN_TEAL, !relative ? "ENABLED" : "DISABLED", RESET);
    }
}

void SDL3Input::handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice)
{
    if (!g.down) return;

    switch (g.button) {
        case SDL_GAMEPAD_BUTTON_EAST: {
            SDL_Event quit{.type = SDL_EVENT_QUIT};
            SDL_PushEvent(&quit);
        } break;

        case SDL_GAMEPAD_BUTTON_START:
            if (audioDevice) {
                bool paused = SDL_AudioDevicePaused(audioDevice);
                paused ? SDL_ResumeAudioDevice(audioDevice) : SDL_PauseAudioDevice(audioDevice);
            }
            break;
    }
}

void SDL3Input::handleGamepadConnection(const SDL_GamepadDeviceEvent& e)
{
    if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (SDL_Gamepad* gp = SDL_OpenGamepad(e.which)) {
            m_gamepads[e.which] = GamepadPtr(gp);

            LOG_SUCCESS_CAT("Input", "{}Gamepad CONNECTED → ID: {} | Total: {}{}", 
                            LIME_GREEN, e.which, m_gamepads.size(), RESET);

            if (m_gamepadConnectCallback) {
                m_gamepadConnectCallback(true, e.which, gp);
            }

            if (Options::Audio::ENABLE_HAPTICS_FEEDBACK) {
                constexpr Uint16 intensity = 32768;
                SDL_RumbleGamepad(gp, intensity, intensity, 500);
            }
        }
    }
    else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
        auto it = m_gamepads.find(e.which);
        if (it != m_gamepads.end()) {
            LOG_INFO_CAT("Input", "{}Gamepad DISCONNECTED → ID: {} | Remaining: {}{}", 
                         AMBER_YELLOW, e.which, m_gamepads.size() - 1, RESET);

            if (m_gamepadConnectCallback) {
                m_gamepadConnectCallback(false, e.which, it->second.get());
            }

            m_gamepads.erase(it);
        }
    }
}

} // namespace SDL3Initializer

// =============================================================================
// Namespace: SDL3Window — The One True Forge
// =============================================================================
namespace SDL3Window {

[[nodiscard]] std::vector<std::string> getVulkanExtensions(SDL_Window* window)
{
    if (!window) window = g_sdl_window.get();

    uint32_t count = 0;
    if (SDL_Vulkan_GetInstanceExtensions(&count) == 0) return {};

    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&count);
    return exts ? std::vector<std::string>(exts, exts + count) : std::vector<std::string>{};
}

bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept
{
    SDL_Event ev;
    quit = toggleFS = false;
    bool eventSeen = false;

    while (SDL_PollEvent(&ev))
    {
        eventSeen = true;

        // === CRITICAL: Immediate quit on X button ===
        if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            quit = true;
            return eventSeen;
        }

        switch (ev.type)
        {
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_F11)
                    toggleFS = true;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                if (!Options::Window::ALLOW_RESIZE) {
                    break;
                }
                {
                    const int w = ev.window.data1;
                    const int h = ev.window.data2;
                    if (w > 0 && h > 0)
                    {
                        g_resizeWidth.store(w);
                        g_resizeHeight.store(h);
                        g_resizeRequested.store(true);

                        LOG_MAIN("Resize requested → {}x{}", w, h);

                        RTX::las().notifyResize();
                    }
                }
                break;

            default:
                break;
        }
    }

    // Always report current size
    if (g_sdl_window)
    {
        int w, h;
        SDL_GetWindowSizeInPixels(g_sdl_window.get(), &w, &h);
        outW = (w > 0) ? w : 1;
        outH = (h > 0) ? h : 1;

        if (g_resizeRequested.load())
        {
            int pendingW = g_resizeWidth.load();
            int pendingH = g_resizeHeight.load();
            if (pendingW > 0 && pendingH > 0)
            {
                outW = pendingW;
                outH = pendingH;

                if (g_vulkanRenderer)
                {
                    g_vulkanRenderer->requestResize(pendingW, pendingH);
                }
            }
            g_resizeRequested.store(false);
        }
    }

    return eventSeen;
}

void toggleFullscreen() noexcept
{
    if (!g_sdl_window) return;
    bool isFS = SDL_GetWindowFlags(g_sdl_window.get()) & SDL_WINDOW_FULLSCREEN;
    SDL_SetWindowFullscreen(g_sdl_window.get(), !isFS);
    LOG_SUCCESS_CAT("Window", "{}FULLSCREEN {}{}", isFS ? "OFF" : "ON", isFS ? RASPBERRY_PINK : EMERALD_GREEN, RESET);
}

void destroy() noexcept
{
    g_sdl_window.reset();
}

} // namespace SDL3Window

// =============================================================================
// SDL3Image — MODERN SDL3_image (2025+) — NO LEGACY FLAGS — PURE EMPIRE
// =============================================================================
namespace SDL3Image {

[[nodiscard]] inline SDL_Surface* load(const char* path)
{
    SDL_Surface* surf = IMG_Load(path);
    if (!surf) {
        LOG_FATAL_CAT("SDL3IMG", "IMG_Load FAILED → {} | {}{}", CRIMSON_MAGENTA, path, SDL_GetError(), RESET);
        LOG_FATAL_CAT("FATAL", "Failed to load image: {}", path);
        return nullptr;
    }

    LOG_SUCCESS_CAT("SDL3IMG", "TEXTURE MANIFESTED → {} | {}x{} {}bpp", 
                    RASPBERRY_PINK, path, surf->w, surf->h, SDL_BYTESPERPIXEL(surf->format), RESET);
    return surf;
}

[[nodiscard]] inline SDL_Surface* load(const std::string& path)
{
    return load(path.c_str());
}

} // namespace SDL3Image

namespace SDL3Audio {

AudioManager::~AudioManager() {
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
    if (device_) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    sounds_.clear();
}

bool AudioManager::initMixer() {
    if (device_) {
        LOG_SUCCESS_CAT("AUDIO", "AudioManager already initialized — photons singing", EMERALD_GREEN, RESET);
        return true;
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        LOG_ERROR_CAT("AUDIO", "SDL_InitSubSystem(AUDIO) failed: {}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        return false;
    }

    device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (device_ == 0) {
        LOG_FATAL_CAT("AUDIO", "SDL_OpenAudioDevice failed: {}", BLOOD_RED, SDL_GetError(), RESET);
        return false;
    }

    SDL_ResumeAudioDevice(device_);
    LOG_SUCCESS_CAT("AUDIO", "AUDIO DEVICE OPENED — ID: {} — PINK PHOTONS HAVE VOICE", VALHALLA_GOLD, device_, RESET);
    return true;
}

bool AudioManager::loadSound(std::string_view path, std::string_view name) {
    SDL_AudioSpec spec{};
    Uint8* buffer = nullptr;
    Uint32 length = 0;

    if (!SDL_LoadWAV(path.data(), &spec, &buffer, &length)) {
        LOG_ERROR_CAT("AUDIO", "Failed to load WAV: {} | {}", AMBER_YELLOW, path, SDL_GetError(), RESET);
        return false;
    }

    auto sound = std::make_unique<SoundData>();
    sound->buffer = buffer;
    sound->length = length;
    sound->spec = spec;

    auto [it, inserted] = sounds_.try_emplace(std::string(name), std::move(sound));
    if (inserted) {
        LOG_SUCCESS_CAT("AUDIO", "SOUND LOADED → \"{}\" | {} bytes | {}Hz {}ch", 
                        RASPBERRY_PINK, name, length, spec.freq, spec.channels, RESET);
    } else {
        LOG_WARN_CAT("AUDIO", "Sound \"{}\" reloaded", AMBER_YELLOW, name, RESET);
        it->second = std::move(sound);
    }
    return true;
}

void AudioManager::playSound(std::string_view name) {
    auto it = sounds_.find(std::string(name));
    if (it == sounds_.end()) {
        LOG_WARN_CAT("AUDIO", "Sound \"{}\" not found — silence falls", CRIMSON_MAGENTA, name, RESET);
        return;
    }

    if (!device_) {
        LOG_ERROR_CAT("AUDIO", "No audio device — cannot play \"{}\"", BLOOD_RED, name, RESET);
        return;
    }

    const auto& sound = it->second;

    if (!stream_) {
        stream_ = SDL_CreateAudioStream(&sound->spec, nullptr);
        if (!stream_) {
            LOG_ERROR_CAT("AUDIO", "SDL_CreateAudioStream failed: {}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
            return;
        }

        if (SDL_BindAudioStream(device_, stream_) == 0) {
            LOG_ERROR_CAT("AUDIO", "SDL_BindAudioStream failed: {}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
            SDL_DestroyAudioStream(stream_);
            stream_ = nullptr;
            return;
        }
    }

    if (SDL_PutAudioStreamData(stream_, sound->buffer, sound->length) == 0) {
        LOG_ERROR_CAT("AUDIO", "SDL_PutAudioStreamData failed: {}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
    } else {
        LOG_INFO_CAT("AUDIO", "PLAY → \"{}\" — PINK PHOTONS SING ACROSS THE VOID", PARTY_PINK, name, RESET);
    }
}

} // namespace SDL3Audio

// =============================================================================
// AMOURANTH RTX — UNIVERSAL IMAGE LOADER v∞ — PINK PHOTONS ETERNAL
// DECEMBER 22, 2025 — FULLY INTEGRATED — EMPIRE COMPLETE
// =============================================================================
namespace AmouranthRTX::ImageLoader {

[[nodiscard]] inline SDL_Surface* loadSurface(const char* path) noexcept
{
    LOG_ATTEMPT_CAT("IMG", "Empire loading sacred image: {}", RASPBERRY_PINK, path, RESET);

#ifdef SDL3_IMAGE_ENABLED
    if (SDL_Surface* surf = IMG_Load(path))
    {
        LOG_SUCCESS_CAT("IMG", "SDL3_image summoned {} → {}×{} ({} KB)", 
                        PLASMA_FUCHSIA, path, surf->w, surf->h,
                        (surf->w * surf->h * 4) / 1024, RESET);
        return surf;
    }
    LOG_WARN_CAT("IMG", "SDL3_image failed for {} — falling back to core SDL3", AURORA_PINK, path, RESET);
#endif

    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (!io) {
        LOG_FATAL_CAT("IMG", "Cannot open file: {}", CRIMSON_MAGENTA, path, RESET);
        return nullptr;
    }

    SDL_Surface* surf = SDL_LoadBMP_IO(io, true);
    if (surf) {
        LOG_SUCCESS_CAT("IMG", "Core SDL3 loaded BMP: {}", SAPPHIRE_BLUE, path, RESET);
        return surf;
    }

    LOG_FATAL_CAT("IMG", "ALL IMAGE RITUALS FAILED for {} — Error: {}", BLOOD_RED, path, SDL_GetError(), RESET);
    return nullptr;
}

[[nodiscard]] inline SDL_Surface* loadSurface(const std::string& path) noexcept
{
    return loadSurface(path.c_str());
}

[[nodiscard]] inline SDL_Texture* loadTexture(SDL_Renderer* renderer, const char* path) noexcept
{
    SDL_Surface* surface = loadSurface(path);
    if (!surface) return nullptr;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    if (tex) {
        LOG_SUCCESS_CAT("IMG", "Texture forged from {} — Empire ascends", VALHALLA_GOLD, path, RESET);
    } else {
        LOG_FATAL_CAT("IMG", "Failed to forge texture from {}: {}", CRIMSON_MAGENTA, path, SDL_GetError(), RESET);
    }
    return tex;
}

} // namespace AmouranthRTX::ImageLoader

// =============================================================================
// SDL_EVENT_WINDOW_CLOSE_REQUESTED handled first
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN
// DECEMBER 22, 2025 — THE LIGHT IS ETERNAL
// =============================================================================