// AMOURANTH RTX Engine, October 2025 - Input handling implementation for keyboard, mouse, and gamepad.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_input.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <source_location>
#include <fstream>

namespace SDL3Initializer {

SDL3Input::~SDL3Input() {
    LOG_INFO_CAT("Input", "Destroying SDL3Input, closing all gamepads", std::source_location::current());
    for (auto& [id, gp] : m_gamepads) {
        LOG_INFO_CAT("Input", "Closing gamepad: ID {}", std::source_location::current(), id);
        SDL_CloseGamepad(gp);
    }
    m_gamepads.clear();
}

void SDL3Input::initialize() {
    // Verify platform support
    std::string_view platform = SDL_GetPlatform();
    if (platform != "Linux" && platform != "Windows") {
        LOG_ERROR_CAT("Input", "Unsupported platform for input: {}", std::source_location::current(), platform);
        throw std::runtime_error(std::string("Unsupported platform for input: ") + std::string(platform));
    }

    LOG_INFO_CAT("Input", "Initializing SDL3Input", std::source_location::current());

    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    int numJoysticks;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&numJoysticks);
    if (joysticks) {
        LOG_INFO_CAT("Input", "Found {} joysticks", std::source_location::current(), numJoysticks);
        for (int i = 0; i < numJoysticks; ++i) {
            if (SDL_IsGamepad(joysticks[i])) {
                LOG_DEBUG_CAT("Input", "Found gamepad: ID {}", std::source_location::current(), joysticks[i]);
                if (auto gp = SDL_OpenGamepad(joysticks[i])) {
                    LOG_INFO_CAT("Input", "Opened gamepad: ID {}", std::source_location::current(), joysticks[i]);
                    m_gamepads[joysticks[i]] = gp;
                    if (m_gamepadConnectCallback) {
                        m_gamepadConnectCallback(true, joysticks[i], gp);
                    }
                } else {
                    LOG_WARNING_CAT("Input", "Failed to open gamepad: ID {}, error: {}", 
                                    std::source_location::current(), joysticks[i], SDL_GetError());
                }
            }
        }
        SDL_free(joysticks);
    } else {
        LOG_WARNING_CAT("Input", "No joysticks found: {}", std::source_location::current(), SDL_GetError());
    }
}

bool SDL3Input::pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        LOG_DEBUG_CAT("Input", "Processing SDL event type: {}", std::source_location::current(), event.type);
        switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                LOG_INFO_CAT("Input", "Quit or window close event received", std::source_location::current());
                return !exitOnClose;
            case SDL_EVENT_WINDOW_RESIZED:
                LOG_INFO_CAT("Input", "Window resized: width={}, height={}", 
                             std::source_location::current(), event.window.data1, event.window.data2);
                if (m_resizeCallback) m_resizeCallback(event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_KEY_DOWN:
                handleKeyboard(event.key, window, audioDevice, consoleOpen);
                if (m_keyboardCallback) m_keyboardCallback(event.key);
                LOG_DEBUG_CAT("Input", "Key down event: {}", std::source_location::current(), SDL_GetKeyName(event.key.key));
                break;
            case SDL_EVENT_KEY_UP:
                if (m_keyboardCallback) m_keyboardCallback(event.key);
                LOG_DEBUG_CAT("Input", "Key up event: {}", std::source_location::current(), SDL_GetKeyName(event.key.key));
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                handleMouseButton(event.button, window);
                if (m_mouseButtonCallback) m_mouseButtonCallback(event.button);
                LOG_DEBUG_CAT("Input", "Mouse button event: button {}", std::source_location::current(), event.button.button);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (m_mouseMotionCallback) m_mouseMotionCallback(event.motion);
                LOG_DEBUG_CAT("Input", "Mouse motion event: xrel={}, yrel={}", 
                              std::source_location::current(), event.motion.xrel, event.motion.yrel);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (m_mouseWheelCallback) m_mouseWheelCallback(event.wheel);
                LOG_DEBUG_CAT("Input", "Mouse wheel event: x={}, y={}", 
                              std::source_location::current(), event.wheel.x, event.wheel.y);
                break;
            case SDL_EVENT_TEXT_INPUT:
                if (m_textInputCallback) m_textInputCallback(event.text);
                LOG_DEBUG_CAT("Input", "Text input event: {}", std::source_location::current(), event.text.text);
                break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
                handleTouch(event.tfinger);
                if (m_touchCallback) m_touchCallback(event.tfinger);
                LOG_DEBUG_CAT("Input", "Touch event: finger {}, x={}, y={}", 
                              std::source_location::current(), event.tfinger.fingerID, event.tfinger.x, event.tfinger.y);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                handleGamepadButton(event.gbutton, audioDevice);
                if (m_gamepadButtonCallback) m_gamepadButtonCallback(event.gbutton);
                LOG_DEBUG_CAT("Input", "Gamepad button event: button {}", std::source_location::current(), event.gbutton.button);
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if (m_gamepadAxisCallback) m_gamepadAxisCallback(event.gaxis);
                LOG_DEBUG_CAT("Input", "Gamepad axis event: axis {}, value {}", 
                              std::source_location::current(), event.gaxis.axis, event.gaxis.value);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                handleGamepadConnection(event.gdevice);
                LOG_INFO_CAT("Input", "Gamepad event: type={}, which={}", 
                             std::source_location::current(), event.gdevice.type, event.gdevice.which);
                break;
        }
    }
    return true;
}

