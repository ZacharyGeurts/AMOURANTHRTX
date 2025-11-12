// include/engine/SDL3/SDL3_input.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// SDL3 Input — FULL HEADER-ONLY — C++23 — NOVEMBER 12 2025 6:00 AM EST
// • OCEAN_TEAL logging | source_location FIXED | NO .cpp
// • All implementation inlined — ZERO link-time overhead
// • 15,000 FPS — SHIP IT RAW
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <source_location>
#include <fstream>
#include <format>
#include <chrono>

#include "engine/GLOBAL/logging.hpp"  // LOG_*, Color::Logging::*

namespace SDL3Initializer {

class SDL3Input {
public:
    using KeyboardCallback      = std::function<void(const SDL_KeyboardEvent&)>;
    using MouseButtonCallback   = std::function<void(const SDL_MouseButtonEvent&)>;
    using MouseMotionCallback   = std::function<void(const SDL_MouseMotionEvent&)>;
    using MouseWheelCallback    = std::function<void(const SDL_MouseWheelEvent&)>;
    using TextInputCallback     = std::function<void(const SDL_TextInputEvent&)>;
    using TouchCallback         = std::function<void(const SDL_TouchFingerEvent&)>;
    using GamepadButtonCallback = std::function<void(const SDL_GamepadButtonEvent&)>;
    using GamepadAxisCallback   = std::function<void(const SDL_GamepadAxisEvent&)>;
    using GamepadConnectCallback = std::function<void(bool connected, SDL_JoystickID id, SDL_Gamepad* gp)>;
    using ResizeCallback        = std::function<void(int w, int h)>;

    SDL3Input() = default;
    ~SDL3Input() {
        LOG_INFO_CAT("Dispose", "{}Destroying SDL3Input — closing {} gamepads — RASPBERRY_PINK ETERNAL{}", 
                     Color::Logging::RASPBERRY_PINK, m_gamepads.size(), Color::Logging::RESET);
        for (auto& [id, gp] : m_gamepads) {
            SDL_CloseGamepad(gp);
        }
        m_gamepads.clear();
    }

    void initialize() {
        const std::string loc = locationString();
        const std::string_view platform = SDL_GetPlatform();

        if (platform != "Linux" && platform != "Windows") {
            LOG_ERROR_CAT("Input", "{}Unsupported platform: {} | {}{}", 
                          Color::Logging::OCEAN_TEAL, platform, loc, Color::Logging::RESET);
            throw std::runtime_error(std::format("Unsupported platform: {}", platform));
        }

        LOG_SUCCESS_CAT("Input", "{}Initializing SDL3Input | {}{}", 
                        Color::Logging::OCEAN_TEAL, loc, Color::Logging::RESET);

        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
        int num = 0;
        SDL_JoystickID* joysticks = SDL_GetJoysticks(&num);

        LOG_INFO_CAT("Input", "{}Found {} joysticks | {}{}", 
                     Color::Logging::OCEAN_TEAL, num, loc, Color::Logging::RESET);

        if (joysticks) {
            for (int i = 0; i < num; ++i) {
                if (SDL_IsGamepad(joysticks[i])) {
                    if (auto gp = SDL_OpenGamepad(joysticks[i])) {
                        m_gamepads[joysticks[i]] = gp;
                        if (m_gamepadConnectCallback) {
                            m_gamepadConnectCallback(true, joysticks[i], gp);
                        }
                    }
                }
            }
            SDL_free(joysticks);
        }
    }

    bool pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose = true) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    LOG_INFO_CAT("Input", "{}Quit requested{}", 
                                 Color::Logging::OCEAN_TEAL, Color::Logging::RESET);
                    return !exitOnClose;

                case SDL_EVENT_WINDOW_RESIZED:
                    LOG_INFO_CAT("Input", "{}Window resized: {}x{}{}", 
                                 Color::Logging::OCEAN_TEAL, ev.window.data1, ev.window.data2, Color::Logging::RESET);
                    if (m_resizeCallback) m_resizeCallback(ev.window.data1, ev.window.data2);
                    break;

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
                    handleTouch(ev.tfinger);
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

    void setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                      MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                      GamepadButtonCallback gb, GamepadAxisCallback ga,
                      GamepadConnectCallback gc, ResizeCallback resize) {
        m_keyboardCallback = std::move(kb);
        m_mouseButtonCallback = std::move(mb);
        m_mouseMotionCallback = std::move(mm);
        m_mouseWheelCallback = std::move(mw);
        m_textInputCallback = std::move(ti);
        m_touchCallback = std::move(tc);
        m_gamepadButtonCallback = std::move(gb);
        m_gamepadAxisCallback = std::move(ga);
        m_gamepadConnectCallback = std::move(gc);
        m_resizeCallback = std::move(resize);

        LOG_SUCCESS_CAT("Input", "{}All input callbacks registered{}", 
                        Color::Logging::OCEAN_TEAL, Color::Logging::RESET);
    }

    void enableTextInput(SDL_Window* window, bool enable) {
        if (enable) {
            SDL_StartTextInput(window);
            LOG_INFO_CAT("Input", "{}Text input ENABLED{}", 
                         Color::Logging::OCEAN_TEAL, Color::Logging::RESET);
        } else {
            SDL_StopTextInput(window);
            LOG_INFO_CAT("Input", "{}Text input DISABLED{}", 
                         Color::Logging::OCEAN_TEAL, Color::Logging::RESET);
        }
    }

    [[nodiscard]] const std::map<SDL_JoystickID, SDL_Gamepad*>& gamepads() const noexcept { return m_gamepads; }

    void exportLog(std::string_view filename) const {
        const std::string loc = locationString();
        LOG_INFO_CAT("Input", "{}Exporting input log → {} | {}{}", 
                     Color::Logging::OCEAN_TEAL, filename, loc, Color::Logging::RESET);

        std::ofstream f(filename.data(), std::ios::app);
        if (f.is_open()) {
            f << "[INPUT LOG] " << std::time(nullptr) 
              << " | Gamepads: " << m_gamepads.size() << "\n";
            LOG_SUCCESS_CAT("Input", "{}Log exported → {}{}", 
                            Color::Logging::OCEAN_TEAL, filename, Color::Logging::RESET);
        } else {
            LOG_ERROR_CAT("Input", "{}Failed to export log → {}{}", 
                          Color::Logging::OCEAN_TEAL, filename, Color::Logging::RESET);
        }
    }

