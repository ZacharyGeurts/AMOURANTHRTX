#pragma once

// Unified DOS input — keyboard (INT 16), mouse (INT 33), joystick/gameport.

#include "FieldDosConfig.hpp"
#include "FieldDosViewport.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>

namespace FieldInput {

struct MouseState {
    std::int32_t x       = 160;
    std::int32_t y       = 100;
    std::int32_t relX    = 0;
    std::int32_t relY    = 0;
    std::int32_t wheel   = 0;
    std::uint8_t buttons = 0;  // bit0=left bit1=right bit2=middle
    std::uint8_t prevButtons = 0;
    std::uint16_t pressCount[3]{};
    std::uint16_t releaseCount[3]{};
    std::uint16_t mickeysX = 8;
    std::uint16_t mickeysY = 8;
    bool visible         = false;
    bool installed       = false;
};

struct JoystickState {
    std::int16_t x1 = 0;
    std::int16_t y1 = 0;
    std::int16_t x2 = 0;
    std::int16_t y2 = 0;
    std::uint8_t buttons1 = 0;
    std::uint8_t buttons2 = 0;
};

struct KeyboardState {
    std::uint16_t biosKey     = 0;   // pending INT 16 AX
    std::uint16_t extendedKey = 0;   // 0xE0 prefix queue
    bool          down        = false;
    bool          shift       = false;
    bool          ctrl        = false;
    bool          alt         = false;
};

constexpr std::uint32_t GP_SOUTH     = 1u << 0;
constexpr std::uint32_t GP_EAST      = 1u << 1;
constexpr std::uint32_t GP_WEST      = 1u << 2;
constexpr std::uint32_t GP_NORTH     = 1u << 3;
constexpr std::uint32_t GP_BACK      = 1u << 4;
constexpr std::uint32_t GP_GUIDE     = 1u << 5;
constexpr std::uint32_t GP_START     = 1u << 6;
constexpr std::uint32_t GP_LSTICK    = 1u << 7;
constexpr std::uint32_t GP_RSTICK    = 1u << 8;
constexpr std::uint32_t GP_LSHOULDER = 1u << 9;
constexpr std::uint32_t GP_RSHOULDER = 1u << 10;
constexpr std::uint32_t GP_DUP       = 1u << 11;
constexpr std::uint32_t GP_DDOWN     = 1u << 12;
constexpr std::uint32_t GP_DLEFT     = 1u << 13;
constexpr std::uint32_t GP_DRIGHT    = 1u << 14;

struct GamepadState {
    bool connected = false;
    char name[48]{};
    float lx = 0.f, ly = 0.f, rx = 0.f, ry = 0.f;
    float lt = 0.f, rt = 0.f;
    std::uint32_t buttons = 0;
};

struct State {
    KeyboardState keyboard;
    MouseState    mouse;
    JoystickState joystick;
    GamepadState  gamepad;
    std::uint32_t primaryKey = 0u;   // legacy DataBus[32]
};

inline State state{};

inline std::uint16_t scancodeToBios(SDL_Scancode sc, bool shift, bool ctrl) noexcept {
    switch (sc) {
    case SDL_SCANCODE_ESCAPE:     return 0x011Bu;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:   return 0x1C0Du;
    case SDL_SCANCODE_BACKSPACE:  return 0x0E08u;
    case SDL_SCANCODE_TAB:        return 0x0F09u;
    case SDL_SCANCODE_SPACE:      return 0x3920u;
    case SDL_SCANCODE_UP:         return 0x4800u;
    case SDL_SCANCODE_DOWN:       return 0x5000u;
    case SDL_SCANCODE_LEFT:       return 0x4B00u;
    case SDL_SCANCODE_RIGHT:      return 0x4D00u;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:      return 0x1D00u;
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:     return 0x2A00u;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:       return 0x3800u;
    case SDL_SCANCODE_F1:  return 0x3B00u;
    case SDL_SCANCODE_F2:  return 0x3C00u;
    case SDL_SCANCODE_F3:  return 0x3D00u;
    case SDL_SCANCODE_F4:  return 0x3E00u;
    case SDL_SCANCODE_F5:  return 0x3F00u;
    case SDL_SCANCODE_F6:  return 0x4000u;
    case SDL_SCANCODE_F7:  return 0x4100u;
    case SDL_SCANCODE_F8:  return 0x4200u;
    case SDL_SCANCODE_F9:  return 0x4300u;
    case SDL_SCANCODE_F10: return 0x4400u;
    case SDL_SCANCODE_F11: return 0x5700u;
    case SDL_SCANCODE_F12: return 0x5800u;
    case SDL_SCANCODE_MINUS:      return shift ? 0x0C5Fu : 0x0C2Du;
    case SDL_SCANCODE_EQUALS:     return shift ? 0x0D2Bu : 0x0D3Du;
    case SDL_SCANCODE_LEFTBRACKET:  return shift ? 0x1A7Bu : 0x1A5Bu;
    case SDL_SCANCODE_RIGHTBRACKET: return shift ? 0x1B7Du : 0x1B5Du;
    case SDL_SCANCODE_SEMICOLON:  return shift ? 0x273Au : 0x273Bu;
    case SDL_SCANCODE_APOSTROPHE: return shift ? 0x2822u : 0x2827u;
    case SDL_SCANCODE_GRAVE:      return shift ? 0x297Eu : 0x2960u;
    case SDL_SCANCODE_BACKSLASH:  return shift ? 0x2B7Cu : 0x2B5Cu;
    case SDL_SCANCODE_COMMA:      return shift ? 0x333Cu : 0x332Cu;
    case SDL_SCANCODE_PERIOD:     return shift ? 0x343Eu : 0x342Eu;
    case SDL_SCANCODE_SLASH:      return shift ? 0x353Fu : 0x352Fu;
    case SDL_SCANCODE_KP_PERIOD:  return 0x532Eu;
    case SDL_SCANCODE_KP_DIVIDE:  return 0x352Fu;
    default: break;
    }
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) {
        const char base = static_cast<char>('a' + (sc - SDL_SCANCODE_A));
        const char ch = shift ? static_cast<char>(base - 32) : base;
        const std::uint8_t scan = static_cast<std::uint8_t>(0x1E + (sc - SDL_SCANCODE_A));
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(scan) << 8) | static_cast<std::uint8_t>(ch));
    }
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) {
        static const char normal[] = "123456789";
        static const char shifted[] = "!@#$%^&*(";
        const int idx = sc - SDL_SCANCODE_1;
        const char ch = shift ? shifted[idx] : normal[idx];
        const std::uint8_t scan = static_cast<std::uint8_t>(0x02 + idx);
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(scan) << 8) | static_cast<std::uint8_t>(ch));
    }
    if (sc == SDL_SCANCODE_0) {
        const char ch = shift ? ')' : '0';
        return static_cast<std::uint16_t>(0x0B00u | static_cast<std::uint8_t>(ch));
    }
    if (ctrl && sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
        return static_cast<std::uint16_t>(0x0001u + static_cast<std::uint16_t>(sc - SDL_SCANCODE_A));
    return 0u;
}

