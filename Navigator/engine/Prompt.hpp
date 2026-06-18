#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Prompt Terminal Client
// Standalone console window for live option editing + debugging
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
// =============================================================================

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <iostream>
#include <cctype>
#include <algorithm>   // for std::clamp, std::max

// Make sure OptionsMenu.hpp is included BEFORE this file
#include "OptionsMenu.hpp"

class PromptTerminal {
public:
    PromptTerminal();
    ~PromptTerminal();

    bool init(int width = 1280, int height = 720);
    void run();
    void shutdown();
    void executeCommand(const std::string& cmd);

private:
    void processInput();
    void handleResize(int newWidth, int newHeight);
    void updateFontSize();
    void render();
    void addLog(const std::string& msg, SDL_Color color = {255, 255, 255, 255});
    void executeLine(const std::string& line);
    void printHelp();
    void listOptions();
    void setOption(const std::string& key, const std::string& value);

    SDL_Window*   window_     = nullptr;
    SDL_Renderer* renderer_   = nullptr;
    TTF_Font*     font_       = nullptr;

    std::vector<std::pair<std::string, SDL_Color>> history_;
    std::string currentLine_;
    int scrollOffset_ = 0;
    bool running_ = false;

    // Dynamic sizing
    int   windowWidth_     = 1280;
    int   windowHeight_    = 720;
    float baseFontSize_    = 24.0f;
    float currentFontSize_ = 24.0f;
    float lineHeight_      = 26.0f;   // Will be calculated as font * 1.1

    using CommandFunc = std::function<void(const std::vector<std::string>&)>;
    std::unordered_map<std::string, CommandFunc> commands_;
};

// =============================================================================
// Implementation
// =============================================================================

inline PromptTerminal::PromptTerminal() {
    commands_["help"] = [this](const auto&) { printHelp(); };
    commands_["guide"] = [this](const auto&) { addLog(Options::GetPuniesGuideText(), {255, 220, 180, 255}); };
    commands_["level"] = [this](const auto& args) {
        if (args.empty()) { addLog("Usage: level puny | adept | tidewalker", {255,200,100,255}); return; }
        std::string l = args[0];
        if (l == "puny") { Options::Mastery::Current = Options::Mastery::Puny; addLog("Mastery: PUNY — safe for us punies 💖", {150,255,150,255}); }
        else if (l == "adept") { Options::Mastery::Current = Options::Mastery::Adept; addLog("Mastery: ADEPT — tuning the living fields", {150,200,255,255}); }
        else if (l == "tidewalker" || l == "smart") { Options::Mastery::Current = Options::Mastery::Tidewalker; addLog("Mastery: TIDEWALKER — every hardware nook + sub-micron is yours now", {255,150,200,255}); }
        else addLog("Unknown. Try: puny | adept | tidewalker", {255,150,150,255});
    };
    commands_["list"] = [this](const auto&) { listOptions(); };
    commands_["set"]  = [this](const auto& args) {
        if (args.size() >= 2) setOption(args[0], args[1]);
        else addLog("Usage: set <key> <value>", {255, 200, 100, 255});
    };
    commands_["quit"] = [this](const auto&) { running_ = false; };
    commands_["exit"] = [this](const auto&) { running_ = false; };
}

inline PromptTerminal::~PromptTerminal() {
    shutdown();
}

inline bool PromptTerminal::init(int width, int height) {
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            std::cerr << "Failed to init SDL video for prompt: " << SDL_GetError() << std::endl;
            return false;
        }
    }

    // Note: Main engine should have initialized video/audio/gamepad first via SDL3System.
    // This ensures the separate Prompt window can open reliably.

    window_ = SDL_CreateWindow(
        "AMOURANTH RTX - Prompt Terminal",
        width, height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );

    if (!window_) { std::cerr << "Failed to create prompt window: " << SDL_GetError() << std::endl;
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) { std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        return false;
    }

    if (!TTF_Init()) { std::cerr << "TTF_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    font_ = TTF_OpenFont("assets/fonts/font.ttf", static_cast<int>(baseFontSize_));
    if (!font_) {
        std::cerr << "Failed to load font: " << SDL_GetError() << std::endl;
        return false;
    }

    windowWidth_  = width;
    windowHeight_ = height;
    updateFontSize();   // Initial scaling

    addLog("AMOURANTH RTX - Prompt Terminal initialized", {100, 255, 100, 255});
    addLog("Type 'help' or 'guide' for puny-friendly how-to. All Options::* (including new AnalogFields + Hardware) are live editable.", {180, 180, 255, 255});
    addLog("Current mastery: " + std::string(Options::Mastery::Name()), {200, 255, 200, 255});
    addLog("AMOURANTH FOREVER 💖  —  the options menu has been taken over for us punies", {255, 100, 180, 255});
    addLog("Window is now resizable - font scales automatically", {140, 200, 255, 255});

    running_ = true;
    return true;
}

