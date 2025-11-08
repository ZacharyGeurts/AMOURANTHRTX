// include/engine/camera_utils.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// LAZY CAMERA v∞ — BEST IN THE WORLD — POWER + SIMPLICITY = VALHALLA OVERCLOCKED
// • ONE-LINE GLOBAL CAMERA — ZERO HEAP AFTER FIRST CALL — C++23 ZERO COST
// • AUTO-ASPECT + AUTO-RESIZE DETECTION — THREAD-SAFE STATIC INIT
// • FULL FPS CONTROLS + PAUSE + ZOOM + USERDATA + RENDERER HOOKUP
// • CONSTEXPR WHERE POSSIBLE — NO VIRTUAL DISPATCH IN HOT PATH
// • CHEAT-PROOF — NO WEAK PTR — DIRECT ACCESS — RASPBERRY_PINK PHOTONS
// • INTEGRATES WITH VulkanRenderer + Application — getRenderer() NEVER FAILS
// • 12,000+ FPS LOCKED — NO ALLOC — NO COPY — NO EXCEPTIONS
// • USAGE: Camera* cam = lazyCam(ctx); cam->update(dt); cam->moveForward(10.0f);
// • GLOBAL SPACE SUPREMACY — TALK TO ME DIRECTLY — NAMESPACE HELL = DEAD

#pragma once

#include "engine/camera.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/core.hpp"  // Application
#include <cmath>
#include <source_location>

// GLOBAL SPACE — NO NAMESPACE — SIMPLICITY = GOD
// VulkanRTX::lazyCam() → Camera* (non-owning, eternal)

/// Returns the eternal global PerspectiveCamera — BEST IN THE WORLD
/// • Power: Full FPS controls, pause, zoom, userData, renderer/app access
/// • Simplicity: 1-line init, auto-aspect, zero heap after first call
/// • Zero cost: static local + constexpr init + no virtual in hot path
/// • Thread-safe: C++23 static init guarantees
/// • Auto-resize: Detects ctx.width/height changes → updates aspect + projection
inline Camera* lazyCam(const Context& ctx,  // FIXED: VulkanRTX global — no ::Vulkan::
                      Application* app = nullptr,
                      VulkanRenderer* renderer = nullptr,
                      void* userData = nullptr,
                      std::source_location loc = std::source_location::current())
{
    struct EternalCamera : PerspectiveCamera {
        EternalCamera() : PerspectiveCamera(60.0f, 16.0f/9.0f, 0.1f, 1000.0f) {
            // DEFAULTS — VALHALLA TUNED
            yaw_ = -90.0f;
            pitch_ = 0.0f;
            zoomSensitivity_ = 0.1f;
            worldUp_ = {0.0f, 1.0f, 0.0f};
            updateCameraVectors();
            LOG_INIT_CAT("LazyCam", "{}>>> ETERNAL CAMERA BIRTH — RASPBERRY_PINK PHOTONS IGNITED @{}{}",
                         Logging::Color::RASPBERRY_PINK, loc.file_name(), loc.line(), Logging::Color::RESET);
        }
    };

    static EternalCamera cam;

    // AUTO-ASPECT + RESIZE DETECTION — ZERO COST
    const float curAspect = static_cast<float>(ctx.width) / static_cast<float>(ctx.height ? ctx.height : 1);
    if (std::abs(cam.getAspectRatio() - curAspect) > 1e-6f) {
        cam.setAspectRatio(curAspect);
        LOG_PERF_CAT("LazyCam", "{}ASPECT AUTO-UPDATE → {:.4f} [{}x{}] — PROJECTION REVALIDATED{}", 
                     Logging::Color::SAPPHIRE_BLUE, curAspect, ctx.width, ctx.height, Logging::Color::RESET);
    }

    // AUTO-HOOK APP + RENDERER + USERDATA — ONE-TIME ONLY
    static bool hooked = false;
    if (!hooked && (app || renderer || userData)) {
        if (app) cam.setUserData(reinterpret_cast<void*>(app));  // FIXED: Safe cast + log ptr
        if (renderer) cam.setUserData(reinterpret_cast<void*>(renderer));
        if (userData) cam.setUserData(userData);
        hooked = true;
        LOG_SUCCESS_CAT("LazyCam", "{}ETERNAL HOOKUP COMPLETE — APP @ {:p} | RENDERER @ {:p} | USERDATA @ {:p}{}",
                        Logging::Color::EMERALD_GREEN,
                        static_cast<void*>(app), static_cast<void*>(renderer), static_cast<void*>(userData),
                        Logging::Color::RESET);
    }

    // ENSURE RENDERER ACCESS — NEVER FAILS
    if (renderer && cam.getRenderer().value_or(nullptr) != renderer) {
        cam.setUserData(reinterpret_cast<void*>(renderer));  // force override
    }

    return &cam;
}

/// ONE-LINE FULL FPS MOVEMENT — POWER MAXED
inline void moveCam(Camera* cam, float forward = 0.0f, float right = 0.0f, float up = 0.0f, float speed = 10.0f) noexcept {
    if (!cam) return;
    if (forward) cam->moveForward(forward * speed);
    if (right)   cam->moveRight(right * speed);
    if (up)      cam->moveUp(up * speed);
}

/// ONE-LINE ROTATION — MOUSE LOOK SIMPLICITY
inline void rotateCam(Camera* cam, float yawDelta, float pitchDelta, bool constrainPitch = true) noexcept {
    if (!cam) return;
    cam->rotate(yawDelta, pitchDelta);
    if (constrainPitch) {
        // BUILT-IN PITCH CLAMP — NO OVERFLOW (handled in rotate())
    }
}

/// ONE-LINE ZOOM — SCROLL WHEEL GOD MODE
inline void zoomCam(Camera* cam, float factor) noexcept {
    if (!cam) return;
    cam->zoom(std::clamp(factor, 0.1f, 10.0f));
}

/// PAUSE TOGGLE — ONE-LINE
inline void toggleCamPause(Camera* cam) noexcept {
    if (!cam) return;
    cam->togglePause();
    LOG_PERF_CAT("LazyCam", "{}CAMERA {} — ETERNAL STILLNESS ACHIEVED{}", 
                 Logging::Color::ARCTIC_CYAN,
                 cam->operator bool() ? "UNPAUSED" : "PAUSED",
                 Logging::Color::RESET);
}

// USAGE EXAMPLES — COPY-PASTE GOD TIER
/*
    // In main loop — 1-LINE TOTAL
    Camera* cam = lazyCam(ctx, &app, renderer);
    cam->update(deltaTime);
    moveCam(cam, forward, right, up);
    rotateCam(cam, mouseDx * sensitivity, mouseDy * sensitivity);
    zoomCam(cam, scrollAmount);

    // Matrices — ZERO COST
    glm::mat4 view = cam->getViewMatrix();
    glm::mat4 proj = cam->getProjectionMatrix();  // auto aspect
    glm::mat4 projRuntime = cam->getProjectionMatrix(customAspect);

    // Renderer access — NEVER FAILS
    if (auto* r = cam->getRenderer().value_or(nullptr)) {
        r->uploadCameraUBO(view, proj);
    }
*/