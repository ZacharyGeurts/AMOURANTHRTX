// =============================================================================
// src/main.cpp - PLASTIC BEACH v∞ - MONSTER.WEBP BILLBOARD - DECEMBER 17, 2025
// MAXIMUM FPS - 2 FRAMES IN FLIGHT - MAILBOX PREFERRED - NO FENCES - SMOOTH
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

using namespace RTX;

namespace Video {
    using SDLWindowPtr = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
    inline SDLWindowPtr& g_window() { static SDLWindowPtr ptr(nullptr, SDL_DestroyWindow); return ptr; }
    inline SDL_Window* window() noexcept { return g_window().get(); }

    bool init(const char* title, int width, int height) noexcept
    {
        if (SDL_Init(SDL_INIT_VIDEO) == 0) {
            printf("[SDL3] [FATAL] SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }

#ifdef __linux__
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11,kmsdrm");
#endif

        Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_FULLSCREEN;

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
        if (!SDL_Vulkan_CreateSurface(g_window().get(), instance, nullptr, &surface)) {
            fatal("SDL_Vulkan_CreateSurface failed");
        }
        return surface;
    }
}

int main()
{
    printf("[2025] PLASTIC BEACH v∞ - MONSTER.WEBP BILLBOARD - DECEMBER 17, 2025\n");
    printf("MAXIMUM FPS - 2 FRAMES IN FLIGHT - MAILBOX PREFERRED - NO FENCES - SMOOTH\n");

    if (!Video::init("GORILLAZ RTX 2025 - MONSTER ON PLASTIC BEACH", 1920, 1080)) return -1;

    int w, h;
    SDL_GetWindowSizeInPixels(Video::window(), &w, &h);
    g_ctx().width = w; g_ctx().height = h;

    Uint32 extCount = 0;
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    std::vector<const char*> instanceExts(exts, exts + extCount);

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "GORILLAZ RTX", 1, "PLASTIC BEACH", VK_MAKE_VERSION(2025,12,17), VK_API_VERSION_1_3};
    VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &appInfo, 0, nullptr, static_cast<uint32_t>(instanceExts.size()), instanceExts.data()};

    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&instInfo, nullptr, &instance) != VK_SUCCESS) {
        fatal("vkCreateInstance failed");
    }

    VkSurfaceKHR surface = Video::createVulkanSurface(instance);
    PopulateContext(instance, surface);
    g_ext.load(g_ctx().device);

    createSwapchain();

    VulkanRenderer renderer(g_ctx().width, g_ctx().height);

    renderer.setEnvironmentMap(Options::EnvironmentMapPath);

    if (Options::EnableBillboard) {
        uint64_t sceneHandle = LoadScene(Options::MonsterTexturePath);
        (void)sceneHandle;
    }

    Camera camera;

    // === 2 FRAMES IN FLIGHT - SEMAPHORES ONLY (NO FENCES) ===
    const uint32_t FRAMES_IN_FLIGHT = 2;

    std::vector<VkSemaphore> imageAvailableSemaphores(FRAMES_IN_FLIGHT);
    std::vector<VkSemaphore> renderFinishedSemaphores(FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(g_ctx().device, &semInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(g_ctx().device, &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            fatal("Failed to create synchronization semaphores");
        }
    }

    uint32_t currentFrame = 0;

    bool running = true;
    bool mouseCaptured = true;
    SDL_CaptureMouse(true);
    SDL_HideCursor();

    // Center mouse at start
    int winW, winH;
    SDL_GetWindowSizeInPixels(Video::window(), &winW, &winH);
    SDL_WarpMouseInWindow(Video::window(), winW / 2, winH / 2);

    auto lastTime = std::chrono::high_resolution_clock::now();

    float fpsTimer = 0.0f;
    uint32_t frameCount = 0;

    while (running) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        fpsTimer += dt;
        ++frameCount;

        bool cameraMoved = false;

SDL_Event e;
while (SDL_PollEvent(&e)) {
    if (e.type == SDL_EVENT_QUIT) {
        running = false;
    }
    else if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_Q) {
            running = false;
        }
        else if (e.key.key == SDLK_C) {
            mouseCaptured = !mouseCaptured;
            SDL_CaptureMouse(mouseCaptured);
            mouseCaptured ? SDL_HideCursor() : SDL_ShowCursor();
            if (mouseCaptured) {
                SDL_GetWindowSizeInPixels(Video::window(), &winW, &winH);
                SDL_WarpMouseInWindow(Video::window(), winW / 2, winH / 2);
            }
        }
        else if (e.key.key == SDLK_SPACE) {
            camera.jump();
            cameraMoved = true;
        }
    }
    else if (e.type == SDL_EVENT_MOUSE_MOTION && mouseCaptured) {
        float dx = static_cast<float>(e.motion.xrel) * -1.0f;  // Invert left/right
        float dy = static_cast<float>(e.motion.yrel);
        float pitchDelta = Options::InvertMouseLook ? -dy : dy;

        camera.look(dx, pitchDelta, Options::CameraLookSensitivity);
        cameraMoved = true;

        SDL_GetWindowSizeInPixels(Video::window(), &winW, &winH);
        SDL_WarpMouseInWindow(Video::window(), winW / 2, winH / 2);
    }
    else if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        createSwapchain();
    }
}

