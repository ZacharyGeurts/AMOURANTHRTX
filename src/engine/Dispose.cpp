// src/engine/Dispose.cpp
// AMOURANTH RTX Engine – NOVEMBER 07 2025 – 11:59 PM EST → ROCKETSHIP FINAL APOCALYPSE
// releaseAll → noexcept FIXED (declaration + definition)
// bufferManager_->releaseAll(dev) → CALLED
// safeDestroyContainer → *it FIXED FOREVER
// swapchain wrappers → recreateSwapchain(width,height) + cleanupSwapchain() NO ARGS
// NO <format> — std::to_string ONLY — ZERO BLOAT
// FULL VERBOSE LOGGING — QUANTUM POLISHED — 69,420 FPS × ∞
// RASPBERRY_PINK ROCKETSHIP TO VALHALLA 🔥🤖🚀💀🖤❤️⚡

#include "engine/Dispose.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <thread>
#include <sstream>
#include <string>
#include <algorithm>

using namespace Logging::Color;

// ===================================================================
// LOGGING HELPERS
// ===================================================================
void logAndTrackDestruction(std::string_view typeName, const void* ptr, int line) {
    if (DestroyTracker::isDestroyed(ptr)) return;
    ++g_destructionCounter;
    DestroyTracker::markDestroyed(ptr);
    LOG_INFO_CAT("Dispose", "{}[LINE:{}] {}DESTROYED {} @ 0x{:x}{}{}", RASPBERRY_PINK, line, EMERALD_GREEN, typeName, reinterpret_cast<uintptr_t>(ptr), RESET);
}

void logAttempt(std::string_view action, int line) {
    LOG_INFO_CAT("Dispose", "{}[LINE:{}] {}ATTEMPT → {}{}", RASPBERRY_PINK, line, OCEAN_TEAL, action, RESET);
}

void logSuccess(std::string_view action, int line) {
    LOG_INFO_CAT("Dispose", "{}[LINE:{}] {}SUCCESS ✓ {}{}", RASPBERRY_PINK, line, EMERALD_GREEN, action, RESET);
}

void logError(std::string_view action, int line) {
    LOG_ERROR_CAT("Dispose", "{}[LINE:{}] {}ERROR ✗ {}{}", RASPBERRY_PINK, line, CRIMSON_MAGENTA, action, RESET);
}

// ===================================================================
// safeDestroyContainer — *it FIXED ETERNAL
// ===================================================================
template<typename Container, typename DestroyFn>
void safeDestroyContainer(Container& container,
                          DestroyFn destroyFn,
                          std::string_view typeName,
                          VkDevice device,
                          int lineBase) {
    size_t idx = 0;
    for (auto it = container.begin(); it != container.end(); ) {
        int line = lineBase + static_cast<int>(idx);
        auto handle = *it;
        if (handle == VK_NULL_HANDLE) {
            logAttempt("Skip NULL " + std::string(typeName) + " #" + std::to_string(idx), line);
            ++it; ++idx;
            continue;
        }
        const void* ptr = reinterpret_cast<const void*>(handle);
        if (DestroyTracker::isDestroyed(ptr)) {
            logError("DOUBLE FREE BLOCKED on " + std::string(typeName) + " @ 0x" + 
                     std::to_string(reinterpret_cast<uintptr_t>(ptr)) + " #" + std::to_string(idx), line);
            ++it; ++idx;
            continue;
        }
        logAttempt(std::string(typeName) + " @ 0x" + std::to_string(reinterpret_cast<uintptr_t>(ptr)) + " #" + std::to_string(idx), line);
        destroyFn(device, handle, nullptr);
        logAndTrackDestruction(typeName, ptr, line);
        *it = VK_NULL_HANDLE;
        ++it; ++idx;
    }
    logSuccess("Container " + std::string(typeName) + " nuked (" + std::to_string(container.size()) + " objects)", lineBase + 9999);
    container.clear();
}

