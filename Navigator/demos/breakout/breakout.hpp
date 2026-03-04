#pragma once

// =============================================================================
// Breakout (Arkanoid clone) for AMOURANTH RTX Engine
// Single-shader GPU-accelerated version
// =============================================================================

#include "engine/camera.hpp"
#include "engine/ELLIE.hpp"
#include "engine/AMOURANTHRTX.hpp"
#include "engine/Pipeline.hpp"
#include "engine/RayCanvas.hpp"

#include <vector>
#include <random>
#include <chrono>

namespace Breakout {

constexpr int   BRICK_COLS          = 14;
constexpr int   BRICK_ROWS          = 9;
constexpr float FIELD_WIDTH         = 12.0f;
constexpr float FIELD_HEIGHT        = 9.0f;
constexpr float PADDLE_WIDTH        = 2.2f;
constexpr float PADDLE_HEIGHT       = 0.22f;
constexpr float PADDLE_Y            = -4.0f;
constexpr float BALL_RADIUS         = 0.12f;
constexpr float BALL_SPEED_BASE     = 5.5f;

// ─────────────────────────────────────────────────────────────────────────────
// Game state (CPU side — minimal, mostly for input & UI)
// The real simulation & rendering lives in the compute shader
// ─────────────────────────────────────────────────────────────────────────────
struct GameState {
    glm::vec2 paddlePos     {0.0f, PADDLE_Y};
    glm::vec2 ballPos       {0.0f, PADDLE_Y + PADDLE_HEIGHT/2 + BALL_RADIUS + 0.1f};
    glm::vec2 ballDir       {0.0f, 1.0f};           // normalized
    float     ballSpeed     = BALL_SPEED_BASE;

    bool      ballOnPaddle  = true;
    int       lives         = 3;
    int       score         = 0;
    bool      gameOver      = false;
    bool      won           = false;

    // Brick hit mask (64-bit chunks — enough for 14×9 = 126 bricks)
    std::vector<uint64_t> brickHitMask;             // 1 = destroyed

    uint32_t  rngSeed       = 0u;
    double    startTime     = 0.0;

    GameState();
    void reset();
    void launchBall();
    void movePaddle(float dx);                      // normalized -1..1
    void tick(double now, float dt);
};

extern GameState state;

// Called once at startup
void init();

// Called every frame before rendering
void update(double now);

// Called from main loop — forwards to raycanvas
void renderFrame();

// Cleanup (if needed)
void shutdown();

} // namespace Breakout