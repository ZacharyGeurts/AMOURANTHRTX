#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Camera (Cinematic Orbiting Edition)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
// =============================================================================

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cmath>

#include "ELLIE.hpp"
#include "OptionsMenu.hpp"

// ────────────────────────────────────────────────
// Camera settings constants (for default view position and look offset)
// ────────────────────────────────────────────────
#define CAMERA_BASE_HEIGHT          4.5f
#define CAMERA_HEIGHT_SWING         2.8f
#define CAMERA_HEIGHT_FREQ          0.11f
#define CAMERA_BASE_DISTANCE        20.0f
#define CAMERA_DISTANCE_SWING       4.5f
#define CAMERA_DISTANCE_FREQ        0.08f
#define CAMERA_FOV_SCALE            1.72f
#define CAMERA_LOOK_AT_Y_OFFSET     -2.35f  // Look below horizon (toward ground/water)

// ────────────────────────────────────────────────
// Camera state (genesis-aware, transition-friendly)
// ────────────────────────────────────────────────
struct CameraState {
    glm::vec3   position       {Options::Camera::START_POSITION};
    glm::quat   orientation    {1.0f, 0.0f, 0.0f, 0.0f};
    float       fov            {Options::Camera::DEFAULT_FOV};
    float       aperture       {Options::Camera::DEFAULT_APERTURE};
    float       focusDistance  {Options::Camera::DEFAULT_FOCUS_DISTANCE};

    constexpr CameraState() = default;
    constexpr CameraState(const glm::vec3& pos, const glm::quat& ori, float f, float a, float fd) noexcept
        : position(pos), orientation(ori), fov(f), aperture(a), focusDistance(fd) {}
};

