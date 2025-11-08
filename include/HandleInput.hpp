// include/engine/input/HandleInput.hpp
// AMOURANTH RTX Engine (C) 2025 – Input Handler Header
// SDL3 | Full callback system | Camera integration | Ready for F key
// FIXED: Namespace removed — global Camera forward decl — no conflict with VulkanRTX class

#pragma once

#include <SDL3/SDL.h>
#include <functional>

class Camera;  // GLOBAL FORWARD DECL — NO NAMESPACE
class Application;

// ---------------------------------------------------------------------------
//  Input handler – works with global Camera
//  Supports: 1-9, H, T, O, F, mouse, gamepad, touch
// ---------------------------------------------------------------------------
class HandleInput {
public:
    using KeyboardCallback       = std::function<void(const SDL_KeyboardEvent&)>;
    using MouseButtonCallback    = std::function<void(const SDL_MouseButtonEvent&)>;
    using MouseMotionCallback    = std::function<void(const SDL_MouseMotionEvent&)>;
    using MouseWheelCallback     = std::function<void(const SDL_MouseWheelEvent&)>;
    using TextInputCallback      = std::function<void(const SDL_TextInputEvent&)>;
    using TouchCallback          = std::function<void(const SDL_TouchFingerEvent&)>;
    using GamepadButtonCallback  = std::function<void(const SDL_GamepadButtonEvent&)>;
    using GamepadAxisCallback    = std::function<void(const SDL_GamepadAxisEvent&)>;
    using GamepadConnectCallback = std::function<void(bool, SDL_JoystickID, SDL_Gamepad*)>;

    explicit HandleInput(Camera& camera);

    void handleInput(Application& app);

    void setCallbacks(
        KeyboardCallback kb,
        MouseButtonCallback mb,
        MouseMotionCallback mm,
        MouseWheelCallback mw,
        TextInputCallback ti,
        TouchCallback tc,
        GamepadButtonCallback gb,
        GamepadAxisCallback ga,
        GamepadConnectCallback gc
    );

    // Default handlers (can be overridden)
    void defaultKeyboardHandler(const SDL_KeyboardEvent& key);
    void defaultMouseButtonHandler(const SDL_MouseButtonEvent& mb);
    void defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm);
    void defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw);
    void defaultTextInputHandler(const SDL_TextInputEvent& ti);
    void defaultTouchHandler(const SDL_TouchFingerEvent& tf);
    void defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb);
    void defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga);
    void defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, SDL_Gamepad* pad);

private:
    Camera& camera_;  // GLOBAL Camera — NO VulkanRTX:: prefix

    KeyboardCallback       keyboardCallback_;
    MouseButtonCallback    mouseButtonCallback_;
    MouseMotionCallback    mouseMotionCallback_;
    MouseWheelCallback     mouseWheelCallback_;
    TextInputCallback      textInputCallback_;
    TouchCallback          touchCallback_;
    GamepadButtonCallback  gamepadButtonCallback_;
    GamepadAxisCallback    gamepadAxisCallback_;
    GamepadConnectCallback gamepadConnectCallback_;
};