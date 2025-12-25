// =============================================================================
// src/main.cpp - PLASTIC BEACH v∞ - CHRISTMAS EVE 2025 - FULL CONTROLLER SUPPORT
// MAXIMUM FPS - 2 FRAMES IN FLIGHT - MAILBOX PREFERRED - NO FENCES - SMOOTH
// FULLY DATA-DRIVEN VIA OPTIONS.HPP - DAY/NIGHT CYCLE - PULSING ATMOSPHERE
// CAMERA BOBBLE - ANIMATED POINT LIGHTS - F1 DEBUG CAROUSEL - F11 FULLSCREEN
// XBOX / PLAYSTATION / GENERIC GAMEPAD SUPPORT - VIBRATION - DEADZONE
// =============================================================================

#define GLM_ENABLE_EXPERIMENTAL

#include "main.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <stb/stb_image.h>
#include <cstdio>
#include <chrono>
#include <vector>
#include <memory>
#include <cmath>

using namespace RTX;

namespace Video {
    using SDLWindowPtr = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
    inline SDLWindowPtr& g_window() { static SDLWindowPtr ptr(nullptr, SDL_DestroyWindow); return ptr; }
    inline SDL_Window* window() noexcept { return g_window().get(); }

    bool init(const char* title, int width, int height) noexcept
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) == 0) {  // Added GAMEPAD init
            printf("[SDL3] [FATAL] SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }

#ifdef __linux__
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11,kmsdrm");
#endif

        Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        if (Options::StartFullscreen) flags |= SDL_WINDOW_FULLSCREEN;

        SDL_Window* win = SDL_CreateWindow(title ? title : "GORILLAZ RTX 2025 - PLASTIC BEACH", width, height, flags);
        if (!win) {
            printf("[SDL3] [FATAL] SDL_CreateWindow failed: %s\n", SDL_GetError());
            return false;
        }

        g_window().reset(win);
        printf("[SDL3] Window opened - %dx%d - maximum performance mode active\n", width, height);
        return true;
    }

    void destroy() noexcept { g_window().reset(); SDL_Quit(); }

    VkSurfaceKHR createVulkanSurface(VkInstance instance) noexcept
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (SDL_Vulkan_CreateSurface(g_window().get(), instance, nullptr, &surface) == 0) {
            fatal("SDL_Vulkan_CreateSurface failed");
        }
        return surface;
    }
}

struct PointLight {
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
    float radius;
};

