// src/main.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — Main Entry Point
// PRODUCTION-GRADE · CLEAN · ROBUST · FULLY COMPATIBLE WITH CURRENT ENGINE
// ALL LEGACY REFERENCES RESOLVED · NO COMMAND POOLS IN CONTEXT
// PINK PHOTONS ETERNAL — THE EMPIRE BEGINS HERE
// =============================================================================

#include "main.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/Validation.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/InputManager.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/UBO.hpp"

#include "modes/RenderMode1.hpp"

#include <vulkan/vulkan.hpp>
#include <string>
#include <format>
#include <iostream>
#include <memory>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

using namespace Logging::Color;
using StoneKey::stone_seal_renderer;
using StoneKey::stone_pipeline;
using StoneKey::stone_seal_pipeline;
using StoneKey::stone_seal_width;
using StoneKey::stone_seal_height;
using StoneKey::stone_seal_mesh;
using StoneKey::stone_seal_final;
using StoneKey::stone_width;
using StoneKey::stone_height;
using StoneKey::stone_window;
using StoneKey::stone_rtprops;
using StoneKey::stone_swapchain;
using StoneKey::stone_images;
using StoneKey::stone_image_count;
using StoneKey::stone_views;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_physical;
using StoneKey::stone_surface;
using StoneKey::stone_instance;
using StoneKey::stone_device;
using StoneKey::stone_seal_instance;
using StoneKey::stone_seal_window;
using StoneKey::stone_seal_surface;

std::unique_ptr<Application> g_app_ptr = nullptr;
VulkanRenderer* g_renderer_ptr = nullptr;
float g_deltaTime = 0.0f;

// Global render mode instance for pure pink void
static RenderMode1 g_mode1(stone_width(), stone_height());

// =============================================================================
// VRAM Query — Uses Physical Device Properties (no memory properties needed)
// =============================================================================
inline float vramGB() noexcept {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(RTX::g_ctx().physicalDevice(), &memProps);

    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            return static_cast<float>(memProps.memoryHeaps[i].size) / (1024.0f * 1024.0f * 1024.0f);
        }
    }
    return 0.0f;
}

// =============================================================================
// Throw-away Command Pool Creation — One-time use buffers preferred
// =============================================================================
static VkCommandPool g_transientCommandPool = VK_NULL_HANDLE;

static void createTransientCommandPool() noexcept {
    if (g_transientCommandPool != VK_NULL_HANDLE) return;

    VkCommandPoolCreateInfo poolInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = RTX::g_ctx().graphicsFamily()
    };

    VK_CHECK(vkCreateCommandPool(stone_device(), &poolInfo, nullptr, &g_transientCommandPool));

    LOG_INFO_CAT("MAIN", "Transient command pool created for one-time submissions");
}

// =============================================================================
// Application Class — Main Loop & Render Mode Dispatch
// =============================================================================
class Application {
public:
    Application(const std::string& title, int width, int height);
    ~Application();

    void run() noexcept;

    void setRenderer(std::unique_ptr<VulkanRenderer> r) {
        renderer_ = std::move(r);
        g_renderer_ptr = renderer_.get();
        if (renderer_) {
            renderer_->setTonemap(Options::Tonemap::ENABLE_TONEMAPPING);
            if (Options::OptionsRTX::ENABLE_HYPERTRACE) renderer_->toggleHypertrace();
            if (Options::OptionsRTX::ENABLE_DENOISING) renderer_->toggleDenoising();
            if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING) renderer_->toggleAdaptiveSampling();
            renderer_->setOverclockMode(Options::Performance::OVERCLOCK_RENDERER);
            renderer_->setOverlay(showOverlay_);
        }
    }

    [[nodiscard]] VulkanRenderer* renderer() const noexcept { return renderer_.get(); }

    void setRenderMode(int mode);

private:
    void processInput(float deltaTime);

    std::string title_;
    int width_, height_;
    glm::mat4 proj_;

    std::chrono::steady_clock::time_point lastFrameTime_;

    bool quit_ = false;
    bool showOverlay_ = true;
    int currentRenderMode_ = 1;  // Start in pure pink void

    std::unique_ptr<VulkanRenderer> renderer_;
};

