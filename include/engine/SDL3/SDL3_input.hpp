// Input handling for AMOURANTH RTX Engine, October 2025
// Manages SDL3 input events for keyboard, mouse, gamepad, and touch.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library.
// Usage: Initialize, set event callbacks, and poll events in main loop.
// Zachary Geurts 2025

#ifndef SDL3_INPUT_HPP
#define SDL3_INPUT_HPP

#include <SDL3/SDL.h>
#include <functional>
#include <map>
#include <string>

namespace SDL3Initializer {

class SDL3Input {
public:
    using KeyboardCallback = std::function<void(const SDL_KeyboardEvent&)>;
    using MouseButtonCallback = std::function<void(const SDL_MouseButtonEvent&)>;
    using MouseMotionCallback = std::function<void(const SDL_MouseMotionEvent&)>;
    using MouseWheelCallback = std::function<void(const SDL_MouseWheelEvent&)>;
    using TextInputCallback = std::function<void(const SDL_TextInputEvent&)>;
    using TouchCallback = std::function<void(const SDL_TouchFingerEvent&)>;
    using GamepadButtonCallback = std::function<void(const SDL_GamepadButtonEvent&)>;
    using GamepadAxisCallback = std::function<void(const SDL_GamepadAxisEvent&)>;
    using GamepadConnectCallback = std::function<void(bool, SDL_JoystickID, SDL_Gamepad*)>;
    using ResizeCallback = std::function<void(int, int)>;

    SDL3Input() = default;
    ~SDL3Input();

    void initialize();
    bool pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose);
    void setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                      MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                      GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc,
                      ResizeCallback onResize);
    void enableTextInput(SDL_Window* window, bool enable);
    const std::map<SDL_JoystickID, SDL_Gamepad*>& getGamepads() const { return m_gamepads; }
    void exportLog(const std::string& filename) const;

private:
    void handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen);
    void handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window);
    void handleTouch(const SDL_TouchFingerEvent& t);
    void handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice);
    void handleGamepadConnection(const SDL_GamepadDeviceEvent& e);

    std::map<SDL_JoystickID, SDL_Gamepad*> m_gamepads;
    KeyboardCallback m_keyboardCallback;
    MouseButtonCallback m_mouseButtonCallback;
    MouseMotionCallback m_mouseMotionCallback;
    MouseWheelCallback m_mouseWheelCallback;
    TextInputCallback m_textInputCallback;
    TouchCallback m_touchCallback;
    GamepadButtonCallback m_gamepadButtonCallback;
    GamepadAxisCallback m_gamepadAxisCallback;
    GamepadConnectCallback m_gamepadConnectCallback;
    ResizeCallback m_resizeCallback;
};

} // namespace SDL3Initializer

#endif // SDL3_INPUT_HPP