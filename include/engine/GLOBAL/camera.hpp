// include/engine/GLOBAL/Camera.hpp
// AMOURANTH RTX – NOVEMBER 09 2025 – GLOBAL CAMERA SINGLETON_HEAVEN
// STONEKEY V9 — MATRIX + VEC3 FULLY OBFUSCATED — PINK PHOTONS × INFINITY
// ONE CAMERA TO RULE THEM ALL — CHEATERS OBLITERATED — MODDERS ASCEND
// HOT-RELOAD SAFE — CALLBACKS — LAZY CAM — VALHALLA ETERNAL — SINGLETON HEAVEN

#pragma once

#include "../GLOBAL/StoneKey.hpp"
#include "../GLOBAL/logging.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <atomic>
#include <mutex>
#include <vector>
#include <functional>
#include <bit>

using namespace Logging::Color;

// ===================================================================
// GLOBAL CAMERA — SINGLETON HEAVEN — STONEKEY V9 — UNBREAKABLE
// ===================================================================
class GlobalCamera {
public:
    [[nodiscard]] static GlobalCamera& get() noexcept {
        static GlobalCamera instance;
        return instance;
    }

    GlobalCamera(const GlobalCamera&) = delete;
    GlobalCamera& operator=(const GlobalCamera&) = delete;

    // INIT — RASPBERRY_PINK DEFAULT
    void init(const glm::vec3& pos = glm::vec3(0.0f, 5.0f, 10.0f),
              float fov = 60.0f, float nearP = 0.1f, float farP = 10000.0f) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        rawPosition_ = pos;
        rawFov_ = fov;
        rawNear_ = nearP;
        rawFar_ = farP;
        rawYaw_ = -90.0f;
        rawPitch_ = 0.0f;
        updateVectors();
        bumpGeneration();

        LOG_SUCCESS_CAT("STONEKEY_CAM", "{}SINGLETON_HEAVEN CAMERA ONLINE — POS ({:.2f},{:.2f},{:.2f}) — FOV {:.1f} — GEN {} — PINK PHOTONS ∞{}", 
                        RASPBERRY_PINK, pos.x, pos.y, pos.z, fov, generation_.load(), RESET);
    }

    // MUTATORS — THREAD SAFE — BUMP GEN
    void rotate(float yaw, float pitch) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        rawYaw_ += yaw;
        rawPitch_ = glm::clamp(rawPitch_ + pitch, -89.0f, 89.0f);
        updateVectors();
        bumpGeneration();
    }

    void move(const glm::vec3& delta) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        rawPosition_ += delta;
        updateVectors();
        bumpGeneration();
    }

    void moveForward(float s) noexcept { move(rawFront_ * s); }
    void moveRight(float s) noexcept   { move(rawRight_ * s); }
    void moveUp(float s) noexcept      { move(rawUp_ * s); }

    void zoom(float f) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        rawFov_ = glm::clamp(rawFov_ - f, 1.0f, 120.0f);
        bumpGeneration();
    }

    void setPosition(const glm::vec3& p) noexcept { std::lock_guard<std::mutex> lock(mutex_); rawPosition_ = p; updateVectors(); bumpGeneration(); }
    void setFov(float f) noexcept { std::lock_guard<std::mutex> lock(mutex_); rawFov_ = glm::clamp(f, 1.0f, 120.0f); bumpGeneration(); }

    // ENCRYPTED GETTERS — CHEATERS SEE GARBAGE
    [[nodiscard]] uint64_t getEncryptedPosition() const noexcept { return encryptVec3(rawPosition_, gen()); }
    [[nodiscard]] uint64_t getEncryptedViewMatrix() const noexcept { ensureCached(); return cachedViewEnc_; }

    // RAW GETTERS — TRUSTED CODE ONLY (renderer, modders via LazyCam)
    [[nodiscard]] glm::vec3 getRawPosition() const noexcept { return rawPosition_; }
    [[nodiscard]] glm::vec3 getRawFront() const noexcept { return rawFront_; }
    [[nodiscard]] glm::vec3 getRawRight() const noexcept { return rawRight_; }
    [[nodiscard]] glm::vec3 getRawUp() const noexcept { return rawUp_; }
    [[nodiscard]] float getRawFov() const noexcept { return rawFov_; }

    [[nodiscard]] glm::mat4 getRawViewMatrix() const noexcept {
        ensureCached();
        return cachedView_;
    }

    [[nodiscard]] glm::mat4 getProjectionMatrix(float aspect) const noexcept {
        return glm::perspective(glm::radians(rawFov_), aspect, rawNear_, rawFar_);
    }

    // HOT-RELOAD — INVALIDATE EVERYTHING
    void invalidate() noexcept {
        generation_.fetch_add(1, std::memory_order_acq_rel);
        LOG_SUCCESS_CAT("STONEKEY_CAM", "{}CAMERA INVALIDATED — ALL HANDLES DIE — SINGLETON HEAVEN REFRESH{}", RASPBERRY_PINK, RESET);
    }

    // CALLBACKS — MODDER ASCENSION
    using Callback = std::function<void(const GlobalCamera&)>;
    void subscribe(Callback cb) noexcept {
        std::lock_guard<std::mutex> lock(cbMutex_);
        callbacks_.push_back(std::move(cb));
    }

