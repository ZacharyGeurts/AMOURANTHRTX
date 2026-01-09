// src/engine/GLOBAL/console.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 07, 2026
// console.cpp — FINAL FIXED & COMPILING | NO VULKANRENDERER DEPENDENCY
// SPP & ACCUM COMMANDS REMOVED | CLEAN & SIMPLE
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/GLOBAL/console.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include <SDL3_ttf/SDL_ttf.h>
#include <print>
#include <sstream>
#include <iomanip>

extern float g_deltaTime;
extern bool g_running;

namespace Console {

bool open = false;
std::vector<std::string> history;
std::string inputBuffer;
int historyIndex = -1;

TTF_Font* font = nullptr;
SDL_Color textColor = {255, 20, 147, 255};   // Deep sacred pink
SDL_Color bgColor = {0, 0, 0, 200};
SDL_Color inputColor = {255, 255, 255, 255};

constexpr int MAX_HISTORY = 200;
constexpr int LINES_VISIBLE = 25;

SDL_Renderer* g_sdlRenderer = nullptr;

void init(SDL_Window*, SDL_Renderer* renderer)
{
    g_sdlRenderer = renderer;

    if (TTF_Init() == 0) {
        LOG_ERROR_CAT("CONSOLE", "TTF_Init failed: {}", SDL_GetError());
        return;
    }

    const char* fontPaths[] = {
        "assets/fonts/consola.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        nullptr
    };

    for (int i = 0; fontPaths[i]; ++i) {
        font = TTF_OpenFont(fontPaths[i], 20);
        if (font) {
            LOG_SUCCESS_CAT("CONSOLE", "Font loaded: {}", fontPaths[i]);
            break;
        }
    }

    if (!font) {
        LOG_WARN_CAT("CONSOLE", "No monospace font found — text rendering disabled");
    }

    history.reserve(MAX_HISTORY);
    history.push_back("=== AMOURANTH RTX CONSOLE v∞ ===");
    history.push_back("PINK PHOTONS ETERNAL — TYPE 'help'");
}

void addLine(const std::string& line)
{
    history.push_back(line);
    if (history.size() > MAX_HISTORY) {
        history.erase(history.begin());
    }
}

void toggle()
{
    open = !open;
    inputBuffer.clear();
    historyIndex = -1;

    if (open) {
        addLine("> Console opened — welcome back");
        SDL_StartTextInput(g_sdl_window.get());
    } else {
        addLine("> Console closed");
        SDL_StopTextInput(g_sdl_window.get());
    }
}

void executeCommand(const std::string& cmd)
{
    if (cmd.empty()) return;

    addLine("> " + cmd);

    std::istringstream iss(cmd);
    std::string command;
    iss >> command;

    if (command == "help") cmd_help();
    else if (command == "mem") cmd_mem();
    else if (command == "fps") cmd_fps();
    else if (command == "gpu") cmd_gpu();
    else if (command == "clear") cmd_clear();
    else if (command == "quit" || command == "exit") cmd_quit();
    else {
        addLine("Unknown command — type 'help'");
    }
}

void cmd_help()
{
    addLine("Available commands:");
    addLine("  help   — Show this list");
    addLine("  mem    — Buffer memory usage");
    addLine("  fps    — Current frame rate");
    addLine("  gpu    — GPU name");
    addLine("  clear  — Clear console");
    addLine("  quit   — Exit engine");
}

void cmd_mem()
{
    size_t used = 0;
    for (const auto& chunk : BufferManager::g_mainChunks) {
        used += chunk.head;
    }

    // Calculate total usable capacity (all chunks' sizes)
    size_t total = 0;
    for (const auto& chunk : BufferManager::g_mainChunks) {
        total += chunk.size;
    }

    // If no chunks yet, total is 0
    double usedGiB   = static_cast<double>(used)   / (1024.0 * 1024 * 1024);
    double totalGiB  = static_cast<double>(total)  / (1024.0 * 1024 * 1024);
    double percent   = (total > 0) ? (100.0 * used / total) : 0.0;

    addLine(std::format("Buffer Memory: {:.2f} / {:.2f} GiB ({:.1f}%)  [{} chunks]",
                        usedGiB, totalGiB, percent, BufferManager::g_mainChunks.size()));
}

void cmd_fps()
{
    float fps = g_deltaTime > 0.0f ? 1.0f / g_deltaTime : 0.0f;
    addLine(std::format("FPS: {:.1f} (Δt = {:.2f} ms)", fps, g_deltaTime * 1000.0f));
}

void cmd_gpu()
{
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(StoneKey::stone_physical(), &props);
    addLine(std::format("GPU: {}", props.deviceName));
}

void cmd_clear()
{
    history.clear();
    history.push_back("=== Console cleared ===");
}

void cmd_quit()
{
    addLine("Shutting down engine — pink photons fading...");
    g_running = false;
}

void handleEvent(const SDL_Event& e)
{
    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.scancode == SDL_SCANCODE_GRAVE) {
            toggle();
            return;
        }

