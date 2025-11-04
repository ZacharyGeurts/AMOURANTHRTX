// include/engine/camera.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// Licensed under CC BY-NC 4.0

#pragma once
#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <source_location>

namespace VulkanRTX {

class Camera {
public:
    virtual ~Camera() = default;

    virtual glm::mat4 getViewMatrix() const = 0;
    virtual glm::mat4 getProjectionMatrix() const = 0;

    virtual int getMode() const = 0;
    virtual glm::vec3 getPosition() const = 0;
    virtual void setPosition(const glm::vec3& position) = 0;

    virtual void setOrientation(float yaw, float pitch) = 0;
    virtual void update(float deltaTime) = 0;

    virtual void moveForward(float speed) = 0;
    virtual void moveRight(float speed) = 0;
    virtual void moveUp(float speed) = 0;

    virtual void rotate(float yawDelta, float pitchDelta) = 0;
    virtual void setFOV(float fov) = 0;
    virtual float getFOV() const = 0;

    virtual void setMode(int mode) = 0;
    virtual void setAspectRatio(float aspectRatio) = 0;

    virtual void moveCamera(float x, float y, float z,
                            std::source_location loc = std::source_location::current()) = 0;
    virtual void rotateCamera(float yaw, float pitch,
                              std::source_location loc = std::source_location::current()) = 0;

    virtual void moveUserCam(float dx, float dy, float dz) = 0;
    virtual void togglePause() = 0;
    virtual void updateZoom(bool zoomIn) = 0;

    virtual void setUserData(void* data) = 0;
    virtual void* getUserData() const = 0;

    // ADD: Smooth zoom via FOV
    virtual void zoom(float factor) = 0;
};

class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(float fov = 60.0f,
                      float aspectRatio = 16.0f / 9.0f,
                      float nearPlane = 0.1f,
                      float farPlane = 1000.0f);

    glm::mat4 getViewMatrix() const override;
    glm::mat4 getProjectionMatrix() const override;

    int getMode() const override;
    glm::vec3 getPosition() const override;
    void setPosition(const glm::vec3& position) override;

    void setOrientation(float yaw, float pitch) override;
    void update(float deltaTime) override;

    void moveForward(float speed) override;
    void moveRight(float speed) override;
    void moveUp(float speed) override;

    void rotate(float yawDelta, float pitchDelta) override;
    void setFOV(float fov) override;
    float getFOV() const override;

    void setMode(int mode) override;
    void setAspectRatio(float aspectRatio) override;

    void rotateCamera(float yaw, float pitch,
                      std::source_location loc = std::source_location::current()) override;
    void moveCamera(float x, float y, float z,
                    std::source_location loc = std::source_location::current()) override;

    void moveUserCam(float dx, float dy, float dz) override;
    void togglePause() override;
    void updateZoom(bool zoomIn) override;

    void setUserData(void* data) override;
    void* getUserData() const override;

    // IMPLEMENTED: Smooth FOV zoom
    void zoom(float factor) override;

    // --- PUBLIC FOR renderFrame() MOVEMENT DETECTION ---
    float yaw_;
    float pitch_;

private:
    void updateCameraVectors();

    glm::vec3 position_;
    glm::vec3 front_;
    glm::vec3 up_;

    float fov_;
    float aspectRatio_;
    float nearPlane_;
    float farPlane_;

    int mode_;
    float movementSpeed_;
    float mouseSensitivity_;
    bool isPaused_;
    void* userData_;
};

} // namespace VulkanRTX

#endif // CAMERA_HPP