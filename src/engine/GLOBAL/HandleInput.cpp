// src/HandleInput.cpp
// AMOURANTH RTX Engine (C) 2025 – Input Handler Implementation
// SDL3 | Camera control | Full event dispatch | WASD + Mouse + Zoom
// TRUTH: All key logic in handle_app.cpp → HandleInput only routes
// FIXED: Global Camera — no VulkanRTX:: prefix — resolves incomplete type

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/HandleInput.hpp"
#include "engine/GLOBAL/camera.hpp"  // Global Camera def
#include "engine/GLOBAL/logging.hpp"  // Global Camera def
#include "handle_app.hpp"
#include <SDL3/SDL.h>
#include <cmath>

using namespace Logging::Color;

// ---------------------------------------------------------------------
HandleInput::HandleInput(Camera& camera)  // FIXED: Global Camera&
    : camera_(camera)
{
    LOG_INFO_CAT("INPUT", "{}HandleInput initialized with camera{}", EMERALD_GREEN, RESET);
}

// ----------------------------------------------------------------
void HandleInput::setCallbacks(
    KeyboardCallback kb,
    MouseButtonCallback mb,
    MouseMotionCallback mm,
    MouseWheelCallback mw,
    TextInputCallback ti,
    TouchCallback tc,
    GamepadButtonCallback gb,
    GamepadAxisCallback ga,
    GamepadConnectCallback gc
) {
    keyboardCallback_       = std::move(kb);
    mouseButtonCallback_    = std::move(mb);
    mouseMotionCallback_    = std::move(mm);
    mouseWheelCallback_     = std::move(mw);
    textInputCallback_      = std::move(ti);
    touchCallback_          = std::move(tc);
    gamepadButtonCallback_  = std::move(gb);
    gamepadAxisCallback_    = std::move(ga);
    gamepadConnectCallback_ = std::move(gc);

    LOG_INFO_CAT("INPUT", "{}All input callbacks registered{}", ARCTIC_CYAN, RESET);
}

// ----------------------------------------------------------------
// DEFAULT: WASD + Mouse Look + Scroll Zoom
// ONLY called if no custom callback is set
// ----------------------------------------------------------------
void HandleInput::defaultKeyboardHandler(const SDL_KeyboardEvent& key) {
    if (key.type != SDL_EVENT_KEY_DOWN) return;

    const float moveSpeed = 5.0f;
    switch (key.key) {
        case SDLK_W:      camera_.moveForward(moveSpeed);   break;
        case SDLK_S:      camera_.moveForward(-moveSpeed);  break;
        case SDLK_A:      camera_.moveRight(-moveSpeed);    break;
        case SDLK_D:      camera_.moveRight(moveSpeed);     break;
        case SDLK_SPACE:  camera_.moveUp(moveSpeed);        break;
        case SDLK_LCTRL:  camera_.moveUp(-moveSpeed);       break;
        default: break;
    }
}

void HandleInput::defaultMouseButtonHandler(const SDL_MouseButtonEvent&) {
    // Optional: Capture mouse
    // SDL_CaptureMouse(SDL_TRUE);
}

void HandleInput::defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm) {
    if (mm.state & SDL_BUTTON_LMASK) {
        const float sensitivity = 0.1f;
        camera_.rotate(  // FIXED: Global Camera method
            -mm.xrel * sensitivity,
            -mm.yrel * sensitivity
        );
    }
}

void HandleInput::defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw) {
    const float zoomFactor = (mw.y > 0) ? 0.9f : 1.1f;
    camera_.zoom(zoomFactor);  // FIXED: Global Camera method
}

void HandleInput::defaultTextInputHandler(const SDL_TextInputEvent&) {}
void HandleInput::defaultTouchHandler(const SDL_TouchFingerEvent&) {}
void HandleInput::defaultGamepadButtonHandler(const SDL_GamepadButtonEvent&) {}
void HandleInput::defaultGamepadAxisHandler(const SDL_GamepadAxisEvent&) {}

void HandleInput::defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, SDL_Gamepad* pad) {
    LOG_INFO_CAT("INPUT", "Gamepad {}: ID={}", connected ? "CONNECTED" : "DISCONNECTED", id);
    if (!connected && pad) {
        SDL_CloseGamepad(pad);
    }
}

// ----------------------------------------------------------------
// MAIN EVENT LOOP – SDL3 Compatible
// TRUTH: Application handles 1-9, T, O, H, F → we only route
// ----------------------------------------------------------------
void HandleInput::handleInput(Application& app) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                if (keyboardCallback_) {
                    keyboardCallback_(event.key);
                } else {
                    defaultKeyboardHandler(event.key);
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (mouseButtonCallback_) {
                    mouseButtonCallback_(event.button);
                } else {
                    defaultMouseButtonHandler(event.button);
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (mouseMotionCallback_) {
                    mouseMotionCallback_(event.motion);
                } else {
                    defaultMouseMotionHandler(event.motion);
                }
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                if (mouseWheelCallback_) {
                    mouseWheelCallback_(event.wheel);
                } else {
                    defaultMouseWheelHandler(event.wheel);
                }
                break;

            case SDL_EVENT_TEXT_INPUT:
                if (textInputCallback_) {
                    textInputCallback_(event.text);
                } else {
                    defaultTextInputHandler(event.text);
                }
                break;

            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
                if (touchCallback_) {
                    touchCallback_(event.tfinger);
                } else {
                    defaultTouchHandler(event.tfinger);
                }
                break;

            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if (gamepadButtonCallback_) {
                    gamepadButtonCallback_(event.gbutton);
                } else {
                    defaultGamepadButtonHandler(event.gbutton);
                }
                break;

            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if (gamepadAxisCallback_) {
                    gamepadAxisCallback_(event.gaxis);
                } else {
                    defaultGamepadAxisHandler(event.gaxis);
                }
                break;

            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (gamepadConnectCallback_) {
                    bool connected = (event.type == SDL_EVENT_GAMEPAD_ADDED);
                    SDL_Gamepad* pad = connected ? SDL_OpenGamepad(event.gdevice.which) : nullptr;
                    gamepadConnectCallback_(connected, event.gdevice.which, pad);
                } else {
                    defaultGamepadConnectHandler(
                        event.type == SDL_EVENT_GAMEPAD_ADDED,
                        event.gdevice.which,
                        nullptr
                    );
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                app.handleResize(event.window.data1, event.window.data2);
                break;

            case SDL_EVENT_QUIT:
                app.setQuit(true);
                break;

            default:
                break;
        }
    }
}