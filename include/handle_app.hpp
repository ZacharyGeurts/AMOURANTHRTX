// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Application handling header for SDL3 + Vulkan integration.
// Dependencies: SDL3, GLM, VulkanRTX_Setup.hpp, logging.hpp, Dispose.hpp, ue_init.hpp
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#ifndef HANDLE_APP_HPP
#define HANDLE_APP_HPP

#include <memory>
#include <optional>
#include <chrono>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <SDL3/SDL.h>
#include <source_location>
#include <functional>
#include "engine/logging.hpp"
#include "engine/Dispose.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp" // For Vulkan types like VkDevice
#include "engine/SDL3/SDL3_init.hpp"         // SDL3Initializer definition
#include "engine/Vulkan/VulkanRenderer.hpp"  // VulkanRenderer definition
#include "ue_init.hpp"                       // AMOURANTH, UniversalEquation, DimensionalNavigator

// Typedefs for HandleInput callbacks
using KeyboardCallback = std::function<void(const SDL_KeyboardEvent&)>;
using MouseButtonCallback = std::function<void(const SDL_MouseButtonEvent&)>;
using MouseMotionCallback = std::function<void(const SDL_MouseMotionEvent&)>;
using MouseWheelCallback = std::function<void(const SDL_MouseWheelEvent&)>;
using TextInputCallback = std::function<void(const SDL_TextInputEvent&)>;
using TouchCallback = std::function<void(const SDL_TouchFingerEvent&)>;
using GamepadButtonCallback = std::function<void(const SDL_GamepadButtonEvent&)>;
using GamepadAxisCallback = std::function<void(const SDL_GamepadAxisEvent&)>;
using GamepadConnectCallback = std::function<void(bool, SDL_JoystickID, SDL_Gamepad*)>;

// Simple OBJ loader prototype
void loadMesh(const std::string& filename, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices);

class Application;

// HandleInput class for managing user input
class HandleInput {
public:
    HandleInput(UE::UniversalEquation& universalEquation, UE::DimensionalNavigator* navigator, UE::AMOURANTH& amouranth);

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

    // Default input handlers
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
    UE::UniversalEquation& universalEquation_;
    UE::DimensionalNavigator* navigator_;
    UE::AMOURANTH& amouranth_;
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

class Application {
public:
    Application(const char* title, int width, int height);
    ~Application();

    void run();
    void render();
    void handleResize(int width, int height);
    void initializeInput();
    void initializeAudio();

    bool shouldQuit() const { return mode_ == 0; }
    void setRenderMode(int mode) { mode_ = mode; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getMode() const { return mode_; }
    VkDevice getDevice() const { return renderer_ ? renderer_->getDevice() : VK_NULL_HANDLE; }
    SDL_Window* getWindow() const { return sdl_ ? sdl_->getWindow() : nullptr; }

private:
    std::string title_;
    int width_;
    int height_;
    int mode_;
    std::unique_ptr<SDL3Initializer::SDL3Initializer> sdl_;
    std::unique_ptr<VulkanRTX::VulkanRenderer> renderer_;
    std::unique_ptr<UE::DimensionalNavigator> navigator_;
    std::optional<UE::AMOURANTH> amouranth_;
    std::unique_ptr<HandleInput> inputHandler_;
    SDL_AudioDeviceID audioDevice_;
    SDL_AudioStream* audioStream_;
    std::chrono::steady_clock::time_point lastFrameTime_;
    std::vector<glm::vec3> vertices_;
    std::vector<uint32_t> indices_;
};

#endif // HANDLE_APP_HPP