// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 06, 2026
// console.hpp — DEVELOPER CONSOLE — EMPIRE COMMAND CENTER
// TOGGLE WITH ~ (GRAVE) — ALWAYS AVAILABLE — NO OPTIONS NEEDED
// SHOW MEMORY, FPS, SPP, ACCUM, GPU STATS, COMMANDS
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <chrono>

namespace Console {

extern bool open;
extern std::vector<std::string> history;
extern std::string inputBuffer;
extern int historyIndex;

void init(SDL_Window* window, SDL_Renderer* renderer);
void handleEvent(const SDL_Event& e);
void render();
void toggle();
void executeCommand(const std::string& cmd);

// Built-in commands
void cmd_help();
void cmd_mem();
void cmd_fps();
void cmd_spp();
void cmd_accum();
void cmd_gpu();
void cmd_quit();
void cmd_clear();

} // namespace Console