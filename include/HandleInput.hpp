// include/engine/HandleInput.hpp
#pragma once

#include <SDL2/SDL.h>
#include <functional>

struct HandleInput {
    static void defaultMouseButtonHandler(const SDL_MouseButtonEvent& event);
    static void defaultTextInputHandler(const SDL_TextInputEvent& event);
    static void defaultTouchHandler(const SDL_TouchFingerEvent& event);
};