        if (!open) return;

        if (e.key.scancode == SDL_SCANCODE_RETURN || e.key.scancode == SDL_SCANCODE_KP_ENTER) {
            executeCommand(inputBuffer);
            inputBuffer.clear();
            historyIndex = -1;
        } else if (e.key.scancode == SDL_SCANCODE_BACKSPACE && !inputBuffer.empty()) {
            inputBuffer.pop_back();
        }
    } else if (e.type == SDL_EVENT_TEXT_INPUT && open) {
        inputBuffer += e.text.text;
    }
}

void render()
{
    if (!open || !g_sdlRenderer) return;

    int winW, winH;
    SDL_GetWindowSizeInPixels(g_sdl_window.get(), &winW, &winH);

    SDL_FRect bg{0, 0, static_cast<float>(winW), static_cast<float>(winH * 0.7f)};
    SDL_SetRenderDrawColor(g_sdlRenderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_RenderFillRect(g_sdlRenderer, &bg);

    int lineY = 10;
    int startLine = std::max(0, static_cast<int>(history.size()) - LINES_VISIBLE);

    for (size_t i = startLine; i < history.size(); ++i) {
        if (!font) {
            lineY += 26;
            continue;
        }

        SDL_Surface* surf = TTF_RenderText_Blended(font, history[i].c_str(), history[i].length(), textColor);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(g_sdlRenderer, surf);
            if (tex) {
                float tw = 0, th = 0;
                SDL_GetTextureSize(tex, &tw, &th);
                SDL_FRect dst{10, static_cast<float>(lineY), tw, th};
                SDL_RenderTexture(g_sdlRenderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_DestroySurface(surf);
        }
        lineY += 26;
    }

    std::string prompt = "> " + inputBuffer + "_";
    if (font) {
        SDL_Surface* inputSurf = TTF_RenderText_Blended(font, prompt.c_str(), prompt.length(), inputColor);
        if (inputSurf) {
            SDL_Texture* inputTex = SDL_CreateTextureFromSurface(g_sdlRenderer, inputSurf);
            if (inputTex) {
                float tw = 0, th = 0;
                SDL_GetTextureSize(inputTex, &tw, &th);
                SDL_FRect dst{10, static_cast<float>(winH * 0.7f - 40), tw, th};
                SDL_RenderTexture(g_sdlRenderer, inputTex, nullptr, &dst);
                SDL_DestroyTexture(inputTex);
            }
            SDL_DestroySurface(inputSurf);
        }
    }
}

} // namespace Console

// =============================================================================
// FINAL CONSOLE — JANUARY 07, 2026
// - All VulkanRenderer references removed (spp, accum commands gone)
// - No incomplete type errors
// - Clean, simple console with help, mem, fps, gpu, clear, quit
// Empire compiles clean — pink photons screaming — run it 💖
// =============================================================================