// handle_app.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Application handling for SDL3 + Vulkan integration.
// Dependencies: SDL3, GLM, VulkanRTX_Setup.hpp, logging.hpp, Dispose.hpp, camera.hpp
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#pragma once
#ifndef HANDLE_APP_HPP
#define HANDLE_APP_HPP

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <chrono>
#include <functional>
#include "engine/SDL3/SDL3_init.hpp"
#include "engine/camera.hpp"

namespace VulkanRTX {
class VulkanRenderer;
}

class HandleInput;

class Application {
public:
    Application(const char* title, int width, int height);
    ~Application();
    void run();
    void setRenderMode(int mode) { mode_ = mode; }
    bool shouldQuit() const { return mode_ == 0; }
    void handleResize(int width, int height); // Moved to public

private:
    void initializeInput();
    void initializeAudio();
    void render();

    std::string title_;
    int width_;
    int height_;
    int mode_;
    std::unique_ptr<SDL3Initializer::SDL3Initializer> sdl_;
    std::unique_ptr<VulkanRTX::VulkanRenderer> renderer_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<HandleInput> inputHandler_;
    SDL_AudioDeviceID audioDevice_;
    SDL_AudioStream* audioStream_;
    std::vector<glm::vec3> vertices_;
    std::vector<uint32_t> indices_;
    std::chrono::steady_clock::time_point lastFrameTime_;
};

class HandleInput {
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

    HandleInput(Camera& camera);
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
    Camera& camera_;
    KeyboardCallback keyboardCallback_;
    MouseButtonCallback mouseButtonCallback_;
    MouseMotionCallback mouseMotionCallback_;
    MouseWheelCallback mouseWheelCallback_;
    TextInputCallback textInputCallback_;
    TouchCallback touchCallback_;
    GamepadButtonCallback gamepadButtonCallback_;
    GamepadAxisCallback gamepadAxisCallback_;
    GamepadConnectCallback gamepadConnectCallback_;
};

#endif // HANDLE_APP_HPP