inline void PromptTerminal::shutdown() {
    if (font_)     { TTF_CloseFont(font_);           font_ = nullptr; }
    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_)   { SDL_DestroyWindow(window_);     window_ = nullptr; }
}

inline void PromptTerminal::updateFontSize() {
    float scale = static_cast<float>(windowHeight_) / 720.0f;
    currentFontSize_ = std::clamp(baseFontSize_ * scale, 12.0f, 48.0f);
    if (font_) { TTF_SetFontSize(font_, static_cast<int>(currentFontSize_)); }
    lineHeight_ = currentFontSize_ * 1.1f;
}

inline void PromptTerminal::handleResize(int newWidth, int newHeight) {
    if (newWidth <= 0 || newHeight <= 0) return;
    windowWidth_  = newWidth;
    windowHeight_ = newHeight;
    updateFontSize();
    int visibleLines = static_cast<int>((windowHeight_ - 80) / lineHeight_);
    scrollOffset_ = std::max(0, static_cast<int>(history_.size()) - visibleLines);
}

inline void PromptTerminal::addLog(const std::string& msg, SDL_Color color) {
    history_.emplace_back(msg, color);
    if (history_.size() > 500) { history_.erase(history_.begin()); }
    int visibleLines = static_cast<int>((windowHeight_ - 80) / lineHeight_);
    scrollOffset_ = std::max(0, static_cast<int>(history_.size()) - visibleLines);
}

inline void PromptTerminal::processInput() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) {
            running_ = false;
            return;
        }

        // Handle window resize
        if (ev.type == SDL_EVENT_WINDOW_RESIZED || 
            ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            handleResize(ev.window.data1, ev.window.data2);
            continue;
        }

        if (ev.type == SDL_EVENT_KEY_DOWN) {
            switch (ev.key.scancode) {
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_KP_ENTER:
                    if (!currentLine_.empty()) {
                        executeLine(currentLine_);
                        currentLine_.clear();
                    }
                    break;

                case SDL_SCANCODE_BACKSPACE:
                    if (!currentLine_.empty()) {
                        currentLine_.pop_back();
                    }
                    break;

                case SDL_SCANCODE_SPACE:
                    currentLine_ += ' ';
                    break;

                // === GRAVE KEY SUPPORT (` and ~) ===
                case SDL_SCANCODE_GRAVE:
                    currentLine_ += (ev.key.mod & SDL_KMOD_SHIFT) ? '~' : '`';
                    break;

                // Letters A-Z
                case SDL_SCANCODE_A ... SDL_SCANCODE_Z:
                {
                    char c = 'a' + static_cast<char>(ev.key.scancode - SDL_SCANCODE_A);
                    if (ev.key.mod & (SDL_KMOD_SHIFT | SDL_KMOD_CAPS)) {
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    currentLine_ += c;
                    break;
                }

                // Numbers 0-9
                case SDL_SCANCODE_0 ... SDL_SCANCODE_9:
                    currentLine_ += '0' + static_cast<char>(ev.key.scancode - SDL_SCANCODE_0);
                    break;

                // Common useful symbols
                case SDL_SCANCODE_MINUS:      currentLine_ += (ev.key.mod & SDL_KMOD_SHIFT) ? '_' : '-'; break;
                case SDL_SCANCODE_EQUALS:     currentLine_ += (ev.key.mod & SDL_KMOD_SHIFT) ? '+' : '='; break;
                case SDL_SCANCODE_SLASH:      currentLine_ += (ev.key.mod & SDL_KMOD_SHIFT) ? '?' : '/'; break;
                case SDL_SCANCODE_PERIOD:     currentLine_ += '.'; break;
                case SDL_SCANCODE_COMMA:      currentLine_ += ','; break;
                case SDL_SCANCODE_APOSTROPHE: currentLine_ += '\''; break;

                default:
                    break;
            }
        }
    }
}

