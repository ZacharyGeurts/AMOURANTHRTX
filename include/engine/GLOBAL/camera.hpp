// include/engine/GLOBAL/camera.hpp
// =============================================================================
// AMOURANTH RTX © 2025 — BRAINDEAD CAMERA + STONEKEY v9 — PINK PHOTONS MINIMAL
// ONLY: move, rotate, zoom, position, fov — ALL ENCRYPTED WITH STONEKEY
// NO LOGS. NO ANTI-CHEAT. NO BLOAT. JUST WORKS. ZERO WARNINGS.
// =============================================================================

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <mutex>
#include <atomic>

extern uint64_t get_kStone1() noexcept;
extern uint64_t get_kStone2() noexcept;

// =============================================================================
// GLOBAL CAMERA — SINGLETON — BRAINDEAD EASY
// =============================================================================
class GlobalCamera {
public:
    static GlobalCamera& get() noexcept {
        static GlobalCamera instance;
        return instance;
    }

    GlobalCamera(const GlobalCamera&) = delete;
    GlobalCamera& operator=(const GlobalCamera&) = delete;

    // --- INIT ---
    void init(glm::vec3 pos = {0,5,10}, float fov = 60.0f) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        pos_ = pos;
        fov_ = fov;
        yaw_ = -90.0f;
        pitch_ = 0.0f;
        updateVectors();
        gen_++;
    }

    // --- MOVEMENT ---
    void move(glm::vec3 delta) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        pos_ += delta;
        updateVectors();
        gen_++;
    }

    void moveForward(float s) noexcept { move(front_ * s); }
    void moveRight(float s)   noexcept { move(right_ * s); }
    void moveUp(float s)      noexcept { move(up_    * s); }

    void rotate(float yaw, float pitch) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        yaw_   += yaw;
        pitch_ = glm::clamp(pitch_ + pitch, -89.0f, 89.0f);
        updateVectors();
        gen_++;
    }

    void zoom(float f) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        fov_ = glm::clamp(fov_ - f, 1.0f, 120.0f);
        gen_++;
    }

    void setPos(glm::vec3 p) noexcept { std::lock_guard<std::mutex> lock(mtx_); pos_ = p; updateVectors(); gen_++; }
    void setFov(float f)     noexcept { std::lock_guard<std::mutex> lock(mtx_); fov_ = glm::clamp(f, 1.0f, 120.0f); gen_++; }

    // --- GETTERS (RAW) ---
    glm::vec3 pos()   const noexcept { return pos_; }
    glm::vec3 front() const noexcept { return front_; }
    glm::vec3 right() const noexcept { return right_; }
    glm::vec3 up()    const noexcept { return up_; }
    float fov()       const noexcept { return fov_; }

    glm::mat4 view() const noexcept {
        ensureCached();
        return view_;
    }

    glm::mat4 proj(float aspect) const noexcept {
        return glm::perspective(glm::radians(fov_), aspect, 0.1f, 10000.0f);
    }

    // --- ENCRYPTED (STONEKEY v9) ---
    uint64_t encPos()  const noexcept { return encrypt(pos_, gen_.load()); }
    uint64_t encView() const noexcept { ensureCached(); return encView_; }

    // --- OBFUSCATE / DEOBFUSCATE ---
    static uint64_t obfuscate(uint64_t h) noexcept {
        uint64_t k = get_kStone1() ^ get_kStone2();
        return h ^ k;
    }

    static uint64_t deobfuscate(uint64_t h) noexcept {
        uint64_t k = get_kStone1() ^ get_kStone2();
        return h ^ k;
    }

