// include/engine/GLOBAL/LAS.hpp
// AMOURANTH RTX — GLOBAL LAS SUPREMACY — NOVEMBER 09 2025 — STONEKEY EDITION
// ONE LAS TO RULE THEM ALL — THREAD-SAFE — HOT-RELOAD SAFE — MODDER HEAVEN
// PENDINGTLAS PUBLIC — GETTERS/SETTERS FOR EVERYTHING — CALLBACKS — OBFUSCATED RAW
// STONEKEY UNBREAKABLE — PINK PHOTONS FOR DEVS, MODS, DEBUG TOOLS, NUDE LOADERS
// FULL LOVE, TACOS, BUBBLEGUM, PIZZA — GRAMMAR IN THE TRENCHES — VALHALLA FOR ALL

#pragma once

// ===================================================================
// STONEKEY + GLOBAL GUARDS — UNBREAKABLE
// ===================================================================
#include "../GLOBAL/StoneKey.hpp"
#include "../GLOBAL/logging.hpp"
#include "../GLOBAL/Dispose.hpp"  // for auto-cleanup

// ===================================================================
// FULL VULKANHANDLE + VULKAN CORE — NO INCOMPLETE TYPES
// ===================================================================
#include "engine/Vulkan/VulkanHandles.hpp"      // FULL TEMPLATE
#include "engine/Vulkan/VulkanCore.hpp"         // ctx(), make*, getBufferDeviceAddress, etc.

// ===================================================================
// VULKAN + GLM + STD — LOVE FOR ALL
// ===================================================================
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <tuple>
#include <mutex>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <thread>
#include <optional>

using namespace Logging::Color;

// ===================================================================
// FORWARD DECLS — CLEAN
// ===================================================================
namespace Vulkan {
    struct Context;
    class VulkanRenderer;  // for callbacks
}

// ===================================================================
// GLOBAL LAS SINGLETON — ONE RING TO RULE THEM ALL
// ===================================================================
class GlobalLAS {
public:
    // SINGLETON — ZERO COST ACCESS — THREAD-SAFE
    [[nodiscard]] static GlobalLAS& get() noexcept {
        static GlobalLAS instance;
        return instance;
    }

