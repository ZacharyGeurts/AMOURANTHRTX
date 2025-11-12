// include/handle_app.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// APPLICATION — REAL SWAPCHAIN — SDL3 — LOGGING PARTY — NOV 12 2025
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <chrono>
#include <string>

#include "engine/SDL3/SDL3_init.hpp"
#include "engine/core.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"

class Application {
public:
    Application(const char* title, int width, int height);
    ~Application();

    void run();
    void setRenderMode(int mode);
    [[nodiscard]] bool shouldQuit() const;
    void handleResize(int w, int h);
    void toggleFullscreen();
    void toggleMaximize();

    [[nodiscard]] SDL_Window* getWindow() const { return sdl_->getWindow(); }
    bool& isMaximizedRef()  { return isMaximized_; }
    bool& isFullscreenRef() { return isFullscreen_; }

    void setRenderer(std::unique_ptr<VulkanRenderer> renderer);
    [[nodiscard]] VulkanRenderer* getRenderer() const { return renderer_.get(); }

    void toggleTonemap();
    void toggleOverlay();
    void toggleHypertrace();
    void toggleFpsTarget();

    void setQuit(bool q) { quit_ = q; }

private:
    void render(float deltaTime, uint32_t imageIndex);
    void updateWindowTitle(float deltaTime);

    std::string title_;
    int width_, height_;
    int mode_{1};
    bool quit_{false};

    glm::mat4 renderView_{1.0f};
    glm::mat4 renderProj_{1.0f};

    std::unique_ptr<SDL3Initializer::SDL3Initializer> sdl_;

    bool isFullscreen_{false};
    bool isMaximized_{false};
    bool showOverlay_{true};
    bool tonemapEnabled_{false};

    std::chrono::steady_clock::time_point lastFrameTime_;

    std::unique_ptr<VulkanRenderer> renderer_;
};