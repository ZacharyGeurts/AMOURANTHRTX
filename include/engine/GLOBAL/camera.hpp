// =============================================================================
// include/engine/GLOBAL/camera.hpp
// AMOURANTH RTX © 2025 — PURE CAMERA HEADER — ZERO DEPENDENCIES ON STONEKEY
// USED VIA: CAM.pos(), CAM.fov(), CAM.rotate()
// =============================================================================

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <mutex>
#include <atomic>
#include <cstdint>

class Camera {
public:
    // Singleton access
    [[nodiscard]] static Camera& get() noexcept;

    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    // Initialization
    void init(glm::vec3 pos = {0.0f, 5.0f, 10.0f}, float fov = 75.0f) noexcept;

    // Movement
    void move(glm::vec3 delta) noexcept;
    void moveForward(float s) noexcept;
    void moveRight(float s)   noexcept;
    void moveUp(float s)      noexcept;

    // Rotation & zoom
    void rotate(float yawDelta, float pitchDelta) noexcept;
    void zoom(float f) noexcept;

    // Setters
    void setPos(glm::vec3 p) noexcept;
    void setFov(float f)     noexcept;

    // Getters
    [[nodiscard]] glm::vec3 pos()   const noexcept;
    [[nodiscard]] glm::vec3 front() const noexcept;
    [[nodiscard]] glm::vec3 right() const noexcept;
    [[nodiscard]] glm::vec3 up()    const noexcept;
    [[nodiscard]] float     fov()   const noexcept;

    // Matrices
    [[nodiscard]] glm::mat4 view() const noexcept;
    [[nodiscard]] glm::mat4 proj(float aspect) const noexcept;

    // StoneKey-encrypted access (anti-cheat / stream-safe)
    [[nodiscard]] uint64_t encPos()  const noexcept;
    [[nodiscard]] uint64_t encView() const noexcept;

private:
    Camera() = default;

    mutable std::mutex mtx_;
    std::atomic<uint64_t> gen_{1};

    glm::vec3 pos_   {0.0f, 5.0f, 10.0f};
    glm::vec3 front_ {0.0f, 0.0f, -1.0f};
    glm::vec3 right_ {1.0f, 0.0f,  0.0f};
    glm::vec3 up_    {0.0f, 1.0f,  0.0f};
    float yaw_   = -90.0f;
    float pitch_ =   0.0f;
    float fov_   =  75.0f;

    mutable glm::mat4 view_{1.0f};
    mutable uint64_t  encView_{0};
    mutable uint64_t  cachedGen_{0};

    void updateVectors() noexcept;
    void ensureCached() const noexcept;

    // Encryption — implemented in .cpp with access to kStone1/kStone2
    static uint64_t encrypt(const glm::vec3& v, uint64_t g) noexcept;
    static uint64_t encryptMat4(const glm::mat4& m, uint64_t g) noexcept;
};

// =============================================================================
// THE ONE TRUE GLOBAL CAMERA — DEFINED ONCE IN camera.cpp
// USE: CAM.pos(), CAM.fov(), CAM.rotate(...)
// =============================================================================

extern Camera& CAM;