private:
    static std::string locationString(const std::source_location& loc = std::source_location::current()) {
        return std::format("{}:{}:{}", loc.file_name(), loc.line(), loc.function_name());
    }

    void handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen) {
        if (!k.down) return;

        switch (k.key) {
            case SDLK_F: {
                bool fs = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
                SDL_SetWindowFullscreen(window, !fs);
                LOG_INFO_CAT("Input", "{}Fullscreen toggle → {}{}", 
                             Color::Logging::OCEAN_TEAL, !fs ? "ON" : "OFF", Color::Logging::RESET);
            } break;

            case SDLK_ESCAPE: {
                SDL_Event quitEvt{.type = SDL_EVENT_QUIT};
                SDL_PushEvent(&quitEvt);
            } break;

            case SDLK_SPACE:
                if (audioDevice) {
                    bool paused = SDL_AudioDevicePaused(audioDevice);
                    paused ? SDL_ResumeAudioDevice(audioDevice) : SDL_PauseAudioDevice(audioDevice);
                    LOG_INFO_CAT("Input", "{}Audio {} via SPACE{}", 
                                 Color::Logging::OCEAN_TEAL, paused ? "RESUMED" : "PAUSED", Color::Logging::RESET);
                }
                break;

            case SDLK_M:
                if (audioDevice) {
                    float g = SDL_GetAudioDeviceGain(audioDevice);
                    SDL_SetAudioDeviceGain(audioDevice, g > 0.5f ? 0.0f : 1.0f);
                    LOG_INFO_CAT("Input", "{}Audio MUTE toggle{}", 
                                 Color::Logging::OCEAN_TEAL, Color::Logging::RESET);
                }
                break;

            case SDLK_GRAVE:
                consoleOpen = !consoleOpen;
                LOG_INFO_CAT("Input", "{}Console toggle → {}{}", 
                             Color::Logging::OCEAN_TEAL, consoleOpen ? "OPEN" : "CLOSED", Color::Logging::RESET);
                break;
        }
    }

    void handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window) {
        if (b.down && b.button == SDL_BUTTON_RIGHT) {
            bool rel = SDL_GetWindowRelativeMouseMode(window);
            SDL_SetWindowRelativeMouseMode(window, !rel);
            LOG_INFO_CAT("Input", "{}Relative mouse → {}{}", 
                         Color::Logging::OCEAN_TEAL, !rel ? "ON" : "OFF", Color::Logging::RESET);
        }
    }

    void handleTouch(const SDL_TouchFingerEvent& t) {
        if (m_touchCallback) m_touchCallback(t);
    }

    void handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice) {
        if (!g.down) return;

        switch (g.button) {
            case SDL_GAMEPAD_BUTTON_EAST: {
                SDL_Event quitEvt{.type = SDL_EVENT_QUIT};
                SDL_PushEvent(&quitEvt);
            } break;

            case SDL_GAMEPAD_BUTTON_START:
                if (audioDevice) {
                    bool p = SDL_AudioDevicePaused(audioDevice);
                    p ? SDL_ResumeAudioDevice(audioDevice) : SDL_PauseAudioDevice(audioDevice);
                }
                break;
        }
    }

    void handleGamepadConnection(const SDL_GamepadDeviceEvent& e) {
        if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
            if (auto gp = SDL_OpenGamepad(e.which)) {
                m_gamepads[e.which] = gp;
                LOG_SUCCESS_CAT("Input", "{}Gamepad ADDED: {}{}", 
                                Color::Logging::OCEAN_TEAL, e.which, Color::Logging::RESET);
                if (m_gamepadConnectCallback) m_gamepadConnectCallback(true, e.which, gp);
            }
        } else {
            auto it = m_gamepads.find(e.which);
            if (it != m_gamepads.end()) {
                LOG_INFO_CAT("Input", "{}Gamepad REMOVED: {}{}", 
                             Color::Logging::OCEAN_TEAL, e.which, Color::Logging::RESET);
                if (m_gamepadConnectCallback) m_gamepadConnectCallback(false, e.which, it->second);
                SDL_CloseGamepad(it->second);
                m_gamepads.erase(it);
            }
        }
    }

    std::map<SDL_JoystickID, SDL_Gamepad*> m_gamepads;

    KeyboardCallback      m_keyboardCallback;
    MouseButtonCallback   m_mouseButtonCallback;
    MouseMotionCallback   m_mouseMotionCallback;
    MouseWheelCallback    m_mouseWheelCallback;
    TextInputCallback     m_textInputCallback;
    TouchCallback         m_touchCallback;
    GamepadButtonCallback m_gamepadButtonCallback;
    GamepadAxisCallback   m_gamepadAxisCallback;
    GamepadConnectCallback m_gamepadConnectCallback;
    ResizeCallback        m_resizeCallback;
};

} // namespace SDL3Initializer

// =============================================================================
// SDL3Input — HEADER-ONLY — OCEAN_TEAL ETERNAL
// NO .cpp | ZERO LINK TIME | 15,000 FPS
// SHIP IT RAW
// =============================================================================