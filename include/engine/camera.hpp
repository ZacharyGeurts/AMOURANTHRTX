// include/engine/camera.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts
// Licensed under CC BY-NC 4.0
// FINAL: Complete camera for ALL developers — beginner to expert
// • lazyInitCamera() → 1-line setup
// • Full FPS controls, pause, zoom
// • Runtime aspect via getProjectionMatrix(aspect)
// • Safe userData, app/renderer access
// • Pure virtual interface for future OrthoCamera, etc.

#pragma once
#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <source_location>

// Forward declarations
class Application;
namespace VulkanRTX { class VulkanRenderer; }

namespace VulkanRTX {

class Camera {
public:
    virtual ~Camera() = default;

    // Core matrices
    virtual glm::mat4 getViewMatrix() const = 0;
    virtual glm::mat4 getProjectionMatrix() const = 0;
    virtual glm::mat4 getProjectionMatrix(float aspectRatio) const = 0;  // ← EXPERT

    // State
    virtual int       getMode() const = 0;
    virtual glm::vec3 getPosition() const = 0;
    virtual void      setPosition(const glm::vec3& pos) = 0;

    // Orientation
    virtual void setOrientation(float yaw, float pitch) = 0;

    // Update
    virtual void update(float deltaTime) = 0;

    // Movement
    virtual void moveForward(float speed) = 0;
    virtual void moveRight(float speed) = 0;
    virtual void moveUp(float speed) = 0;

    // Rotation
    virtual void rotate(float yawDelta, float pitchDelta) = 0;

    // FOV
    virtual void  setFOV(float fov) = 0;
    virtual float getFOV() const = 0;

    // Aspect & Mode
    virtual void  setMode(int mode) = 0;
    virtual void  setAspectRatio(float aspect) = 0;
    virtual float getAspectRatio() const = 0;

    // Convenience
    virtual void moveCamera(float x, float y, float z,
                            std::source_location loc = std::source_location::current()) = 0;
    virtual void rotateCamera(float yaw, float pitch,
                              std::source_location loc = std::source_location::current()) = 0;

    // User-relative movement
    virtual void moveUserCam(float dx, float dy, float dz) = 0;

    // Controls
    virtual void togglePause() = 0;
    virtual void updateZoom(bool zoomIn) = 0;
    virtual void zoom(float factor) = 0;

    // Integration
    virtual void  setUserData(void* data) = 0;
    virtual void* getUserData() const = 0;
    virtual Application*     getApp() const { return nullptr; }
    virtual VulkanRenderer*  getRenderer() const { return nullptr; }
};

class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(float fov = 60.0f,
                      float aspectRatio = 16.0f / 9.0f,
                      float nearPlane = 0.1f,
                      float farPlane = 1000.0f);

    // Core
    glm::mat4 getViewMatrix() const override;
    glm::mat4 getProjectionMatrix() const override;
    glm::mat4 getProjectionMatrix(float aspectRatio) const override;  // ← NEW

    // State
    int       getMode() const override;
    glm::vec3 getPosition() const override;
    void      setPosition(const glm::vec3& pos) override;

    void setOrientation(float yaw, float pitch) override;
    void update(float deltaTime) override;

    void moveForward(float speed) override;
    void moveRight(float speed) override;
    void moveUp(float speed) override;

    void rotate(float yawDelta, float pitchDelta) override;
    void setFOV(float fov) override;
    float getFOV() const override;

    void setMode(int mode) override;
    void setAspectRatio(float aspect) override;
    float getAspectRatio() const override;

    void moveCamera(float x, float y, float z,
                    std::source_location loc = std::source_location::current()) override;
    void rotateCamera(float yaw, float pitch,
                      std::source_location loc = std::source_location::current()) override;

    void moveUserCam(float dx, float dy, float dz) override;
    void togglePause() override;
    void updateZoom(bool zoomIn) override;
    void zoom(float factor) override;

    void  setUserData(void* data) override;
    void* getUserData() const override;
    Application*     getApp() const override;
    VulkanRenderer*  getRenderer() const override;

    // Public for lazy camera
    float yaw_ = -90.0f;
    float pitch_ = 0.0f;
    float aspectRatio_;

private:
    void updateCameraVectors();

    glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 front_    = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up_       = glm::vec3(0.0f, 1.0f, 0.0f);

    float fov_ = 60.0f;
    float nearPlane_ = 0.1f;
    float farPlane_ = 1000.0f;

    int   mode_ = 0;
    float movementSpeed_ = 2.5f;
    float mouseSensitivity_ = 0.1f;
    bool  isPaused_ = false;
    void* userData_ = nullptr;
};

} // namespace VulkanRTX

#endif // CAMERA_HPP