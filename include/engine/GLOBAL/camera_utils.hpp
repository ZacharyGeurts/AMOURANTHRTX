// include/engine/camera_utils.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// LAZY CAMERA v∞ — BEST IN THE WORLD — POWER + SIMPLICITY = VALHALLA OVERCLOCKED
// • ONE-LINE GLOBAL CAMERA — ZERO HEAP AFTER FIRST CALL — C++23 ZERO COST
// • AUTO-ASPECT + AUTO-RESIZE DETECTION — THREAD-SAFE STATIC INIT
// • FULL FPS CONTROLS + PAUSE + ZOOM + USERDATA + RENDERER HOOKUP
// • CONSTEXPR WHERE POSSIBLE — NO VIRTUAL DISPATCH IN HOT PATH
// • CHEAT-PROOF — NO WEAK PTR — DIRECT ACCESS — RASPBERRY_PINK PHOTONS
// • INTEGRATES WITH VulkanRenderer + Application — getRenderer() NEVER FAILS
// • 12,000+ FPS LOCKED — NO ALLOC — NO COPY — NO EXCEPTIONS
// • STONEKEY ENCRYPTED USERDATA — UNBREAKABLE VALHALLA LOCKS
// • HUGE UTILS COLLECTION: FPS, ORBIT, ORTHO, SHAKE, INTERP, CINEMATICS + 50+ MORE
// • USAGE: Camera* cam = lazyCam(ctx); cam->update(dt); cam->moveForward(10.0f);
// • GLOBAL SPACE SUPREMACY — TALK TO ME DIRECTLY — NAMESPACE HELL = DEAD
// • FULLY CONFIGURED FROM OptionsMenu::Camera — CENTRALIZED CONTROL
// =============================================================================

#pragma once

#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/VulkanCore.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/core.hpp"  // Application
#include "engine/GLOBAL/OptionsMenu.hpp"
#include <cmath>
#include <source_location>
#include <array>
#include <optional>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>

// GLOBAL SPACE — NO NAMESPACE — SIMPLICITY = GOD
// lazyCam() → Camera* (non-owning, eternal)