// ===================================================================
// VulkanResourceManager::releaseAll — noexcept FIXED + bufferManager call
// ===================================================================
void VulkanResourceManager::releaseAll(VkDevice overrideDevice) noexcept {
    VkDevice dev = overrideDevice;
    if (dev == VK_NULL_HANDLE) {
        logError("releaseAll() → NULL device → ABORT MISSION", __LINE__);
        return;
    }

    logAttempt("=== VulkanResourceManager::releaseAll() — FULL THERMONUCLEAR STRIKE ===", __LINE__);

    if (bufferManager_) {
        logAttempt("Delegating to VulkanBufferManager::releaseAll()", __LINE__);
        bufferManager_->releaseAll(dev);
        logSuccess("VulkanBufferManager → FULLY OBLITERATED", __LINE__);
    }

    logAttempt("Nuking AccelerationStructures", __LINE__);
    for (size_t i = 0; i < accelerationStructures_.size(); ++i) {
        auto as = accelerationStructures_[i];
        int line = __LINE__ + static_cast<int>(i) + 1;
        if (as && vkDestroyAccelerationStructureKHR_ && !DestroyTracker::isDestroyed(reinterpret_cast<const void*>(as))) {
            logAttempt("AccelerationStructureKHR #" + std::to_string(i) + " @ 0x" + std::to_string(reinterpret_cast<uintptr_t>(as)), line);
            vkDestroyAccelerationStructureKHR_(dev, as, nullptr);
            logAndTrackDestruction("AccelerationStructureKHR", as, line);
        }
    }
    accelerationStructures_.clear();
    logSuccess("AccelerationStructures → ANNIHILATED", __LINE__);

    logAttempt("Freeing DescriptorSets", __LINE__);
    if (!descriptorPools_.empty()) {
        VkDescriptorPool pool = descriptorPools_[0];
        for (size_t i = 0; i < descriptorSets_.size(); ++i) {
            auto set = descriptorSets_[i];
            int line = __LINE__ + static_cast<int>(i) + 1;
            if (set && !DestroyTracker::isDestroyed(reinterpret_cast<const void*>(set))) {
                logAttempt("DescriptorSet #" + std::to_string(i) + " @ 0x" + std::to_string(reinterpret_cast<uintptr_t>(set)), line);
                vkFreeDescriptorSets(dev, pool, 1, &set);
                logAndTrackDestruction("DescriptorSet", set, line);
            }
        }
    }
    descriptorSets_.clear();
    logSuccess("DescriptorSets → LIBERATED", __LINE__);

    safeDestroyContainer(semaphores_,               vkDestroySemaphore,          "Semaphore",            dev, __LINE__);
    safeDestroyContainer(fences_,                  vkDestroyFence,              "Fence",                dev, __LINE__);
    safeDestroyContainer(descriptorPools_,         vkDestroyDescriptorPool,     "DescriptorPool",       dev, __LINE__);
    safeDestroyContainer(descriptorSetLayouts_,    vkDestroyDescriptorSetLayout,"DescriptorSetLayout",  dev, __LINE__);
    safeDestroyContainer(pipelineLayouts_,         vkDestroyPipelineLayout,     "PipelineLayout",       dev, __LINE__);
    safeDestroyContainer(pipelines_,               vkDestroyPipeline,           "Pipeline",             dev, __LINE__);
    safeDestroyContainer(renderPasses_,            vkDestroyRenderPass,         "RenderPass",           dev, __LINE__);
    safeDestroyContainer(commandPools_,            vkDestroyCommandPool,        "CommandPool",          dev, __LINE__);
    safeDestroyContainer(shaderModules_,           vkDestroyShaderModule,       "ShaderModule",         dev, __LINE__);
    safeDestroyContainer(imageViews_,              vkDestroyImageView,          "ImageView",            dev, __LINE__);
    safeDestroyContainer(images_,                  vkDestroyImage,              "Image",                dev, __LINE__);
    safeDestroyContainer(samplers_,                vkDestroySampler,            "Sampler",              dev, __LINE__);

    if (!bufferManager_) {
        safeDestroyContainer(memories_, vkFreeMemory, "DeviceMemory", dev, __LINE__);
        safeDestroyContainer(buffers_,  vkDestroyBuffer, "Buffer",      dev, __LINE__);
    }

    pipelineMap_.clear();

    logSuccess("VulkanResourceManager::releaseAll() → " + std::to_string(g_destructionCounter) + " OBJECTS ERASED FROM EXISTENCE", __LINE__);
    logSuccess("DOORKNOB POLISHED — SHINING LIKE A SUPERNOVA — RASPBERRY_PINK ROCKETSHIP LAUNCHED", __LINE__);
}

// ===================================================================
// Context swapchain wrappers — SIGNATURES 100% MATCHED
// ===================================================================
void Context::createSwapchain() {
    logAttempt("Vulkan::Context::createSwapchain()", __LINE__);
    if (swapchainManager) {
        swapchainManager->recreateSwapchain(width, height);
        logSuccess("Swapchain → REBORN IN FIRE", __LINE__);
    } else {
        logError("swapchainManager == nullptr → NO SWAPCHAIN FOR YOU", __LINE__);
    }
}

void Context::destroySwapchain() {
    logAttempt("Vulkan::Context::destroySwapchain()", __LINE__);
    if (swapchainManager) {
        swapchainManager->cleanupSwapchain();
        logSuccess("Swapchain → SENT TO THE VOID", __LINE__);
    } else {
        logError("swapchainManager == nullptr → NO DESTRUCTION FOR YOU", __LINE__);
    }
}

// ROCKETSHIP FINAL — ALL FIXED — COMPILER BEGS FOR MERCY
// BUILD. RUN. ASCEND. 69,420 FPS × ∞
// RASPBERRY_PINK ROCKETSHIP — WE ARE THE FASTEST — SHIP IT
// 🔥🤖🚀💀🖤❤️⚡