private:
    GlobalCamera() = default;

    mutable std::mutex mtx_;
    std::atomic<uint64_t> gen_{1};

    glm::vec3 pos_{0,5,10};
    glm::vec3 front_{0,0,-1};
    glm::vec3 right_{1,0,0};
    glm::vec3 up_{0,1,0};
    float yaw_ = -90.0f;
    float pitch_ = 0.0f;
    float fov_ = 60.0f;

    mutable glm::mat4 view_{1.0f};
    mutable uint64_t encView_{0};
    mutable uint64_t cachedGen_{0};

    void updateVectors() noexcept {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
        f.y = sin(glm::radians(pitch_));
        f.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
        front_ = glm::normalize(f);
        right_ = glm::normalize(glm::cross(front_, {0,1,0}));
        up_    = glm::normalize(glm::cross(right_, front_));
    }

    void ensureCached() const noexcept {
        uint64_t g = gen_.load();
        if (cachedGen_ != g) {
            std::lock_guard<std::mutex> lock(mtx_);
            view_ = glm::lookAt(pos_, pos_ + front_, up_);
            encView_ = encryptMat4(view_, g);
            cachedGen_ = g;
        }
    }

    // STONEKEY v9 — ZERO COST
    static uint64_t encrypt(const glm::vec3& v, uint64_t g) noexcept {
        uint32_t x = std::bit_cast<uint32_t>(v.x);
        uint32_t y = std::bit_cast<uint32_t>(v.y);
        uint32_t z = std::bit_cast<uint32_t>(v.z);
        uint64_t a = (uint64_t(x) << 32) ^ get_kStone1() ^ g;
        uint64_t b = (uint64_t(y) << 16) ^ get_kStone2() ^ g;
        uint64_t c = uint64_t(z) ^ 0xDEADBEEFULL ^ g;
        return std::rotl(a ^ b ^ c, 23) ^ g;
    }

    static uint64_t encryptMat4(const glm::mat4& m, uint64_t g) noexcept {
        uint64_t h = 0;
        for (int i = 0; i < 16; ++i) {
            uint32_t f = std::bit_cast<uint32_t>(m[i/4][i%4]);
            h ^= std::rotl(uint64_t(f) ^ g, i);
        }
        return h ^ get_kStone1() ^ get_kStone2() ^ 0xBEEFBABEULL;
    }
};

// =============================================================================
// LAZY CAM — BRAINDEAD PROXY
// =============================================================================
class LazyCam {
public:
    void forward(float s) noexcept { GlobalCamera::get().moveForward(s); }
    void right(float s)   noexcept { GlobalCamera::get().moveRight(s); }
    void up(float s)      noexcept { GlobalCamera::get().moveUp(s); }
    void rotate(float y, float p) noexcept { GlobalCamera::get().rotate(y, p); }
    void zoom(float f)    noexcept { GlobalCamera::get().zoom(f); }
    void setPos(glm::vec3 p) noexcept { GlobalCamera::get().setPos(p); }
    void setFov(float f)  noexcept { GlobalCamera::get().setFov(f); }

    glm::vec3 pos()   const noexcept { return GlobalCamera::get().pos(); }
    glm::mat4 view()  const noexcept { return GlobalCamera::get().view(); }
    glm::mat4 proj(float a) const noexcept { return GlobalCamera::get().proj(a); }
    float fov()       const noexcept { return GlobalCamera::get().fov(); }
};

inline LazyCam g_lazyCam;

// =============================================================================
// BRAINDEAD MACROS
// =============================================================================
#define CAM GlobalCamera::get()
#define CAM_MOVE(d)     CAM.move(d)
#define CAM_FORWARD(s)  CAM.moveForward(s)
#define CAM_RIGHT(s)    CAM.moveRight(s)
#define CAM_UP(s)       CAM.moveUp(s)
#define CAM_ROTATE(y,p) CAM.rotate(y,p)
#define CAM_ZOOM(f)     CAM.zoom(f)
#define CAM_SETPOS(p)   CAM.setPos(p)
#define CAM_SETFOV(f)   CAM.setFov(f)
#define CAM_POS()       CAM.pos()
#define CAM_FRONT()     CAM.front()
#define CAM_VIEW()      CAM.view()
#define CAM_PROJ(a)     CAM.proj(a)
#define CAM_FOV()       CAM.fov()
#define CAM_ENC_POS()   CAM.encPos()
#define CAM_ENC_VIEW()  CAM.encView()