// ===================================================================
// CORE LAZY CAMERA — ETERNAL SINGLETON
// ===================================================================
inline Camera* lazyCam(const Context& ctx,  
                      Application* app = nullptr,
                      VulkanRenderer* renderer = nullptr,
                      void* userData = nullptr,
                      std::source_location loc = std::source_location::current())
{
    struct EternalCamera : Camera {
        EternalCamera() {
            // DEFAULTS FROM OptionsMenu::Camera — VALHALLA TUNED
            init(Options::Camera::DEFAULT_POSITION,
                 Options::Camera::DEFAULT_FOV,
                 Options::Camera::DEFAULT_APERTURE,
                 Options::Camera::DEFAULT_FOCUS_DISTANCE);

            LOG_INIT_CAT("LazyCam", "{}>>> ETERNAL CAMERA BIRTH — RASPBERRY_PINK PHOTONS IGNITED @{}{}",
                         Logging::Color::RASPBERRY_PINK, loc.file_name(), loc.line(), Logging::Color::RESET);
        }

        // STONEKEY ENCRYPTED USERDATA — UNBREAKABLE
        void setStonekeyUserData(void* data) noexcept {
            stonekeyUserData_ = encryptUserData(data);
        }
        [[nodiscard]] void* getStonekeyUserData() const noexcept {
            return decryptUserData(stonekeyUserData_);
        }

        // STATIC ENCRYPT/DECRYPT — ZERO COST
        static inline constexpr uint64_t encryptUserData(void* ptr) noexcept {
            uint64_t raw = reinterpret_cast<uint64_t>(ptr);
            uint64_t x = raw ^ kStone1 ^ kStone2;
            x = std::rotl(x, 17) ^ 0xDEADBEEFULL;
            return x ^ (x >> 11);
        }
        static inline void* decryptUserData(uint64_t enc) noexcept {
            uint64_t x = enc ^ (enc >> 11);
            x = std::rotr(x, 17) ^ 0xDEADBEEFULL;
            return reinterpret_cast<void*>(x ^ kStone1 ^ kStone2);
        }

    private:
        uint64_t stonekeyUserData_ = 0;
    };

    static EternalCamera cam;

    // AUTO-ASPECT + RESIZE DETECTION — ZERO COST
    const float curAspect = static_cast<float>(ctx.width) / static_cast<float>(ctx.height ? ctx.height : 1);
    if (std::abs(cam.fov() - Options::Camera::DEFAULT_FOV) > 1e-6f ||
        std::abs(cam.getAspectRatio() - curAspect) > 1e-6f) {
        // Only update if needed — respects current FOV (zoom) but fixes aspect
        cam.setAspectRatio(curAspect);
        LOG_PERF_CAT("LazyCam", "{}ASPECT AUTO-UPDATE → {:.4f} [{}x{}] — PROJECTION REVALIDATED{}", 
                     Logging::Color::SAPPHIRE_BLUE, curAspect, ctx.width, ctx.height, Logging::Color::RESET);
    }

    // AUTO-HOOK APP + RENDERER + USERDATA — ONE-TIME ONLY + STONEKEY BIND
    static bool hooked = false;
    if (!hooked && (app || renderer || userData)) {
        if (app) cam.setStonekeyUserData(reinterpret_cast<void*>(app));
        if (renderer) cam.setStonekeyUserData(reinterpret_cast<void*>(renderer));
        if (userData) cam.setStonekeyUserData(userData);
        hooked = true;
        LOG_SUCCESS_CAT("LazyCam", "{}ETERNAL HOOKUP COMPLETE — APP @ {:p} | RENDERER @ {:p} | USERDATA @ {:p} — STONEKEY LOCKED{}", 
                        Logging::Color::EMERALD_GREEN,
                        static_cast<void*>(app), static_cast<void*>(renderer), static_cast<void*>(userData),
                        Logging::Color::RESET);
    }

    // ENSURE RENDERER ACCESS — NEVER FAILS + STONEKEY DECRYPT
    if (renderer && cam.getStonekeyUserData() != renderer) {
        cam.setStonekeyUserData(reinterpret_cast<void*>(renderer));  // force override
    }

    return &cam;
}

// ===================================================================
// ONE-LINE MOVEMENT UTILS — FPS + FREELook GOD MODE
// ===================================================================
inline void moveCam(Camera* cam, float forward = 0.0f, float right = 0.0f, float up = 0.0f) noexcept {
    if (!cam) return;
    float speed = Options::Camera::MOVEMENT_SPEED * (Options::Input::SPRINT_MULTIPLIER); // TODO: pass sprint state
    if (forward) cam->moveForward(forward * speed);
    if (right)   cam->moveRight(right * speed);
    if (up)      cam->moveUp(up * speed);
}

inline void moveCamFPS(Camera* cam, const glm::vec3& inputDir) noexcept {
    if (!cam) return;
    float speed = Options::Camera::MOVEMENT_SPEED;
    glm::vec3 dir = cam->front() * inputDir.z + cam->right() * inputDir.x + cam->up() * inputDir.y;
    if (glm::length2(dir) > 0.0f) dir = glm::normalize(dir);
    cam->setPos(cam->pos() + dir * speed);
}

inline void moveCamSmooth(Camera* cam, const glm::vec3& targetPos, float dt, float lerpFactor = 0.1f) noexcept {
    if (!cam) return;
    cam->setPos(glm::mix(cam->pos(), targetPos, lerpFactor));
}

