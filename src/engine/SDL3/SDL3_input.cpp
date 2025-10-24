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

bool SDL3Input::pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
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
                break;
            case SDL_EVENT_KEY_UP:
                if (m_keyboardCallback) m_keyboardCallback(event.key);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                handleMouseButton(event.button, window);
                if (m_mouseButtonCallback) m_mouseButtonCallback(event.button);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (m_mouseMotionCallback) m_mouseMotionCallback(event.motion);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (m_mouseWheelCallback) m_mouseWheelCallback(event.wheel);
                break;
            case SDL_EVENT_TEXT_INPUT:
                if (m_textInputCallback) m_textInputCallback(event.text);
                break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
                handleTouch(event.tfinger);
                if (m_touchCallback) m_touchCallback(event.tfinger);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                handleGamepadButton(event.gbutton, audioDevice);
                if (m_gamepadButtonCallback) m_gamepadButtonCallback(event.gbutton);
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if (m_gamepadAxisCallback) m_gamepadAxisCallback(event.gaxis);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                handleGamepadConnection(event.gdevice);
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
    if (m_touchCallback) m_touchCallback(t);
}

void SDL3Input::handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen) {
    if (k.type != SDL_EVENT_KEY_DOWN) return;
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
            }
            break;
        case SDLK_SPACE:
            if (audioDevice) {
                if (SDL_AudioDevicePaused(audioDevice)) {
                    SDL_ResumeAudioDevice(audioDevice);
                } else {
                    SDL_PauseAudioDevice(audioDevice);
                }
            }
            break;
        case SDLK_M:
            if (audioDevice) {
                float gain = SDL_GetAudioDeviceGain(audioDevice);
                SDL_SetAudioDeviceGain(audioDevice, gain == 0.0f ? 1.0f : 0.0f);
            }
            break;
        case SDLK_GRAVE:
            consoleOpen = !consoleOpen;
            break;
    }
}

void SDL3Input::handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window) {
    if (b.type == SDL_EVENT_MOUSE_BUTTON_DOWN && b.button == SDL_BUTTON_RIGHT) {
        bool relative = SDL_GetWindowRelativeMouseMode(window);
        SDL_SetWindowRelativeMouseMode(window, !relative);
    }
}

void SDL3Input::handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice) {
    if (g.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        switch (g.button) {
            case SDL_GAMEPAD_BUTTON_EAST:
                {
                    SDL_Event evt{.type = SDL_EVENT_QUIT};
                    SDL_PushEvent(&evt);
                }
                break;
            case SDL_GAMEPAD_BUTTON_START:
                if (audioDevice) {
                    if (SDL_AudioDevicePaused(audioDevice)) {
                        SDL_ResumeAudioDevice(audioDevice);
                    } else {
                        SDL_PauseAudioDevice(audioDevice);
                    }
                }
                break;
            default:
                break;
        }
    }
}

void SDL3Input::handleGamepadConnection(const SDL_GamepadDeviceEvent& e) {
    if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (auto gp = SDL_OpenGamepad(e.which)) {
            m_gamepads[e.which] = gp;
            if (m_gamepadConnectCallback) m_gamepadConnectCallback(true, e.which, gp);
        }
    } else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
        auto it = m_gamepads.find(e.which);
        if (it != m_gamepads.end()) {
            if (m_gamepadConnectCallback) m_gamepadConnectCallback(false, e.which, it->second);
            SDL_CloseGamepad(it->second);
            m_gamepads.erase(it);
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