inline std::uint16_t pollBiosKey(const bool* keys) noexcept {
    if (!keys || !FieldDosConfig::cfg.keyboardEnabled) return 0u;
    const bool shift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    const bool ctrl  = keys[SDL_SCANCODE_LCTRL]  || keys[SDL_SCANCODE_RCTRL];
    const bool alt   = keys[SDL_SCANCODE_LALT]   || keys[SDL_SCANCODE_RALT];
    state.keyboard.shift = shift;
    state.keyboard.ctrl  = ctrl;
    state.keyboard.alt   = alt;

    if (alt) {
        for (int sc = SDL_SCANCODE_A; sc <= SDL_SCANCODE_Z; ++sc) {
            if (keys[sc])
                return static_cast<std::uint16_t>(0x8000u
                    | static_cast<std::uint16_t>('A' + (sc - SDL_SCANCODE_A)));
        }
        if (keys[SDL_SCANCODE_X])
            return static_cast<std::uint16_t>(0x8000u | static_cast<std::uint16_t>('X'));
    }

    static const SDL_Scancode priority[] = {
        SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_ESCAPE,
        SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL, SDL_SCANCODE_SPACE,
        SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4,
        SDL_SCANCODE_F5, SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8,
        SDL_SCANCODE_F9, SDL_SCANCODE_F10, SDL_SCANCODE_F11, SDL_SCANCODE_F12,
        SDL_SCANCODE_TAB, SDL_SCANCODE_BACKSPACE,
    };
    for (SDL_Scancode sc : priority) {
        if (keys[sc]) {
            const std::uint16_t k = scancodeToBios(sc, shift, ctrl);
            if (k) return k;
        }
    }
    for (int sc = SDL_SCANCODE_A; sc <= SDL_SCANCODE_Z; ++sc) {
        if (keys[sc]) {
            const std::uint16_t k = scancodeToBios(static_cast<SDL_Scancode>(sc), shift, ctrl);
            if (k) return k;
        }
    }
    for (int sc = SDL_SCANCODE_1; sc <= SDL_SCANCODE_0; ++sc) {
        if (keys[sc]) {
            const std::uint16_t k = scancodeToBios(static_cast<SDL_Scancode>(sc), shift, ctrl);
            if (k) return k;
        }
    }
    static const SDL_Scancode punctuation[] = {
        SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS,
        SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_RIGHTBRACKET,
        SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_APOSTROPHE,
        SDL_SCANCODE_GRAVE, SDL_SCANCODE_BACKSLASH,
        SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH,
        SDL_SCANCODE_KP_PERIOD, SDL_SCANCODE_KP_DIVIDE,
    };
    for (SDL_Scancode sc : punctuation) {
        if (keys[sc]) {
            const std::uint16_t k = scancodeToBios(sc, shift, ctrl);
            if (k) return k;
        }
    }
    return 0u;
}