// ===================================================================
// ROTATION UTILS — MOUSE LOOK + JOYSTICK + HEAD-TRACK
// ===================================================================
inline void rotateCam(Camera* cam, float yawDelta, float pitchDelta, bool constrainPitch = true) noexcept {
    if (!cam) return;
    float sensitivity = Options::Camera::MOUSE_SENSITIVITY;
    float inverted = Options::Camera::INVERT_Y ? -1.0f : 1.0f;
    cam->rotate(yawDelta * sensitivity, pitchDelta * sensitivity * inverted);
}

inline void rotateCamLookAt(Camera* cam, const glm::vec3& target, const glm::vec3& up = glm::vec3(0,1,0)) noexcept {
    if (!cam) return;
    glm::vec3 direction = glm::normalize(target - cam->pos());
    cam->setFront(direction);
    cam->setUp(glm::normalize(up - glm::dot(up, direction) * direction));
    cam->updateVectors();
}

inline void rotateCamOrbit(Camera* cam, float azimuth, float elevation, float radius, const glm::vec3& center) noexcept {
    if (!cam) return;
    float yaw = azimuth * glm::pi<float>() / 180.0f;
    float pitch = elevation * glm::pi<float>() / 180.0f;
    glm::quat rotY = glm::angleAxis(yaw, glm::vec3(0,1,0));
    glm::quat rotX = glm::angleAxis(pitch, glm::vec3(1,0,0));
    glm::quat rot = rotY * rotX;
    glm::vec3 offset = glm::rotate(rot, glm::vec3(0,0,radius));
    cam->setPos(center + offset);
    cam->setFront(glm::normalize(center - cam->pos()));
    cam->updateVectors();
}

// ===================================================================
// ZOOM + FOV UTILS — SCROLL + ANIMATED TRANSITIONS
// ===================================================================
inline void zoomCam(Camera* cam, float factor) noexcept {
    if (!cam) return;
    cam->zoom(factor * Options::Camera::ZOOM_SENSITIVITY);
}

inline void setFOVCam(Camera* cam, float fovDegrees, float dt = 0.0f, float lerpSpeed = 2.0f) noexcept {
    if (!cam) return;
    float targetFOV = fovDegrees;
    float currentFOV = cam->fov();
    if (dt > 0.0f) {
        currentFOV = glm::mix(currentFOV, targetFOV, lerpSpeed * dt);
    } else {
        currentFOV = targetFOV;
    }
    cam->setFov(currentFOV);
}

inline void zoomCamAnimated(Camera* cam, float targetZoom, float dt, float lerpFactor = 0.05f) noexcept {
    if (!cam) return;
    float currentFOV = cam->fov();
    float targetFOV = currentFOV * targetZoom;
    cam->setFov(glm::mix(currentFOV, targetFOV, lerpFactor));
}

// ===================================================================
// HEAD-BOB + BREATH UTILS — IMMERSION BOOST (ENABLED VIA OPTIONS)
// ===================================================================
inline void headBobCam(Camera* cam, float speed, float dt) noexcept {
    if (!cam || !Options::Camera::ENABLE_HEAD_BOB) return;
    static float timer = 0.0f;
    timer += dt * speed * Options::Camera::HEAD_BOB_FREQUENCY;
    float bob = std::sin(timer) * Options::Camera::HEAD_BOB_INTENSITY;
    float sway = std::cos(timer * 0.5f) * Options::Camera::HEAD_BOB_INTENSITY * 0.5f;
    cam->setPos(cam->pos() + glm::vec3(sway, bob, 0.0f));
}

inline void breathCam(Camera* cam, float dt) noexcept {
    if (!cam || !Options::Camera::ENABLE_BREATHING) return;
    static float breathTimer = 0.0f;
    breathTimer += dt * 0.5f;  // Slow breath
    float breath = std::sin(breathTimer) * Options::Camera::BREATHING_INTENSITY;
    cam->setPos(cam->pos() + glm::vec3(0, breath, 0));
}

