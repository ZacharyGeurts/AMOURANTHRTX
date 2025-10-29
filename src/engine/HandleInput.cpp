// src/engine/HandleInput.cpp
// AMOURANTH RTX Engine © 2025 – Input Defaults
#include "HandleInput.hpp"
#include "engine/logging.hpp"

#include <SDL2/SDL.h>

void HandleInput::defaultMouseButtonHandler(const SDL_MouseButtonEvent& event) {
    LOG_DEBUG_CAT("Input", "Mouse button {}: {} (x={}, y={})",
                  event.button,
                  (event.state == SDL_PRESSED) ? "PRESSED" : "RELEASED",
                  event.x, event.y);
    // Default: no-op
}

void HandleInput::defaultTextInputHandler(const SDL_TextInputEvent& event) {
    LOG_DEBUG_CAT("Input", "Text input: {}", event.text);
    // Default: no-op
}

void HandleInput::defaultTouchHandler(const SDL_TouchFingerEvent& event) {
    LOG_DEBUG_CAT("Input", "Touch finger {}: {} (x={}, y={})",
                  event.fingerId,
                  (event.type == SDL_FINGERDOWN) ? "DOWN" :
                  (event.type == SDL_FINGERUP) ? "UP" : "MOTION",
                  event.x, event.y);
    // Default: no-op
}