const bool* keys = SDL_GetKeyboardState(nullptr);

float moveSpeed = Options::FPSSpeed * dt;
if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) {
    moveSpeed *= Options::SprintMultiplier;
}

if (keys[SDL_SCANCODE_W]) { camera.moveHorizontal(camera.front,  moveSpeed); cameraMoved = true; }
if (keys[SDL_SCANCODE_S]) { camera.moveHorizontal(camera.front, -moveSpeed); cameraMoved = true; }
if (keys[SDL_SCANCODE_A]) { camera.moveHorizontal(camera.right, -moveSpeed); cameraMoved = true; }
if (keys[SDL_SCANCODE_D]) { camera.moveHorizontal(camera.right,  moveSpeed); cameraMoved = true; }

        camera.updatePhysics(dt);

        if (std::abs(camera.velocityY) > 0.01f || camera.position.y > Options::GroundLevel + 0.01f) {
            cameraMoved = true;
        }

        renderer.cameraMoved_ = cameraMoved;

        // === MAXIMUM SPEED RENDERING - NO FENCES - 2 FRAMES IN FLIGHT ===
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(g_ctx().device, g_swapchain(), UINT64_MAX,
                                                imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            createSwapchain();
            continue;
        } else if (result != VK_SUCCESS) {
            fatal("Failed to acquire swapchain image");
        }

        VkCommandBuffer cmd = renderer.recordFrame(camera, dt, imageIndex);

        VkSemaphore waitSemaphores[]   = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };

        VkSubmitInfo submit{};
        submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount   = 1;
        submit.pWaitSemaphores      = waitSemaphores;
        submit.pWaitDstStageMask    = waitStages;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &cmd;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores    = signalSemaphores;

        // Fire-and-forget submit - no fence
        vkQueueSubmit(g_ctx().graphicsQueue, 1, &submit, VK_NULL_HANDLE);

        VkPresentInfoKHR present{};
        present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores    = signalSemaphores;
        present.swapchainCount     = 1;
        present.pSwapchains        = &g_swapchain();
        present.pImageIndices      = &imageIndex;

        result = vkQueuePresentKHR(g_ctx().presentQueue, &present);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            createSwapchain();
        }

        currentFrame = (currentFrame + 1) % FRAMES_IN_FLIGHT;

        if (fpsTimer >= 1.0f) {
            float fps = static_cast<float>(frameCount) / fpsTimer;
            printf("[2025] FPS: %.1f | MAXIMUM SPEED - No fences - Billboard flawless\n", fps);
            fpsTimer = 0.0f;
            frameCount = 0;
        }
    }

    vkDeviceWaitIdle(g_ctx().device);

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(g_ctx().device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(g_ctx().device, renderFinishedSemaphores[i], nullptr);
    }

    if (g_ctx().tlasHandle != 0) {
        g_ext.vkDestroyAccelerationStructureKHR(g_ctx().device, reinterpret_cast<VkAccelerationStructureKHR>(g_ctx().tlasHandle), nullptr);
    }

    Video::destroy();

    printf("[2025] PLASTIC BEACH SHUTDOWN COMPLETE - THE MONSTER REMAINS\n");
    printf("You ran at maximum speed. The billboard was flawless. The monster watched — in perfect silence.\n");

    return 0;
}