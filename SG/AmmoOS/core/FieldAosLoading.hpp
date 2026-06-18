#pragma once

// =============================================================================
// AMOURANTH RTX — AmmoOS loading overlay (post-splash SDL card)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// =============================================================================

#include "ELLIE.hpp"
#include "OptionsMenu.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cmath>
#include <algorithm>
#include <cstring>

namespace FieldAosLoading {

inline SDL_Window*   overlayWin_    = nullptr;
inline SDL_Renderer* overlayRen_    = nullptr;
inline SDL_Texture*  ammoTex_       = nullptr;
inline TTF_Font*     titleFont_     = nullptr;
inline TTF_Font*     subFont_       = nullptr;
inline SDL_Window*   parentWin_     = nullptr;
inline bool          active_        = false;
inline bool          fadingOut_     = false;
inline bool          dismissReq_    = false;
inline bool          booted_        = false;
inline bool          guestSeeded_   = false;
inline double        startTime_     = 0.0;
inline double        fadeOutStart_  = 0.0;
inline int           presentCount_  = 0;

constexpr double kMinShowSec   = 0.45;
constexpr double kFadeOutSec   = 0.35;
constexpr double kForceDismiss = 8.0;
constexpr float  kCardW        = 440.0f;
constexpr float  kCardH        = 300.0f;
constexpr float  kPad          = 28.0f;

inline float clamp01(float v) noexcept {
    return std::clamp(v, 0.0f, 1.0f);
}

inline float easeOutCubic(float t) noexcept {
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

inline float easeInOutSine(float t) noexcept {
    return 0.5f - 0.5f * std::cos(static_cast<float>(M_PI) * t);
}

inline void setDrawColorA(SDL_Renderer* ren, Uint8 r, Uint8 g, Uint8 b, Uint8 a) noexcept {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, r, g, b, a);
}

inline bool openFont(TTF_Font** out, int size) noexcept {
    if (!out || *out) return *out != nullptr;
    const char* paths[] = {
        "assets/fonts/JetBrainsMono-Regular.ttf",
        "assets/fonts/font.ttf",
        "assets/fonts/brass-mono-font/BrassMonoRegular-d9WLg.ttf",
        nullptr
    };
    for (int i = 0; paths[i]; ++i) {
        if (TTF_Font* f = TTF_OpenFont(paths[i], size)) {
            *out = f;
            return true;
        }
    }
    return false;
}

inline void drawText(SDL_Renderer* ren, TTF_Font* font, const char* text,
                     float x, float y, SDL_Color color, float alpha) noexcept {
    if (!ren || !font || !text || !text[0] || alpha <= 0.01f) return;
    SDL_Surface* surf = TTF_RenderText_Blended(font, text, 0, color);
    if (!surf) return;
    const float tw = static_cast<float>(surf->w);
    const float th = static_cast<float>(surf->h);
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_DestroySurface(surf);
    if (!tex) return;
    SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(clamp01(alpha) * 255.0f));
    SDL_FRect dst{ x, y, tw, th };
    SDL_RenderTexture(ren, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

inline void syncOverlayToParent() noexcept {
    if (!overlayWin_ || !parentWin_) return;
    int px = 0, py = 0, pw = 0, ph = 0;
    SDL_GetWindowPosition(parentWin_, &px, &py);
    SDL_GetWindowSize(parentWin_, &pw, &ph);
    if (pw < 1) pw = Options::SDL3::DefaultWidth;
    if (ph < 1) ph = Options::SDL3::DefaultHeight;

    const int ow = static_cast<int>(kCardW + kPad * 2.0f);
    const int oh = static_cast<int>(kCardH + kPad * 2.0f);
    const int ox = px + (pw - ow) / 2;
    const int oy = py + (ph - oh) / 2;
    SDL_SetWindowPosition(overlayWin_, ox, oy);
    SDL_SetWindowSize(overlayWin_, ow, oh);
}

inline void applyDeferredFullscreen() noexcept {
    if (Options::SDL3::PendingFullscreenAfterLoad) {
        Options::SDL3::PendingFullscreenAfterLoad = false;
        Options::SDL3::PendingFullscreenApply = true;
    }
}

inline void shutdown() noexcept {
    applyDeferredFullscreen();
    if (titleFont_) { TTF_CloseFont(titleFont_); titleFont_ = nullptr; }
    if (subFont_)   { TTF_CloseFont(subFont_);   subFont_   = nullptr; }
    if (ammoTex_)   { SDL_DestroyTexture(ammoTex_); ammoTex_ = nullptr; }
    if (overlayRen_) { SDL_DestroyRenderer(overlayRen_); overlayRen_ = nullptr; }
    if (overlayWin_) { SDL_DestroyWindow(overlayWin_); overlayWin_ = nullptr; }
    parentWin_    = nullptr;
    active_       = false;
    fadingOut_    = false;
    dismissReq_   = false;
    booted_       = false;
    guestSeeded_  = false;
    presentCount_ = 0;
}

inline void requestDismiss() noexcept {
    if (!active_ || dismissReq_) return;
    dismissReq_ = true;
    fadingOut_  = true;
    fadeOutStart_ = TotalTime::get().seconds();
    if (overlayWin_) SDL_HideWindow(overlayWin_);
    LOG_INFO_CAT("AOS_LOAD", "AmmoOS loading overlay dismissing (booted={} presents={})",
        booted_ ? "yes" : "no", presentCount_);
}

inline void tick() noexcept;  // defined below

inline bool begin(SDL_Window* parent) noexcept {
    if (Options::SDL3::HeadlessMode || !parent) return false;
    if (active_) return true;

    parentWin_ = parent;
    const int ow = static_cast<int>(kCardW + kPad * 2.0f);
    const int oh = static_cast<int>(kCardH + kPad * 2.0f);

    overlayWin_ = SDL_CreateWindow(
        "AmmoOS Loading",
        ow, oh,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_HIDDEN);
    if (!overlayWin_) {
        LOG_WARNING_CAT("AOS_LOAD", "Overlay window create failed: {}", SDL_GetError());
        return false;
    }
    syncOverlayToParent();

    overlayRen_ = SDL_CreateRenderer(overlayWin_, nullptr);
    if (!overlayRen_) {
        LOG_WARNING_CAT("AOS_LOAD", "Overlay renderer failed: {}", SDL_GetError());
        shutdown();
        return false;
    }
    SDL_SetRenderVSync(overlayRen_, 0);

    const char* texPaths[] = { "assets/textures/ammo.png", nullptr };
    for (int i = 0; texPaths[i]; ++i) {
        if (SDL_Surface* surf = IMG_Load(texPaths[i])) {
            ammoTex_ = SDL_CreateTextureFromSurface(overlayRen_, surf);
            SDL_DestroySurface(surf);
            if (ammoTex_) break;
        }
    }

    if (TTF_WasInit()) {
        openFont(&titleFont_, 34);
        openFont(&subFont_, 20);
    }

    startTime_     = TotalTime::get().seconds();
    fadeOutStart_  = 0.0;
    dismissReq_    = false;
    fadingOut_     = false;
    booted_        = false;
    guestSeeded_   = false;
    presentCount_  = 0;
    active_        = true;

    SDL_ShowWindow(overlayWin_);
    SDL_RaiseWindow(overlayWin_);
    tick();
    LOG_INFO_CAT("AOS_LOAD", "AmmoOS loading card active");
    return true;
}

inline void signalGuestSeeded() noexcept {
    guestSeeded_ = true;
}

inline void signalBooted() noexcept {
    booted_ = true;
    guestSeeded_ = true;
    if (!active_) return;
    LOG_INFO_CAT("AOS_LOAD", "AmmoOS booted — removing loading overlay");
    shutdown();
}

inline void onPresent() noexcept {
    if (!active_ && !fadingOut_) return;
    ++presentCount_;
}

inline void evaluateDismiss() noexcept {
    if (!active_ || dismissReq_) return;

    const double elapsed = TotalTime::get().seconds() - startTime_;
    if (elapsed >= kForceDismiss) {
        LOG_WARNING_CAT("AOS_LOAD", "Force-dismissing loading overlay after {:.1f}s", elapsed);
        requestDismiss();
        return;
    }
    if (elapsed < kMinShowSec) return;
    if (presentCount_ < 1) return;

    if (booted_) {
        requestDismiss();
        return;
    }
    if (guestSeeded_ && presentCount_ >= 2 && elapsed >= 1.2) {
        LOG_WARNING_CAT("AOS_LOAD", "Guest seeded without shell boot — dismissing overlay anyway");
        requestDismiss();
    }
}

inline bool isActive() noexcept {
    return active_ || fadingOut_;
}

inline void tick() noexcept {
    if (!active_ && !fadingOut_) return;
    if (!overlayRen_ || !overlayWin_) return;

    syncOverlayToParent();
    evaluateDismiss();

    int ww = 0, wh = 0;
    SDL_GetWindowSize(overlayWin_, &ww, &wh);
    if (ww < 1) ww = static_cast<int>(kCardW + kPad * 2.0f);
    if (wh < 1) wh = static_cast<int>(kCardH + kPad * 2.0f);

    const double now = TotalTime::get().seconds();
    const double t = now - startTime_;
    float masterAlpha = 1.0f;
    if (fadingOut_) {
        const double fadeT = (now - fadeOutStart_) / kFadeOutSec;
        if (fadeT >= 1.0) {
            shutdown();
            LOG_SUCCESS_CAT("AOS_LOAD", "Loading overlay removed — main screen visible");
            return;
        }
        masterAlpha = 1.0f - easeOutCubic(static_cast<float>(fadeT));
    } else {
        masterAlpha = easeOutCubic(clamp01(static_cast<float>(t / 0.30)));
    }

    const float cx = ww * 0.5f;
    const float cy = wh * 0.5f;
    const float cardX = cx - kCardW * 0.5f;
    const float cardY = cy - kCardH * 0.5f;

    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(t * 2.4));
    const float glowA = masterAlpha * (0.18f + 0.14f * pulse);

    SDL_SetRenderDrawBlendMode(overlayRen_, SDL_BLENDMODE_BLEND);
    setDrawColorA(overlayRen_, 8, 4, 12, static_cast<Uint8>(225.0f * masterAlpha));
    SDL_RenderClear(overlayRen_);

    for (int ring = 3; ring >= 0; --ring) {
        const float expand = static_cast<float>(ring) * 3.0f;
        const Uint8 ringA = static_cast<Uint8>(glowA * 255.0f * (0.35f + 0.2f * ring));
        setDrawColorA(overlayRen_, 255, 40, 140, ringA);
        SDL_FRect glow{
            cardX - 10.0f - expand, cardY - 10.0f - expand,
            kCardW + 20.0f + expand * 2.0f, kCardH + 20.0f + expand * 2.0f
        };
        SDL_RenderRect(overlayRen_, &glow);
    }

    setDrawColorA(overlayRen_, 24, 14, 30, static_cast<Uint8>(235.0f * masterAlpha));
    SDL_FRect card{ cardX, cardY, kCardW, kCardH };
    SDL_RenderFillRect(overlayRen_, &card);

    const Uint8 borderA = static_cast<Uint8>((170.0f + 60.0f * pulse) * masterAlpha);
    setDrawColorA(overlayRen_, 255, 60, 150, borderA);
    SDL_RenderRect(overlayRen_, &card);

    setDrawColorA(overlayRen_, 255, 105, 175, static_cast<Uint8>(90.0f * masterAlpha));
    SDL_FRect inner{ cardX + 2.0f, cardY + 2.0f, kCardW - 4.0f, kCardH - 4.0f };
    SDL_RenderRect(overlayRen_, &inner);

    const float iconSize = 72.0f;
    const float iconY = cardY + 36.0f;
    if (ammoTex_) {
        const float spin = static_cast<float>(t * 95.0);
        const float scale = 1.0f + 0.06f * std::sin(static_cast<float>(t * 3.1));
        const float sz = iconSize * scale;
        SDL_FRect iconDst{ cx - sz * 0.5f, iconY, sz, sz };
        SDL_RenderTextureRotated(overlayRen_, ammoTex_, nullptr, &iconDst, spin, nullptr, SDL_FLIP_NONE);
    }

    if (titleFont_) {
        const char* title = "AmmoOS";
        int tw = 0, th = 0;
        if (TTF_GetStringSize(titleFont_, title, 0, &tw, &th)) {
            drawText(overlayRen_, titleFont_, title,
                cx - tw * 0.5f, iconY + iconSize + 18.0f,
                { 255, 230, 245, 255 }, masterAlpha);
        }
    }

    if (subFont_) {
        const int dotPhase = static_cast<int>(t * 3.5) % 4;
        char subtitle[24];
        std::snprintf(subtitle, sizeof(subtitle), "Now Loading%s",
            dotPhase == 0 ? "" : dotPhase == 1 ? "." : dotPhase == 2 ? ".." : "...");
        int sw = 0, sh = 0;
        if (TTF_GetStringSize(subFont_, subtitle, 0, &sw, &sh)) {
            drawText(overlayRen_, subFont_, subtitle,
                cx - sw * 0.5f, iconY + iconSize + 58.0f,
                { 200, 170, 195, 255 }, masterAlpha * (0.75f + 0.25f * pulse));
        }
    }

    const float barW = 260.0f;
    const float barH = 7.0f;
    const float barX = cx - barW * 0.5f;
    const float barY = cardY + kCardH - 52.0f;

    setDrawColorA(overlayRen_, 40, 24, 48, static_cast<Uint8>(220.0f * masterAlpha));
    SDL_FRect barBg{ barX, barY, barW, barH };
    SDL_RenderFillRect(overlayRen_, &barBg);

    float loadProgress = 0.12f;
    if (dismissReq_) {
        const double fadeT = std::min((now - fadeOutStart_) / (kFadeOutSec * 0.55), 1.0);
        loadProgress = 0.88f + 0.12f * easeOutCubic(static_cast<float>(fadeT));
    } else {
        loadProgress = 0.12f + 0.80f * (1.0f - std::exp(static_cast<float>(-t * 0.65)));
    }
    loadProgress = clamp01(loadProgress);

    setDrawColorA(overlayRen_, 255, 70, 155, static_cast<Uint8>(240.0f * masterAlpha));
    SDL_FRect barFill{ barX, barY, barW * loadProgress, barH };
    SDL_RenderFillRect(overlayRen_, &barFill);

    const float shimmerW = 42.0f;
    const float shimmerTravel = barW + shimmerW;
    const float shimmerX = barX - shimmerW + shimmerTravel * easeInOutSine(
        static_cast<float>(std::fmod(t * 0.85, 1.0)));
    setDrawColorA(overlayRen_, 255, 200, 230, static_cast<Uint8>(120.0f * masterAlpha));
    SDL_FRect shimmer{ shimmerX, barY, shimmerW, barH };
    SDL_RenderFillRect(overlayRen_, &shimmer);

    SDL_RenderPresent(overlayRen_);
}

} // namespace FieldAosLoading