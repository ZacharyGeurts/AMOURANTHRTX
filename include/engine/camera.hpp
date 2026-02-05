// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <mutex>
#include <atomic>
#include <cmath>

#include "engine/ELLIE.hpp"
#include "engine/OptionsMenu.hpp"

// ────────────────────────────────────────────────
// Camera state (constexpr-friendly)
// ────────────────────────────────────────────────
struct CameraState {
    glm::vec3   position       {0.0f, 1.8f, 5.0f};
    glm::quat   orientation    {1.0f, 0.0f, 0.0f, 0.0f};
    float       fov            {75.0f};
    float       aperture       {2.8f};
    float       focusDistance  {8.0f};

    constexpr CameraState() = default;
    constexpr CameraState(const glm::vec3& pos, const glm::quat& ori, float f, float a, float fd) noexcept
        : position(pos), orientation(ori), fov(f), aperture(a), focusDistance(fd) {}
};

// ────────────────────────────────────────────────
// THE ONE TRUE CAMERA — thread-safe singleton
// ────────────────────────────────────────────────
class Camera final {
public:
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
    Camera(Camera&&) = delete;
    Camera& operator=(Camera&&) = delete;

    [[nodiscard]] static Camera& get() noexcept {
        static Camera instance;
        return instance;
    }

    // Reset to defaults from OptionsMenu
    void reset() noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        currentState_ = CameraState(
            Options::Camera::START_POSITION,
            glm::quat(glm::vec3(0.0f, glm::radians(-90.0f), 0.0f)),
            Options::Camera::DEFAULT_FOV,
            Options::Camera::DEFAULT_APERTURE,
            Options::Camera::DEFAULT_FOCUS_DISTANCE
        );
        targetState_ = currentState_;
        transitionProgress_ = 1.0f;
        invalidateCache();
        LOG_AMOURANTH("Camera reset — secure pink photons online");
    }

    // Smooth transition to target state
    void transitionTo(const CameraState& target, float durationSec = 1.5f) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        targetState_ = target;
        transitionStartTime_ = std::chrono::steady_clock::now();
        transitionDuration_  = std::chrono::duration<float>(durationSec);
        transitionProgress_  = 0.0f;
    }

    // Update transition (call every frame)
    void update(float dt) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        if (transitionProgress_ >= 1.0f) return;

        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - transitionStartTime_).count();
        transitionProgress_ = std::min(1.0f, elapsed / transitionDuration_.count());

        // Cubic ease-out
        float t = transitionProgress_;
        t = 1.0f - std::pow(1.0f - t, 3.0f);

        currentState_.position      = glm::mix(currentState_.position,      targetState_.position,      t);
        currentState_.orientation   = glm::slerp(currentState_.orientation, targetState_.orientation,   t);
        currentState_.fov           = glm::mix(currentState_.fov,           targetState_.fov,           t);
        currentState_.aperture      = glm::mix(currentState_.aperture,      targetState_.aperture,      t);
        currentState_.focusDistance = glm::mix(currentState_.focusDistance, targetState_.focusDistance, t);

        invalidateCache();
    }

    // ────────────────────────────────────────────────
    // Secure setters (thread-safe)
    // ────────────────────────────────────────────────
    void setPosition(const glm::vec3& p, bool instant = false) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        if (instant) {
            currentState_.position = targetState_.position = p;
        } else {
            targetState_.position = p;
            transitionProgress_ = 0.0f;
        }
        invalidateCache();
    }

    void setFov(float newFov, bool instant = true) noexcept {
        newFov = glm::clamp(newFov, 1.0f, 120.0f);
        std::lock_guard<std::mutex> lock(mtx_);
        if (instant) {
            currentState_.fov = targetState_.fov = newFov;
        } else {
            targetState_.fov = newFov;
            transitionProgress_ = 0.0f;
        }
        invalidateCache();
    }

    void setAperture(float a) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        currentState_.aperture = targetState_.aperture = glm::max(a, 0.01f);
    }

    void setFocusDistance(float d) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        currentState_.focusDistance = targetState_.focusDistance = glm::max(d, 0.1f);
    }

    // ────────────────────────────────────────────────
    // Cinematic movement helpers
    // ────────────────────────────────────────────────
    void zoom(float delta) noexcept {
        float newFov = currentState_.fov - delta * Options::Camera::ZOOM_SENSITIVITY;
        setFov(newFov, false);  // smooth transition
    }

    void dolly(float amount, float dt = 1.0f) noexcept {
        moveForward(amount * Options::Camera::DOLLY_SPEED * dt);
    }

    void crane(float amount, float dt = 1.0f) noexcept {
        moveUp(amount * Options::Camera::CRANE_SPEED * dt);
    }

    void rackFocus(float target, float duration = 2.0f) noexcept {
        targetState_.focusDistance = glm::max(target, 0.1f);
        transitionDuration_ = std::chrono::duration<float>(duration);
        transitionProgress_ = 0.0f;
    }

    // Internal movement helpers
    void moveForward(float amount) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        currentState_.position += currentState_.orientation * glm::vec3(0, 0, -amount);
        targetState_.position = currentState_.position;
        invalidateCache();
    }

    void moveRight(float amount) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        currentState_.position += currentState_.orientation * glm::vec3(amount, 0, 0);
        targetState_.position = currentState_.position;
        invalidateCache();
    }

    void moveUp(float amount) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        currentState_.position += glm::vec3(0, amount, 0);
        targetState_.position = currentState_.position;
        invalidateCache();
    }

    void lookAt(const glm::vec3& target, bool instant = false) noexcept {
        glm::vec3 dir = glm::normalize(target - currentState_.position);
        glm::quat ori = glm::quatLookAt(dir, glm::vec3(0,1,0));
        if (instant) {
            currentState_.orientation = targetState_.orientation = ori;
        } else {
            targetState_.orientation = ori;
            transitionProgress_ = 0.0f;
        }
        invalidateCache();
    }

    // ────────────────────────────────────────────────
    // Output matrices — cached & secure
    // ────────────────────────────────────────────────
    [[nodiscard]] glm::mat4 view() const noexcept {
        ensureCached();
        return viewCache_;
    }

    [[nodiscard]] glm::mat4 projection(float aspect) const noexcept {
        return glm::perspective(glm::radians(currentState_.fov), aspect, 0.05f, 20000.0f);
    }

    // ────────────────────────────────────────────────
    // Getters — const, thread-safe
    // ────────────────────────────────────────────────
    [[nodiscard]] glm::vec3 position()    const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.position; }
    [[nodiscard]] glm::vec3 forward()     const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.orientation * glm::vec3(0,0,-1); }
    [[nodiscard]] glm::vec3 right()       const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.orientation * glm::vec3(1,0,0); }
    [[nodiscard]] glm::vec3 up()          const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.orientation * glm::vec3(0,1,0); }
    [[nodiscard]] float     fov()         const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.fov; }
    [[nodiscard]] float     aperture()    const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.aperture; }
    [[nodiscard]] float     focusDistance() const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.focusDistance; }

