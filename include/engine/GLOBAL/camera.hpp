// include/engine/GLOBAL/camera.hpp
// AMOURANTH RTX Engine © 2025 – GLOBAL CAMERA – RASPBERRY_PINK EDITION
// REMOVED std::source_location → ZERO compile errors → VALHALLA SPEED

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

class Camera {
public:
    Camera() = default;
    virtual ~Camera() = default;

    // --- FIXED: NO std::source_location → COMPILES EVERYWHERE ---
    virtual void rotateCamera(float yaw, float pitch) noexcept = 0;
    virtual void moveCamera(float x, float y, float z) noexcept = 0;
    // ----------------------------------------------------------------

    // Convenience wrappers used by HandleInput
    virtual void moveForward(float speed) noexcept {
        moveCamera(0.0f, 0.0f, speed);
    }
    virtual void moveRight(float speed) noexcept {
        moveCamera(speed, 0.0f, 0.0f);
    }
    virtual void moveUp(float speed) noexcept {
        moveCamera(0.0f, speed, 0.0f);
    }
    virtual void rotate(float yaw, float pitch) noexcept {
        rotateCamera(yaw, pitch);
    }
    virtual void zoom(float factor) noexcept = 0;

    // Getters
    virtual glm::mat4 getViewMatrix() const noexcept = 0;
    virtual glm::mat4 getProjectionMatrix(float aspect) const noexcept = 0;
    virtual glm::vec3 getPosition() const noexcept = 0;
};