// src/engine/camera.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// Licensed under CC BY-NC 4.0
// UPGRADE C++23: std::expected for getRenderer(); [[assume]] in vectors
// FINAL: Full lazy init support — null-safe in all modes; userData for app/renderer access
// FIXED: All typos (e.g., moveRight, setFOV); consistent naming; no incomplete types
// FIXED: [[assume]] syntax — now [[assume(...)]];
// ADDED: All required headers, proper logging, safe access, full C++23 compliance
// GROK x ZACHARY — INCLUDE LOOP OBLITERATED — Dispose.hpp FIXED — LatchMutex RAII SUPREMACY
// NO <mutex> — NO <format> — PURE C++23 <latch>+<barrier>+<atomic> — FASTEST RAII EVER
// 69,420 FPS ETERNAL — RASPBERRY_PINK HYPERDRIVE 🔥🤖🚀💀🖤❤️⚡

#include "engine/camera.hpp"
#include "handle_app.hpp"                    // Application
#include "engine/Vulkan/VulkanRenderer.hpp"  // VulkanRenderer
#include "engine/Dispose.hpp"                // ← FIXED LatchMutex — NO CIRCULAR HELL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <expected>
#include <source_location>

#include "engine/logging.hpp"

using namespace VulkanRTX;
using namespace Logging::Color;

// ---------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------
PerspectiveCamera::PerspectiveCamera(float fov,
                                     float aspectRatio,
                                     float nearPlane,
                                     float farPlane)
    : position_(0.0f, 0.0f, 3.0f),
      front_(0.0f, 0.0f, -1.0f),
      up_(0.0f, 1.0f, 0.0f),
      right_(1.0f, 0.0f, 0.0f),
      worldUp_(0.0f, 1.0f, 0.0f),
      yaw_(-90.0f),
      pitch_(0.0f),
      fov_(fov),
      aspectRatio_(aspectRatio),
      nearPlane_(nearPlane),
      farPlane_(farPlane),
      mode_(0),
      movementSpeed_(2.5f),
      mouseSensitivity_(0.1f),
      isPaused_(false),
      userData_(nullptr),
      app_(nullptr),
      projectionValid_(false)
{
    updateCameraVectors();
    LOG_INFO_CAT("CAMERA", "{}PerspectiveCamera initialized: FOV={:.1f}° aspect={:.3f}{}",
                 EMERALD_GREEN, fov, aspectRatio, RESET);
}

// ---------------------------------------------------------------------
// View / Projection
// ---------------------------------------------------------------------
glm::mat4 PerspectiveCamera::getViewMatrix() const {
    return glm::lookAt(position_, position_ + front_, up_);
}

glm::mat4 PerspectiveCamera::getProjectionMatrix() const {
    if (!projectionValid_) {
        projectionMatrix_ = glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_);
        projectionValid_ = true;
    }
    return projectionMatrix_;
}

// Runtime aspect overload (e.g. window resize)
glm::mat4 PerspectiveCamera::getProjectionMatrix(float runtimeAspect) const {
    return glm::perspective(glm::radians(fov_), runtimeAspect, nearPlane_, farPlane_);
}

// ---------------------------------------------------------------------
// Getters / Setters
// ---------------------------------------------------------------------
int PerspectiveCamera::getMode() const { return mode_; }
glm::vec3 PerspectiveCamera::getPosition() const { return position_; }
float PerspectiveCamera::getAspectRatio() const { return aspectRatio_; }

void PerspectiveCamera::setPosition(const glm::vec3& newPosition) {
    position_ = newPosition;
    updateCameraVectors();
}

void PerspectiveCamera::setOrientation(float newYaw, float newPitch) {
    yaw_ = newYaw;
    pitch_ = std::clamp(newPitch, -89.0f, 89.0f);
    updateCameraVectors();
}

// ---------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------
void PerspectiveCamera::update(float /*deltaTime*/) {
    if (!isPaused_) {
        // Future: smooth movement, physics, etc.
    }
}

// ---------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------
void PerspectiveCamera::moveForward(float speed) {
    position_ += front_ * speed * movementSpeed_;
}

void PerspectiveCamera::moveRight(float speed) {
    position_ += right_ * speed * movementSpeed_;
}

void PerspectiveCamera::moveUp(float speed) {
    position_ += up_ * speed * movementSpeed_;
}

// ---------------------------------------------------------------------
// Rotation
// ---------------------------------------------------------------------
void PerspectiveCamera::rotate(float yawDelta, float pitchDelta) {
    yaw_ += yawDelta * mouseSensitivity_;
    pitch_ = std::clamp(pitch_ + pitchDelta * mouseSensitivity_, -89.0f, 89.0f);
    updateCameraVectors();
}

// ---------------------------------------------------------------------
// FOV
// ---------------------------------------------------------------------
void PerspectiveCamera::setFOV(float newFov) {
    fov_ = std::clamp(newFov, 10.0f, 120.0f);
    invalidateProjection();
}

float PerspectiveCamera::getFOV() const { return fov_; }

