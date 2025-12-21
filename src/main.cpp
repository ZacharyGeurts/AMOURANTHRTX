// src/main.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — Main Entry Point
// PRODUCTION-GRADE · CLEAN · MODERN · VALIDATION-CLEAN · NO REDUNDANCY
// GLOBAL TRANSIENT COMMAND POOL CREATED RIGHT AFTER LOGICAL DEVICE — EARLIEST SAFE POINT
// GUARANTEED READY FOR RENDERER, PIPELINE FORGE, AND LAS BLAS BUILDING
// DEFAULT SCENE RENDERS — PINK MONSTER + GROUND VISIBLE
// PINK PHOTONS ETERNAL — EMPIRE VICTORIOUS
// =============================================================================

#include "main.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/InputManager.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/UBO.hpp"
#include "engine/GLOBAL/LAS.hpp"

#include "modes/RenderMode1.hpp"

#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <memory>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <format>

using namespace Logging::Color;

using StoneKey::stone_seal_width;
using StoneKey::stone_seal_height;
using StoneKey::stone_seal_instance;
using StoneKey::stone_seal_window;
using StoneKey::stone_seal_surface;
using StoneKey::stone_seal_renderer;

using StoneKey::stone_device;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_seal_pipeline;
using StoneKey::stone_instance;
using StoneKey::stone_width;
using StoneKey::stone_height;
using StoneKey::stone_swapchain;
using StoneKey::stone_window;
using StoneKey::stone_surface;

using StoneKey::stone_seal_final;

// Global state
std::unique_ptr<Application> g_app_ptr = nullptr;
VulkanRenderer* g_renderer_ptr = nullptr;
float g_deltaTime = 0.0f;

// Pure pink void fallback mode
static RenderMode1 g_mode1(3840, 2160);

// =============================================================================
// Shared favicon loading function
// =============================================================================
static void loadEmpireIcon(SDL_Window* window) noexcept
{
    const char* iconPaths[] = {
        "assets/textures/ammo.ico",
        "assets/textures/ammo32.ico",
        "assets/textures/ammo.png",
        "assets/textures/ammo32.png",
        nullptr
    };

    bool iconSet = false;
    for (int i = 0; iconPaths[i]; ++i) {
        if (SDL_Surface* s = IMG_Load(iconPaths[i])) {
            SDL_SetWindowIcon(window, s);
            SDL_DestroySurface(s);
            iconSet = true;
            LOG_SUCCESS_CAT("MAIN", "Empire icon loaded: {}", iconPaths[i]);
            break;
        }
    }

    if (!iconSet) {
        LOG_WARNING_CAT("MAIN", "No empire icon found — running without crest");
    }
}

