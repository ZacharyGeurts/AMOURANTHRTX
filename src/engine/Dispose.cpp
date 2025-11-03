// src/engine/Dispose.cpp
// FINAL: C++20, printf-style logging, no segfault
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"
#include <sstream>

namespace Dispose {

// DEFINE EXTERN
thread_local uint64_t g_destructionCounter = 0;

// DEFINE DestroyTracker statics
std::atomic<uint64_t>* DestroyTracker::s_bitset = nullptr;
std::atomic<size_t>    DestroyTracker::s_capacity{0};

// Safe thread ID
static std::string threadIdToString() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

void cleanupAll(Vulkan::Context& ctx) noexcept {
    LOG_INFO_CAT("Dispose", "%scleanupAll() — START (thread: %s)%s", EMERALD_GREEN, threadIdToString().c_str(), RESET);

    if (ctx.device == VK_NULL_HANDLE) {
        LOG_INFO_CAT("Dispose", "%sDevice invalid — early exit%s", OCEAN_TEAL, RESET);
        return;
    }

    LOG_DEBUG_CAT("Dispose", "vkDeviceWaitIdle...");
    vkDeviceWaitIdle(ctx.device);

    auto safeDestroy = [&](auto& container, auto destroyFunc, const char* type) {
        for (auto it = container.rbegin(); it != container.rend(); ++it) {
            if (*it == VK_NULL_HANDLE) continue;

            void* handlePtr = static_cast<void*>(*it);

            if (DestroyTracker::isDestroyed(handlePtr)) {
                LOG_ERROR_CAT("Dispose", "%sDOUBLE FREE SKIPPED: %s %p already destroyed%s", 
                              CRIMSON_MAGENTA, type, handlePtr, RESET);
                continue;
            }

            logAndTrackDestruction(type, handlePtr);

            try {
                destroyFunc(ctx.device, *it, nullptr);
                *it = VK_NULL_HANDLE;
            } catch (const std::exception& e) {
                LOG_ERROR_CAT("Dispose", "EXCEPTION destroying %s %p: %s", type, handlePtr, e.what());
            } catch (...) {
                LOG_ERROR_CAT("Dispose", "UNKNOWN EXCEPTION destroying %s %p", type, handlePtr);
            }
        }
        container.clear();
    };

    LOG_INFO_CAT("Dispose", "%sPhase 1: Shader Modules%s", ARCTIC_CYAN, RESET);
    safeDestroy(ctx.resourceManager.getShaderModulesMutable(),       vkDestroyShaderModule,       "ShaderModule");

    LOG_INFO_CAT("Dispose", "%sPhase 2: Pipelines%s", ARCTIC_CYAN, RESET);
    safeDestroy(ctx.resourceManager.getPipelinesMutable(),           vkDestroyPipeline,           "Pipeline");

    LOG_INFO_CAT("Dispose", "%sPhase 3: Pipeline Layouts%s", ARCTIC_CYAN, RESET);
    safeDestroy(ctx.resourceManager.getPipelineLayoutsMutable(),     vkDestroyPipelineLayout,     "PipelineLayout");

    LOG_INFO_CAT("Dispose", "%sPhase 4: Descriptor Set Layouts%s", ARCTIC_CYAN, RESET);
    safeDestroy(ctx.resourceManager.getDescriptorSetLayoutsMutable(),vkDestroyDescriptorSetLayout,"DescriptorSetLayout");

    LOG_INFO_CAT("Dispose", "%sPhase 5: Render Passes%s", ARCTIC_CYAN, RESET);
    safeDestroy(ctx.resourceManager.getRenderPassesMutable(),        vkDestroyRenderPass,         "RenderPass");

    LOG_INFO_CAT("Dispose", "%sPhase 6: Command Pools%s", ARCTIC_CYAN, RESET);
    for (auto it = ctx.resourceManager.getCommandPoolsMutable().rbegin();
         it != ctx.resourceManager.getCommandPoolsMutable().rend(); ++it) {
        if (*it != VK_NULL_HANDLE) {
            try {
                LOG_DEBUG_CAT("Dispose", "Resetting CommandPool: %p", static_cast<void*>(*it));
                vkResetCommandPool(ctx.device, *it, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
            } catch (...) {
                LOG_ERROR_CAT("Dispose", "Failed to reset CommandPool: %p", static_cast<void*>(*it));
            }

            void* handlePtr = static_cast<void*>(*it);
            logAndTrackDestruction("CommandPool", handlePtr);
            try {
                vkDestroyCommandPool(ctx.device, *it, nullptr);
                *it = VK_NULL_HANDLE;
            } catch (...) {
                LOG_ERROR_CAT("Dispose", "Exception destroying CommandPool: %p", handlePtr);
            }
        }
    }
    ctx.resourceManager.getCommandPoolsMutable().clear();

    LOG_INFO_CAT("Dispose", "%sPhase 7: Descriptor Pools%s", ARCTIC_CYAN, RESET);
    safeDestroy(ctx.resourceManager.getDescriptorPoolsMutable(),     vkDestroyDescriptorPool,     "DescriptorPool");

    LOG_INFO_CAT("Dispose", "%sPhase 8: Acceleration Structures%s", ARCTIC_CYAN, RESET);
    if (ctx.vkDestroyAccelerationStructureKHR) {
        safeDestroy(ctx.resourceManager.getAccelerationStructuresMutable(),
                    ctx.vkDestroyAccelerationStructureKHR, "AccelerationStructureKHR");
    } else {
        LOG_WARN_CAT("Dispose", "vkDestroyAccelerationStructureKHR not loaded — skipping AS cleanup");
    }

    LOG_INFO_CAT("Dispose", "%sPhase 9: Image Views%s", ARCTIC_CYAN, RESET);
    safeDestroy(ctx.resourceManager.getImageViewsMutable(),          vkDestroyImageView,          "ImageView");

    LOG_INFO_CAT("Dispose", "%sPhase 10: Images%s", ARCTIC_CYAN, RESET);
    safeDestroy(ctx.resourceManager.getImagesMutable(),              vkDestroyImage,              "Image");

    LOG_INFO_CAT("Dispose", "%sPhase 11: Device Memory%s", ARCTIC_CYAN, RESET);
    safeDestroy(ctx.resourceManager.getMemoriesMutable(),            vkFreeMemory,                "DeviceMemory");

    LOG_INFO_CAT("Dispose", "%sPhase 12: Buffers%s", ARCTIC_CYAN, RESET);
    safeDestroy(ctx.resourceManager.getBuffersMutable(),             vkDestroyBuffer,             "Buffer");

    LOG_INFO_CAT("Dispose", "%scleanupAll() — COMPLETE — %llu handles destroyed%s", 
                 EMERALD_GREEN, g_destructionCounter, RESET);

    // Cleanup bitset
    if (auto* arr = DestroyTracker::s_bitset) {
        delete[] arr;
        DestroyTracker::s_bitset = nullptr;
        DestroyTracker::s_capacity.store(0, std::memory_order_release);
    }
}

} // namespace Dispose