int main()
{
    printf("[2025] PLASTIC BEACH v∞ - CHRISTMAS EVE 2025 - FULL CONTROLLER SUPPORT\n");

    if (Video::init("GORILLAZ RTX 2025 - PLASTIC BEACH", 1920, 1080) == 0) return -1;

    int w, h;
    SDL_GetWindowSizeInPixels(Video::window(), &w, &h);
    g_ctx().width = w;
    g_ctx().height = h;

    Uint32 extCount = 0;
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    std::vector<const char*> instanceExts(exts, exts + extCount);

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "GORILLAZ RTX", 1, "PLASTIC BEACH", VK_MAKE_VERSION(2025,12,24), VK_API_VERSION_1_3};
    VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &appInfo, 0, nullptr, static_cast<uint32_t>(instanceExts.size()), instanceExts.data()};

    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&instInfo, nullptr, &instance) != VK_SUCCESS) {
        fatal("vkCreateInstance failed");
    }

    VkSurfaceKHR surface = Video::createVulkanSurface(instance);
    PopulateContext(instance, surface);
    g_ext().load(g_ctx().device);

    createSwapchain();

    VulkanRenderer renderer(g_ctx().width, g_ctx().height);
    renderer.setEnvironmentMap(Options::EnvironmentMapPath);

    Camera camera;

    std::vector<PointLight> pointLights = {
        {{ 15.0f,  5.0f,  10.0f}, {1.0f, 0.2f, 0.3f}, 15.0f, 20.0f},
        {{ -8.0f,  8.0f, -12.0f}, {0.2f, 1.0f, 0.3f}, 18.0f, 25.0f},
        {{  0.0f, 10.0f,   0.0f}, {0.3f, 0.4f, 1.0f}, 20.0f, 30.0f},
        {{-20.0f,  6.0f,   5.0f}, {1.0f, 0.7f, 0.2f}, 12.0f, 18.0f},
        {{ 12.0f,  7.0f, -18.0f}, {0.8f, 0.2f, 1.0f}, 16.0f, 22.0f},
        {{  5.0f, 12.0f,  15.0f}, {1.0f, 1.0f, 0.4f}, 14.0f, 20.0f},
        {{ 25.0f,  4.0f,  -5.0f}, {0.5f, 0.5f, 1.0f}, 17.0f, 24.0f},
        {{-15.0f,  9.0f,   8.0f}, {1.0f, 0.5f, 0.0f}, 13.0f, 19.0f},
        {{ 10.0f,  6.0f,  20.0f}, {0.0f, 1.0f, 1.0f}, 19.0f, 28.0f},
    };

    const uint32_t FRAMES_IN_FLIGHT = 2;

    std::vector<VkSemaphore> imageAvailableSemaphores(FRAMES_IN_FLIGHT);
    std::vector<VkSemaphore> renderFinishedSemaphores(FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        vkCreateSemaphore(g_ctx().device, &semInfo, nullptr, &imageAvailableSemaphores[i]);
        vkCreateSemaphore(g_ctx().device, &semInfo, nullptr, &renderFinishedSemaphores[i]);
    }

    uint32_t currentFrame = 0;

    bool running = true;
    bool mouseCaptured = Options::StartWithMouseCapture;

    // Controller state
    SDL_Gamepad* gamepad = nullptr;
    SDL_JoystickID gamepadInstanceID = -1;  // Store the instance ID for comparison
    bool controllerConnected = false;
    float controllerDeadzone = 0.15f;
    float controllerLookSensitivity = Options::CameraLookSensitivity * 0.8f; // Slightly reduced for thumbsticks

    // Initial mouse capture
    if (mouseCaptured) {
        SDL_SetWindowRelativeMouseMode(Video::window(), true);
        SDL_CaptureMouse(true);
        SDL_HideCursor();
        int winW, winH;
        SDL_GetWindowSizeInPixels(Video::window(), &winW, &winH);
        SDL_WarpMouseInWindow(Video::window(), winW / 2, winH / 2);
    } else {
        SDL_ShowCursor();
    }

    auto lastTime = std::chrono::high_resolution_clock::now();

    float logTimer = 0.0f;
    uint32_t frameCount = 0;
    float totalTime = 0.0f;

    float skyRotation = Options::EnvironmentRotationY;
    float fogPulse = 0.0f;
    int debugMode = 0;

    while (running) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        totalTime += dt;

        logTimer += dt;
        ++frameCount;

        bool cameraMoved = false;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_Q) {
                    running = false;
                } else if (e.key.key == SDLK_C) {
                    mouseCaptured = !mouseCaptured;
                    SDL_SetWindowRelativeMouseMode(Video::window(), mouseCaptured);
                    SDL_CaptureMouse(mouseCaptured);
                    if (mouseCaptured) {
                        SDL_HideCursor();
                        int winW, winH;
                        SDL_GetWindowSizeInPixels(Video::window(), &winW, &winH);
                        SDL_WarpMouseInWindow(Video::window(), winW / 2, winH / 2);
                    } else {
                        SDL_ShowCursor();
                    }
                } else if (e.key.key == SDLK_SPACE) {
                    camera.jump();
                    cameraMoved = true;
                } else if (e.key.key == SDLK_F1) {
                    debugMode = (debugMode + 1) % 5;
                    Options::ShowNormals          = (debugMode == 1);
                    Options::ShowUVs              = (debugMode == 2);
                    Options::ShowWireframe        = (debugMode == 3);
                    Options::ForceEnvironmentOnly = (debugMode == 4);
                    Options::ShowHotPinkOnHit     = (debugMode == 0);
                } else if (e.key.key == SDLK_F11) {
                    Uint32 flags = SDL_GetWindowFlags(Video::window());
                    bool fullscreen = flags & SDL_WINDOW_FULLSCREEN;
                    SDL_SetWindowFullscreen(Video::window(), !fullscreen);
                }
            } else if (e.type == SDL_EVENT_MOUSE_MOTION && mouseCaptured) {
                float dx = static_cast<float>(e.motion.xrel);
                float dy = static_cast<float>(e.motion.yrel);
                camera.look(dx, dy, Options::CameraLookSensitivity);
                cameraMoved = true;
            } else if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                createSwapchain();
                renderer.width_ = g_ctx().width;
                renderer.height_ = g_ctx().height;
            }
            // Controller connect/disconnect
            else if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
                if (!gamepad) {
                    gamepad = SDL_OpenGamepad(e.gdevice.which);
                    if (gamepad) {
                        gamepadInstanceID = e.gdevice.which;  // Store the instance ID
                        controllerConnected = true;
                        printf("[CONTROLLER] Connected: %s\n", SDL_GetGamepadName(gamepad));
                        // Optional: small vibration feedback on connect
                        SDL_RumbleGamepad(gamepad, 0x3333, 0x3333, 300);
                    }
                }
            } else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
                if (gamepad && e.gdevice.which == gamepadInstanceID) {
                    printf("[CONTROLLER] Disconnected\n");
                    SDL_CloseGamepad(gamepad);
                    gamepad = nullptr;
                    gamepadInstanceID = -1;
                    controllerConnected = false;
                }
            }
        }

        // === KEYBOARD INPUT ===
        const bool* keys = SDL_GetKeyboardState(nullptr);

        float moveSpeed = Options::FPSSpeed * dt;
        if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) moveSpeed *= Options::SprintMultiplier;

        glm::vec3 moveDir(0.0f);
        if (keys[SDL_SCANCODE_W]) moveDir += camera.front;
        if (keys[SDL_SCANCODE_S]) moveDir -= camera.front;
        if (keys[SDL_SCANCODE_A]) moveDir -= camera.right;
        if (keys[SDL_SCANCODE_D]) moveDir += camera.right;

        // === CONTROLLER INPUT ===
        if (controllerConnected && gamepad) {
            // Left stick - movement
            float lx = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
            float ly = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;

            if (std::abs(lx) < controllerDeadzone) lx = 0.0f;
            if (std::abs(ly) < controllerDeadzone) ly = 0.0f;

            moveDir -= camera.right * lx;
            moveDir += camera.front * -ly;  // Y is inverted on most controllers

            // Right stick - look
            float rx = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
            float ry = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;

            if (std::abs(rx) >= controllerDeadzone || std::abs(ry) >= controllerDeadzone) {
                float lookX = rx * controllerLookSensitivity * 300.0f * dt;  // scaled by dt
                float lookY = ry * controllerLookSensitivity * 300.0f * dt;
                camera.look(lookX, lookY);
                cameraMoved = true;
            }

            // Jump - A button (Xbox) / Cross (PS)
            if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH)) {
                if (camera.onGround) {
                    camera.jump();
                    cameraMoved = true;
                    SDL_RumbleGamepad(gamepad, 0x4000, 0x4000, 150);  // Jump feedback
                }
            }

            // Sprint - Left trigger > 50%
            float lt = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
            if (lt > 0.5f) moveSpeed *= Options::SprintMultiplier;
        }

        bool isMoving = glm::length(moveDir) > 0.0f;
        if (isMoving) {
            camera.moveHorizontal(glm::normalize(moveDir), moveSpeed);
            cameraMoved = true;
        }

        camera.updatePhysics(dt);

        if (std::abs(camera.velocityY) > 0.01f) cameraMoved = true;

        camera.applyBobble(totalTime, isMoving);

        renderer.cameraMoved_ = cameraMoved;

        // Animate point lights
        for (auto& light : pointLights) {
            light.position.y += std::sin(totalTime * Options::LightBobSpeed + light.radius) * Options::LightBobAmplitude;
            float orbit = totalTime * Options::LightOrbitSpeed;
            light.position.x += std::sin(orbit + light.intensity) * Options::LightOrbitAmplitude;
            light.position.z += std::cos(orbit + light.intensity) * Options::LightOrbitAmplitude;

            float pulse = static_cast<float>(std::sin(totalTime * Options::LightColorPulseSpeed)) * Options::LightColorPulseAmount;
            light.color.r = glm::clamp(light.color.r + pulse, 0.2f, 1.0f);
            light.color.g = glm::clamp(light.color.g + pulse * 0.7f, 0.2f, 1.0f);
            light.color.b = glm::clamp(light.color.b + pulse * 1.3f, 0.2f, 1.0f);
        }

        // Day-night cycle
        skyRotation += dt * Options::SkyRotationSpeed;
        Options::EnvironmentRotationY = skyRotation;

        // Pulsing fog
        fogPulse += dt * Options::FogPulseSpeed;
        Options::SkyIntensity = 1.0f + Options::FogPulseAmount * std::sin(fogPulse);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(g_ctx().device, g_swapchain(), UINT64_MAX,
                                                imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            createSwapchain();
            renderer.width_ = g_ctx().width;
            renderer.height_ = g_ctx().height;
            continue;
        } else if (result != VK_SUCCESS) {
            fatal("Failed to acquire swapchain image");
        }

        VkCommandBuffer cmd = renderer.recordFrame(camera, dt, imageIndex);

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};

        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = waitSemaphores;
        submit.pWaitDstStageMask = waitStages;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = signalSemaphores;

        vkQueueSubmit(g_ctx().graphicsQueue, 1, &submit, VK_NULL_HANDLE);

        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = signalSemaphores;
        present.swapchainCount = 1;
        present.pSwapchains = &g_swapchain();
        present.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(g_ctx().presentQueue, &present);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            createSwapchain();
            renderer.width_ = g_ctx().width;
            renderer.height_ = g_ctx().height;
        }

        currentFrame = (currentFrame + 1) % FRAMES_IN_FLIGHT;

        if (logTimer >= Options::StatusLogInterval) {
            float fps = frameCount / logTimer;
            const char* input = controllerConnected ? "CONTROLLER" : (mouseCaptured ? "MOUSE+KB" : "KEYBOARD");
            printf("[2025] STATUS | FPS: %.1f | SPP: %u | Pos: (%.2f, %.2f, %.2f) | Yaw: %.1f° Pitch: %.1f° | %s | Input: %s | Lights: %zu\n",
                   fps, renderer.currentSpp_, camera.position.x, camera.position.y, camera.position.z,
                   camera.yaw, camera.pitch, camera.onGround ? "On Ground" : "Airborne", input, pointLights.size());

            logTimer = 0.0f;
            frameCount = 0;
        }
    }

    vkDeviceWaitIdle(g_ctx().device);

    if (gamepad) SDL_CloseGamepad(gamepad);

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(g_ctx().device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(g_ctx().device, renderFinishedSemaphores[i], nullptr);
    }

    Video::destroy();

    printf("[2025] PLASTIC BEACH SHUTDOWN COMPLETE - MERRY CHRISTMAS\n");
    printf("The snow falls gently. The lights twinkle. The monster smiles — in perfect silence.\n");

    return 0;
}