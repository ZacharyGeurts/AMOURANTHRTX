// handle_app.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FINAL: setRenderer() added — Vulkan init moved to main.cpp

#pragma once
#ifndef HANDLE_APP_HPP
#define HANDLE_APP_HPP

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <chrono>
#include <functional>
#include <string>
#include <cstdio>  // for snprintf

#include "engine/SDL3/SDL3_init.hpp"
#include "engine/camera.hpp"           // brings in VulkanRTX::Camera
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"

namespace VulkanRTX { class VulkanRenderer; }

class Application;

// ---------------------------------------------------------------------------
//  Input handler – works with any Camera implementation
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

    // NOTE: Camera is now fully qualified
    explicit HandleInput(VulkanRTX::Camera& camera);

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
    VulkanRTX::Camera& camera_;                     // <-- qualified
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

// ---------------------------------------------------------------------------
//  Main Application
// ---------------------------------------------------------------------------
class Application {
public:
    Application(const char* title, int width, int height);
    ~Application();

    void run();
    void setRenderMode(int mode) { mode_ = mode; }
    [[nodiscard]] bool shouldQuit() const { return mode_ == 0; }
    void handleResize(int width, int height);
    void toggleFullscreen();
    void toggleMaximize();

    [[nodiscard]] SDL_Window* getWindow() const { return sdl_->getWindow(); }
    bool& isMaximizedRef()  { return isMaximized_; }
    bool& isFullscreenRef() { return isFullscreen_; }

    static const std::vector<glm::vec3>& getGlobalVertices()  { return globalVertices_; }
    static const std::vector<uint32_t>& getGlobalIndices()   { return globalIndices_; }

    void setRenderer(std::unique_ptr<VulkanRTX::VulkanRenderer> renderer);

private:
    void initializeInput();
    void render();

    std::string title_;
    int width_;
    int height_;
    int mode_{1};

    std::unique_ptr<SDL3Initializer::SDL3Initializer> sdl_;
    std::unique_ptr<VulkanRTX::VulkanRenderer> renderer_;

    // NOTE: Camera is now a concrete VulkanRTX::Camera (PerspectiveCamera is a subclass)
    std::unique_ptr<VulkanRTX::Camera> camera_;

    std::unique_ptr<HandleInput> inputHandler_;

    bool isFullscreen_{false};
    bool isMaximized_{false};

    std::vector<glm::vec3> vertices_;
    std::vector<uint32_t> indices_;
    std::chrono::steady_clock::time_point lastFrameTime_;

    static inline std::vector<glm::vec3> globalVertices_;
    static inline std::vector<uint32_t> globalIndices_;
};

#endif // HANDLE_APP_HPP