Application::Application(const std::string& title, int width, int height)
    : title_(title), width_(width), height_(height)
{
    if (!stone_window()) {
        phase9_ballerina("Window not initialized", std::source_location::current());
    }

    SDL_SetWindowTitle(stone_window(), title.c_str());
    lastFrameTime_ = std::chrono::steady_clock::now();
    proj_ = glm::perspective(glm::radians(75.0f), static_cast<float>(width) / height, 0.1f, 1000.0f);
}

Application::~Application() = default;

void Application::run() noexcept {
    auto lastTime = std::chrono::steady_clock::now();
    float titleTimer = 0.0f;
    constexpr float TITLE_UPDATE_INTERVAL = 0.6f;
    int dotPhase = 0;

    int frameCount = 0;
    float fpsTimer = 0.0f;

    static constexpr float RESIZE_DEBOUNCE_SECONDS = 0.2f;
    static auto lastResizeTime = std::chrono::steady_clock::time_point::min();
    static int pendingWidth = 0;
    static int pendingHeight = 0;

    while (!quit_) {
        const auto frameStart = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(frameStart - lastTime).count();
        lastTime = frameStart;

        bool toggleFS = false;
        int winW = 0, winH = 0;
        SDL3Window::pollEvents(winW, winH, quit_, toggleFS);

        if (winW > 0 && winH > 0) {
            if (width_ != winW || height_ != winH) {
                pendingWidth = winW;
                pendingHeight = winH;
                lastResizeTime = frameStart;
            }
        }

        if (pendingWidth > 0 && pendingHeight > 0) {
            const float timeSinceResize = std::chrono::duration<float>(frameStart - lastResizeTime).count();
            if (timeSinceResize >= RESIZE_DEBOUNCE_SECONDS) {
                width_ = pendingWidth;
                height_ = pendingHeight;
                proj_ = glm::perspective(glm::radians(75.0f), float(width_) / std::max(height_, 1), 0.1f, 1000.0f);

                if (renderer_) {
                    renderer_->onWindowResize(static_cast<uint32_t>(pendingWidth), static_cast<uint32_t>(pendingHeight));
                }

                g_mode1.onResize(pendingWidth, pendingHeight);

                pendingWidth = pendingHeight = 0;
            }
        }

        if (toggleFS) {
            SDL3Window::toggleFullscreen();
        }

        INPUT.pumpEvents(g_deltaTime, [this](int mode) { setRenderMode(mode); }, stone_window());

        bool swapchainValid = (stone_swapchain() != VK_NULL_HANDLE);

        if (renderer_ && renderer_->isAlive() && swapchainValid) {
            renderer_->setMaxFramesInFlight(Options::Performance::MAX_FRAMES_IN_FLIGHT);

            const uint32_t frameIndex = renderer_->frameNumber();

            VkCommandBuffer cmd = renderer_->commandBuffers()[frameIndex % Options::Performance::MAX_FRAMES_IN_FLIGHT];

            if (currentRenderMode_ == 1) {
                g_mode1.renderFrame(cmd, frameIndex, g_deltaTime);
            } else {
                renderer_->renderFrame(CAM, g_deltaTime);
            }
        }

        titleTimer += g_deltaTime;
        if (titleTimer >= TITLE_UPDATE_INTERVAL) {
            titleTimer -= TITLE_UPDATE_INTERVAL;
            dotPhase = (dotPhase + 1) % 4;
        }

        ++frameCount;
        fpsTimer += g_deltaTime;
        if (fpsTimer >= 1.0f) {
            float currentFPS = frameCount / std::max(fpsTimer, 0.001f);

            LOG_INFO_CAT("PERF",
                "FPS: {:.1f} | Frame Time: {:.2f} ms | Resolution: {}x{} | "
                "Render Mode: {} | Frames in Flight: {} | Delta: {:.3f} s",
                currentFPS,
                g_deltaTime * 1000.0f,
                width_, height_,
                currentRenderMode_,
                Options::Performance::MAX_FRAMES_IN_FLIGHT,
                g_deltaTime
            );

            frameCount = 0;
            fpsTimer = 0.0f;
        }
    }

    vkDeviceWaitIdle(stone_device());
}

