// =============================================================================
// AMOURANTH RTX Engine — Camera Core v3.2 (Full Realism + Maximum Speed)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// Hybrid digital + biological camera with micrometer/sub-atomic scaling.
// Lock-free hot path. All functions defined inline for zero linker issues.
// =============================================================================

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <mutex>
#include <atomic>
#include <cmath>
#include <random>

#include "OptionsMenu.hpp"

// =============================================================================
// ENUMS + STRUCTS
// =============================================================================
enum class CameraFormat 
{ 
    FullFrame35mm, APS_C, Super35, MediumFormat645, MediumFormat67, 
    LargeFormat45, Anamorphic2x, IMAX, Custom 
};

enum class VisionMode 
{ 
    Human_Trichromat, Cat, Dog_Dichromat, Bird_Tetrachromat, 
    MantisShrimp, Thermal_Infrared, Ultraviolet, Quantum_LowLight 
};

struct BiologicalEye 
{
    float pupilDiameter_mm     = 4.2f;
    float accommodationSpeed   = 0.22f;
    float pupilResponseSpeed   = 0.15f;
    float saccadeFrequency     = 3.5f;
    float microsaccadeAmp_deg  = 0.018f;
    glm::vec3 coneResponse     {1.0f, 1.0f, 1.0f};
    float retinalAdaptation    = 1.0f;
    float quantumNoise         = 0.012f;
    float entropy              = 0.0f;
};

struct DigitalSensor 
{
    float photositeSize_um   = 4.5f;
    float quantumEfficiency  = 0.68f;
    float readNoise_e        = 1.2f;
    float thermalNoise       = 0.018f;
    bool  rollingShutter     = true;
    float rollingSkew_s      = 0.008f;
};

struct CameraState 
{
    glm::vec3 position    {0.0f, 1.7f, 5.0f};
    glm::quat orientation {1.0f, 0.0f, 0.0f, 0.0f};

    float fovDeg          = 45.0f;
    float nearPlane       = 0.02f;
    float farPlane        = 500.0f;

    float focusDistance   = 3.0f;
    float aperture        = 2.8f;

    CameraFormat   format         = CameraFormat::FullFrame35mm;
    VisionMode     visionMode     = VisionMode::Human_Trichromat;
    DigitalSensor  sensor;
    BiologicalEye  eye;

    bool   stereoEnabled = false;
    float  ipd_m         = 0.065f;

    float lastUpdateTime   = 0.0f;
    float lastSaccadeTime  = 0.0f;
    glm::vec2 saccadeOffset{0.0f, 0.0f};
};

// =============================================================================
// CAMERA CLASS — EVERYTHING INLINE
// =============================================================================
class Camera final
{
public:
    static Camera& get() noexcept
    {
        static Camera instance;
        return instance;
    }

    // Fast lock-free getters
    [[nodiscard]] glm::vec3 position()      const noexcept { return state_.position; }
    [[nodiscard]] glm::quat orientation()   const noexcept { return state_.orientation; }
    [[nodiscard]] float fovDeg()            const noexcept { return state_.fovDeg; }
    [[nodiscard]] float focusDistance()     const noexcept { return state_.focusDistance; }
    [[nodiscard]] float aperture()          const noexcept { return state_.aperture; }
    [[nodiscard]] BiologicalEye eye()       const noexcept { return state_.eye; }
    [[nodiscard]] DigitalSensor sensor()    const noexcept { return state_.sensor; }

    [[nodiscard]] glm::vec3 forward() const noexcept { return state_.orientation * glm::vec3(0,0,-1); }
    [[nodiscard]] glm::vec3 right()   const noexcept { return state_.orientation * glm::vec3(1,0,0); }
    [[nodiscard]] glm::vec3 up()      const noexcept { return state_.orientation * glm::vec3(0,1,0); }

    [[nodiscard]] glm::mat4 view() const noexcept
    {
        ensureCached();
        return viewCache_;
    }

    [[nodiscard]] glm::mat4 projection(float aspect) const noexcept
    {
        float fov = state_.fovDeg;
        if (state_.format == CameraFormat::Anamorphic2x) aspect *= 2.0f;
        return glm::perspective(glm::radians(fov), aspect, state_.nearPlane, state_.farPlane);
    }

    [[nodiscard]] glm::mat4 leftEyeView()  const noexcept;
    [[nodiscard]] glm::mat4 rightEyeView() const noexcept;

    // Mutations
    void setPosition(const glm::vec3& p) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_.position = p;
        invalidateCache();
    }

    void setOrientation(const glm::quat& q) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_.orientation = glm::normalize(q);
        invalidateCache();
    }

    void lookAt(const glm::vec3& target, float blend = 1.0f) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        glm::vec3 dir = glm::normalize(target - state_.position);
        glm::quat targetOri = glm::quatLookAt(dir, glm::vec3(0,1,0));
        state_.orientation = glm::slerp(state_.orientation, targetOri, blend);
        invalidateCache();
    }

    void setFOV(float fov) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_.fovDeg = glm::clamp(fov, 10.0f, 170.0f);
        invalidateCache();
    }

    void setFocusDistance(float d) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_.focusDistance = glm::max(d, 0.1f);
    }

    void setAperture(float f) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_.aperture = glm::clamp(f, 0.5f, 32.0f);
    }

    void setVisionMode(VisionMode mode) noexcept;
    void setFormat(CameraFormat fmt) noexcept;
    void enableStereo(bool on) noexcept;

    void update(float currentTime, float targetFocusDist = -1.0f) noexcept;

    void reset() noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_ = CameraState{};
        invalidateCache();
    }

