// handle_app.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FINAL: T = toggle tonemap | O = toggle overlay | 1-9 = render modes | On-screen title

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
#include <cstdio>

#include "engine/SDL3/SDL3_init.hpp"
#include "engine/camera.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/logging.hpp"
#include "HandleInput.hpp"  // FIXED: Include for HandleInput definition

// Forward declaration for HandleInput::handleInput(Application& app)
class Application;

// ---------------------------------------------------------------------------
//  Main Application
// ---------------------------------------------------------------------------
class Application {
public:
    Application(const char* title, int width, int height);
    ~Application();

    void run();
    void setRenderMode(int mode);
    [[nodiscard]] bool shouldQuit() const;
    void handleResize(int width, int height);
    void toggleFullscreen();
    void toggleMaximize();

    [[nodiscard]] SDL_Window* getWindow() const { return sdl_->getWindow(); }
    bool& isMaximizedRef()  { return isMaximized_; }
    bool& isFullscreenRef() { return isFullscreen_; }

    void setRenderer(std::unique_ptr<VulkanRTX::VulkanRenderer> renderer);

    // === USER CONTROLS ===
    void toggleTonemap();      // T key
    void toggleOverlay();      // O key

    // FIXED: Public setter for quit_ (SDL3 event handling)
    void setQuit(bool q) { quit_ = q; }

private:
    void initializeInput();
    void render();
    void updateWindowTitle();

    std::string title_;
    int width_;
    int height_;
    int mode_{1};  // 1-9 via core dispatch
    bool quit_{false};  // SDL3: Quit flag for event-driven exit

    std::unique_ptr<SDL3Initializer::SDL3Initializer> sdl_;
    std::unique_ptr<VulkanRTX::VulkanRenderer> renderer_;

    std::unique_ptr<VulkanRTX::Camera> camera_;
    std::unique_ptr<HandleInput> inputHandler_;  // FIXED: Now resolves with include

    bool isFullscreen_{false};
    bool isMaximized_{false};
    bool showOverlay_{true};

    std::vector<glm::vec3> vertices_;
    std::vector<uint32_t> indices_;
    std::chrono::steady_clock::time_point lastFrameTime_;
};

#endif // HANDLE_APP_HPP