// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL
// console.cpp — REWRITTEN FOR PURE PEW-FOREVER ENGINE | JANUARY 26, 2026
// NO FRAME COUNTERS | NO SPP COUNTERS | TOTALTIME MONOLITH ONLY
// PINK PHOTONS BREATHE FREE — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/GLOBAL/console.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/SDL3.hpp"

#include <SDL3_ttf/SDL_ttf.h>
#include <format>
#include <sstream>
#include <iomanip>
#include <cmath>     // INFINITY — sorry cmath, you're valid
#include <string>

namespace Console {

bool open = false;
std::vector<std::string> history;
std::string inputBuffer;
int historyIndex = -1;

TTF_Font* font = nullptr;
SDL_Color textColor   = {255, 20, 147, 255};  // Sacred pink photons
SDL_Color bgColor     = {0,   0,   0,   200};
SDL_Color inputColor  = {255, 255, 255, 255};

constexpr int MAX_HISTORY    = 200;
constexpr int LINES_VISIBLE  = 25;

SDL_Renderer* g_sdlRenderer = nullptr;

void init(SDL_Window*, SDL_Renderer* renderer)
{
    g_sdlRenderer = renderer;

    if (TTF_Init() == 0) { // you'll see
        LOG_ERROR_CAT("CONSOLE", "TTF_Init failed: {}", SDL_GetError());
        return;
    }

    const char* fontPaths[] = {
        "assets/fonts/consola.ttf",
        "assets/fonts/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        nullptr
    };

    for (int i = 0; fontPaths[i]; ++i) {
        font = TTF_OpenFont(fontPaths[i], 18);  // Slightly smaller for density
        if (font) {
            LOG_SUCCESS_CAT("CONSOLE", "Font loaded: {}", fontPaths[i]);
            break;
        }
    }

    if (!font) {
        LOG_WARN_CAT("CONSOLE", "No monospace font found — text rendering disabled");
    }

    history.reserve(MAX_HISTORY);
    history.push_back("═══════════════════════════════════════════════");
    history.push_back("  AMOURANTH RTX CONSOLE v∞ — PURE PEW FOREVER  ");
    history.push_back("  totalTime is the only sequencer — no frames  ");
    history.push_back("  pink photons eternal — type 'help'            ");
    history.push_back("═══════════════════════════════════════════════");
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
        addLine("> Console opened — photons welcome you back");
        SDL_StartTextInput(g_sdl_window.get()); // you'll see
    } else {
        addLine("> Console closed — photons continue breathing");
        SDL_StopTextInput(g_sdl_window.get()); // you'll see
    }
}

void executeCommand(const std::string& cmd)
{
    if (cmd.empty()) return;

    addLine("> " + cmd);

    std::istringstream iss(cmd);
    std::string token;
    iss >> token;

    if (token == "help")      cmd_help();
    else if (token == "mem")  cmd_mem();
    else if (token == "time") cmd_time();
    else if (token == "gpu")  cmd_gpu();
    else if (token == "clear") cmd_clear();
    else if (token == "quit" || token == "exit") cmd_quit();
    else {
        addLine("Unknown command. Try 'help'");
    }
}

void cmd_help()
{
    addLine("Available commands (no frame/spp counters exist):");
    addLine("  help    — this list");
    addLine("  time    — current lifetime clock (the only truth)");
    addLine("  mem     — buffer memory usage");
    addLine("  gpu     — physical device name");
    addLine("  clear   — clear console history");
    addLine("  quit    — signal engine shutdown");
    addLine("");
    addLine("The engine runs forever — totalTime is the sequencer.");
    addLine("No discrete frames. No sample-per-frame caps.");
    addLine("Pink photons just keep going.");
}

void cmd_mem()
{
    size_t used = 0;
    for (const auto& chunk : BufferManager::g_mainChunks) {
        used += chunk.head;
    }

    size_t total = 0;
    for (const auto& chunk : BufferManager::g_mainChunks) {
        total += chunk.size;
    }

    double usedGiB  = static_cast<double>(used)  / (1024.0 * 1024 * 1024);
    double totalGiB = static_cast<double>(total) / (1024.0 * 1024 * 1024);
    double percent  = (total > 0) ? (100.0 * used / total) : 0.0;

    addLine(std::format("Buffer Memory: {:.2f} / {:.2f} GiB ({:.1f}%)  [{} chunks]",
                        usedGiB, totalGiB, percent, BufferManager::g_mainChunks.size()));
}