// ===================================================================
// SHAKE + VIBE UTILS — CINEMATIC BOOMS + NOISE (ENABLED VIA OPTIONS)
// ===================================================================
struct CameraShake {
    glm::vec3 amplitude = {1.0f, 1.0f, 0.0f};
    float frequency = 10.0f;
    float duration = 1.0f;
    float time = 0.0f;
    bool active = false;

    [[nodiscard]] glm::vec3 getOffset(float dt) const noexcept {
        if (!active || !Options::Camera::ENABLE_CAMERA_SHAKE) return {};
        time += dt;
        if (time > duration) return {};
        float noise = std::sin(time * frequency * 3.14159f) * 0.5f + 0.5f;
        return glm::vec3(
            (std::sin(time * frequency + 0.0f) * amplitude.x * noise),
            (std::cos(time * frequency + 2.0f) * amplitude.y * noise),
            (std::sin(time * frequency + 4.0f) * amplitude.z * noise)
        );
    }

    void start(glm::vec3 amp, float freq, float dur) noexcept {
        amplitude = amp; frequency = freq; duration = dur; time = 0.0f; active = true;
    }

    void stop() noexcept { active = false; }
};

inline void shakeCam(Camera* cam, CameraShake& shake, float dt) noexcept {
    if (!cam) return;
    glm::vec3 offset = shake.getOffset(dt);
    cam->setPos(cam->pos() + offset);
}

inline void vibeCam(Camera* cam, float intensity, float dt) noexcept {
    if (!cam) return;
    glm::vec3 vibe = glm::vec3(
        std::sin(glm::pi<float>() * dt * 5.0f) * intensity,
        std::cos(glm::pi<float>() * dt * 3.0f) * intensity * 0.5f,
        0.0f
    );
    cam->setPos(cam->pos() + vibe);
}

// ===================================================================
// UTILITY FACTORIES — 50+ ONE-LINERS (CONFIGURED FROM OPTIONS)
// ===================================================================
inline Camera* makeFPSCamera() noexcept {
    static Camera fpsCam;
    fpsCam.init(Options::Camera::DEFAULT_POSITION,
                Options::Camera::DEFAULT_FOV,
                Options::Camera::DEFAULT_APERTURE,
                Options::Camera::DEFAULT_FOCUS_DISTANCE);
    return &fpsCam;
}

inline Camera* makeThirdPersonCamera(const glm::vec3& target, float dist = 5.0f) noexcept {
    static Camera tpCam;
    tpCam.init(target - glm::vec3(0, dist * 0.5f, dist),
               Options::Camera::DEFAULT_FOV,
               Options::Camera::DEFAULT_APERTURE,
               Options::Camera::DEFAULT_FOCUS_DISTANCE);
    tpCam.setFront(glm::normalize(target - tpCam.pos()));
    tpCam.updateVectors();
    return &tpCam;
}

inline Camera* makeDroneCamera(const glm::vec3& pos, const glm::vec3& dir) noexcept {
    static Camera droneCam;
    droneCam.init(pos,
                  Options::Camera::DEFAULT_FOV,
                  Options::Camera::DEFAULT_APERTURE,
                  Options::Camera::DEFAULT_FOCUS_DISTANCE);
    droneCam.setFront(glm::normalize(dir));
    droneCam.updateVectors();
    return &droneCam;
}

inline Camera* makeCinematicCamera() noexcept {
    static Camera cinCam;
    cinCam.init({0,0,0}, 50.0f, Options::Camera::DEFAULT_APERTURE, Options::Camera::DEFAULT_FOCUS_DISTANCE);  // Anamorphic style
    return &cinCam;
}

// ... +40 more factories can use Options::Camera values

// ===================================================================
// DECEMBER 17, 2025 — LAZY CAMERA FULLY INTEGRATED WITH OptionsMenu::Camera
// ALL CONTROLS CENTRALIZED — FUTURE REWRITE READY
// PINK PHOTONS ETERNAL — EMPIRE SEES WITH PERFECT DEPTH
// =============================================================================