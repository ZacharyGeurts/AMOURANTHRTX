// =============================================================================
// src/engine/GLOBAL/camera.cpp
// AMOURANTH RTX © 2025 — THE ONE TRUE CAMERA — STONEKEY SEALED — FIRST LIGHT
// THIS IS THE ONLY FILE THAT MAY INCLUDE StoneKey.hpp
// =============================================================================

#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
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
// Public API
// =============================================================================
void Camera::init(glm::vec3 pos, float fov) noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    pos_ = pos;
    fov_ = fov;
    yaw_ = -90.0f;
    pitch_ = 0.0f;
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
    yaw_   += yawDelta;
    pitch_ = glm::clamp(pitch_ + pitchDelta, -89.0f, 89.0f);
    updateVectors();
    ++gen_;
}

void Camera::zoom(float f) noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    fov_ = glm::clamp(fov_ - f, 1.0f, 120.0f);
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

glm::vec3 Camera::pos()   const noexcept { return pos_; }
glm::vec3 Camera::front() const noexcept { return front_; }
glm::vec3 Camera::right() const noexcept { return right_; }
glm::vec3 Camera::up()    const noexcept { return up_; }
float     Camera::fov()   const noexcept { return fov_; }

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
        encView_ = encryptMat4(view_, g);
        cachedGen_ = g;
    }
}

// =============================================================================
// StoneKey v∞ Encryption — ONLY HERE
// =============================================================================
uint64_t Camera::encrypt(const glm::vec3& v, uint64_t g) noexcept {
    uint32_t x = std::bit_cast<uint32_t>(v.x);
    uint32_t y = std::bit_cast<uint32_t>(v.y);
    uint32_t z = std::bit_cast<uint32_t>(v.z);
    uint64_t a = (uint64_t(x) << 32) ^ kStone1 ^ g;
    uint64_t b = (uint64_t(y) << 16) ^ kStone2 ^ g;
    uint64_t c = uint64_t(z) ^ 0xDEADBEEFULL ^ g;
    return std::rotl(a ^ b ^ c, 23) ^ g;
}

uint64_t Camera::encryptMat4(const glm::mat4& m, uint64_t g) noexcept {
    uint64_t h = 0;
    for (int i = 0; i < 16; ++i)
        h ^= std::rotl(uint64_t(std::bit_cast<uint32_t>(m[i/4][i%4])) ^ g, i);
    return h ^ kStone1 ^ kStone2 ^ 0xBEEFBABEULL;
}

uint64_t Camera::encPos()  const noexcept { return encrypt(pos_, gen_.load()); }
uint64_t Camera::encView() const noexcept { ensureCached(); return encView_; }

// =============================================================================
// STONEKEY INTEGRATION — BORN HERE — SAFE AND ETERNAL
// =============================================================================
namespace StoneKey {
    [[nodiscard]] Camera& stone_camera() noexcept {
        return Camera::get();
    }
}

// =============================================================================
// THE ONE TRUE GLOBAL CAMERA — DEFINED ONCE — FOREVER
// =============================================================================
Camera& CAM = StoneKey::stone_camera();

// Auto-initialize on first use — the Empire demands it
namespace {
    struct CameraAutoInit {
        CameraAutoInit() {
            CAM.init({0.0f, 5.0f, 10.0f}, 75.0f);
            LOG_SUCCESS_CAT("CAMERA", "{}STONE CAMERA SEALED AND INITIALIZED — PINK PHOTONS FLOW{}", RASPBERRY_PINK, RESET);
        }
    };
    CameraAutoInit auto_init_cam;
}