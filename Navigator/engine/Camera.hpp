#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Camera Core (Math + State Only)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// This is the low-level camera: position, orientation, projection math.
// No input handling, no animation/orbit logic — those belong in a controller.
// Designed for clean temporal accumulation, raymarching, hardware RT, path tracing.
// =============================================================================

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <mutex>
#include <atomic>
#include <cmath>

#include "OptionsMenu.hpp"  // Options::Camera::*

// ────────────────────────────────────────────────
// Projection types (perspective vs orthographic)
// ────────────────────────────────────────────────
enum class ProjectionType
{
    Perspective,
    Orthographic
};

// ────────────────────────────────────────────────
// Core camera state (what shaders actually need)
// ────────────────────────────────────────────────
struct CameraState
{
    glm::vec3 position    {Options::Camera::StartPosition};
    glm::quat orientation {1.0f, 0.0f, 0.0f, 0.0f};  // w,x,y,z — identity

    float fovDeg          {Options::Camera::CurrentFOV};
    float orthoZoom       {Options::Camera::OrthoZoom};

    float nearPlane       {Options::Camera::NearPlane};
    float farPlane        {Options::Camera::FarPlane};

    float aperture        {Options::Camera::Aperture};
    float focusDistance   {Options::Camera::FocusDistance};

    constexpr CameraState() = default;
};

// ────────────────────────────────────────────────
// THE CAMERA — singleton, thread-safe getters
// ────────────────────────────────────────────────
class Camera final
{
public:
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
    Camera(Camera&&) = delete;
    Camera& operator=(Camera&&) = delete;

    [[nodiscard]] static Camera& get() noexcept
    {
        static Camera instance;
        return instance;
    }

    // ── Reset to defaults ───────────────────────────────────────────────
    void reset() noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_ = CameraState{};
        invalidateCache();
    }

    // ── Direct state mutation (used by controller / transitions) ───────
    void setPosition(const glm::vec3& pos) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_.position = pos;
        invalidateCache();
    }

    void setOrientation(const glm::quat& ori) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_.orientation = glm::normalize(ori);
        invalidateCache();
    }

    void setFOV(float fovDeg) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_.fovDeg = glm::clamp(fovDeg, Options::Camera::MinFOV, Options::Camera::MaxFOV);
        invalidateCache();
    }

    void setOrthoZoom(float zoom) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_.orthoZoom = glm::clamp(zoom,
                                       Options::Camera::MinOrthoZoom,
                                       Options::Camera::MaxOrthoZoom);
        invalidateCache();
    }

    void setDoF(float aperture, float focusDist) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_.aperture      = aperture;
        state_.focusDistance = focusDist;
        // no invalidate needed — DoF not in view/proj matrices
    }

    // ── Look-at helper ──────────────────────────────────────────────────
    void lookAt(const glm::vec3& target, bool instant = true) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        glm::vec3 dir = glm::normalize(target - state_.position);
        glm::quat newOri = glm::quatLookAt(dir, glm::vec3(0,1,0));
        state_.orientation = instant ? newOri : glm::slerp(state_.orientation, newOri, 0.25f);
        invalidateCache();
    }

    // ── Getters (thread-safe) ───────────────────────────────────────────
    [[nodiscard]] glm::vec3   position()    const noexcept { std::lock_guard<std::mutex> l(mtx_); return state_.position; }
    [[nodiscard]] glm::quat   orientation() const noexcept { std::lock_guard<std::mutex> l(mtx_); return state_.orientation; }
    [[nodiscard]] float       fovDeg()      const noexcept { std::lock_guard<std::mutex> l(mtx_); return state_.fovDeg; }
    [[nodiscard]] float       orthoZoom()   const noexcept { std::lock_guard<std::mutex> l(mtx_); return state_.orthoZoom; }
    [[nodiscard]] float       near()        const noexcept { std::lock_guard<std::mutex> l(mtx_); return state_.nearPlane; }
    [[nodiscard]] float       far()         const noexcept { std::lock_guard<std::mutex> l(mtx_); return state_.farPlane; }
    [[nodiscard]] float       aperture()    const noexcept { std::lock_guard<std::mutex> l(mtx_); return state_.aperture; }
    [[nodiscard]] float       focusDist()   const noexcept { std::lock_guard<std::mutex> l(mtx_); return state_.focusDistance; }

    [[nodiscard]] glm::vec3 forward() const noexcept { std::lock_guard<std::mutex> l(mtx_); return state_.orientation * glm::vec3(0,0,-1); }
    [[nodiscard]] glm::vec3 right()   const noexcept { std::lock_guard<std::mutex> l(mtx_); return state_.orientation * glm::vec3(1,0,0); }
    [[nodiscard]] glm::vec3 up()      const noexcept { std::lock_guard<std::mutex> l(mtx_); return state_.orientation * glm::vec3(0,1,0); }

    // ── Matrices ────────────────────────────────────────────────────────
    [[nodiscard]] glm::mat4 view() const noexcept
    {
        ensureCached();
        return viewCache_;
    }

    [[nodiscard]] glm::mat4 projection(float aspect) const noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (Options::GameStyle::CurrentPerspective == Options::GameStyle::CameraPerspective::Orthographic2D ||
            Options::GameStyle::CurrentPerspective == Options::GameStyle::CameraPerspective::Isometric)
        {
            float halfW = aspect * state_.orthoZoom * 10.0f;
            float halfH = state_.orthoZoom * 10.0f;
            return glm::ortho(-halfW, halfW, -halfH, halfH, state_.nearPlane, state_.farPlane);
        }

        return glm::perspective(glm::radians(state_.fovDeg), aspect, state_.nearPlane, state_.farPlane);
    }

private:
    Camera() { reset(); }

    mutable std::mutex          mtx_;
    mutable glm::mat4           viewCache_{1.0f};
    mutable std::atomic<uint64_t> generation_{0};
    mutable uint64_t            cachedGeneration_{0};

    CameraState                 state_;

    void invalidateCache() const noexcept
    {
        generation_.fetch_add(1, std::memory_order_release);
    }

    void ensureCached() const noexcept
    {
        uint64_t gen = generation_.load(std::memory_order_acquire);
        if (cachedGeneration_ != gen)
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (cachedGeneration_ != gen)
            {
                glm::vec3 eye    = state_.position;
                glm::vec3 center = eye + forward();
                glm::vec3 upVec  = up();
                viewCache_ = glm::lookAt(eye, center, upVec);
                cachedGeneration_ = gen;
            }
        }
    }
};

// ────────────────────────────────────────────────
// Global accessors (use these everywhere)
// ────────────────────────────────────────────────
inline Camera& CAM = Camera::get();

inline glm::vec3   CAM_POS()          { return CAM.position(); }
inline glm::quat   CAM_ORI()          { return CAM.orientation(); }
inline glm::vec3   CAM_FWD()          { return CAM.forward(); }
inline glm::vec3   CAM_RIGHT()        { return CAM.right(); }
inline glm::vec3   CAM_UP()           { return CAM.up(); }
inline float       CAM_FOV()          { return CAM.fovDeg(); }
inline float       CAM_ZOOM()         { return CAM.orthoZoom(); }
inline glm::mat4   CAM_VIEW()         { return CAM.view(); }
inline glm::mat4   CAM_PROJ(float a)  { return CAM.projection(a); }

// Quick helpers
inline void CAM_RESET()               { CAM.reset(); }
inline void CAM_LOOKAT(const glm::vec3& t, bool instant = true) { CAM.lookAt(t, instant); }