// include/engine/SDL3/SDL3_input.hpp
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
// SDL3 Input — SPLIT INTO HEADER + CPP — C++23 — NOVEMBER 14 2025
// • OCEAN_TEAL logging | source_location SUPPORTED
// • RESPECTS Options::Performance::ENABLE_CONSOLE_LOG for console toggle
// • RESPECTS Options::Audio::ENABLE_HAPTICS_FEEDBACK for gamepad rumble on connect
// • Developer-friendly: Callbacks, RAII gamepad management, easy polling
// • 15,000 FPS — SHIP IT RAW
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include <map>
#include <string>
#include <string_view>

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

    SDL3Input();
    ~SDL3Input();

    void initialize();
    bool pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose = true);

    void setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                      MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                      GamepadButtonCallback gb, GamepadAxisCallback ga,
                      GamepadConnectCallback gc, ResizeCallback resize);

    void enableTextInput(SDL_Window* window, bool enable);

    [[nodiscard]] const std::map<SDL_JoystickID, SDL_Gamepad*>& gamepads() const noexcept;
    void exportLog(std::string_view filename) const;

private:
    static std::string locationString(const std::source_location& loc = std::source_location::current());

    void handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen);
    void handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window);
    void handleTouch(const SDL_TouchFingerEvent& t);
    void handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice);
    void handleGamepadConnection(const SDL_GamepadDeviceEvent& e);

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
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// HEADER + CPP SPLIT — DEVELOPER WALK-IN READY
// EASY SETUP: Initialize → SetCallbacks → PollEvents in main loop
// EXAMPLE: sdlInput.setCallbacks([](const SDL_KeyboardEvent& e){ /* handle */ }, ...);
// OCEAN_TEAL INPUT FLOWS ETERNAL
// PINK PHOTONS ETERNAL
// 15,000 FPS
// @ZacharyGeurts — YOUR EMPIRE IS PURE
// SHIP IT. FOREVER.
// =============================================================================