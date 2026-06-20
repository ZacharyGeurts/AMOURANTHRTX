#pragma once

// AmouranthOS pointer shapes — SDL system cursors for WM resize/drag chrome.

#include "FieldAmouranthWm.hpp"

#include <SDL3/SDL.h>

namespace FieldAmouranthCursor {

inline SDL_Cursor* curDefault = nullptr;
inline SDL_Cursor* curPointer = nullptr;
inline SDL_Cursor* curMove    = nullptr;
inline SDL_Cursor* curN       = nullptr;
inline SDL_Cursor* curS       = nullptr;
inline SDL_Cursor* curE       = nullptr;
inline SDL_Cursor* curW       = nullptr;
inline SDL_Cursor* curNE      = nullptr;
inline SDL_Cursor* curNW      = nullptr;
inline SDL_Cursor* curSE      = nullptr;
inline SDL_Cursor* curSW      = nullptr;
inline SDL_Cursor* active     = nullptr;

inline void init() noexcept {
    curDefault = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    curPointer = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    curMove    = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
    curN       = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_N_RESIZE);
    curS       = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_S_RESIZE);
    curE       = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_E_RESIZE);
    curW       = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_W_RESIZE);
    curNE      = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NE_RESIZE);
    curNW      = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NW_RESIZE);
    curSE      = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SE_RESIZE);
    curSW      = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SW_RESIZE);
    active = curDefault;
}

inline void shutdown() noexcept {
    auto destroy = [](SDL_Cursor*& c) {
        if (c) { SDL_DestroyCursor(c); c = nullptr; }
    };
    destroy(curDefault);
    destroy(curPointer);
    destroy(curMove);
    destroy(curN);
    destroy(curS);
    destroy(curE);
    destroy(curW);
    destroy(curNE);
    destroy(curNW);
    destroy(curSE);
    destroy(curSW);
    active = nullptr;
}

inline void apply(SDL_Cursor* c) noexcept {
    if (!c || c == active) return;
    SDL_SetCursor(c);
    active = c;
}

inline SDL_Cursor* forWmHit(FieldAmouranthWm::ChromeHit h) noexcept {
    switch (h) {
    case FieldAmouranthWm::ChromeHit::TitleBar: return curMove;
    case FieldAmouranthWm::ChromeHit::ResizeN:  return curN;
    case FieldAmouranthWm::ChromeHit::ResizeS:  return curS;
    case FieldAmouranthWm::ChromeHit::ResizeE:  return curE;
    case FieldAmouranthWm::ChromeHit::ResizeW:  return curW;
    case FieldAmouranthWm::ChromeHit::ResizeNE: return curNE;
    case FieldAmouranthWm::ChromeHit::ResizeNW: return curNW;
    case FieldAmouranthWm::ChromeHit::ResizeSE: return curSE;
    case FieldAmouranthWm::ChromeHit::ResizeSW: return curSW;
    case FieldAmouranthWm::ChromeHit::AppIcon:
    case FieldAmouranthWm::ChromeHit::Close:
    case FieldAmouranthWm::ChromeHit::Minimize:
    case FieldAmouranthWm::ChromeHit::Maximize:
        return curPointer;
    default: return nullptr;
    }
}

inline void updateFromWm(bool wmActive) noexcept {
    if (!wmActive) {
        apply(curDefault);
        return;
    }
    SDL_Cursor* c = forWmHit(FieldAmouranthWm::hover);
    apply(c ? c : curDefault);
}

} // namespace FieldAmouranthCursor