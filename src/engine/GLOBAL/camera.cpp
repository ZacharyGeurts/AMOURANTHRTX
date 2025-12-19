// src/engine/GLOBAL/camera.cpp
// =============================================================================
// AMOURANTH RTX © 2025 — THE ONE TRUE CAMERA — EXTENDED & CORRECTED
// FULLY COMPATIBLE WITH DreamUBO & OptionsMenu::Camera — CENTRALIZED CONFIG
// STONEKEY ENCRYPTION IMPLEMENTED HERE — HEADER REMAINS CLEAN
// PINK PHOTONS FLOW WITH DEPTH AND CLARITY — THE EMPIRE SEES ALL
// =============================================================================

#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

// =============================================================================
// Singleton
// =============================================================================
Camera& Camera::get() noexcept {
    static Camera instance;
    return instance;
}

// =============================================================================
// Public API — Extended init with DOF parameters
// =============================================================================
void Camera::init(glm::vec3 pos, float fov, float aperture, float focusDistance) noexcept
{
    std::lock_guard<std::mutex> lock(mtx_);
    pos_           = pos;
    fov_           = fov;
    aperture_      = aperture;
    focusDistance_ = focusDistance;
    yaw_           = -90.0f;
    pitch_         = 0.0f;
    updateVectors();
    ++gen_;
}

void Camera::move(glm::vec3 delta) noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    pos_ += delta;
    ++gen_;
}

void Camera::moveForward(float s) noexcept { move(front_ * s); }
void Camera::moveRight(float s)   noexcept { move(right_ * s); }
void Camera::moveUp(float s)      noexcept { move(up_    * s); }

void Camera::rotate(float yawDelta, float pitchDelta) noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    yaw_   += yawDelta * Options::Camera::MOUSE_SENSITIVITY;
    float pitchSign = Options::Camera::INVERT_MOUSE_LOOK ? -1.0f : 1.0f;
    pitch_ = glm::clamp(pitch_ + pitchDelta * Options::Camera::MOUSE_SENSITIVITY * pitchSign, -89.0f, 89.0f);
    updateVectors();
    ++gen_;
}

void Camera::zoom(float f) noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    fov_ = glm::clamp(fov_ - f * Options::Camera::ZOOM_SENSITIVITY, 1.0f, 120.0f);
    ++gen_;
}

void Camera::setPos(glm::vec3 p) noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    pos_ = p;
    ++gen_;
}

void Camera::setFov(float f) noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    fov_ = glm::clamp(f, 1.0f, 120.0f);
    ++gen_;
}

void Camera::setAperture(float a) noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    aperture_ = glm::max(a, 0.1f);
    ++gen_;
}

void Camera::setFocusDistance(float d) noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    focusDistance_ = glm::max(d, 0.1f);
    ++gen_;
}

// =============================================================================
// Matrices
// =============================================================================
glm::mat4 Camera::view() const noexcept {
    ensureCached();
    return view_;
}

glm::mat4 Camera::proj(float aspect) const noexcept {
    return glm::perspective(glm::radians(fov_), aspect, 0.1f, 10000.0f);
}

void Camera::updateVectors() noexcept {
    const float cy = cos(glm::radians(yaw_));
    const float sy = sin(glm::radians(yaw_));
    const float cp = cos(glm::radians(pitch_));
    const float sp = sin(glm::radians(pitch_));

    front_ = glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
    right_ = glm::normalize(glm::cross(front_, {0.0f, 1.0f, 0.0f}));
    up_    = glm::normalize(glm::cross(right_, front_));
}

void Camera::ensureCached() const noexcept {
    uint64_t g = gen_.load();
    if (cachedGen_ != g) {
        std::lock_guard<std::mutex> lock(mtx_);
        view_    = glm::lookAt(pos_, pos_ + front_, up_);
        cachedGen_ = g;
    }
}

// =============================================================================
// THE ONE TRUE GLOBAL CAMERA — DEFINED ONCE — FOREVER
// =============================================================================
Camera& CAM = Camera::get();

// Auto-initialize on first use — driven by OptionsMenu::Camera
namespace {
    struct CameraAutoInit {
        CameraAutoInit() {
            CAM.init(
                Options::Camera::CAMERA_START_POSITION,
                Options::Camera::DEFAULT_FOV,
                Options::Camera::DEFAULT_APERTURE,
                Options::Camera::DEFAULT_FOCUS_DISTANCE
            );
            LOG_SUCCESS_CAT("CAMERA", "{}STONE CAMERA SEALED AND INITIALIZED — DOF ENABLED — PINK PHOTONS FLOW WITH DEPTH{}", RASPBERRY_PINK, RESET);
        }
    };
    CameraAutoInit auto_init_cam;
}