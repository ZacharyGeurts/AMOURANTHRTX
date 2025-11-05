// handle_app.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FINAL: T = toggle tonemap | O = toggle overlay | 1-9 = render modes | H = HYPERTRACE
// On-screen title reflects all active modes

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

#include "engine/SDL3/SDL3_init.hpp"
#include "engine/camera.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/logging.hpp"
#include "HandleInput.hpp"

class Application;

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

    // USER CONTROLS
    void toggleTonemap();      // T / t
    void toggleOverlay();      // O / o
    void toggleHypertrace();  // H / h — 12,000+ FPS mode

    void setQuit(bool q) { quit_ = q; }

private:
    void initializeInput();
    void render();
    void updateWindowTitle();

    std::string title_;
    int width_;
    int height_;
    int mode_{1};               // 1-9
    bool quit_{false};

    std::unique_ptr<SDL3Initializer::SDL3Initializer> sdl_;
    std::unique_ptr<VulkanRTX::VulkanRenderer> renderer_;
    std::unique_ptr<VulkanRTX::Camera> camera_;
    std::unique_ptr<HandleInput> inputHandler_;

    bool isFullscreen_{false};
    bool isMaximized_{false};
    bool showOverlay_{true};

    // NEW – independent tonemap flag
    bool tonemapEnabled_{false};

    std::vector<glm::vec3> vertices_;
    std::vector<uint32_t> indices_;
    std::chrono::steady_clock::time_point lastFrameTime_;
};

#endif // HANDLE_APP_HPP