// ────────────────────────────────────────────────
// THE ONE TRUE CAMERA — thread-safe, genesis-timed singleton
// All motion relative to sealed genesis clock
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

    void reset() noexcept {
        std::lock_guard<std::mutex> lock(mtx_);

        glm::vec3 startPos = glm::vec3(0.0f, CAMERA_BASE_HEIGHT, CAMERA_BASE_DISTANCE);

        // Look lower — toward the water / ball landing zone
        glm::vec3 lookTarget = glm::vec3(0.0f, CAMERA_LOOK_AT_Y_OFFSET, 0.0f);
        glm::vec3 dir = glm::normalize(lookTarget - startPos);
        glm::quat ori = glm::quatLookAt(dir, glm::vec3(0.0f, 1.0f, 0.0f));

        currentState_ = CameraState(
            startPos,
            ori,
            Options::Camera::DEFAULT_FOV,
            Options::Camera::DEFAULT_APERTURE,
            Options::Camera::DEFAULT_FOCUS_DISTANCE
        );
        targetState_ = currentState_;
        prevPosition_ = currentState_.position;
        transitionProgress_ = 1.0f;
        transitionStartGenesis_ = TotalTime::get().seconds();
        invalidateCache();
    }

    // Smooth transition to target state (duration in real seconds)
    void transitionTo(const CameraState& target, float durationSec = 1.5f) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        targetState_ = target;
        transitionStartGenesis_ = TotalTime::get().seconds();
        transitionDurationSec_  = durationSec;
        transitionProgress_     = 0.0f;
    }

    // Update transition using genesis clock
    void update() noexcept {
        std::lock_guard<std::mutex> lock(mtx_);

        if (transitionProgress_ >= 1.0f) {
            prevPosition_ = currentState_.position;
            return;
        }

        double nowGenesis = TotalTime::get().seconds();
        double elapsed = nowGenesis - transitionStartGenesis_;
        transitionProgress_ = std::min(1.0f,
                              static_cast<float>(elapsed) /
                              static_cast<float>(transitionDurationSec_));

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
    // Secure setters
    // ────────────────────────────────────────────────
    void setPosition(const glm::vec3& p, bool instant = false) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        prevPosition_ = currentState_.position;
        if (instant) {
            currentState_.position = targetState_.position = p;
        } else {
            targetState_.position = p;
            transitionProgress_ = 0.0f;
            transitionStartGenesis_ = TotalTime::get().seconds();
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
            transitionStartGenesis_ = TotalTime::get().seconds();
        }
        invalidateCache();
    }

    void setAperture(float a) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        currentState_.aperture = targetState_.aperture = glm::max(a, 0.01f);
        invalidateCache();
    }

    void setFocusDistance(float d) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        currentState_.focusDistance = targetState_.focusDistance = glm::max(d, 0.1f);
        invalidateCache();
    }

    // ────────────────────────────────────────────────
    // Cinematic movement helpers
    // ────────────────────────────────────────────────
    void zoom(float delta) noexcept {
        float newFov = currentState_.fov - delta * Options::Camera::ZoomSensitivity;
        setFov(newFov, false);
    }

    void dolly(float amount) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        double now = TotalTime::get().seconds();
        float scaled = amount * Options::Camera::DollySpeed * static_cast<float>(now - lastUpdateGenesis_);
        prevPosition_ = currentState_.position;
        currentState_.position += currentState_.orientation * glm::vec3(0.0f, 0.0f, -scaled);
        targetState_.position = currentState_.position;
        lastUpdateGenesis_ = now;
        invalidateCache();
    }

    void crane(float amount) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        double now = TotalTime::get().seconds();
        float scaled = amount * Options::Camera::CraneSpeed * static_cast<float>(now - lastUpdateGenesis_);
        prevPosition_ = currentState_.position;
        currentState_.position += glm::vec3(0.0f, scaled, 0.0f);
        targetState_.position = currentState_.position;
        lastUpdateGenesis_ = now;
        invalidateCache();
    }

    void rackFocus(float target, float duration = 2.0f) noexcept {
        targetState_.focusDistance = glm::max(target, 0.1f);
        transitionStartGenesis_ = TotalTime::get().seconds();
        transitionDurationSec_ = duration;
        transitionProgress_ = 0.0f;
    }

    void moveForward(float amount) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        prevPosition_ = currentState_.position;
        currentState_.position += currentState_.orientation * glm::vec3(0.0f, 0.0f, -amount);
        targetState_.position = currentState_.position;
        invalidateCache();
    }

    void moveRight(float amount) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        prevPosition_ = currentState_.position;
        currentState_.position += currentState_.orientation * glm::vec3(amount, 0.0f, 0.0f);
        targetState_.position = currentState_.position;
        invalidateCache();
    }

    void moveUp(float amount) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        prevPosition_ = currentState_.position;
        currentState_.position += glm::vec3(0.0f, amount, 0.0f);
        targetState_.position = currentState_.position;
        invalidateCache();
    }

    void lookAt(const glm::vec3& target, bool instant = false) noexcept {
        glm::vec3 dir = glm::normalize(target - currentState_.position);
        glm::quat ori = glm::quatLookAt(dir, glm::vec3(0.0f, 1.0f, 0.0f));
        std::lock_guard<std::mutex> lock(mtx_);
        if (instant) {
            currentState_.orientation = targetState_.orientation = ori;
        } else {
            targetState_.orientation = ori;
            transitionProgress_ = 0.0f;
            transitionStartGenesis_ = TotalTime::get().seconds();
        }
        invalidateCache();
    }

    // ────────────────────────────────────────────────
    // Output matrices — cached
    // ────────────────────────────────────────────────
    [[nodiscard]] glm::mat4 view() const noexcept {
        ensureCached();
        return viewCache_;
    }

    [[nodiscard]] glm::mat4 projection(float aspect) const noexcept {
        if (Options::GameStyle::CurrentPerspective == Options::GameStyle::CameraPerspective::Orthographic2D) {
            return glm::ortho(-aspect, aspect, -1.0f, 1.0f, 0.1f, 100.0f);
        }
        return glm::perspective(glm::radians(currentState_.fov), aspect, 0.05f, 20000.0f);
    }

    // ────────────────────────────────────────────────
    // Getters
    // ────────────────────────────────────────────────
    [[nodiscard]] glm::vec3 position()    const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.position; }
    [[nodiscard]] glm::vec3 prevPosition() const noexcept { std::lock_guard<std::mutex> l(mtx_); return prevPosition_; }
    [[nodiscard]] glm::quat orientation() const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.orientation; }
    [[nodiscard]] glm::vec3 forward()     const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.orientation * glm::vec3(0.0f, 0.0f, -1.0f); }
    [[nodiscard]] glm::vec3 right()       const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.orientation * glm::vec3(1.0f, 0.0f, 0.0f); }
    [[nodiscard]] glm::vec3 up()          const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.orientation * glm::vec3(0.0f, 1.0f, 0.0f); }
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
    glm::vec3   prevPosition_{};

    double      transitionStartGenesis_{0.0};
    float       transitionDurationSec_{1.5f};
    float       transitionProgress_{1.0f};
    double      lastUpdateGenesis_{0.0};

    void invalidateCache() noexcept {
        generation_.fetch_add(1, std::memory_order_release);
    }

    void ensureCached() const noexcept {
        uint64_t gen = generation_.load(std::memory_order_acquire);
        if (cachedGeneration_ != gen) {
            std::lock_guard<std::mutex> lock(mtx_);
            if (cachedGeneration_ != gen) {
                glm::vec3 fwd = currentState_.orientation * glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 up  = currentState_.orientation * glm::vec3(0.0f, 1.0f, 0.0f);
                viewCache_ = glm::lookAt(currentState_.position, currentState_.position + fwd, up);
                cachedGeneration_ = gen;
            }
        }
    }
};

// ────────────────────────────────────────────────
inline Camera& CAM = Camera::get();

// Inline wrappers
inline void CAM_RESET()                         { CAM.reset(); }
inline glm::vec3 CAM_POS()                      { return CAM.position(); }
inline glm::vec3 CAM_PREV_POS()                 { return CAM.prevPosition(); }
inline glm::quat CAM_ORI()                      { return CAM.orientation(); }
inline glm::vec3 CAM_FWD()                      { return CAM.forward(); }
inline void CAM_LOOK_AT(const glm::vec3& t)     { CAM.lookAt(t); }
inline void CAM_DOLLY(float d)                  { CAM.dolly(d); }
inline void CAM_CRANE(float h)                  { CAM.crane(h); }
inline void CAM_ZOOM(float d)                   { CAM.zoom(d); }
inline void CAM_FOV(float f)                    { CAM.setFov(f); }
inline void CAM_FOCUS(float d)                  { CAM.setFocusDistance(d); }
inline void CAM_APERTURE(float a)               { CAM.setAperture(a); }
inline void CAM_RACK(float t, float dur=2.0f)   { CAM.rackFocus(t, dur); }