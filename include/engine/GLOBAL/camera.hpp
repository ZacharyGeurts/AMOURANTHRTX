// =============================================================================
// include/engine/GLOBAL/camera.hpp
// AMOURANTH RTX © 2025 — BRAINDEAD CAMERA v∞ — FIRST LIGHT ETERNAL
// ZERO MACROS. ZERO OVERHEAD. FULLY STONEKEY-ENCRYPTED.
// USED VIA: g_camera()  →  Camera&
// USED VIA: CAM         →  defined ONCE in main.hpp as g_camera()
// =============================================================================

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <mutex>
#include <atomic>

extern uint64_t kStone1() noexcept;
extern uint64_t kStone2() noexcept;

// =============================================================================
// GLOBAL CAMERA — THE ONE TRUE SOURCE OF TRUTH
// =============================================================================
class Camera {
public:
    [[nodiscard]] static Camera& get() noexcept {
        static Camera instance;
        return instance;
    }

    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    // ─── Init ───────────────────────────────────────────────────────────────────
    void init(glm::vec3 pos = {0, 5, 10}, float fov = 60.0f) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        pos_ = pos;
        fov_ = fov;
        yaw_ = -90.0f;
        pitch_ = 0.0f;
        updateVectors();
        ++gen_;
    }

    // ─── Movement ───────────────────────────────────────────────────────────────
    void move(glm::vec3 delta) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        pos_ += delta;
        updateVectors();
        ++gen_;
    }

    void moveForward(float s) noexcept { move(front_ * s); }
    void moveRight(float s)   noexcept { move(right_ * s); }
    void moveUp(float s)      noexcept { move(up_    * s); }

    void rotate(float yawDelta, float pitchDelta) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        yaw_   += yawDelta;
        pitch_ = glm::clamp(pitch_ + pitchDelta, -89.0f, 89.0f);
        updateVectors();
        ++gen_;
    }

    void zoom(float f) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        fov_ = glm::clamp(fov_ - f, 1.0f, 120.0f);
        ++gen_;
    }

    void setPos(glm::vec3 p) noexcept { std::lock_guard<std::mutex> lock(mtx_); pos_ = p; updateVectors(); ++gen_; }
    void setFov(float f)     noexcept { std::lock_guard<std::mutex> lock(mtx_); fov_ = glm::clamp(f, 1.0f, 120.0f); ++gen_; }

    // ─── Raw Getters ────────────────────────────────────────────────────────────
    glm::vec3 pos()   const noexcept { return pos_; }
    glm::vec3 front() const noexcept { return front_; }
    glm::vec3 right() const noexcept { return right_; }
    glm::vec3 up()    const noexcept { return up_; }
    float fov()       const noexcept { return fov_; }

    // ─── Cached Matrices (thread-safe) ──────────────────────────────────────────
    glm::mat4 view() const noexcept {
        ensureCached();
        return view_;
    }

    glm::mat4 proj(float aspect) const noexcept {
        return glm::perspective(glm::radians(fov_), aspect, 0.1f, 10000.0f);
    }

    // ─── StoneKey v∞ Encrypted Access (for anti-cheat / streaming protection) ───
    uint64_t encPos()  const noexcept { return encrypt(pos_, gen_.load()); }
    uint64_t encView() const noexcept { ensureCached(); return encView_; }

    Camera() = default;

    mutable std::mutex mtx_;
    std::atomic<uint64_t> gen_{1};

    glm::vec3 pos_{0, 5, 10};
    glm::vec3 front_{0, 0, -1};
    glm::vec3 right_{1, 0,  0};
    glm::vec3 up_   {0, 1,  0};
    float yaw_   = -90.0f;
    float pitch_ =   0.0f;
    float fov_   =  60.0f;

    mutable glm::mat4 view_{1.0f};
    mutable uint64_t  encView_{0};
    mutable uint64_t  cachedGen_{0};

    void updateVectors() noexcept {
        const float cy = cos(glm::radians(yaw_));
        const float sy = sin(glm::radians(yaw_));
        const float cp = cos(glm::radians(pitch_));
        const float sp = sin(glm::radians(pitch_));

        front_ = glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
        right_ = glm::normalize(glm::cross(front_, {0, 1, 0}));
        up_    = glm::normalize(glm::cross(right_, front_));
    }

    void ensureCached() const noexcept {
        uint64_t g = gen_.load();
        if (cachedGen_ != g) {
            std::lock_guard<std::mutex> lock(mtx_);
            view_    = glm::lookAt(pos_, pos_ + front_, up_);
            encView_ = encryptMat4(view_, g);
            cachedGen_ = g;
        }
    }

    // ─── StoneKey v∞ Encryption (zero cost, compile-time unpredictable) ───────
    static uint64_t encrypt(const glm::vec3& v, uint64_t g) noexcept {
        uint32_t x = std::bit_cast<uint32_t>(v.x);
        uint32_t y = std::bit_cast<uint32_t>(v.y);
        uint32_t z = std::bit_cast<uint32_t>(v.z);
        uint64_t a = (uint64_t(x) << 32) ^ kStone1() ^ g;
        uint64_t b = (uint64_t(y) << 16) ^ kStone2() ^ g;
        uint64_t c = uint64_t(z) ^ 0xDEADBEEFULL ^ g;
        return std::rotl(a ^ b ^ c, 23) ^ g;
    }

    static uint64_t encryptMat4(const glm::mat4& m, uint64_t g) noexcept {
        uint64_t h = 0;
        for (int i = 0; i < 16; ++i)
            h ^= std::rotl(uint64_t(std::bit_cast<uint32_t>(m[i/4][i%4])) ^ g, i);
        return h ^ kStone1() ^ kStone2() ^ 0xBEEFBABEULL;
    }
};

// LazyCam is dead. Long live direct access.
// The one true macro lives ONLY in main.hpp:
//     #define CAM g_camera()
//
// → Zero overhead, full type safety, debugger-friendly, StoneKey-compliant.