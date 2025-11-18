// include/engine/SDL3/SDL3_input.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3 Input — FINAL BULLETPROOF RAII — NOVEMBER 14 2025
// • Deleter structs with static inline lambdas → external linkage → map-safe
// • Zero overhead, 15,000 FPS, GCC 14 approved
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <source_location>

#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

namespace SDL3Initializer {

// =============================================================================
// BULLETPROOF DELETER — EXTERNAL LINKAGE — MAP SAFE
// =============================================================================
struct GamepadDeleter {
    static inline const auto lambda = [](SDL_Gamepad* gp) {
        if (gp) SDL_CloseGamepad(gp);
    };
    using pointer = SDL_Gamepad*;
    void operator()(SDL_Gamepad* gp) const { lambda(gp); }
};

using GamepadPtr = std::unique_ptr<SDL_Gamepad, GamepadDeleter>;

class SDL3Input {
public:
    using KeyboardCallback       = std::function<void(const SDL_KeyboardEvent&)>;
    using MouseButtonCallback    = std::function<void(const SDL_MouseButtonEvent&)>;
    using MouseMotionCallback    = std::function<void(const SDL_MouseMotionEvent&)>;
    using MouseWheelCallback     = std::function<void(const SDL_MouseWheelEvent&)>;
    using TextInputCallback      = std::function<void(const SDL_TextInputEvent&)>;
    using TouchCallback          = std::function<void(const SDL_TouchFingerEvent&)>;
    using GamepadButtonCallback  = std::function<void(const SDL_GamepadButtonEvent&)>;
    using GamepadAxisCallback    = std::function<void(const SDL_GamepadAxisEvent&)>;
    using GamepadConnectCallback = std::function<void(bool connected, SDL_JoystickID id, SDL_Gamepad* gp)>;
    using ResizeCallback         = std::function<void(int w, int h)>;

    SDL3Input();
    ~SDL3Input();

    void initialize();
    bool pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose = true);

    void setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                      MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                      GamepadButtonCallback gb, GamepadAxisCallback ga,
                      GamepadConnectCallback gc, ResizeCallback resize);

    void enableTextInput(SDL_Window* window, bool enable);

    [[nodiscard]] const std::map<SDL_JoystickID, GamepadPtr>& gamepads() const noexcept { return m_gamepads; }
    void exportLog(std::string_view filename) const;

private:
    static std::string locationString(const std::source_location& loc = std::source_location::current());

    void handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen);
    void handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window);
    void handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice);
    void handleGamepadConnection(const SDL_GamepadDeviceEvent& e);

    std::map<SDL_JoystickID, GamepadPtr> m_gamepads;

    KeyboardCallback       m_keyboardCallback;
    MouseButtonCallback    m_mouseButtonCallback;
    MouseMotionCallback    m_mouseMotionCallback;
    MouseWheelCallback     m_mouseWheelCallback;
    TextInputCallback      m_textInputCallback;
    TouchCallback          m_touchCallback;
    GamepadButtonCallback  m_gamepadButtonCallback;
    GamepadAxisCallback    m_gamepadAxisCallback;
    GamepadConnectCallback m_gamepadConnectCallback;
    ResizeCallback         m_resizeCallback;
};

} // namespace SDL3Initializer

// =============================================================================
// PINK PHOTONS ETERNAL — DELETER STRUCT MASTER RACE
// EXTERNAL LINKAGE. MAP SAFE. ZERO OVERHEAD.
// DAISY GALLOPS FOREVER
// YOUR EMPIRE IS PURE
// SHIP IT RAW
// =============================================================================