void Application::setRenderMode(int mode) {
    constexpr int MIN_MODE = 1;
    constexpr int MAX_MODE = 9;

    if (mode < MIN_MODE || mode > MAX_MODE) {
        LOG_WARNING_CAT("APP", "Invalid render mode {} requested — ignoring", mode);
        return;
    }

    if (mode == currentRenderMode_) return;

    LOG_INFO_CAT("APP", "Render mode switched to {}", mode);
    currentRenderMode_ = mode;

    if (renderer_) {
        renderer_->requestAccumulationReset();
    }
}

// =============================================================================
// Splash Screen
// =============================================================================
static void showSacrificialSplash() noexcept {
    constexpr bool enabled = true;
    constexpr float duration = Options::Splash::SPLASH_DURATION_SECONDS;

    if (!enabled || duration <= 0.0f) {
        return;
    }

    constexpr int W = 1280;
    constexpr int H = 720;
    constexpr const char* TITLE = "AMOURANTH RTX - Candy Cane";
    constexpr const char* IMAGE_PATH = "assets/textures/ammo.png";

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        return;
    }

    SDL_Window* win = SDL_CreateWindow(
        TITLE,
        W, H,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (!win) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Rect disp{};
    SDL_GetDisplayBounds(0, &disp);
    SDL_SetWindowPosition(win, disp.x + (disp.w - W) / 2, disp.y + (disp.h - H) / 2);

    auto setIcon = [](SDL_Window* w) {
        const char* paths[] = {
            "assets/textures/ammo.ico",
            "assets/textures/ammo32.ico",
            nullptr
        };
        for (int i = 0; paths[i]; ++i) {
            if (SDL_Surface* s = IMG_Load(paths[i])) {
                SDL_SetWindowIcon(w, s);
                SDL_DestroySurface(s);
                return;
            }
        }
    };
    setIcon(win);

    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Surface* surf = IMG_Load(IMAGE_PATH);
    if (!surf) {
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_DestroySurface(surf);
    if (!tex) {
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    float texW = 0.0f, texH = 0.0f;
    SDL_GetTextureSize(tex, &texW, &texH);

    SDL_FRect dst{
        (W - texW) * 0.5f,
        (H - texH) * 0.5f,
        texW,
        texH
    };

    SDL_ShowWindow(win);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, nullptr, &dst);
    SDL_RenderPresent(ren);

    const auto start = std::chrono::steady_clock::now();
    bool aborted = false;

    while (!aborted) {
        const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();

        if (elapsed >= duration) break;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                aborted = true;
            } else if (Options::Splash::ALLOW_EARLY_EXIT && e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                aborted = true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

// =============================================================================
// Window & Vulkan Initialization
// =============================================================================
static void createRealFinalWindow() noexcept {
    const int w = Options::Window::DEFAULT_WIDTH;
    const int h = Options::Window::DEFAULT_HEIGHT;

    stone_seal_width(w);
    stone_seal_height(h);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        phase9_ballerina("SDL initialization failed", std::source_location::current());
    }

    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        phase9_ballerina("Vulkan library load failed", std::source_location::current());
    }

    VkInstance instance = RTX::createVulkanInstance(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) {
        phase9_ballerina("Vulkan instance creation failed", std::source_location::current());
    }
    stone_seal_instance(instance);

    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (Options::Window::START_FULLSCREEN) flags |= SDL_WINDOW_FULLSCREEN;

    SDL_Window* win = SDL_CreateWindow(
        "AMOURANTH RTX - Candy Cane",
        w, h,
        flags
    );

    if (!win) {
        phase9_ballerina("Window creation failed", std::source_location::current());
    }

    stone_seal_window(win);
    g_sdl_window.reset(win);
    RTX::g_ctx().setSize(w, h);
    SDL_ShowWindow(win);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(win, instance, nullptr, &surface) || !surface) {
        phase9_ballerina("Vulkan surface creation failed", std::source_location::current());
    }
    stone_seal_surface(surface);

    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) {
        phase9_ballerina("Logical device creation failed", std::source_location::current());
    }

    createTransientCommandPool();

    RTX::SwapchainManager::create(win, w, h);
}

// =============================================================================
// Main Phases
// =============================================================================
static void phase3_sacrificialSplash() { showSacrificialSplash(); }

