#include "breakout.hpp"
#include <format>

namespace Breakout {

GameState state;

GameState::GameState() {
    brickHitMask.assign((BRICK_COLS * BRICK_ROWS + 63) / 64, 0ull); // all bits 0 = alive
    reset();
}

void GameState::reset() {
    paddlePos   = {0.0f, PADDLE_Y};
    ballPos     = {0.0f, PADDLE_Y + PADDLE_HEIGHT/2 + BALL_RADIUS + 0.1f};
    ballDir     = glm::normalize(glm::vec2(0.4f, 1.0f));
    ballSpeed   = BALL_SPEED_BASE;
    ballOnPaddle = true;

    lives = 3;
    score = 0;
    gameOver = false;
    won = false;

    std::fill(brickHitMask.begin(), brickHitMask.end(), 0ull);

    rngSeed = static_cast<uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count() ^ 0xBEEFCAFEu
    );

    startTime = TotalTime::get().seconds();
}

void GameState::launchBall() {
    if (!ballOnPaddle) return;
    ballOnPaddle = false;

    // slight random angle from center
    float angle = (rnd() - 0.5f) * 0.8f;
    ballDir = glm::normalize(glm::vec2(std::sin(angle), std::cos(angle)));
}

void GameState::movePaddle(float dx) {  // dx in [-1,1]
    paddlePos.x += dx * 8.0f * (float)TotalTime::get().deltaSeconds();
    paddlePos.x = glm::clamp(paddlePos.x, -FIELD_WIDTH/2 + PADDLE_WIDTH/2, FIELD_WIDTH/2 - PADDLE_WIDTH/2);

    if (ballOnPaddle) {
        ballPos.x = paddlePos.x;
    }
}

void GameState::tick(double now, float dt) {
    if (gameOver || won) return;

    if (ballOnPaddle) {
        ballPos.x = paddlePos.x;
        ballPos.y = PADDLE_Y + PADDLE_HEIGHT/2 + BALL_RADIUS + 0.08f;
        return;
    }

    // Very basic CPU prediction (for immediate feel & sound triggers)
    glm::vec2 next = ballPos + ballDir * ballSpeed * dt;

    // Wall bounce (left/right/top)
    if (next.x <= -FIELD_WIDTH/2 + BALL_RADIUS || next.x >= FIELD_WIDTH/2 - BALL_RADIUS) {
        ballDir.x = -ballDir.x;
    }
    if (next.y >= FIELD_HEIGHT/2 - BALL_RADIUS) {
        ballDir.y = -ballDir.y;
    }

    // Bottom → lose life
    if (next.y <= -FIELD_HEIGHT/2 - BALL_RADIUS) {
        lives--;
        if (lives <= 0) {
            gameOver = true;
        } else {
            reset(); // soft reset — keep score
            ballOnPaddle = true;
        }
        return;
    }

    // Paddle collision (simple AABB check)
    if (next.y <= PADDLE_Y + PADDLE_HEIGHT/2 + BALL_RADIUS &&
        next.y >= PADDLE_Y - PADDLE_HEIGHT/2 - BALL_RADIUS &&
        std::abs(next.x - paddlePos.x) <= PADDLE_WIDTH/2 + BALL_RADIUS) {
        ballDir.y = std::abs(ballDir.y); // force upward

        // Angle based on hit position
        float hitFrac = (next.x - paddlePos.x) / (PADDLE_WIDTH/2);
        ballDir.x += hitFrac * 0.8f;
        ballDir = glm::normalize(ballDir);

        // slight speed increase per bounce
        ballSpeed = std::min(ballSpeed + 0.15f, 9.5f);
    }

    ballPos = next;
}

void init() {
    state.reset();
    LOG_AMOURANTH("Breakout initialized — waiting for activation 💖");
}

void update(double now) {
    float dt = static_cast<float>(now - state.startTime); // or use delta
    state.tick(now, dt);
}

void renderFrame() {
    if (raycanvas) {
        raycanvas->maybeUpdateCanvas();
    }
}

void shutdown() {
    // nothing persistent yet
}

} // namespace Breakout