private:
    Camera() { reset(); }

    mutable std::mutex mtx_;
    mutable glm::mat4  viewCache_{1.0f};
    mutable std::atomic<uint64_t> generation_{0};
    mutable uint64_t cachedGeneration_{0};

    CameraState currentState_;
    CameraState targetState_;
    std::chrono::steady_clock::time_point transitionStartTime_;
    std::chrono::duration<float>          transitionDuration_{1.5f};
    float                                 transitionProgress_{1.0f};

    void invalidateCache() noexcept {
        generation_.fetch_add(1, std::memory_order_release);
    }

    void ensureCached() const noexcept {
        uint64_t gen = generation_.load(std::memory_order_acquire);
        if (cachedGeneration_ != gen) {
            std::lock_guard<std::mutex> lock(mtx_);
            if (cachedGeneration_ != gen) {
                glm::vec3 fwd = currentState_.orientation * glm::vec3(0,0,-1);
                glm::vec3 up  = currentState_.orientation * glm::vec3(0,1,0);
                viewCache_ = glm::lookAt(currentState_.position, currentState_.position + fwd, up);
                cachedGeneration_ = gen;
            }
        }
    }
};

// ────────────────────────────────────────────────
// Global access — secure, no macro hell
// ────────────────────────────────────────────────
inline Camera& CAM = Camera::get();

// Inline wrappers — now correctly call existing members
inline void CAM_RESET()                         { CAM.reset(); }
inline glm::vec3 CAM_POS()                      { return CAM.position(); }
inline glm::vec3 CAM_FWD()                      { return CAM.forward(); }
inline void CAM_LOOK_AT(const glm::vec3& t)     { CAM.lookAt(t); }
inline void CAM_DOLLY(float d, float dt=1.0f)   { CAM.dolly(d, dt); }
inline void CAM_CRANE(float h, float dt=1.0f)   { CAM.crane(h, dt); }
inline void CAM_ZOOM(float d)                   { CAM.zoom(d); }
inline void CAM_FOV(float f)                    { CAM.setFov(f); }
inline void CAM_FOCUS(float d)                  { CAM.setFocusDistance(d); }
inline void CAM_APERTURE(float a)               { CAM.setAperture(a); }
inline void CAM_RACK(float t, float dur=2.0f)   { CAM.rackFocus(t, dur); }