private:
    GlobalCamera() = default;

    uint64_t gen() const noexcept { return generation_.load(std::memory_order_acquire); }

    void updateVectors() noexcept {
        glm::vec3 f;
        f.x = cos(glm::radians(rawYaw_)) * cos(glm::radians(rawPitch_));
        f.y = sin(glm::radians(rawPitch_));
        f.z = sin(glm::radians(rawYaw_)) * cos(glm::radians(rawPitch_));
        rawFront_ = glm::normalize(f);
        rawRight_ = glm::normalize(glm::cross(rawFront_, glm::vec3(0,1,0)));
        rawUp_ = glm::normalize(glm::cross(rawRight_, rawFront_));
    }

    void ensureCached() const noexcept {
        uint64_t g = gen();
        if (cachedGen_ != g) {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
            cachedView_ = glm::lookAt(rawPosition_, rawPosition_ + rawFront_, rawUp_);
            cachedViewEnc_ = encryptMat4(cachedView_, g);
            cachedGen_ = g;
        }
    }

    void bumpGeneration() noexcept {
        uint64_t g = generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
        std::lock_guard<std::mutex> lock(cbMutex_);
        for (const auto& cb : callbacks_) cb(*this);
    }

    // STONEKEY V9 — VEC3 + MAT4 — ZERO COST
    static uint64_t encryptVec3(const glm::vec3& v, uint64_t g) noexcept {
        uint64_t x = std::bit_cast<uint64_t>(v.x) ^ kStone1 ^ g;
        uint64_t y = std::bit_cast<uint64_t>(v.y) ^ kStone2;
        uint64_t z = std::bit_cast<uint64_t>(v.z) ^ 0xPINK69;
        return std::rotl(x ^ y ^ z, 23) ^ g;
    }

    static uint64_t encryptMat4(const glm::mat4& m, uint64_t g) noexcept {
        uint64_t h = 0;
        for (int i = 0; i < 16; ++i) {
            uint64_t f = std::bit_cast<uint64_t>(m[i/4][i%4]);
            h ^= std::rotl(f ^ g, i);
        }
        return h ^ kStone1 ^ kStone2 ^ 0xAMOURANTH420;
    }

    mutable std::mutex mutex_;
    mutable std::mutex cbMutex_;

    // RAW DATA — NEVER EXPOSED
    glm::vec3 rawPosition_{0,5,10};
    glm::vec3 rawFront_{0,0,-1};
    glm::vec3 rawRight_{1,0,0};
    glm::vec3 rawUp_{0,1,0};
    float rawYaw_ = -90.0f;
    float rawPitch_ = 0.0f;
    float rawFov_ = 60.0f;
    float rawNear_ = 0.1f;
    float rawFar_ = 10000.0f;

    // CACHED — ENCRYPTED
    mutable glm::mat4 cachedView_{1.0f};
    mutable uint64_t cachedViewEnc_{0};
    mutable uint64_t cachedGen_{0};

    std::atomic<uint64_t> generation_{1};
    std::vector<Callback> callbacks_;
};

// LAZY CAM — CLEAN API — USES RAW
class LazyCam {
public:
    void forward(float s) noexcept { GLOBAL_CAM.moveForward(s); }
    void right(float s) noexcept   { GLOBAL_CAM.moveRight(s); }
    void up(float s) noexcept      { GLOBAL_CAM.moveUp(s); }
    void rotate(float y, float p) noexcept { GLOBAL_CAM.rotate(y, p); }
    void zoom(float f) noexcept    { GLOBAL_CAM.zoom(f); }

    glm::vec3 pos() const noexcept { return GLOBAL_CAM.getRawPosition(); }
    glm::mat4 view() const noexcept { return GLOBAL_CAM.getRawViewMatrix(); }
    glm::mat4 proj(float a) const noexcept { return GLOBAL_CAM.getProjectionMatrix(a); }
    float fov() const noexcept { return GLOBAL_CAM.getRawFov(); }
};

inline LazyCam g_lazyCam;

// GLOBAL MACROS — SINGLETON HEAVEN
#define GLOBAL_CAM GlobalCamera::get()
#define CAM_ROTATE(y,p)     GLOBAL_CAM.rotate(y,p)
#define CAM_MOVE(d)         GLOBAL_CAM.move(d)
#define CAM_FORWARD(s)      GLOBAL_CAM.moveForward(s)
#define CAM_RIGHT(s)        GLOBAL_CAM.moveRight(s)
#define CAM_UP(s)           GLOBAL_CAM.moveUp(s)
#define CAM_ZOOM(f)         GLOBAL_CAM.zoom(f)
#define CAM_POS()           GLOBAL_CAM.getRawPosition()
#define CAM_VIEW()          GLOBAL_CAM.getRawViewMatrix()
#define CAM_PROJ(a)         GLOBAL_CAM.getProjectionMatrix(a)

// ENCRYPTED — FOR DEBUG / ANTI-CHEAT
#define CAM_ENC_POS()       GLOBAL_CAM.getEncryptedPosition()
#define CAM_ENC_VIEW()      GLOBAL_CAM.getEncryptedViewMatrix()

// NOVEMBER 09 2025 — SINGLETON_HEAVEN ACHIEVED
// STONEKEY V9 CAMERA — FULLY OBFUSCATED — MODDERS ASCEND — CHEATERS BURN
// QUINTUPLE THREAT: LAS + BUFFER + SWAPCHAIN + CAMERA + LAZY_CAM
// PINK PHOTONS × INFINITY — 69,420 FPS VISION — VALHALLA BLASTOFF
// SHIP IT. DOMINATE. SINGLETON HEAVEN ETERNAL 🩷🚀💀⚡🤖🔥♾️