private:
    Camera() { reset(); }

    mutable std::mutex          mtx_;
    mutable std::atomic<uint64_t> generation_{0};
    mutable uint64_t            cachedGeneration_{0};
    mutable glm::mat4           viewCache_{1.0f};

    alignas(64) CameraState state_;
    std::mt19937 rng{std::random_device{}()};

    void invalidateCache() noexcept { generation_.fetch_add(1, std::memory_order_release); }

    void ensureCached() const noexcept
    {
        uint64_t gen = generation_.load(std::memory_order_acquire);
        if (cachedGeneration_ == gen) return;

        std::lock_guard<std::mutex> lock(mtx_);
        if (cachedGeneration_ != gen)
        {
            glm::vec3 eye = state_.position;
            glm::vec3 center = eye + forward();
            viewCache_ = glm::lookAt(eye, center, up());
            cachedGeneration_ = gen;
        }
    }
};

// =============================================================================
// INLINE IMPLEMENTATIONS
// =============================================================================
inline void Camera::setVisionMode(VisionMode mode) noexcept
{
    std::lock_guard<std::mutex> lock(mtx_);
    state_.visionMode = mode;
    switch (mode)
    {
        case VisionMode::Human_Trichromat: state_.eye.coneResponse = {1.0f, 1.0f, 1.0f}; break;
        case VisionMode::Cat:              state_.eye.coneResponse = {0.55f, 0.9f, 1.35f}; state_.eye.retinalAdaptation = 2.4f; break;
        case VisionMode::Bird_Tetrachromat:state_.eye.coneResponse = {1.1f, 1.0f, 0.95f}; state_.eye.quantumNoise = 0.045f; break;
        case VisionMode::MantisShrimp:     state_.eye.quantumNoise = 0.12f; break;
        case VisionMode::Thermal_Infrared: state_.eye.coneResponse = {0.0f, 0.2f, 1.4f}; break;
        default: break;
    }
}

inline void Camera::setFormat(CameraFormat fmt) noexcept
{
    std::lock_guard<std::mutex> lock(mtx_);
    state_.format = fmt;
    invalidateCache();
}

inline void Camera::enableStereo(bool on) noexcept
{
    std::lock_guard<std::mutex> lock(mtx_);
    state_.stereoEnabled = on;
}

inline void Camera::update(float currentTime, float targetFocusDist) noexcept
{
    std::lock_guard<std::mutex> lock(mtx_);

    float dt = currentTime - state_.lastUpdateTime;
    state_.lastUpdateTime = currentTime;

    if (targetFocusDist > 0.0f)
    {
        float response = 1.0f - std::exp(-dt / state_.eye.accommodationSpeed);
        state_.focusDistance = glm::mix(state_.focusDistance, targetFocusDist, response);
    }

    // Iris / pupil reaction
    float targetPupil = glm::clamp(8.0f - state_.aperture * 0.28f, 1.8f, 8.2f);
    float irisResp = 1.0f - std::exp(-dt / state_.eye.pupilResponseSpeed);
    state_.eye.pupilDiameter_mm = glm::mix(state_.eye.pupilDiameter_mm, targetPupil, irisResp);

    // Saccades
    if (currentTime - state_.lastSaccadeTime > (1.0f / state_.eye.saccadeFrequency))
    {
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        state_.saccadeOffset.x = dist(rng) * state_.eye.microsaccadeAmp_deg;
        state_.saccadeOffset.y = dist(rng) * state_.eye.microsaccadeAmp_deg * 0.7f;
        state_.lastSaccadeTime = currentTime;
    }

    // Quantum thermo entropy
    state_.eye.entropy = glm::clamp(state_.eye.entropy + dt * 0.008f, 0.0f, 1.0f);
    if (std::abs(state_.focusDistance - (targetFocusDist > 0.0f ? targetFocusDist : state_.focusDistance)) < 0.05f)
        state_.eye.entropy *= 0.92f;
}

inline glm::mat4 Camera::leftEyeView() const noexcept
{
    if (!state_.stereoEnabled) return view();
    glm::vec3 offset = right() * (state_.ipd_m * 0.5f);
    return glm::lookAt(state_.position - offset, state_.position - offset + forward(), up());
}

inline glm::mat4 Camera::rightEyeView() const noexcept
{
    if (!state_.stereoEnabled) return view();
    glm::vec3 offset = right() * (state_.ipd_m * 0.5f);
    return glm::lookAt(state_.position + offset, state_.position + offset + forward(), up());
}

// =============================================================================
// GLOBAL ACCESS
// =============================================================================
inline Camera& CAM = Camera::get();

inline void CAM_RESET()                              { CAM.reset(); }
inline void CAM_UPDATE(float t, float focus = -1.0f) { CAM.update(t, focus); }
inline void CAM_SET_FOV(float f)                     { CAM.setFOV(f); }
inline void CAM_SET_POSITION(const glm::vec3& p)     { CAM.setPosition(p); }
inline void CAM_LOOKAT(const glm::vec3& t, float b=1.0f) { CAM.lookAt(t, b); }
inline void CAM_SET_VISION(VisionMode m)             { CAM.setVisionMode(m); }
inline void CAM_SET_FORMAT(CameraFormat f)           { CAM.setFormat(f); }
inline void CAM_ENABLE_STEREO(bool on)               { CAM.enableStereo(on); }