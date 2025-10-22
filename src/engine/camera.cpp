// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Camera implementation for 3D rendering
// Dependencies: GLM, C++20 standard library
// Supported platforms: Linux, Windows
// Zachary Geurts 2025

#include "engine/camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

PerspectiveCamera::PerspectiveCamera(float fov, float aspectRatio, float nearPlane, float farPlane)
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
      mouseSensitivity_(0.1f) {
    updateCameraVectors();
}

glm::mat4 PerspectiveCamera::getViewMatrix() const {
    return glm::lookAt(position_, position_ + front_, up_);
}

glm::mat4 PerspectiveCamera::getProjectionMatrix() const {
    return glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_);
}

int PerspectiveCamera::getMode() const {
    return mode_;
}

glm::vec3 PerspectiveCamera::getPosition() const {
    return position_;
}

void PerspectiveCamera::setPosition(const glm::vec3& position) {
    position_ = position;
    updateCameraVectors();
}

void PerspectiveCamera::setOrientation(float yaw, float pitch) {
    yaw_ = yaw;
    pitch_ = pitch;
    updateCameraVectors();
}

void PerspectiveCamera::update([[maybe_unused]] float deltaTime) {
    // No-op; can be extended for dynamic behavior
}

void PerspectiveCamera::moveForward(float speed) {
    position_ += front_ * speed * movementSpeed_;
}

void PerspectiveCamera::moveRight(float speed) {
    position_ += glm::normalize(glm::cross(front_, up_)) * speed * movementSpeed_;
}

void PerspectiveCamera::moveUp(float speed) {
    position_ += up_ * speed * movementSpeed_;
}

void PerspectiveCamera::rotate(float yawDelta, float pitchDelta) {
    yaw_ += yawDelta * mouseSensitivity_;
    pitch_ = std::clamp(pitch_ + pitchDelta * mouseSensitivity_, -89.0f, 89.0f);
    updateCameraVectors();
}

void PerspectiveCamera::setFOV(float fov) {
    fov_ = std::clamp(fov, 10.0f, 120.0f);
}

float PerspectiveCamera::getFOV() const {
    return fov_;
}

void PerspectiveCamera::setMode(int mode) {
    mode_ = mode;
}

void PerspectiveCamera::rotateCamera(float yaw, float pitch, [[maybe_unused]] std::source_location loc) {
    rotate(yaw, pitch);
}

void PerspectiveCamera::moveCamera(float x, float y, float z, [[maybe_unused]] std::source_location loc) {
    moveRight(x);
    moveUp(y);
    moveForward(z);
}

void PerspectiveCamera::setAspectRatio(float aspectRatio) {
    aspectRatio_ = aspectRatio;
}

void PerspectiveCamera::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front.y = sin(glm::radians(pitch_));
    front.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front_ = glm::normalize(front);
    glm::vec3 right = glm::normalize(glm::cross(front_, glm::vec3(0.0f, 1.0f, 0.0f)));
    up_ = glm::normalize(glm::cross(right, front_));
}