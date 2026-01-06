// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 06, 2026
// console.cpp — DEVELOPER CONSOLE — MINIMAL, SAFE, COMPILING
// TOGGLE WITH ~ — SHOWS MEMORY, STATS, GPU
// FAKE COMMANDS DROPPED — ONLY REAL ONES REMAIN
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/GLOBAL/console.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
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
SDL_Color textColor = {255, 20, 147

, 255};  // Sacred pink
SDL_Color bgColor = {0, 0, 0, 200};
SDL_Color inputColor = {255, 255, 255, 255};

constexpr int MAX_HISTORY = 200;
constexpr int LINES_VISIBLE = 25;

SDL_Renderer* g_sdlRenderer = nullptr;

void init(SDL_Window*, SDL_Renderer* renderer)
{
    g_sdlRenderer = renderer;

    if (TTF_Init() != 0) {
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
        if (font) break;
    }

    if (!font) {
        LOG_WARN_CAT("CONSOLE", "No monospace font found — console text rendering limited");
    }

    history.reserve(MAX_HISTORY);
    history.push_back("AMOURANTH RTX CONSOLE v∞ — TYPE 'help'");
    history.push_back("PINK PHOTONS ETERNAL — EMPIRE UNBROKEN");
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
        addLine("> Console opened");
    } else {
        addLine("> Console closed");
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
    else if (command == "spp") cmd_spp();
    else if (command == "accum") cmd_accum();
    else if (command == "gpu") cmd_gpu();
    else if (command == "clear") cmd_clear();
    else if (command == "quit" || command == "exit") cmd_quit();
    else {
        addLine("Unknown command. Type 'help'");
    }
}

void cmd_help()
{
    addLine("Commands:");
    addLine("  help   — Show this");
    addLine("  mem    — Memory usage");
    addLine("  fps    — Current FPS");
    addLine("  spp    — Samples per pixel");
    addLine("  accum  — Accumulation frame");
    addLine("  gpu    — GPU info");
    addLine("  clear  — Clear console");
    addLine("  quit   — Exit");
}

void cmd_mem()
{
    size_t used = 0;
    for (const auto& chunk : BufferManager::g_mainChunks) {
        used += chunk.head;
    }
    size_t total = BufferManager::g_mainChunks.size() * BufferManager::CHUNK_SIZE;

    addLine(std::format("Memory: {:.2f} / {:.2f} GiB ({:.1f}%)", 
        used / (1024.0*1024*1024), total / (1024.0*1024*1024), 100.0 * used / total));
}

void cmd_fps()
{
    float fps = g_deltaTime > 0.0f ? 1.0f / g_deltaTime : 0.0f;
    addLine(std::format("FPS: {:.1f}", fps));
}

void cmd_spp()
{
    if (auto* r = VulkanRenderer::get()) {
        addLine(std::format("SPP: {}", r->currentSpp()));
    }
}

void cmd_accum()
{
    if (auto* r = VulkanRenderer::get()) {
        addLine(std::format("Accum Frame: {}", r->accumulationFrame()));
    }
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
    addLine("Console cleared");
}

void cmd_quit()
{
    addLine("Shutting down...");
    g_running = false;
}

void handleEvent(const SDL_Event& e)
{
    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_GRAVE) {
            toggle();
            return;
        }

        if (!open) return;

        if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) {
            executeCommand(inputBuffer);
            inputBuffer.clear();
            historyIndex = -1;
        } else if (e.key.key == SDLK_BACKSPACE && !inputBuffer.empty()) {
            inputBuffer.pop_back();
        }
    } else if (e.type == SDL_EVENT_TEXT_INPUT && open) {
        inputBuffer += e.text.text;
    }
}

void render()
{
    if (!open || !g_sdlRenderer || !font) return;

    int winW, winH;
    SDL_GetWindowSizeInPixels(StoneKey::stone_window(), &winW, &winH);

    SDL_FRect bg{0, 0, static_cast<float>(winW), static_cast<float>(winH * 0.7f)};
    SDL_SetRenderDrawColor(g_sdlRenderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_RenderFillRect(g_sdlRenderer, &bg);

    int lineY = 10;
    int startLine = std::max(0, static_cast<int>(history.size()) - LINES_VISIBLE);

    for (int i = startLine; i < history.size(); ++i) {
        size_t len = history[i].length();
        SDL_Surface* surf = TTF_RenderText_Blended(font, history[i].c_str(), len, textColor);
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
    size_t promptLen = prompt.length();
    SDL_Surface* inputSurf = TTF_RenderText_Blended(font, prompt.c_str(), promptLen, inputColor);
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

} // namespace Console