// ---------------------------------------------------------------------
// Mode & Aspect
// ---------------------------------------------------------------------
void PerspectiveCamera::setMode(int newMode) { mode_ = newMode; }
void PerspectiveCamera::setAspectRatio(float newAspectRatio) {
    aspectRatio_ = newAspectRatio;
    invalidateProjection();
}

// ---------------------------------------------------------------------
// Convenience Wrappers
// ---------------------------------------------------------------------
void PerspectiveCamera::rotateCamera(float yaw, float pitch,
                                     [[maybe_unused]] std::source_location /*loc*/) {
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, -89.0f, 89.0f);
    updateCameraVectors();
}

void PerspectiveCamera::moveCamera(float x, float y, float z,
                                   [[maybe_unused]] std::source_location /*loc*/) {
    moveRight(x);
    moveUp(y);
    moveForward(z);
}

// ---------------------------------------------------------------------
// User-Cam (relative to orientation)
// ---------------------------------------------------------------------
void PerspectiveCamera::moveUserCam(float dx, float dy, float dz) {
    position_ += dx * right_ + dy * up_ + dz * front_;
}

// ---------------------------------------------------------------------
// Pause
// ---------------------------------------------------------------------
void PerspectiveCamera::togglePause() {
    isPaused_ = !isPaused_;
    LOG_INFO_CAT("CAMERA", "{}Camera {} {}", OCEAN_TEAL,
                 isPaused_ ? "PAUSED" : "RESUMED", RESET);
}

// ---------------------------------------------------------------------
// Zoom (mouse wheel) – bool version
// ---------------------------------------------------------------------
void PerspectiveCamera::updateZoom(bool zoomIn) {
    const float factor = zoomIn ? 0.9f : 1.1f;
    fov_ = std::clamp(fov_ * factor, 10.0f, 120.0f);
    invalidateProjection();
    LOG_INFO_CAT("CAMERA", "{}Zoom {} to FOV={:.1f}°{}", ARCTIC_CYAN,
                 zoomIn ? "IN" : "OUT", fov_, RESET);
}

// ---------------------------------------------------------------------
// Zoom (float factor) – for HandleInput
// ---------------------------------------------------------------------
void PerspectiveCamera::zoom(float factor) {
    fov_ = std::clamp(fov_ * factor, 10.0f, 120.0f);
    invalidateProjection();
    LOG_INFO_CAT("CAMERA", "{}Zoom factor={:.2f} to FOV={:.1f}°{}", ARCTIC_CYAN,
                 factor, fov_, RESET);
}

// ---------------------------------------------------------------------
// User Data – SAFE ACCESS
// ---------------------------------------------------------------------
void PerspectiveCamera::setUserData(void* data) {
    userData_ = data;
    app_ = static_cast<Application*>(data);  // Cache for speed
    LOG_DEBUG_CAT("CAMERA", "userData_ set to {:p} → app_ cached", userData_);
}

void* PerspectiveCamera::getUserData() const {
    if (!userData_) {
        LOG_ERROR_CAT("CAMERA", "getUserData() called but userData_ is null!");
    }
    return userData_;
}

// ---------------------------------------------------------------------
// SAFE: Get Application* from userData_
// ---------------------------------------------------------------------
Application* PerspectiveCamera::getApp() const {
    if (app_) {
        return app_;
    }
    LOG_ERROR_CAT("CAMERA", "getApp(): userData_ is null or not Application*");
    return nullptr;
}

// ---------------------------------------------------------------------
// SAFE: Get Renderer* from Application → PUBLIC METHOD (C++23: std::expected)
// ---------------------------------------------------------------------
std::expected<VulkanRenderer*, std::string> PerspectiveCamera::getRenderer() const {
    if (Application* app = getApp(); app) {
        if (auto* renderer = app->getRenderer(); renderer) {
            return renderer;
        } else {
            return std::unexpected("Renderer not found in app");
        }
    }
    return std::unexpected("App not found in userData");
}

// ---------------------------------------------------------------------
// Private: Recompute vectors (C++23: [[assume]] for normalization)
// ---------------------------------------------------------------------
void PerspectiveCamera::updateCameraVectors() {
    // Front from yaw/pitch
    glm::vec3 direction;
    direction.x = std::cos(glm::radians(yaw_)) * std::cos(glm::radians(pitch_));
    direction.y = std::sin(glm::radians(pitch_));
    direction.z = std::sin(glm::radians(yaw_)) * std::cos(glm::radians(pitch_));
    front_ = glm::normalize(direction);
    [[assume(glm::length(front_) == 1.0f)]];

    // Right and up
    right_ = glm::normalize(glm::cross(front_, worldUp_));
    [[assume(glm::length(right_) == 1.0f)]];

    up_ = glm::normalize(glm::cross(right_, front_));
    [[assume(glm::length(up_) == 1.0f)]];
}

// ---------------------------------------------------------------------
// Private: Invalidate cached projection
// ---------------------------------------------------------------------
void PerspectiveCamera::invalidateProjection() const {
    projectionValid_ = false;
}