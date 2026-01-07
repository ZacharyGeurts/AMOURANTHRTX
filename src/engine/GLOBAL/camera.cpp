// src/engine/GLOBAL/camera.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — THE ONE TRUE CAMERA — v2.0 — DECEMBER 22, 2025
// FULLY COMPATIBLE WITH camera.hpp — THREAD-SAFE — CACHED VIEW MATRIX
// DOF PARAMETERS READY — CENTRALIZED CONFIG VIA OptionsMenu::Camera
// PINK PHOTONS FLOW WITH DEPTH AND CLARITY — THE EMPIRE SEES ALL
// =============================================================================

#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

// =============================================================================
// Singleton Accessor
// =============================================================================
Camera& Camera::get() noexcept
{
    static Camera instance;
    return instance;
}

// =============================================================================
// Initialization — called once at startup
// =============================================================================
void Camera::init(glm::vec3 pos, float fov, float aperture, float focusDistance) noexcept
{
    std::lock_guard<std::mutex> lock(mtx_);
    pos_           = pos;
    fov_           = glm::clamp(fov, 1.0f, 120.0f);
    aperture_      = glm::max(aperture, 0.1f);
    focusDistance_ = glm::max(focusDistance, 0.1f);
    yaw_           = -90.0f;
    pitch_         = 0.0f;

    updateVectors();
    view_ = glm::lookAt(pos_, pos_ + front_, up_);
    cachedGen_ = ++gen_;

    LOG_SUCCESS_CAT("CAMERA", "Camera initialized — pos: ({:.2f},{:.2f},{:.2f}) | FOV: {:.1f}° | Aperture: f/{:.1f} | Focus: {:.2f}m",
                    pos_.x, pos_.y, pos_.z, fov_, aperture_, focusDistance_);
}

// =============================================================================
// Movement
// =============================================================================
void Camera::move(glm::vec3 delta) noexcept
{
    std::lock_guard<std::mutex> lock(mtx_);
    pos_ += delta;
    view_ = glm::lookAt(pos_, pos_ + front_, up_);
    ++gen_;
}

void Camera::moveForward(float s) noexcept { move(front_ * s); }
void Camera::moveRight(float s)   noexcept { move(right_ * s); }
void Camera::moveUp(float s)      noexcept { move(glm::vec3(0.0f, 1.0f, 0.0f) * s); }

// =============================================================================
// Rotation
// =============================================================================
void Camera::rotate(float yawDelta, float pitchDelta) noexcept
{
    std::lock_guard<std::mutex> lock(mtx_);
    yaw_ += yawDelta * Options::Camera::MOUSE_SENSITIVITY;
    float pitchSign = Options::Camera::INVERT_MOUSE_LOOK ? -1.0f : 1.0f;
    pitch_ = glm::clamp(pitch_ + pitchDelta * Options::Camera::MOUSE_SENSITIVITY * pitchSign, -89.0f, 89.0f);

    updateVectors();
    view_ = glm::lookAt(pos_, pos_ + front_, up_);
    ++gen_;
}

// =============================================================================
// Zoom / FOV
// =============================================================================
void Camera::zoom(float f) noexcept
{
    std::lock_guard<std::mutex> lock(mtx_);
    fov_ = glm::clamp(fov_ - f * Options::Camera::ZOOM_SENSITIVITY, 1.0f, 120.0f);
    ++gen_;
}

// =============================================================================
// DOF Controls
// =============================================================================
void Camera::setAperture(float a) noexcept
{
    std::lock_guard<std::mutex> lock(mtx_);
    aperture_ = glm::max(a, 0.1f);
    ++gen_;
}

void Camera::setFocusDistance(float d) noexcept
{
    std::lock_guard<std::mutex> lock(mtx_);
    focusDistance_ = glm::max(d, 0.1f);
    ++gen_;
}

// =============================================================================
// Direct Setters
// =============================================================================
void Camera::setPos(glm::vec3 p) noexcept
{
    std::lock_guard<std::mutex> lock(mtx_);
    pos_ = p;
    view_ = glm::lookAt(pos_, pos_ + front_, up_);
    ++gen_;
}

void Camera::setFov(float f) noexcept
{
    std::lock_guard<std::mutex> lock(mtx_);
    fov_ = glm::clamp(f, 1.0f, 120.0f);
    ++gen_;
}

// =============================================================================
// Matrix Access
// =============================================================================
glm::mat4 Camera::view() const noexcept
{
    ensureCached();
    return view_;
}

glm::mat4 Camera::proj(float aspect) const noexcept
{
    return glm::perspective(glm::radians(fov_), aspect, 0.1f, 10000.0f);
}

// =============================================================================
// Internal Helpers
// =============================================================================
void Camera::updateVectors() noexcept
{
    const float cy = cos(glm::radians(yaw_));
    const float sy = sin(glm::radians(yaw_));
    const float cp = cos(glm::radians(pitch_));
    const float sp = sin(glm::radians(pitch_));

    front_ = glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
    right_ = glm::normalize(glm::cross(front_, glm::vec3(0.0f, 1.0f, 0.0f)));
    up_    = glm::normalize(glm::cross(right_, front_));
}

void Camera::ensureCached() const noexcept
{
    uint64_t currentGen = gen_.load(std::memory_order_acquire);
    if (cachedGen_ != currentGen) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (cachedGen_ != currentGen) {
            view_ = glm::lookAt(pos_, pos_ + front_, up_);
            cachedGen_ = currentGen;
        }
    }
}

// =============================================================================
// THE ONE TRUE GLOBAL CAMERA — ACCESS VIA CAM
// =============================================================================
Camera& CAM = Camera::get();

// Auto-initialize from OptionsMenu on first access
namespace {
    struct CameraAutoInitializer {
        CameraAutoInitializer() noexcept
        {
            CAM.init(
                Options::Camera::START_POSITION,
                Options::Camera::DEFAULT_FOV,
                Options::Camera::DEFAULT_APERTURE,
                Options::Camera::DEFAULT_FOCUS_DISTANCE
            );
            LOG_AMOURANTH("STONE CAMERA AWAKENS — DOF ENABLED — PINK PHOTONS SEE WITH DEPTH AND CLARITY");
        }
    };
    [[maybe_unused]] CameraAutoInitializer g_cameraAutoInit;
}