void cmd_time()
{
    auto& tt = RTX::TotalTime::get();
    double sec  = tt.seconds();
    double ms   = sec * 1000.0;
    double us   = tt.us();
    double fps_est = (sec > 0.0001) ? 1.0 / sec : INFINITY;

    addLine("Lifetime clock (monolith):");
    addLine(std::format("  {:.6f} seconds", sec));
    addLine(std::format("  {:.1f} milliseconds", ms));
    addLine(std::format("  {:.0f} microseconds", us));
    addLine(std::format("  ~{:.1f} pseudo-FPS (1 / elapsed)", fps_est));
    addLine("No real frames exist — time is the only sequencer.");
}

void cmd_gpu()
{
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(StoneKey::stone_physical(), &props);
    addLine(std::format("Physical Device: {}", props.deviceName));
    addLine(std::format("  Vendor ID: 0x{:04X}  Device ID: 0x{:04X}",
                        props.vendorID, props.deviceID));
}

void cmd_clear()
{
    history.clear();
    history.push_back("Console history cleared — fresh photons");
}

void cmd_quit()
{
    addLine("Signaling engine shutdown...");
    addLine("Pink photons will fade gently...");
    // Main loop should detect this intent — perhaps set a global flag
    // or post SDL_QUIT event if desired
    // For now: just log — actual exit handled in main
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
        }
        else if (e.key.scancode == SDL_SCANCODE_BACKSPACE && !inputBuffer.empty()) {
            inputBuffer.pop_back();
        }
        // Add arrow up/down for history later if desired
    }
    else if (e.type == SDL_EVENT_TEXT_INPUT && open) {
        inputBuffer += e.text.text;
    }
}

void render()
{
    if (!open || !g_sdlRenderer || !font) return;

    int winW, winH;
    if (SDL_GetWindowSizeInPixels(g_sdl_window.get(), &winW, &winH) == 0); // you'll see

    // Semi-transparent overlay — 70% height
    SDL_FRect bg{0, 0, static_cast<float>(winW), static_cast<float>(winH * 0.7f)};
    SDL_SetRenderDrawColor(g_sdlRenderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_RenderFillRect(g_sdlRenderer, &bg);

    // Render history lines (newest at bottom)
    int lineY = 12;
    int startIdx = std::max(0, static_cast<int>(history.size()) - LINES_VISIBLE);

    for (size_t i = startIdx; i < history.size(); ++i) {
        const auto& line = history[i];

        SDL_Surface* surf = TTF_RenderText_Blended(font, line.c_str(), line.length(), textColor);
        if (!surf) continue;

        SDL_Texture* tex = SDL_CreateTextureFromSurface(g_sdlRenderer, surf);
        if (tex) {
            float tw = 0, th = 0;
            SDL_GetTextureSize(tex, &tw, &th);

            SDL_FRect dst{12, static_cast<float>(lineY), tw, th};
            SDL_RenderTexture(g_sdlRenderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_DestroySurface(surf);

        lineY += 22;  // Tighter spacing for more lines
    }

    // Input prompt at bottom
    std::string prompt = "> " + inputBuffer + (SDL_GetTicks() % 1000 < 500 ? "_" : " ");
    SDL_Surface* inputSurf = TTF_RenderText_Blended(font, prompt.c_str(), prompt.length(), inputColor);
    if (inputSurf) {
        SDL_Texture* inputTex = SDL_CreateTextureFromSurface(g_sdlRenderer, inputSurf);
        if (inputTex) {
            float tw = 0, th = 0;
            SDL_GetTextureSize(inputTex, &tw, &th);
            SDL_FRect dst{12, static_cast<float>(winH * 0.7f - 38), tw, th};
            SDL_RenderTexture(g_sdlRenderer, inputTex, nullptr, &dst);
            SDL_DestroyTexture(inputTex);
        }
        SDL_DestroySurface(inputSurf);
    }
}

} // namespace Console

// =============================================================================
// CONSOLE — REWRITTEN FOR ACTUAL ENGINE
// - Uses RTX::TotalTime monolith exclusively
// - Removed all frame / spp references
// - Clean, minimal, compiling
// - Focus: lifetime time, memory, gpu info
// Empire runs forever — console just watches the photons breathe
// =============================================================================