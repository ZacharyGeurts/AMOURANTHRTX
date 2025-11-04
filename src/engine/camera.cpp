// src/engine/camera.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// Licensed under CC BY-NC 4.0

#include "engine/camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
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
      userData_(nullptr)
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
    return glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_);
}

// ---------------------------------------------------------------------
// Getters / Setters
// ---------------------------------------------------------------------
int PerspectiveCamera::getMode() const { return mode_; }
glm::vec3 PerspectiveCamera::getPosition() const { return position_; }

void PerspectiveCamera::setPosition(const glm::vec3& position) {
    position_ = position;
    updateCameraVectors();
}

void PerspectiveCamera::setOrientation(float yaw, float pitch) {
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, -89.0f, 89.0f);
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
    position_ += glm::normalize(glm::cross(front_, up_)) * speed * movementSpeed_;
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
void PerspectiveCamera::setFOV(float fov) {
    fov_ = std::clamp(fov, 10.0f, 120.0f);
}

float PerspectiveCamera::getFOV() const { return fov_; }

// ---------------------------------------------------------------------
// Mode & Aspect
// ---------------------------------------------------------------------
void PerspectiveCamera::setMode(int mode) { mode_ = mode; }
void PerspectiveCamera::setAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }

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
    glm::vec3 right = glm::normalize(glm::cross(front_, up_));
    position_ += dx * right + dy * up_ + dz * front_;
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
    LOG_INFO_CAT("CAMERA", "{}Zoom {} to FOV={:.1f}°{}", ARCTIC_CYAN,
                 zoomIn ? "IN" : "OUT", fov_, RESET);
}

// ---------------------------------------------------------------------
// Zoom (float factor) – for HandleInput
// ---------------------------------------------------------------------
void PerspectiveCamera::zoom(float factor) {
    fov_ = std::clamp(fov_ * factor, 10.0f, 120.0f);
    LOG_INFO_CAT("CAMERA", "{}Zoom factor={:.2f} to FOV={:.1f}°{}", ARCTIC_CYAN,
                 factor, fov_, RESET);
}

// ---------------------------------------------------------------------
// User Data
// ---------------------------------------------------------------------
void PerspectiveCamera::setUserData(void* data) { userData_ = data; }
void* PerspectiveCamera::getUserData() const { return userData_; }

// ---------------------------------------------------------------------
// Private: Recompute vectors
// ---------------------------------------------------------------------
void PerspectiveCamera::updateCameraVectors() {
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    direction.y = sin(glm::radians(pitch_));
    direction.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front_ = glm::normalize(direction);

    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(front_, worldUp));
    up_ = glm::normalize(glm::cross(right, front_));
}