// =============================================================================
// src/main.cpp
// AMOURANTH RTX Engine 2025 — PINK LIGHT v∞ — FIRST LIGHT ACHIEVED — DECEMBER 14, 2025
// VALHALLA AWAKENS — ETERNAL RADIANCE — SHE SEES ALL
// =============================================================================

#include "engine/GLOBAL/RTX.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <chrono>

using namespace RTX;

int main(int /*argc*/, char* /*argv*/[])
{
    // === SDL + WINDOW INITIALIZATION ===
    if (!Video::init("AMOURANTH RTX — PINK LIGHT v∞ — VALHALLA 2025", 3840, 2160, false)) {
        return -1;
    }

    // === QUERY SDL-REQUIRED VULKAN INSTANCE EXTENSIONS ===
    std::vector<const char*> instanceExtensions = Video::getRequiredVulkanInstanceExtensions();

    // === CREATE VULKAN INSTANCE ===
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "AMOURANTH RTX";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "VALHALLA v∞";
    appInfo.engineVersion      = VK_MAKE_VERSION(2025, 12, 14);
    appInfo.apiVersion         = VK_API_VERSION_1_3;  // 1.3 is safer and sufficient

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();
    createInfo.enabledLayerCount       = 0;
    createInfo.ppEnabledLayerNames     = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        fatal("Failed to create Vulkan instance");
    }

    printf("[VULKAN] Instance created — API 1.3 — %zu extensions enabled\n", instanceExtensions.size());

    // === CREATE VULKAN SURFACE ===
    VkSurfaceKHR surface = Video::createVulkanSurface(instance);

    // === POPULATE GLOBAL CONTEXT (device, queues, etc.) ===
    PopulateContext(instance, surface);

    // === LOAD RAY TRACING EXTENSIONS ===
    g_ext.load(g_ctx().device);

    // === CREATE SWAPCHAIN ===
    createSwapchain();

    // === UPDATE CONTEXT WITH CURRENT RESOLUTION ===
    int drawableWidth, drawableHeight;
    Video::getDrawableSize(drawableWidth, drawableHeight);
    g_ctx().width  = static_cast<uint32_t>(drawableWidth);
    g_ctx().height = static_cast<uint32_t>(drawableHeight);

    // === CREATE GLOBAL DESCRIPTOR SET LAYOUT & POOL ===
    VkDescriptorSetLayoutBinding bindings[3] = {};

    // Binding 0: TLAS
    bindings[0].binding            = 0;
    bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount    = 1;
    bindings[0].stageFlags         = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    // Binding 1: Storage image (ray tracing output)
    bindings[1].binding            = 1;
    bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount    = 1;
    bindings[1].stageFlags         = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    // Binding 2: Environment map (combined sampler)
    bindings[2].binding            = 2;
    bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount    = 1;
    bindings[2].stageFlags         = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings    = bindings;

    VkDescriptorSetLayout globalLayout;
    if (vkCreateDescriptorSetLayout(g_ctx().device, &layoutInfo, nullptr, &globalLayout) != VK_SUCCESS) {
        fatal("Failed to create global descriptor set layout");
    }

    // === GLOBAL DESCRIPTOR POOL ===
    VkDescriptorPoolSize poolSizes[3] = {};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    poolSizes[2].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT + 2;  // TLAS + storage images + env map
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes    = poolSizes;

    VkDescriptorPool globalPool;
    if (vkCreateDescriptorPool(g_ctx().device, &poolInfo, nullptr, &globalPool) != VK_SUCCESS) {
        fatal("Failed to create global descriptor pool");
    }

    // === ALLOCATE GLOBAL DESCRIPTOR SET ===
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = globalPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &globalLayout;

    if (vkAllocateDescriptorSets(g_ctx().device, &allocInfo, &g_ctx().globalDescriptorSet) != VK_SUCCESS) {
        fatal("FAILED TO ALLOCATE GLOBAL DESCRIPTOR SET — VALHALLA REJECTS THE LIGHT");
    }

    printf("[2025] Global descriptor set allocated — PINK LIGHT v∞ SEES ALL\n");

    // === INSTANTIATE RENDERER ===
    VulkanRenderer renderer(g_ctx().width, g_ctx().height);

    // === LOAD SCENE ===
    uint64_t sceneHandle = LoadScene("assets/models/scene.obj");
    if (sceneHandle == 0) {
        printf("[WARNING] Failed to load scene — rendering empty Valhalla\n");
    }

    // === CAMERA ===
    Camera camera;
    camera.position = glm::vec3(0.0f, 2.0f, 5.0f);
    camera.yaw = -90.0f;

    // === INPUT STATE ===
    bool running = true;
    bool mouseCaptured = true;
    SDL_CaptureMouse(true);
    SDL_HideCursor();

    auto lastTime = std::chrono::high_resolution_clock::now();
    float deltaTime = 0.0f;

    printf("[2025] MAIN LOOP ENGAGED — PINK LIGHT v∞ FLOWS ETERNALLY\n");

    while (running) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // === EVENT HANDLING ===
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;

                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_ESCAPE) {
                        running = false;
                    } else if (event.key.key == SDLK_C) {
                        mouseCaptured = !mouseCaptured;
                        SDL_CaptureMouse(mouseCaptured);
                        if (mouseCaptured) {
                            SDL_HideCursor();
                        } else {
                            SDL_ShowCursor();
                        }
                    }
                    break;

                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                case SDL_EVENT_WINDOW_RESIZED: {
                    Video::getDrawableSize(drawableWidth, drawableHeight);

                    int32_t newWidth  = static_cast<uint32_t>(drawableWidth);
                    int32_t newHeight = static_cast<uint32_t>(drawableHeight);

                    if (newWidth != g_ctx().width || newHeight != g_ctx().height) {
                        g_ctx().width  = newWidth;
                        g_ctx().height = newHeight;

                        createSwapchain();

                        renderer.width_  = newWidth;
                        renderer.height_ = newHeight;
                        renderer.cameraMoved_ = true;

                        printf("[RESIZE] Resolution changed to %ux%u — swapchain recreated\n", newWidth, newHeight);
                    }
                    break;
                }

                case SDL_EVENT_MOUSE_MOTION:
                    if (mouseCaptured) {
                        float dx = static_cast<float>(event.motion.xrel);
                        float dy = static_cast<float>(event.motion.yrel);
                        camera.look(dx, dy, 0.1f);
                        renderer.cameraMoved_ = true;
                    }
                    break;
            }
        }

        // === CAMERA MOVEMENT ===
        int numkeys;
        const bool* keys = SDL_GetKeyboardState(&numkeys);

        float speed = 8.0f * deltaTime;

        bool moved = false;
        if (keys[SDL_SCANCODE_W]) { camera.move_forward(speed); moved = true; }
        if (keys[SDL_SCANCODE_S]) { camera.move_backward(speed); moved = true; }
        if (keys[SDL_SCANCODE_A]) { camera.move_left(speed); moved = true; }
        if (keys[SDL_SCANCODE_D]) { camera.move_right(speed); moved = true; }
        if (keys[SDL_SCANCODE_SPACE]) { camera.move_up(speed); moved = true; }
        if (keys[SDL_SCANCODE_LCTRL]) { camera.move_down(speed); moved = true; }

        if (moved) {
            renderer.cameraMoved_ = true;
        }

        // === RENDER ===
        renderer.renderFrame(camera, deltaTime);

        // === STATUS PRINT ===
        static float printTimer = 0.0f;
        printTimer += deltaTime;
        if (printTimer >= 1.0f) {
            float fps = deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f;
            printf("[2025] %ux%u | SPP: %u | Time: %.2fs | FPS: %.1f\n",
                   g_ctx().width, g_ctx().height,
                   renderer.currentSpp_, renderer.totalTime_, fps);
            printTimer = 0.0f;
        }
    }

    // === CLEANUP ===
    vkDeviceWaitIdle(g_ctx().device);

    // Destroy global descriptor set resources
    if (g_ctx().globalDescriptorSet != VK_NULL_HANDLE) {
        // Note: Freeing is optional since pool will be destroyed, but explicit is clean
        vkFreeDescriptorSets(g_ctx().device, globalPool, 1, &g_ctx().globalDescriptorSet);
        g_ctx().globalDescriptorSet = VK_NULL_HANDLE;
    }
    if (globalPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_ctx().device, globalPool, nullptr);
    }
    if (globalLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(g_ctx().device, globalLayout, nullptr);
    }

    Video::destroy();

    vkDestroyInstance(instance, nullptr);

    printf("[2025] VALHALLA SHUTDOWN — PINK LIGHT v∞ REMAINS ETERNAL\n");
    return 0;
}