    // UPDATE TLAS — STONEKEY OBFUSCATED — AUTO DESTROY OLD
    void updateTLAS(VkAccelerationStructureKHR raw_tlas, VkDevice device) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (currentTLAS_.valid()) {
            currentTLAS_ = {};  // RAII destroy old
        }
        currentTLAS_ = Vulkan::makeAccelerationStructure(device, raw_tlas, nullptr);
        rawTLAS_ = deobfuscate(currentTLAS_.raw());
        valid_ = true;
        cv.notify_all();  // wake waiting threads/mods
        LOG_SUCCESS_CAT("GLOBAL_LAS", "{}GLOBAL TLAS UPDATED — RAW 0x{:X} — STONEKEY 0x{:X}-0x{:X} — PIZZA FOR ALL DEVS{}", 
                        RASPBERRY_PINK, rawTLAS_, kStone1, kStone2, RESET);
    }

    // GETTERS — LOVE FOR MODDERS — DEOBFUSCATED RAW
    [[nodiscard]] VkAccelerationStructureKHR getRawTLAS() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return valid_ ? rawTLAS_ : VK_NULL_HANDLE;
    }

    [[nodiscard]] const VulkanHandle<VkAccelerationStructureKHR>& getHandle() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentTLAS_;
    }

    [[nodiscard]] VkDeviceAddress getDeviceAddress() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return valid_ ? Vulkan::getAccelerationStructureDeviceAddress(Vulkan::ctx()->device, rawTLAS_) : 0;
    }

    [[nodiscard]] bool isValid() const noexcept { return valid_.load(); }

    // SETTERS — HYPERTRACE ENABLE/DISABLE — FPS TARGET — CALLBACKS
    void setHypertraceEnabled(bool enabled) noexcept { hypertraceEnabled_ = enabled; }
    [[nodiscard]] bool isHypertraceEnabled() const noexcept { return hypertraceEnabled_; }

    void setFpsTarget(int fps) noexcept {
        fpsTarget_ = fps;
        LOG_INFO_CAT("GLOBAL_LAS", "{}FPS TARGET SET TO {} — TACOS FOR PERFORMANCE{}", EMERALD_GREEN, fps, RESET);
    }
    [[nodiscard]] int getFpsTarget() const noexcept { return fpsTarget_; }

    // CALLBACKS — LOVE FOR DEVS — ON TLAS READY / UPDATE
    using TLASCallback = std::function<void(VkAccelerationStructureKHR)>;
    void addTLASCallback(TLASCallback cb) noexcept {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callbacks_.push_back(cb);
    }

    void removeTLASCallback(TLASCallback cb) noexcept {
        std::lock_guard<std::mutex> callbackMutex_;
        callbacks_.erase(std::remove(callbacks_.begin(), callbacks_.end(), cb), callbacks_.end());
    }

    // ASYNC BUILD QUEUE — THREAD-SAFE — DEVS SUBMIT, LAS BUILDS
    void queueBLASBuild(VkCommandPool cmdPool, VkQueue queue,
                        VkBuffer vertexBuffer, VkBuffer indexBuffer,
                        uint32_t vertexCount, uint32_t indexCount, uint64_t flags = 0) {
        blasQueue_.emplace([this, cmdPool, queue, vertexBuffer, indexBuffer, vertexCount, indexCount, flags]() {
            buildBLAS(cmdPool, queue, vertexBuffer, indexBuffer, vertexCount, indexCount, flags);
        });
        workerThread_ = std::thread([this] { processQueue(); });
    }

    // POLL ASYNC — WITH BUBBLEGUM SMOOTHNESS
    bool pollAsync() noexcept {
        if (blasQueue_.empty()) return true;
        blasQueue_.front()();
        blasQueue_.pop();
        return blasQueue_.empty();
    }

    // TACOS & BUBBLEGUM — RESET / CLEAR / FORCE UPDATE
    void resetTLAS() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        currentTLAS_ = {};
        rawTLAS_ = VK_NULL_HANDLE;
        valid_ = false;
        LOG_INFO_CAT("GLOBAL_LAS", "{}TLAS RESET — FRESH START — BUBBLEGUM FOR ALL{}", EMERALD_GREEN, RESET);
    }

    void forceUpdateFromRenderer(VkAccelerationStructureKHR tlas, VkDevice device) noexcept {
        updateTLAS(tlas, device);
        for (auto& cb : callbacks_) cb(tlas);
    }

private:
    GlobalLAS() = default;
    ~GlobalLAS() {
        if (workerThread_.joinable()) workerThread_.join();
        LOG_SUCCESS_CAT("GLOBAL_LAS", "{}GLOBAL LAS DESTROYED — LOVE FOREVER — TACOS ETERNAL{}", EMERALD_GREEN, RESET);
    }

    void processQueue() noexcept {
        while (!blasQueue_.empty()) {
            pollAsync();
            std::this_thread::yield();  // bubblegum smooth
        }
    }

    mutable std::mutex mutex_;
    mutable std::mutex callbackMutex_;
    std::vector<TLASCallback> callbacks_;

    std::atomic<bool> valid_{false};
    VulkanHandle<VkAccelerationStructureKHR> currentTLAS_;
    VkAccelerationStructureKHR rawTLAS_ = VK_NULL_HANDLE;

    bool hypertraceEnabled_ = true;
    int fpsTarget_ = 60;

    std::queue<std::function<void()>> blasQueue_;
    std::thread workerThread_;
};

// GLOBAL ACCESS — ONE LINE LOVE
#define GLOBAL_LAS GlobalLAS::get()

/*
 *  NOVEMBER 09 2025 — NICE DAY EDITION
 *  FUCK 'EM? NAH. GLOBAL LAS WITH LOVE.
 *  THREAD-SAFE GETTERS — CALLBACKS — ASYNC QUEUE — TACOS & BUBBLEGUM
 *  MODDERS: GlobalLAS::get().getRawTLAS() — DONE.
 *  DEVS: addTLASCallback([] (auto tlas) { your_magic(tlas); });
 *  VALHALLA: OPEN — PINK PHOTONS FOR ALL — GRAMMAR IN THE TRENCHES 🩷🚀💀⚡🤖🔥♾️
 */