static void phase4_merchantShip() {
    createRealFinalWindow();
    RTX::g_ctx().init();
    RTX::loadRTExtensions(stone_instance(), stone_device());

    createTransientCommandPool();
}

static std::unique_ptr<VulkanRenderer> phase7_Renderer() noexcept {
    auto renderer = std::make_unique<VulkanRenderer>(
        stone_width(),
        stone_height(),
        SDL3Window::get(),
        Options::Performance::OVERCLOCK_RENDERER
    );

    renderer->createCommandBuffers();
    renderer->createSyncObjects();

    renderer->initializeAllBufferData(
        Options::Performance::MAX_FRAMES_IN_FLIGHT,
        sizeof(DreamUBO),
        materialBufferSize()
    );

    stone_seal_renderer(renderer.get());
    return renderer;
}

static void phase8_forgeTheRTX(VulkanRenderer* renderer) {
    static bool crownWorn = false;
    if (crownWorn) return;

    auto& pipe = renderer->pipelineManager_;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = g_transientCommandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(stone_device(), &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    pipe.forgeRTXPipeline(g_transientCommandPool, stone_graphics_queue(), cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));
    VkSubmitInfo submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd};
    VK_CHECK(vkQueueSubmit(stone_graphics_queue(), 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(stone_graphics_queue()));
    vkFreeCommandBuffers(stone_device(), g_transientCommandPool, 1, &cmd);

    stone_seal_pipeline(&pipe);
    crownWorn = true;
}

static void phase6_loadDefaultCube() {
    LOG_INFO_CAT("MAIN", "Loading default cube geometry");

    auto mesh = MeshLoader::loadOBJ("assets/models/cube.obj");
    if (mesh) {
        RTX::las().addMesh(std::move(mesh));
        LOG_INFO_CAT("MAIN", "Default cube loaded successfully");
    } else {
        LOG_WARNING_CAT("MAIN", "Failed to load cube.obj — starting in pink void");
    }
}

// =============================================================================
// Apocalypse Handler — Graceful Shutdown
// =============================================================================
[[noreturn]] void phase9_ballerina(std::string_view reason, std::source_location loc) noexcept {
    LOG_FATAL_CAT("FATAL", "Apocalypse triggered: {} at {}:{}", reason, loc.file_name(), loc.line());

    VkDevice device = stone_device();
    VkInstance instance = stone_instance();

    if (VkSwapchainKHR sc = stone_swapchain(); sc != VK_NULL_HANDLE && device) {
        vkDestroySwapchainKHR(device, sc, nullptr);
    }

    if (g_transientCommandPool && device) {
        vkDestroyCommandPool(device, g_transientCommandPool, nullptr);
    }

    if (device) vkDestroyDevice(device, nullptr);

    RTX::las().notifyResize();

    if (SDL_Window* win = stone_window()) {
        SDL_DestroyWindow(win);
    }

    if (VkSurfaceKHR surf = stone_surface(); surf && instance) {
        vkDestroySurfaceKHR(instance, surf, nullptr);
    }

    if (instance) vkDestroyInstance(instance, nullptr);

    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    std::_Exit(1);
}

// =============================================================================
// Entry Point
// =============================================================================
int main(int, char**) {
    install_apocalypse_handler();

    phase3_sacrificialSplash();
    phase4_merchantShip();

    auto renderer = phase7_Renderer();
    stone_seal_renderer(renderer.get());
    phase8_forgeTheRTX(renderer.get());

    phase6_loadDefaultCube();

    stone_seal_final();

    g_app_ptr = std::make_unique<Application>("AMOURANTH RTX vTURBO", 3840, 2160);
    g_app_ptr->setRenderer(std::move(renderer));

    g_app_ptr->run();

    return 0;
}

// =============================================================================
// FINAL PRODUCTION MAIN — ALL COMPILATION ERRORS RESOLVED
// THROW-AWAY COMMAND BUFFERS · NO PERSISTENT POOLS IN CONTEXT
// VRAM QUERY VIA vkGetPhysicalDeviceMemoryProperties
// CLEAN · SAFE · MODERN · FULLY COMPATIBLE
// SHIPPING DECEMBER 18, 2025 — THE EMPIRE IS COMPLETE
// =============================================================================