inline void PromptTerminal::render() {
    SDL_SetRenderDrawColor(renderer_, 8, 8, 18, 255);
    SDL_RenderClear(renderer_);

    float y = 12.0f;
    const float bottomMargin = 50.0f;
    const int maxVisible = static_cast<int>((windowHeight_ - bottomMargin) / lineHeight_);

    // Render history
    for (size_t i = scrollOffset_; i < history_.size() && y < windowHeight_ - bottomMargin; ++i) {
        const auto& [text, color] = history_[i];

        SDL_Surface* surf = TTF_RenderText_Blended(font_, text.c_str(), 0, color);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
            if (tex) {
                SDL_FRect dst{12.0f, y, static_cast<float>(surf->w), static_cast<float>(surf->h)};
                SDL_RenderTexture(renderer_, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_DestroySurface(surf);
        }
        y += lineHeight_;
    }

    // Current input prompt
    std::string prompt = "> " + currentLine_ + "_";
    SDL_Surface* surf = TTF_RenderText_Blended(font_, prompt.c_str(), 0, {255, 255, 100, 255});
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
        if (tex) {
            SDL_FRect dst{12.0f, y, static_cast<float>(surf->w), static_cast<float>(surf->h)};
            SDL_RenderTexture(renderer_, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_DestroySurface(surf);
    }

    SDL_RenderPresent(renderer_);
}

inline void PromptTerminal::run() {
    while (running_) {
        processInput();
        render();
        SDL_Delay(16);
    }
}

inline void PromptTerminal::executeLine(const std::string& line) {
    if (line.empty()) return;
    addLog("> " + line, {180, 180, 255, 255});

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    auto it = commands_.find(cmd);
    if (it != commands_.end()) {
        std::vector<std::string> args;
        std::string arg;
        while (iss >> arg) args.push_back(arg);
        it->second(args);
    } else {
        addLog("Unknown command: " + cmd + ". Type 'help'", {255, 150, 150, 255});
    }
}

inline void PromptTerminal::printHelp() {
    addLog("Available commands (OptionsMenu has been taken over for punies):", {255, 255, 100, 255});
    addLog("  help                  - this", {200, 200, 255, 255});
    addLog("  guide                 - big friendly 'how to do everything' for punies + levels", {200, 200, 255, 255});
    addLog("  level puny|adept|tidewalker  - change complexity (affects docs + some smart defaults)", {200, 200, 255, 255});
    addLog("  list [prefix]         - e.g. list AnalogFields or list Hardware", {200, 200, 255, 255});
    addLog("  set <Option.Path> <v> - live edit ANYTHING (e.g. set AnalogFields.WaveSpeed 1.4)", {200, 200, 255, 255});
    addLog("  quit / exit           - close this terminal", {200, 200, 255, 255});
    addLog("New power (simple + smart): AnalogFields (the fields + gates) + Hardware (spiderweb + real clocks + sub-micron)", {180,255,180,255});
}

inline void PromptTerminal::listOptions() {
    addLog("=== CURRENT OPTIONS (Mastery: " + std::string(Options::Mastery::Name()) + ") ===", {100, 255, 100, 255});
    addLog("Rendering.EnableHardwareRayTracing = " + std::to_string(Options::Rendering::EnableHardwareRayTracing), {220,220,255,255});
    addLog("Rendering.EnableRTXGI              = " + std::to_string(Options::Rendering::EnableRTXGI), {220,220,255,255});
    addLog("Camera.CurrentFOV                  = " + std::to_string(Options::Camera::CurrentFOV), {220,220,255,255});
    addLog("LivingWorld.CurrentTimeOfDay       = " + std::to_string(Options::LivingWorld::CurrentTimeOfDay), {220,220,255,255});
    addLog("GameStyle.CurrentDimension         = " + std::to_string(static_cast<int>(Options::GameStyle::CurrentDimension)), {220,220,255,255});

    // New stuff the OptionsMenu took over
    addLog("AnalogFields.TimeScale             = " + std::to_string(Options::AnalogFields::TimeScale), {180,255,200,255});
    addLog("AnalogFields.WaveSpeed             = " + std::to_string(Options::AnalogFields::WaveSpeed), {180,255,200,255});
    addLog("AnalogFields.GateFidelity          = " + std::to_string(Options::AnalogFields::GateFidelity), {180,255,200,255});
    addLog("Hardware.TargetCoreClockMHz        = " + std::to_string(Options::Hardware::TargetCoreClockMHz), {255,200,180,255});
    addLog("Hardware.CurrentCoreMHz (live)     = " + std::to_string(Options::Hardware::CurrentCoreMHz), {255,200,180,255});
    addLog("Hardware.ActiveUnits (live)        = " + std::to_string(Options::Hardware::ActiveUnits), {255,200,180,255});
    addLog("=== End of list (more in source or use list AnalogFields / list Hardware) ===", {100, 255, 100, 255});
}

inline void PromptTerminal::setOption(const std::string& key, const std::string& value) {
    bool success = false;

    if (key == "Rendering.EnableHardwareRayTracing") { Options::Rendering::EnableHardwareRayTracing = (value == "1" || value == "true" || value == "on");
        success = true;
    }
    else if (key == "Rendering.EnableRTXGI") { Options::Rendering::EnableRTXGI = (value == "1" || value == "true" || value == "on");
        success = true;
    }
    else if (key == "Camera.CurrentFOV") { try { Options::Camera::CurrentFOV = std::stof(value); success = true; } catch (...) {}
    }
    else if (key == "LivingWorld.CurrentTimeOfDay") { try { Options::LivingWorld::CurrentTimeOfDay = std::stof(value); success = true; } catch (...) {}
    }
    else if (key == "GameStyle.CurrentDimension") {
        try {
            int v = std::stoi(value);
            Options::GameStyle::CurrentDimension = static_cast<Options::GameStyle::DimensionMode>(v);
            success = true;
        } catch (...) {}
    }
    // New centralized options (AnalogFields + Hardware spiderweb — taken over for punies)
    else if (key == "AnalogFields.TimeScale") { try { Options::AnalogFields::TimeScale = std::stof(value); success = true; } catch (...) {} }
    else if (key == "AnalogFields.WaveSpeed") { try { Options::AnalogFields::WaveSpeed = std::stof(value); success = true; } catch (...) {} }
    else if (key == "AnalogFields.GateFidelity") { try { Options::AnalogFields::GateFidelity = std::stof(value); success = true; } catch (...) {} }
    else if (key == "AnalogFields.InjectStrength") { try { Options::AnalogFields::InjectStrength = std::stof(value); success = true; } catch (...) {} }
    else if (key == "Hardware.TargetCoreClockMHz") { try { Options::Hardware::TargetCoreClockMHz = std::stof(value); success = true; } catch (...) {} }
    else if (key == "Hardware.ThermalSensitivity") { try { Options::Hardware::ThermalSensitivity = std::stof(value); success = true; } catch (...) {} }
    else if (key == "Mastery.Level") {
        std::string l = value;
        if (l == "puny") Options::Mastery::Current = Options::Mastery::Puny;
        else if (l == "adept") Options::Mastery::Current = Options::Mastery::Adept;
        else if (l == "tidewalker") Options::Mastery::Current = Options::Mastery::Tidewalker;
        success = true;
    }

    if (success) {
        addLog("✓ Set " + key + " = " + value, {100, 255, 100, 255});
    } else {
        addLog("✗ Unknown option or invalid value: " + key, {255, 100, 100, 255});
        addLog("Type 'list' or 'guide' to discover more (new AnalogFields & Hardware namespaces are fully supported).", {255, 180, 100, 255});
    }
}

inline void PromptTerminal::executeCommand(const std::string& cmd) {
    executeLine(cmd);
}

// Global instance
inline PromptTerminal Prompt;