inline void pollMouse(float viewX, float viewY, float viewW, float viewH,
                      std::uint32_t mouseState) noexcept {
    if (!FieldDosConfig::cfg.mouseEnabled) return;
    const auto& c = FieldDosConfig::cfg;
    std::int32_t newX = 0, newY = 0;
    if (FieldDosViewport::focused)
        FieldDosViewport::mapMouse(viewX, viewY, newX, newY);
    if (newX < 0 || newY < 0) {
        const float nx = (viewW > 1.f) ? (viewX / viewW) : 0.5f;
        const float ny = (viewH > 1.f) ? (viewY / viewH) : 0.5f;
        newX = c.mouseMinX + static_cast<std::int32_t>(
            nx * static_cast<float>(c.mouseMaxX - c.mouseMinX));
        newY = c.mouseMinY + static_cast<std::int32_t>(
            ny * static_cast<float>(c.mouseMaxY - c.mouseMinY));
    }
    state.mouse.relX = newX - state.mouse.x;
    state.mouse.relY = newY - state.mouse.y;
    state.mouse.x = std::clamp(newX, static_cast<std::int32_t>(c.mouseMinX),
                               static_cast<std::int32_t>(c.mouseMaxX));
    state.mouse.y = std::clamp(newY, static_cast<std::int32_t>(c.mouseMinY),
                               static_cast<std::int32_t>(c.mouseMaxY));
    std::uint8_t btn = 0;
    if (mouseState & SDL_BUTTON_LMASK) btn |= 1u;
    if (mouseState & SDL_BUTTON_RMASK) btn |= 2u;
    if (mouseState & SDL_BUTTON_MMASK) btn |= 4u;
    const std::uint8_t prev = state.mouse.prevButtons;
    for (int i = 0; i < 3; ++i) {
        const std::uint8_t mask = static_cast<std::uint8_t>(1u << i);
        if ((btn & mask) && !(prev & mask) && state.mouse.pressCount[i] < 0xFFFFu)
            ++state.mouse.pressCount[i];
        if (!(btn & mask) && (prev & mask) && state.mouse.releaseCount[i] < 0xFFFFu)
            ++state.mouse.releaseCount[i];
    }
    state.mouse.prevButtons = btn;
    state.mouse.buttons = btn;
    state.mouse.installed = true;
}

inline void pollJoystick(float lx, float ly, float rx, float ry,
                         bool fire, bool use) noexcept {
    if (!FieldDosConfig::cfg.joystickEnabled) return;
    state.joystick.x1 = static_cast<std::int16_t>(std::clamp(lx, -1.f, 1.f) * 127.f);
    state.joystick.y1 = static_cast<std::int16_t>(std::clamp(-ly, -1.f, 1.f) * 127.f);
    state.joystick.x2 = static_cast<std::int16_t>(std::clamp(rx, -1.f, 1.f) * 127.f);
    state.joystick.y2 = static_cast<std::int16_t>(std::clamp(-ry, -1.f, 1.f) * 127.f);
    state.joystick.buttons1 = static_cast<std::uint8_t>((fire ? 1u : 0u) | (use ? 2u : 0u));
}

