#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Camera (Cinematic + Interactive Edition)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// Supports cinematic orbiting + interactive 3D/2D control via INPUT singleton
// Uses only existing SDL3 input methods (leftStickX, rightTrigger)
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
#include "SDL3.hpp"  // INPUT access

// ────────────────────────────────────────────────
// Camera modes
// ────────────────────────────────────────────────
enum class CameraMode {
    CinematicOrbit,     // smooth orbiting / dolly/crane
    FreeLook3D,         // FPS-style WASD + mouse/controller look
    Ortho2D             // top-down / UI / sprite view (position x/z, zoom)
};

// ────────────────────────────────────────────────
// Constants
// ────────────────────────────────────────────────
constexpr float CAMERA_BASE_HEIGHT          = 4.5f;
constexpr float CAMERA_HEIGHT_SWING         = 2.8f;
constexpr float CAMERA_HEIGHT_FREQ          = 0.11f;
constexpr float CAMERA_BASE_DISTANCE        = 20.0f;
constexpr float CAMERA_DISTANCE_SWING       = 4.5f;
constexpr float CAMERA_DISTANCE_FREQ        = 0.08f;
constexpr float CAMERA_FOV_SCALE            = 1.72f;
constexpr float CAMERA_LOOK_AT_Y_OFFSET     = -2.35f;

constexpr float CAMERA_MOVE_SPEED           = 8.0f;     // units/sec
constexpr float CAMERA_SPRINT_MULTIPLIER    = 2.5f;
constexpr float CAMERA_CONTROLLER_DEADZONE  = 0.15f;
constexpr float CAMERA_ROT_SPEED_MOUSE      = 0.15f;    // degrees per pixel
constexpr float CAMERA_ROT_SPEED_CONTROLLER = 120.0f;   // degrees per sec (unused until right stick added)

// ────────────────────────────────────────────────
// Camera state
// ────────────────────────────────────────────────
struct CameraState {
    glm::vec3   position       {Options::Camera::START_POSITION};
    glm::quat   orientation    {1.0f, 0.0f, 0.0f, 0.0f};
    float       fov            {Options::Camera::DEFAULT_FOV};
    float       aperture       {Options::Camera::DEFAULT_APERTURE};
    float       focusDistance  {Options::Camera::DEFAULT_FOCUS_DISTANCE};
    float       zoom           {1.0f};  // for Ortho2D

    constexpr CameraState() = default;
};