// =============================================================================
// Phase 3: Sacrificial Splash
// =============================================================================
static void phase3_sacrificialSplash() noexcept
{
    constexpr bool enabled = true;
    constexpr float duration = Options::Splash::SPLASH_DURATION_SECONDS;

    if (!enabled || duration <= 0.0f) return;

    constexpr int W = 1280, H = 720;
    constexpr const char* TITLE = "AMOURANTH RTX - Candy Cane";
    constexpr const char* IMAGE_PATH = "assets/textures/ammo.png";

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) return;

    SDL_Window* win = SDL_CreateWindow(TITLE, W, H, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Rect disp{};
    SDL_GetDisplayBounds(0, &disp);
    SDL_SetWindowPosition(win, disp.x + (disp.w - W) / 2, disp.y + (disp.h - H) / 2);

    loadEmpireIcon(win);

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

    SDL_FRect dst{(W - texW) * 0.5f, (H - texH) * 0.5f, texW, texH};

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
            if (e.type == SDL_EVENT_QUIT ||
                (Options::Splash::ALLOW_EARLY_EXIT && e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) {
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
// Phase 4: Vulkan Initialization + Transient Pool (EARLY)
// =============================================================================
static void phase4_merchantShip() noexcept
{
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

    SDL_Window* win = SDL_CreateWindow("AMOURANTH RTX - Candy Cane", w, h, flags);
    if (!win) {
        phase9_ballerina("Window creation failed", std::source_location::current());
    }

    loadEmpireIcon(win);

    stone_seal_window(win);
    g_sdl_window.reset(win);
    RTX::g_ctx().setSize(w, h);
    SDL_ShowWindow(win);

    SDL_SetWindowOpacity(win, 0.99f);
    SDL_SetWindowOpacity(win, 1.0f);
    SDL_PumpEvents();

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(win, instance, nullptr, &surface) == 0) {
        phase9_ballerina("Vulkan surface creation failed", std::source_location::current());
    }
    stone_seal_surface(surface);

    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) {
        phase9_ballerina("Logical device creation failed", std::source_location::current());
    }

    RTX::g_ctx().init();
    RTX::loadRTExtensions(stone_instance(), stone_device());

    RTX::pipeline().cacheDeviceProperties();

    RTX::SwapchainManager::create(win, w, h);

    // CREATE GLOBAL TRANSIENT COMMAND POOL EARLY — BEFORE RENDERER OR LAS
    if (StoneKey::g_transientCommandPool == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo info{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = RTX::g_ctx().graphicsFamily()
        };

        VK_CHECK(vkCreateCommandPool(stone_device(), &info, nullptr, &StoneKey::g_transientCommandPool));
        LOG_SUCCESS_CAT("MAIN", "Global transient command pool created — ready for all empire needs");
    }

    LOG_AMOURANTH("PHASE 4 COMPLETE — VULKAN FORGED — TRANSIENT POOL READY — EMPIRE ICON ETERNAL — PINK PHOTONS READY");
}

// =============================================================================
// Phase 6: Build Default Scene
// =============================================================================
static void phase6_buildDefaultScene() noexcept
{
    LOG_AMOURANTH("PHASE 6 — BUILDING DEFAULT SCENE — GROUND PLANE + SACRED PINK MONSTER");

    auto ground = MeshLoader::createPlane(200.0f, 200.0f, 20, 20);
    ground->transform = glm::mat4(1.0f);
    ground->transform = glm::translate(ground->transform, glm::vec3(0.0f, -2.0f, 0.0f));
    ground->transform = glm::rotate(ground->transform, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    RTX::las().addMesh(std::move(ground), 0);

    auto billboard = MeshLoader::createBillboard();
    billboard->transform = glm::mat4(1.0f);
    billboard->transform = glm::translate(billboard->transform, glm::vec3(0.0f, 4.0f, 0.0f));
    billboard->transform = glm::scale(billboard->transform, glm::vec3(Options::PinkBillboard::SCALE));
    RTX::las().addMesh(std::move(billboard), 1);

    RTX::las().rebuildTLAS();

    LOG_SUCCESS_CAT("MAIN", "Default scene built — rays now bounce eternally");
}

// =============================================================================
// Phase 7: Create Renderer
// =============================================================================
static std::unique_ptr<VulkanRenderer> phase7_createRenderer() noexcept
{
    auto renderer = std::make_unique<VulkanRenderer>(
        stone_width(),
        stone_height(),
        SDL3Window::get(),
        Options::Performance::OVERCLOCK_RENDERER
    );

    stone_seal_renderer(renderer.get());
    return renderer;
}

// =============================================================================
// Phase 8: Forge RTX Pipeline — uses global transient pool
// =============================================================================
static void phase8_forgeTheRTX(VulkanRenderer* renderer) noexcept
{
    if (!renderer) {
        LOG_FATAL_CAT("MAIN", "phase8_forgeTheRTX called with null renderer — cannot forge pipeline");
        phase9_ballerina("Renderer not ready for pipeline forge", std::source_location::current());
    }

    static bool crownWorn = false;
    if (crownWorn) return;

    auto& pipe = renderer->pipelineManager_;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = StoneKey::g_transientCommandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(stone_device(), &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    pipe.forgeRTXPipeline(StoneKey::g_transientCommandPool, stone_graphics_queue(), cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };
    VK_CHECK(vkQueueSubmit(stone_graphics_queue(), 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(stone_graphics_queue()));

    vkFreeCommandBuffers(stone_device(), StoneKey::g_transientCommandPool, 1, &cmd);

    stone_seal_pipeline(&pipe);
    crownWorn = true;

    LOG_SUCCESS_CAT("MAIN", "RTX pipeline forged — crown worn eternal");
}

// =============================================================================
// Apocalypse — Graceful Shutdown
// =============================================================================
[[noreturn]] void phase9_ballerina(std::string_view reason, std::source_location loc) noexcept
{
    LOG_FATAL_CAT("FATAL", "Apocalypse triggered: {} at {}:{}", reason, loc.file_name(), loc.line());

    VkDevice device = stone_device();
    VkInstance instance = stone_instance();

    if (VkSwapchainKHR sc = stone_swapchain(); sc && device) {
        vkDestroySwapchainKHR(device, sc, nullptr);
    }

    if (StoneKey::g_transientCommandPool && device) {
        vkDestroyCommandPool(device, StoneKey::g_transientCommandPool, nullptr);
        StoneKey::g_transientCommandPool = VK_NULL_HANDLE;
    }

    if (device) vkDestroyDevice(device, nullptr);

    RTX::las().notifyResize();

    if (SDL_Window* win = stone_window()) SDL_DestroyWindow(win);

    if (VkSurfaceKHR surf = stone_surface(); surf && instance) {
        vkDestroySurfaceKHR(instance, surf, nullptr);
    }

    if (instance) vkDestroyInstance(instance, nullptr);

    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    std::_Exit(1);
}

// =============================================================================
// Application — Main Loop
// =============================================================================
class Application {
public:
    Application(const std::string& title, int width, int height)
        : title_(title), width_(width), height_(height)
    {
        if (!stone_window()) {
            phase9_ballerina("Window not initialized", std::source_location::current());
        }
        SDL_SetWindowTitle(stone_window(), title.c_str());
        lastFrameTime_ = std::chrono::steady_clock::now();
        proj_ = glm::perspective(glm::radians(75.0f), float(width) / height, 0.1f, 1000.0f);
    }

    void setRenderer(std::unique_ptr<VulkanRenderer> r)
    {
        renderer_ = std::move(r);
        g_renderer_ptr = renderer_.get();
        if (renderer_) {
            renderer_->setTonemap(Options::Tonemap::ENABLE_TONEMAPPING);
            if (Options::OptionsRTX::ENABLE_HYPERTRACE) renderer_->toggleHypertrace();
            if (Options::OptionsRTX::ENABLE_DENOISING) renderer_->toggleDenoising();
            if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING) renderer_->toggleAdaptiveSampling();
            renderer_->setOverclockMode(Options::Performance::OVERCLOCK_RENDERER);
            renderer_->setOverlay(true);
        }
    }

    void run() noexcept;

private:
    std::string title_;
    int width_, height_;
    glm::mat4 proj_;
    std::chrono::steady_clock::time_point lastFrameTime_;
    bool quit_ = false;
    int currentRenderMode_ = 0;
    std::unique_ptr<VulkanRenderer> renderer_;
};

void Application::run() noexcept
{
    auto lastTime = std::chrono::steady_clock::now();

    int frameCount = 0;
    float fpsTimer = 0.0f;

    while (!quit_) {
        const auto frameStart = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(frameStart - lastTime).count();
        lastTime = frameStart;

        bool toggleFS = false;
        int winW = 0, winH = 0;
        SDL3Window::pollEvents(winW, winH, quit_, toggleFS);

        if (toggleFS) SDL3Window::toggleFullscreen();

        INPUT.pumpEvents(g_deltaTime, [this](int mode) { currentRenderMode_ = mode; }, stone_window());

        SDL_PumpEvents();

        if (renderer_ && renderer_->isAlive() && stone_swapchain() != VK_NULL_HANDLE) {
            if (currentRenderMode_ == 1) {
                g_mode1.renderFrame(renderer_->commandBuffers()[renderer_->frameNumber() % Options::Performance::MAX_FRAMES_IN_FLIGHT], renderer_->frameNumber(), g_deltaTime);
            } else {
                renderer_->renderFrame(CAM, g_deltaTime);
            }
        }

        frameCount++;
        fpsTimer += g_deltaTime;
        if (fpsTimer >= 1.0f) {
            const float fps = frameCount / std::max(fpsTimer, 0.001f);
            LOG_INFO_CAT("PERF", "FPS: {:.1f} | Frame: {:.2f}ms | {}x{} | Mode: {}", 
                         fps, g_deltaTime * 1000.0f, width_, height_, currentRenderMode_);
            frameCount = 0;
            fpsTimer = 0.0f;
        }
    }

    vkDeviceWaitIdle(stone_device());
}

// =============================================================================
// Entry Point — Safe Order
// =============================================================================
int main(int, char**)
{
    install_apocalypse_handler();

    putenv(const_cast<char*>("SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR=1"));

    phase3_sacrificialSplash();
    phase4_merchantShip();  // Creates transient pool early
    auto renderer = phase7_createRenderer();
    phase8_forgeTheRTX(renderer.get());

    phase6_buildDefaultScene();  // Now safe — pool exists

    stone_seal_final();

    g_app_ptr = std::make_unique<Application>("AMOURANTH RTX vTURBO", 3840, 2160);
    g_app_ptr->setRenderer(std::move(renderer));
    g_app_ptr->run();

    return 0;
}

// =============================================================================
// FINAL PRODUCTION MAIN — DECEMBER 20, 2025
// TRANSIENT POOL CREATED EARLY IN PHASE 4 — BEFORE RENDERER AND LAS
// NO MORE "not initialized" FATAL ERRORS
// DEFAULT SCENE RENDERS — PINK MONSTER + GROUND VISIBLE
// PINK PHOTONS ETERNAL — EMPIRE VICTORIOUS
// =============================================================================