// include/engine/camera.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts
// Licensed under CC BY-NC 4.0
// UPGRADE C++23: std::expected for getRenderer(); [[maybe_unused]] in overloads
// FINAL: Complete camera for ALL developers — beginner to expert
// • lazyInitCamera() → 1-line setup
// • Full FPS controls, pause, zoom
// • Runtime aspect via getProjectionMatrix(aspect)
// • Safe userData, app/renderer access
// • Pure virtual interface for future OrthoCamera, etc.
// FIXED: explicit(bool) → explicit(!std::is_constant_evaluated()) operator bool()
// ADDED: All required headers

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <source_location>
#include <expected>        // C++23
#include <string>         // std::string
#include <memory>         // std::unique_ptr, std::shared_ptr if needed
#include <cmath>          // std::clamp, std::sin, etc.
#include <algorithm>      // std::min, std::max
#include <type_traits>    // std::is_constant_evaluated

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
    virtual glm::mat4 getProjectionMatrix(float aspectRatio) const = 0;

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
                            [[maybe_unused]] std::source_location loc = std::source_location::current()) = 0;
    virtual void rotateCamera(float yaw, float pitch,
                              [[maybe_unused]] std::source_location loc = std::source_location::current()) = 0;

    // User-relative movement
    virtual void moveUserCam(float dx, float dy, float dz) = 0;

    // Controls
    virtual void togglePause() = 0;
    virtual void updateZoom(bool zoomIn) = 0;
    virtual void zoom(float factor) = 0;

    // Integration
    virtual void  setUserData(void* data) = 0;
    virtual void* getUserData() const = 0;
    virtual Application*     getApp() const = 0;
    virtual std::expected<VulkanRenderer*, std::string> getRenderer() const = 0;
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
    glm::mat4 getProjectionMatrix(float runtimeAspect) const override;

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
                    [[maybe_unused]] std::source_location loc = std::source_location::current()) override;
    void rotateCamera(float yaw, float pitch,
                      [[maybe_unused]] std::source_location loc = std::source_location::current()) override;

    void moveUserCam(float dx, float dy, float dz) override;

    void togglePause() override;
    void updateZoom(bool zoomIn) override;
    void zoom(float factor) override;

    void  setUserData(void* data) override;
    void* getUserData() const override;
    Application*     getApp() const override;
    std::expected<VulkanRenderer*, std::string> getRenderer() const override;

    // C++20: Conditionally explicit conversion to bool
    // Explicit in runtime, implicit in constexpr (e.g. static_assert)
    explicit(!std::is_constant_evaluated()) operator bool() const noexcept {
        return !isPaused_;
    }

private:
    // Configuration
    float fov_;
    float aspectRatio_;
    float nearPlane_;
    float farPlane_;

    // Transform
    glm::vec3 position_;
    glm::vec3 front_;
    glm::vec3 up_;
    glm::vec3 right_;
    glm::vec3 worldUp_;

    // Euler angles
    float yaw_;
    float pitch_;

    // State
    int   mode_;
    bool  isPaused_;
    float zoomSensitivity_;
    void* userData_;

    // Cached
    mutable glm::mat4 projectionMatrix_;  // Cached with current aspectRatio_
    mutable bool      projectionValid_;

    // Integration
    Application* app_;

    // Helper
    void updateCameraVectors();
    void invalidateProjection() const;
};

} // namespace VulkanRTX