// ────────────────────────────────────────────────
// THE ONE TRUE CAMERA — singleton
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
        currentState_ = CameraState{};
        targetState_ = currentState_;
        mode_ = CameraMode::CinematicOrbit;
        transitionProgress_ = 1.0f;
        invalidateCache();
    }

    // Per-frame update — reads INPUT, applies movement/look
    void update(float dt) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);

        // Handle smooth transitions
        if (transitionProgress_ < 1.0f) {
            transitionProgress_ = std::min(1.0f, transitionProgress_ + dt / transitionDurationSec_);
            float t = transitionProgress_;
            t = 1.0f - std::pow(1.0f - t, 3.0f);  // cubic ease-out

            currentState_.position      = glm::mix(currentState_.position,      targetState_.position,      t);
            currentState_.orientation   = glm::slerp(currentState_.orientation, targetState_.orientation,   t);
            currentState_.fov           = glm::mix(currentState_.fov,           targetState_.fov,           t);
            currentState_.aperture      = glm::mix(currentState_.aperture,      targetState_.aperture,      t);
            currentState_.focusDistance = glm::mix(currentState_.focusDistance, targetState_.focusDistance, t);
            currentState_.zoom          = glm::mix(currentState_.zoom,          targetState_.zoom,          t);
        }

        // Input handling by mode
        switch (mode_) {
            case CameraMode::CinematicOrbit: {
                // Gentle auto-orbit if no lateral input
                if (!INPUT.down("move_left") && !INPUT.down("move_right")) {
                    double seconds = TotalTime::get().seconds();
                    float orbitAngle = static_cast<float>(seconds * 0.05);
                    currentState_.position.x = std::sin(orbitAngle) * CAMERA_BASE_DISTANCE;
                    currentState_.position.z = std::cos(orbitAngle) * CAMERA_BASE_DISTANCE;
                    currentState_.position.y = CAMERA_BASE_HEIGHT + std::sin(orbitAngle * 0.7f) * CAMERA_HEIGHT_SWING;
                }
                break;
            }

            case CameraMode::FreeLook3D: {
                glm::vec3 moveDir(0.0f);
                if (INPUT.down("move_forward"))  moveDir.z -= 1.0f;
                if (INPUT.down("move_backward")) moveDir.z += 1.0f;
                if (INPUT.down("move_left"))     moveDir.x -= 1.0f;
                if (INPUT.down("move_right"))    moveDir.x += 1.0f;

                // Controller: left stick X for strafe (Y not yet implemented in SDL3)
                float lx = INPUT.leftStickX(0);
                if (std::abs(lx) > CAMERA_CONTROLLER_DEADZONE) {
                    moveDir.x = lx;
                }

                float speed = CAMERA_MOVE_SPEED;
                if (INPUT.down("sprint") || INPUT.rightTrigger(0) > 0.3f) {
                    speed *= CAMERA_SPRINT_MULTIPLIER;
                }

                if (glm::length(moveDir) > 0.01f) {
                    moveDir = glm::normalize(moveDir);
                    currentState_.position += currentState_.orientation * moveDir * speed * dt;
                }

                // Mouse look (fallback for rotation)
                glm::vec2 delta = INPUT.mouseDelta();
                float yaw   = -delta.x * CAMERA_ROT_SPEED_MOUSE;
                float pitch = -delta.y * CAMERA_ROT_SPEED_MOUSE;

                if (std::abs(yaw) > 0.001f || std::abs(pitch) > 0.001f) {
                    glm::quat rotYaw   = glm::angleAxis(glm::radians(yaw),   glm::vec3(0,1,0));
                    glm::quat rotPitch = glm::angleAxis(glm::radians(pitch), currentState_.orientation * glm::vec3(1,0,0));
                    currentState_.orientation = rotYaw * currentState_.orientation * rotPitch;
                    currentState_.orientation = glm::normalize(currentState_.orientation);
                }
                break;
            }

            case CameraMode::Ortho2D: {
                glm::vec2 move(0.0f);
                if (INPUT.down("move_forward"))  move.y -= 1.0f;
                if (INPUT.down("move_backward")) move.y += 1.0f;
                if (INPUT.down("move_left"))     move.x -= 1.0f;
                if (INPUT.down("move_right"))    move.x += 1.0f;

                // Controller left stick X for horizontal pan
                float lx = INPUT.leftStickX(0);
                if (std::abs(lx) > CAMERA_CONTROLLER_DEADZONE) {
                    move.x = lx;
                }

                if (glm::length(move) > 0.01f) {
                    move = glm::normalize(move) * CAMERA_MOVE_SPEED * dt / currentState_.zoom;
                    currentState_.position.x += move.x;
                    currentState_.position.z += move.y;
                }

                // Zoom boost with right trigger
                float zoomDelta = -INPUT.rightTrigger(0) * 2.0f * dt;
                if (zoomDelta != 0.0f) {
                    currentState_.zoom = glm::clamp(currentState_.zoom + zoomDelta, 0.25f, 8.0f);
                }
                break;
            }
        }

        invalidateCache();
    }

    // ────────────────────────────────────────────────
    // Mode switching
    // ────────────────────────────────────────────────
    void setMode(CameraMode newMode, float transitionSec = 1.0f) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        if (mode_ == newMode) return;

        mode_ = newMode;
        transitionProgress_ = 0.0f;
        transitionDurationSec_ = transitionSec;
        transitionStartGenesis_ = TotalTime::get().seconds();

        if (newMode == CameraMode::Ortho2D) {
            targetState_.orientation = glm::quat(1,0,0,0);
            targetState_.zoom = 1.0f;
        }
    }

    [[nodiscard]] CameraMode mode() const noexcept {
        std::lock_guard<std::mutex> l(mtx_);
        return mode_;
    }

    // ────────────────────────────────────────────────
    // Cinematic helpers
    // ────────────────────────────────────────────────
    void transitionTo(const CameraState& target, float durationSec = 1.5f) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        targetState_ = target;
        transitionStartGenesis_ = TotalTime::get().seconds();
        transitionDurationSec_  = durationSec;
        transitionProgress_     = 0.0f;
    }

    void lookAt(const glm::vec3& target, bool instant = false) noexcept {
        glm::vec3 dir = glm::normalize(target - currentState_.position);
        glm::quat ori = glm::quatLookAt(dir, glm::vec3(0,1,0));
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
    // Output matrices & getters
    // ────────────────────────────────────────────────
    [[nodiscard]] glm::mat4 view() const noexcept {
        ensureCached();
        return viewCache_;
    }

    [[nodiscard]] glm::mat4 projection(float aspect) const noexcept {
        std::lock_guard<std::mutex> l(mtx_);
        if (mode_ == CameraMode::Ortho2D) {
            float halfW = aspect * currentState_.zoom * 10.0f;
            float halfH = currentState_.zoom * 10.0f;
            return glm::ortho(-halfW, halfW, -halfH, halfH, 0.1f, 20000.0f);
        }
        return glm::perspective(glm::radians(currentState_.fov), aspect, 0.05f, 20000.0f);
    }

    [[nodiscard]] glm::vec3 position()    const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.position; }
    [[nodiscard]] glm::quat orientation() const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.orientation; }
    [[nodiscard]] float     fov()         const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.fov; }
    [[nodiscard]] float     zoom()        const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.zoom; }

    [[nodiscard]] glm::vec3 forward()     const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.orientation * glm::vec3(0,0,-1); }
    [[nodiscard]] glm::vec3 right()       const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.orientation * glm::vec3(1,0,0); }
    [[nodiscard]] glm::vec3 up()          const noexcept { std::lock_guard<std::mutex> l(mtx_); return currentState_.orientation * glm::vec3(0,1,0); }

private:
    Camera() { reset(); }

    mutable std::mutex mtx_;
    mutable glm::mat4  viewCache_{1.0f};
    mutable std::atomic<uint64_t> generation_{0};
    mutable uint64_t cachedGeneration_{0};

    CameraState currentState_;
    CameraState targetState_;

    CameraMode mode_{CameraMode::CinematicOrbit};

    double      transitionStartGenesis_{0.0};
    float       transitionDurationSec_{1.5f};
    float       transitionProgress_{1.0f};

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
inline Camera& CAM = Camera::get();

// Inline helpers
inline void CAM_RESET()               { CAM.reset(); }
inline void CAM_UPDATE(float dt)      { CAM.update(dt); }
inline void CAM_MODE(CameraMode m)    { CAM.setMode(m); }
inline glm::vec3 CAM_POS()            { return CAM.position(); }
inline glm::quat CAM_ORI()            { return CAM.orientation(); }
inline glm::vec3 CAM_FWD()            { return CAM.forward(); }
inline float     CAM_ZOOM()           { return CAM.zoom(); }