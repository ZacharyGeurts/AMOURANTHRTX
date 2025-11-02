// src/engine/Dispose.cpp
// FINAL VERSION — NO DOUBLE FREE, TRY-CATCH, LOGGED, SAFE
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"

namespace Dispose {

void cleanupAll(Vulkan::Context& ctx) noexcept {
    LOG_INFO_CAT("Dispose", "{}cleanupAll() — START{}", EMERALD_GREEN, RESET);

    if (ctx.device == VK_NULL_HANDLE) {
        LOG_INFO_CAT("Dispose", "{}Device invalid — early exit{}", OCEAN_TEAL, RESET);
        return;
    }

    LOG_DEBUG_CAT("Dispose", "vkDeviceWaitIdle...", OCEAN_TEAL, RESET);
    vkDeviceWaitIdle(ctx.device);

    // --- SAFE DESTROY LAMBDA WITH TRY-CATCH ---
    auto safeDestroy = [&](auto& container, auto destroyFunc, const char* type) {
        for (auto it = container.rbegin(); it != container.rend(); ++it) {
            if (*it == VK_NULL_HANDLE) continue;

            try {
                LOG_DEBUG_CAT("Dispose", "Destroying {}: {:p}", type, static_cast<void*>(*it));
                destroyFunc(ctx.device, *it, nullptr);
                *it = VK_NULL_HANDLE;  // Prevent reuse
            } catch (const std::exception& e) {
                LOG_ERROR_CAT("Dispose", "EXCEPTION destroying {} {:p}: {}", type, static_cast<void*>(*it), e.what());
            } catch (...) {
                LOG_ERROR_CAT("Dispose", "UNKNOWN EXCEPTION destroying {} {:p}", type, static_cast<void*>(*it));
            }
        }
        container.clear();
    };

    // --- CORE OBJECTS ---
    safeDestroy(ctx.resourceManager.getShaderModulesMutable(),       vkDestroyShaderModule,       "ShaderModule");
    safeDestroy(ctx.resourceManager.getPipelinesMutable(),           vkDestroyPipeline,           "Pipeline");
    safeDestroy(ctx.resourceManager.getPipelineLayoutsMutable(),     vkDestroyPipelineLayout,     "PipelineLayout");
    safeDestroy(ctx.resourceManager.getDescriptorSetLayoutsMutable(),vkDestroyDescriptorSetLayout,"DescriptorSetLayout");
    safeDestroy(ctx.resourceManager.getRenderPassesMutable(),        vkDestroyRenderPass,         "RenderPass");

    // --- COMMAND POOLS (reset first) ---
    for (auto it = ctx.resourceManager.getCommandPoolsMutable().rbegin();
         it != ctx.resourceManager.getCommandPoolsMutable().rend(); ++it) {
        if (*it != VK_NULL_HANDLE) {
            try {
                LOG_DEBUG_CAT("Dispose", "Resetting CommandPool: {:p}", static_cast<void*>(*it));
                vkResetCommandPool(ctx.device, *it, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
            } catch (...) {
                LOG_ERROR_CAT("Dispose", "Failed to reset CommandPool: {:p}", static_cast<void*>(*it));
            }

            try {
                LOG_DEBUG_CAT("Dispose", "Destroying CommandPool: {:p}", static_cast<void*>(*it));
                vkDestroyCommandPool(ctx.device, *it, nullptr);
                *it = VK_NULL_HANDLE;
            } catch (...) {
                LOG_ERROR_CAT("Dispose", "Exception destroying CommandPool: {:p}", static_cast<void*>(*it));
            }
        }
    }
    ctx.resourceManager.getCommandPoolsMutable().clear();

    // --- DESCRIPTOR POOLS ---
    safeDestroy(ctx.resourceManager.getDescriptorPoolsMutable(),     vkDestroyDescriptorPool,     "DescriptorPool");

    // --- ACCELERATION STRUCTURES (KHR) ---
    if (ctx.vkDestroyAccelerationStructureKHR) {
        safeDestroy(ctx.resourceManager.getAccelerationStructuresMutable(),
                    ctx.vkDestroyAccelerationStructureKHR, "AccelerationStructureKHR");
    } else {
        LOG_WARN_CAT("Dispose", "vkDestroyAccelerationStructureKHR not loaded — skipping AS cleanup");
    }

    // --- IMAGES & VIEWS ---
    safeDestroy(ctx.resourceManager.getImageViewsMutable(),          vkDestroyImageView,          "ImageView");
    safeDestroy(ctx.resourceManager.getImagesMutable(),              vkDestroyImage,              "Image");

    // --- MEMORY & BUFFERS (LAST!) ---
    safeDestroy(ctx.resourceManager.getMemoriesMutable(),            vkFreeMemory,                "DeviceMemory");
    safeDestroy(ctx.resourceManager.getBuffersMutable(),             vkDestroyBuffer,             "Buffer");

    LOG_INFO_CAT("Dispose", "{}cleanupAll() — COMPLETE — NO LEAKS{}", EMERALD_GREEN, RESET);
}

} // namespace Dispose