void SDL3Input::setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                             MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                             GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc,
                             ResizeCallback onResize) {
    m_keyboardCallback = kb;
    m_mouseButtonCallback = mb;
    m_mouseMotionCallback = mm;
    m_mouseWheelCallback = mw;
    m_textInputCallback = ti;
    m_touchCallback = tc;
    m_gamepadButtonCallback = gb;
    m_gamepadAxisCallback = ga;
    m_gamepadConnectCallback = gc;
    m_resizeCallback = onResize;
    LOG_INFO_CAT("Input", "Input callbacks set", std::source_location::current());
}

void SDL3Input::enableTextInput(SDL_Window* window, bool enable) {
    if (enable) {
        SDL_StartTextInput(window);
        LOG_INFO_CAT("Input", "Text input enabled", std::source_location::current());
    } else {
        SDL_StopTextInput(window);
        LOG_INFO_CAT("Input", "Text input disabled", std::source_location::current());
    }
}

void SDL3Input::handleTouch(const SDL_TouchFingerEvent& t) {
    LOG_DEBUG_CAT("Input", "Handling touch event: finger {}, x={}, y={}", 
                  std::source_location::current(), t.fingerID, t.x, t.y);
    if (m_touchCallback) m_touchCallback(t);
}

void SDL3Input::handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen) {
    if (k.type != SDL_EVENT_KEY_DOWN) return;
    LOG_DEBUG_CAT("Input", "Handling key: {}", std::source_location::current(), SDL_GetKeyName(k.key));
    switch (k.key) {
        case SDLK_F:
            {
                Uint32 flags = SDL_GetWindowFlags(window);
                bool isFullscreen = flags & SDL_WINDOW_FULLSCREEN;
                SDL_SetWindowFullscreen(window, isFullscreen ? 0 : SDL_WINDOW_FULLSCREEN);
                LOG_INFO_CAT("Input", "Toggling fullscreen mode: {}", std::source_location::current(), isFullscreen ? "off" : "on");
            }
            break;
        case SDLK_ESCAPE:
            {
                SDL_Event evt{.type = SDL_EVENT_QUIT};
                SDL_PushEvent(&evt);
                LOG_INFO_CAT("Input", "Pushing quit event", std::source_location::current());
            }
            break;
        case SDLK_SPACE:
            if (audioDevice) {
                if (SDL_AudioDevicePaused(audioDevice)) {
                    SDL_ResumeAudioDevice(audioDevice);
                    LOG_INFO_CAT("Input", "Resuming audio device", std::source_location::current());
                } else {
                    SDL_PauseAudioDevice(audioDevice);
                    LOG_INFO_CAT("Input", "Pausing audio device", std::source_location::current());
                }
            } else {
                LOG_WARNING_CAT("Input", "No audio device available for pause/resume", std::source_location::current());
            }
            break;
        case SDLK_M:
            if (audioDevice) {
                float gain = SDL_GetAudioDeviceGain(audioDevice);
                SDL_SetAudioDeviceGain(audioDevice, gain == 0.0f ? 1.0f : 0.0f);
                LOG_INFO_CAT("Input", "Toggling audio {}", std::source_location::current(), gain == 0.0f ? "unmuted" : "muted");
            } else {
                LOG_WARNING_CAT("Input", "No audio device available for mute/unmute", std::source_location::current());
            }
            break;
        case SDLK_GRAVE:
            consoleOpen = !consoleOpen;
            LOG_INFO_CAT("Input", "Toggling console: {}", std::source_location::current(), consoleOpen ? "open" : "closed");
            break;
    }
}