inline void packDataBus(std::uint32_t* bus, std::size_t count) noexcept {
    if (count < 42) return;
    bus[32] = state.primaryKey;
    bus[33] = state.keyboard.down ? 1u : 0u;
    bus[34] = static_cast<std::uint32_t>(state.mouse.x);
    bus[35] = static_cast<std::uint32_t>(state.mouse.y);
    bus[36] = static_cast<std::uint32_t>(state.mouse.buttons);
    bus[37] = static_cast<std::uint32_t>(state.mouse.wheel);
    bus[38] = static_cast<std::uint32_t>(static_cast<std::int32_t>(state.joystick.x1) & 0xFFFF);
    bus[39] = static_cast<std::uint32_t>(static_cast<std::int32_t>(state.joystick.y1) & 0xFFFF);
    bus[40] = static_cast<std::uint32_t>(state.joystick.buttons1);
    bus[41] = static_cast<std::uint32_t>(state.keyboard.biosKey);
}

inline void applyDoomMovementKeys(const bool* keys, bool gpUp, bool gpDown,
                                  bool gpLeft, bool gpRight) noexcept {
    if (!keys) return;
    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W] || gpUp)
        state.primaryKey = 328u;
    else if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S] || gpDown)
        state.primaryKey = 336u;
    else if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A] || gpLeft)
        state.primaryKey = 331u;
    else if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D] || gpRight)
        state.primaryKey = 333u;
    else if (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL])
        state.primaryKey = 17u;
    else if (keys[SDL_SCANCODE_SPACE])
        state.primaryKey = ' ';
    else if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER])
        state.primaryKey = '\r';
    else if (keys[SDL_SCANCODE_ESCAPE])
        state.primaryKey = 27u;
}

inline void applyGamepad(float lx, float ly, float rx, float ry,
                         float lt, float rt, std::uint32_t buttons,
                         const char* name = nullptr) noexcept {
    state.gamepad.connected = buttons != 0u || std::fabs(lx) > 0.01f || std::fabs(ly) > 0.01f
        || std::fabs(rx) > 0.01f || std::fabs(ry) > 0.01f || lt > 0.01f || rt > 0.01f;
    state.gamepad.lx = lx;
    state.gamepad.ly = ly;
    state.gamepad.rx = rx;
    state.gamepad.ry = ry;
    state.gamepad.lt = lt;
    state.gamepad.rt = rt;
    state.gamepad.buttons = buttons;
    if (name) std::snprintf(state.gamepad.name, sizeof state.gamepad.name, "%s", name);
    else state.gamepad.name[0] = '\0';
}

inline void pollFrame(const bool* keys, float mx, float my, float mw, float mh,
                      std::uint32_t mouseState,
                      float joyLx = 0.f, float joyLy = 0.f,
                      float joyRx = 0.f, float joyRy = 0.f,
                      bool joyFire = false, bool joyUse = false,
                      bool gpUp = false, bool gpDown = false,
                      bool gpLeft = false, bool gpRight = false) noexcept {
    const std::uint16_t bk = pollBiosKey(keys);
    state.keyboard.biosKey = bk;
    state.keyboard.down = bk != 0u;
    state.primaryKey = 0u;
    if (bk) {
        switch (bk) {
        case 0x4800u: state.primaryKey = 328u; break;
        case 0x5000u: state.primaryKey = 336u; break;
        case 0x4B00u: state.primaryKey = 331u; break;
        case 0x4D00u: state.primaryKey = 333u; break;
        case 0x1D00u: state.primaryKey = 17u; break;
        case 0x1C0Du: state.primaryKey = '\r'; break;
        default:
            if (bk & 0xFFu) state.primaryKey = static_cast<std::uint32_t>(bk & 0xFFu);
            else state.primaryKey = static_cast<std::uint32_t>(bk >> 8);
            break;
        }
    }
    if (FieldDosConfig::cfg.s1e1Playthrough)
        applyDoomMovementKeys(keys, gpUp, gpDown, gpLeft, gpRight);
    if (state.primaryKey != 0u) {
        state.keyboard.down = true;
        switch (state.primaryKey) {
        case 328u: state.keyboard.biosKey = 0x4800u; break;
        case 336u: state.keyboard.biosKey = 0x5000u; break;
        case 331u: state.keyboard.biosKey = 0x4B00u; break;
        case 333u: state.keyboard.biosKey = 0x4D00u; break;
        case 17u:  state.keyboard.biosKey = 0x1D00u; break;
        case '\r': state.keyboard.biosKey = 0x1C0Du; break;
        case 27u:  state.keyboard.biosKey = 0x011Bu; break;
        default: break;
        }
    }
    pollMouse(mx, my, mw, mh, mouseState);
    if (FieldDosConfig::cfg.gamepadAsJoystick)
        pollJoystick(joyLx, joyLy, joyRx, joyRy, joyFire, joyUse);
}

} // namespace FieldInput