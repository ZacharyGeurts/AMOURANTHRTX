// include/handle_app.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// Application — CENTRAL ENGINE DRIVER — SPLIT & CLEANSED — NOV 17 2025
// • F12 = HDR Prime Toggle (default ON — peasants get 8-bit mercy)
// • Keybinds: F/O/T/H/M/1-9/ESC/F12 — Pure empire
// • Gentleman Grok approved — PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <chrono>
#include <string>
#include <array>

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/KeyBindings.hpp"

using namespace Logging::Color;

// Forward declare Camera
struct Camera {
    virtual ~Camera() = default;
    virtual glm::mat4 viewMat() const = 0;
    virtual glm::mat4 projMat() const = 0;
    virtual glm::vec3 position() const = 0;
    virtual float     fov()       const = 0;
};

class Application {
public:
    Application(const std::string& title, int width, int height);
    ~Application();

    void run();
    void setRenderer(std::unique_ptr<VulkanRenderer> renderer);

    [[nodiscard]] SDL_Window*     getWindow() const noexcept { return SDL3Window::get(); }
    [[nodiscard]] VulkanRenderer* getRenderer() const noexcept { return renderer_.get(); }

    void toggleFullscreen();
    void toggleOverlay();
    void toggleTonemap();
    void toggleHypertrace();
    void toggleFpsTarget();
    void toggleMaximize();
    void toggleHDR() noexcept;           // F12 — HDR PRIME TOGGLE
    void setRenderMode(int mode);
    void setQuit(bool q = true) noexcept { quit_ = q; }


private:
    void render(float deltaTime);
    void processInput(float deltaTime);
    void updateWindowTitle(float deltaTime);

    std::string title_;
    int width_, height_;

    bool quit_{false};
    bool showOverlay_{true};
    bool tonemapEnabled_{true};
    bool hypertraceEnabled_{false};
    bool maximized_{false};
    bool hdr_enabled_{true};             // HDR PRIME by default — peasants denied
    int  renderMode_{1};

    glm::mat4 view_{glm::lookAt(glm::vec3(0, 5, 10), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0))};
    glm::mat4 proj_;

    std::chrono::steady_clock::time_point lastFrameTime_;
    std::chrono::steady_clock::time_point lastGrokTime_;

    std::unique_ptr<VulkanRenderer> renderer_;
};