void SDL3Input::handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window) {
    LOG_DEBUG_CAT("Input", "{} mouse button {} at ({}, {})",
                  std::source_location::current(),
                  b.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Pressed" : "Released",
                  b.button == SDL_BUTTON_LEFT ? "Left" : b.button == SDL_BUTTON_RIGHT ? "Right" : "Middle",
                  b.x, b.y);
    if (b.type == SDL_EVENT_MOUSE_BUTTON_DOWN && b.button == SDL_BUTTON_RIGHT) {
        bool relative = SDL_GetWindowRelativeMouseMode(window);
        SDL_SetWindowRelativeMouseMode(window, !relative);
        LOG_INFO_CAT("Input", "Toggling relative mouse mode: {}", std::source_location::current(), !relative ? "enabled" : "disabled");
    }
}

void SDL3Input::handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice) {
    if (g.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        LOG_DEBUG_CAT("Input", "Gamepad button down: {}", std::source_location::current(), g.button);
        switch (g.button) {
            case SDL_GAMEPAD_BUTTON_SOUTH:
                LOG_INFO_CAT("Input", "South (A/Cross) pressed", std::source_location::current());
                break;
            case SDL_GAMEPAD_BUTTON_EAST:
                {
                    SDL_Event evt{.type = SDL_EVENT_QUIT};
                    SDL_PushEvent(&evt);
                    LOG_INFO_CAT("Input", "East (B/Circle) pressed, pushing quit event", std::source_location::current());
                }
                break;
            case SDL_GAMEPAD_BUTTON_WEST:
                LOG_INFO_CAT("Input", "West (X/Square) pressed", std::source_location::current());
                break;
            case SDL_GAMEPAD_BUTTON_NORTH:
                LOG_INFO_CAT("Input", "North (Y/Triangle) pressed", std::source_location::current());
                break;
            case SDL_GAMEPAD_BUTTON_START:
                if (audioDevice) {
                    if (SDL_AudioDevicePaused(audioDevice)) {
                        SDL_ResumeAudioDevice(audioDevice);
                        LOG_INFO_CAT("Input", "Start pressed, resuming audio device", std::source_location::current());
                    } else {
                        SDL_PauseAudioDevice(audioDevice);
                        LOG_INFO_CAT("Input", "Start pressed, pausing audio device", std::source_location::current());
                    }
                } else {
                    LOG_WARNING_CAT("Input", "Start pressed, no audio device available for pause/resume", std::source_location::current());
                }
                break;
            default:
                LOG_INFO_CAT("Input", "Button {} pressed", std::source_location::current(), static_cast<int>(g.button));
                break;
        }
    } else {
        LOG_DEBUG_CAT("Input", "Gamepad button {} released", std::source_location::current(), static_cast<int>(g.button));
    }
}

void SDL3Input::handleGamepadConnection(const SDL_GamepadDeviceEvent& e) {
    if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (auto gp = SDL_OpenGamepad(e.which)) {
            m_gamepads[e.which] = gp;
            LOG_INFO_CAT("Input", "Gamepad connected: ID {}", std::source_location::current(), e.which);
            if (m_gamepadConnectCallback) m_gamepadConnectCallback(true, e.which, gp);
        } else {
            LOG_WARNING_CAT("Input", "Failed to open gamepad: ID {}, error: {}", 
                            std::source_location::current(), e.which, SDL_GetError());
        }
    } else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
        auto it = m_gamepads.find(e.which);
        if (it != m_gamepads.end()) {
            LOG_INFO_CAT("Input", "Gamepad disconnected: ID {}", std::source_location::current(), e.which);
            if (m_gamepadConnectCallback) m_gamepadConnectCallback(false, e.which, it->second);
            SDL_CloseGamepad(it->second);
            m_gamepads.erase(it);
        } else {
            LOG_WARNING_CAT("Input", "Gamepad disconnect event for unknown ID {}", std::source_location::current(), e.which);
        }
    }
}

void SDL3Input::exportLog(const std::string& filename) const {
    LOG_INFO_CAT("Input", "Exporting log to {}", std::source_location::current(), filename);
    std::ofstream outFile(filename, std::ios::app);
    if (outFile.is_open()) {
        outFile << "SDL3Input log exported at " << std::time(nullptr) << "\n";
        outFile.close();
        LOG_INFO_CAT("Input", "Log exported successfully to {}", std::source_location::current(), filename);
    } else {
        LOG_ERROR_CAT("Input", "Failed to open log file {}", std::source_location::current(), filename);
    }
}

} // namespace SDL3Initializer