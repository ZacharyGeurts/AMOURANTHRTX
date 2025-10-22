// AMOURANTH RTX Engine, October 2025 - Centralized resource disposal for SDL3 and Vulkan.
// One-liner functions where possible, assuming nullptr allocator for Vulkan.
// Usage: Call Dispose::destroy* functions in destructors or cleanup methods.
// Thread-safe using Vulkan::cleanupMutex. All functions log errors and ensure passthrough to prevent freezes.
// Zachary Geurts 2025

#ifndef DISPOSE_HPP
#define DISPOSE_HPP

#include "engine/Vulkan/VulkanCore.hpp"
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <vector>
#include <mutex>
#include <format>
#include <typeinfo>
#include <chrono>
#include "engine/logging.hpp"

namespace Dispose {

/**
 * @brief Destroys a vector of Vulkan handles of the specified type.
 * @tparam HandleType The type of Vulkan handle (e.g., VkSemaphore, VkFramebuffer).
 * @tparam DestroyFunc The Vulkan function to destroy the handle (e.g., vkDestroySemaphore).
 * @param device The Vulkan device owning the handles.
 * @param handles The vector of handles to destroy. Cleared after destruction.
 * @note Thread-safe using Vulkan::cleanupMutex. Handles are nullified, and errors are logged without throwing.
 */
template <typename HandleType, void (*DestroyFunc)(VkDevice, HandleType, const VkAllocationCallbacks*)>
inline void destroyHandles(VkDevice device, std::vector<HandleType>& handles) noexcept {
    std::lock_guard<std::mutex> lock(Vulkan::cleanupMutex);
    if (device != VK_NULL_HANDLE) {
        size_t index = 0;
        for (auto& handle : handles) {
            if (handle != VK_NULL_HANDLE) {
                try {
                    DestroyFunc(device, handle, nullptr);
                    LOG_DEBUG(std::format("Destroyed handle[{}] (type: {}): {:p}", index, typeid(HandleType).name(), static_cast<void*>(handle)));
                    handle = VK_NULL_HANDLE;
                } catch (const std::exception& e) {
                    LOG_ERROR(std::format("Failed to destroy handle[{}] (type: {}): {}", index, typeid(HandleType).name(), e.what()));
                }
            } else {
                LOG_WARNING(std::format("Skipping null handle at index {} (type: {})", index, typeid(HandleType).name()));
            }
            ++index;
        }
        handles.clear();
        LOG_INFO(std::format("Cleared Vulkan handles (type: {})", typeid(HandleType).name()));
    }
}

/**
 * @brief Destroys a single Vulkan handle of the specified type.
 * @tparam HandleType The type of Vulkan handle (e.g., VkImageView, VkPipeline).
 * @tparam DestroyFunc The Vulkan function to destroy the handle (e.g., vkDestroyImageView).
 * @param device The Vulkan device owning the handle.
 * @param handle The handle to destroy. Set to VK_NULL_HANDLE after destruction.
 * @note Thread-safe using Vulkan::cleanupMutex. Errors are logged without throwing.
 */
template <typename HandleType, void (*DestroyFunc)(VkDevice, HandleType, const VkAllocationCallbacks*)>
inline void destroySingle(VkDevice device, HandleType& handle) noexcept {
    std::lock_guard<std::mutex> lock(Vulkan::cleanupMutex);
    if (device != VK_NULL_HANDLE && handle != VK_NULL_HANDLE) {
        try {
            DestroyFunc(device, handle, nullptr);
            LOG_INFO(std::format("Destroyed single Vulkan handle (type: {}): {:p}", typeid(HandleType).name(), static_cast<void*>(handle)));
            handle = VK_NULL_HANDLE;
        } catch (const std::exception& e) {
            LOG_ERROR(std::format("Failed to destroy single handle (type: {}): {}", typeid(HandleType).name(), e.what()));
            handle = VK_NULL_HANDLE;
        }
    }
}

/**
 * @brief Destroys an SDL window.
 * @param window The SDL window to destroy.
 * @note Thread-safe using Vulkan::cleanupMutex. Checks SDL_GetError for errors and logs.
 */
inline void destroyWindow(SDL_Window* window) noexcept {
    std::lock_guard<std::mutex> lock(Vulkan::cleanupMutex);
    if (window) {
        try {
            SDL_DestroyWindow(window);
            if (const char* error = SDL_GetError(); error && *error) {
                LOG_WARNING(std::format("SDL window destruction reported error: {}", error));
            } else {
                LOG_INFO("Destroyed SDL window");
            }
        } catch (const std::exception& e) {
            LOG_ERROR(std::format("Unexpected error destroying SDL window: {}", e.what()));
        }
    }
}

/**
 * @brief Quits the SDL subsystem.
 * @note Thread-safe using Vulkan::cleanupMutex. Checks SDL_GetError for errors and logs.
 */
inline void quitSDL() noexcept {
    std::lock_guard<std::mutex> lock(Vulkan::cleanupMutex);
    try {
        SDL_Quit();
        if (const char* error = SDL_GetError(); error && *error) {
            LOG_WARNING(std::format("SDL quit reported error: {}", error));
        } else {
            LOG_INFO("SDL quit");
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::format("Unexpected error quitting SDL: {}", e.what()));
    }
}

/**
 * @brief Destroys a vector of Vulkan framebuffers.
 * @param device The Vulkan device owning the framebuffers.
 * @param framebuffers The vector of framebuffers to destroy. Cleared after destruction.
 */
inline void destroyFramebuffers(VkDevice device, std::vector<VkFramebuffer>& framebuffers) noexcept {
    destroyHandles<VkFramebuffer, vkDestroyFramebuffer>(device, framebuffers);
}

/**
 * @brief Destroys a vector of Vulkan semaphores.
 * @param device The Vulkan device owning the semaphores.
 * @param semaphores The vector of semaphores to destroy. Cleared after destruction.
 */
inline void destroySemaphores(VkDevice device, std::vector<VkSemaphore>& semaphores) noexcept {
    destroyHandles<VkSemaphore, vkDestroySemaphore>(device, semaphores);
}

/**
 * @brief Destroys a vector of Vulkan fences.
 * @param device The Vulkan device owning the fences.
 * @param fences The vector of fences to destroy. Cleared after destruction.
 */
inline void destroyFences(VkDevice device, std::vector<VkFence>& fences) noexcept {
    destroyHandles<VkFence, vkDestroyFence>(device, fences);
}

/**
 * @brief Frees a vector of Vulkan command buffers.
 * @param device The Vulkan device owning the command pool.
 * @param commandPool The command pool owning the command buffers.
 * @param commandBuffers The vector of command buffers to free. Cleared after freeing.
 */
inline void freeCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers) noexcept {
    std::lock_guard<std::mutex> lock(Vulkan::cleanupMutex);
    if (!commandBuffers.empty() && commandPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        try {
            vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
            commandBuffers.clear();
            LOG_INFO("Freed command buffers");
        } catch (const std::exception& e) {
            LOG_ERROR(std::format("Failed to free command buffers: {}", e.what()));
            commandBuffers.clear();
        }
    }
}

/**
 * @brief Destroys a vector of Vulkan image views.
 * @param device The Vulkan device owning the image views.
 * @param imageViews The vector of image views to destroy. Cleared after destruction.
 */
inline void destroyImageViews(VkDevice device, std::vector<VkImageView>& imageViews) noexcept {
    destroyHandles<VkImageView, vkDestroyImageView>(device, imageViews);
}

/**
 * @brief Destroys a single Vulkan image view.
 * @param device The Vulkan device owning the image view.
 * @param imageView The image view to destroy. Set to VK_NULL_HANDLE after destruction.
 */
inline void destroySingleImageView(VkDevice device, VkImageView& imageView) noexcept {
    destroySingle<VkImageView, vkDestroyImageView>(device, imageView);
}

/**
 * @brief Destroys a single Vulkan image.
 * @param device The Vulkan device owning the image.
 * @param image The image to destroy. Set to VK_NULL_HANDLE after destruction.
 */
inline void destroySingleImage(VkDevice device, VkImage& image) noexcept {
    destroySingle<VkImage, vkDestroyImage>(device, image);
}

/**
 * @brief Frees a vector of Vulkan device memories.
 * @param device The Vulkan device owning the memories.
 * @param memories The vector of device memories to free. Cleared after freeing.
 */
inline void freeDeviceMemories(VkDevice device, std::vector<VkDeviceMemory>& memories) noexcept {
    destroyHandles<VkDeviceMemory, vkFreeMemory>(device, memories);
}

/**
 * @brief Frees a single Vulkan device memory.
 * @param device The Vulkan device owning the memory.
 * @param memory The device memory to free. Set to VK_NULL_HANDLE after freeing.
 */
inline void freeSingleDeviceMemory(VkDevice device, VkDeviceMemory& memory) noexcept {
    destroySingle<VkDeviceMemory, vkFreeMemory>(device, memory);
}

/**
 * @brief Destroys a vector of Vulkan buffers.
 * @param device The Vulkan device owning the buffers.
 * @param buffers The vector of buffers to destroy. Cleared after destruction.
 */
inline void destroyBuffers(VkDevice device, std::vector<VkBuffer>& buffers) noexcept {
    destroyHandles<VkBuffer, vkDestroyBuffer>(device, buffers);
}

/**
 * @brief Destroys a single Vulkan buffer.
 * @param device The Vulkan device owning the buffer.
 * @param buffer The buffer to destroy. Set to VK_NULL_HANDLE after destruction.
 */
inline void destroySingleBuffer(VkDevice device, VkBuffer& buffer) noexcept {
    destroySingle<VkBuffer, vkDestroyBuffer>(device, buffer);
}

/**
 * @brief Destroys a single Vulkan sampler.
 * @param device The Vulkan device owning the sampler.
 * @param sampler The sampler to destroy. Set to VK_NULL_HANDLE after destruction.
 */
inline void destroySingleSampler(VkDevice device, VkSampler& sampler) noexcept {
    destroySingle<VkSampler, vkDestroySampler>(device, sampler);
}

/**
 * @brief Frees a single Vulkan descriptor set.
 * @param device The Vulkan device owning the descriptor pool.
 * @param descriptorPool The descriptor pool owning the descriptor set.
 * @param descriptorSet The descriptor set to free. Set to VK_NULL_HANDLE after freeing.
 */
inline void freeSingleDescriptorSet(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSet& descriptorSet) noexcept {
    std::lock_guard<std::mutex> lock(Vulkan::cleanupMutex);
    if (descriptorSet != VK_NULL_HANDLE && descriptorPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        try {
            vkFreeDescriptorSets(device, descriptorPool, 1, &descriptorSet);
            descriptorSet = VK_NULL_HANDLE;
            LOG_INFO("Freed single descriptor set");
        } catch (const std::exception& e) {
            LOG_ERROR(std::format("Failed to free single descriptor set: {}", e.what()));
            descriptorSet = VK_NULL_HANDLE;
        }
    }
}

/**
 * @brief Destroys a single Vulkan descriptor pool.
 * @param device The Vulkan device owning the descriptor pool.
 * @param descriptorPool The descriptor pool to destroy. Set to VK_NULL_HANDLE after destruction.
 */
inline void destroySingleDescriptorPool(VkDevice device, VkDescriptorPool& descriptorPool) noexcept {
    destroySingle<VkDescriptorPool, vkDestroyDescriptorPool>(device, descriptorPool);
}

/**
 * @brief Destroys a single Vulkan descriptor set layout.
 * @param device The Vulkan device owning the descriptor set layout.
 * @param layout The descriptor set layout to destroy. Set to VK_NULL_HANDLE after destruction.
 */
inline void destroySingleDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& layout) noexcept {
    destroySingle<VkDescriptorSetLayout, vkDestroyDescriptorSetLayout>(device, layout);
}

/**
 * @brief Destroys a single Vulkan pipeline.
 * @param device The Vulkan device owning the pipeline.
 * @param pipeline The pipeline to destroy. Set to VK_NULL_HANDLE after destruction.
 */
inline void destroySinglePipeline(VkDevice device, VkPipeline& pipeline) noexcept {
    destroySingle<VkPipeline, vkDestroyPipeline>(device, pipeline);
}

/**
 * @brief Destroys a single Vulkan pipeline layout.
 * @param device The Vulkan device owning the pipeline layout.
 * @param layout The pipeline layout to destroy. Set to VK_NULL_HANDLE after destruction.
 */
inline void destroySinglePipelineLayout(VkDevice device, VkPipelineLayout& layout) noexcept {
    destroySingle<VkPipelineLayout, vkDestroyPipelineLayout>(device, layout);
}

/**
 * @brief Destroys a single Vulkan render pass.
 * @param device The Vulkan device owning the render pass.
 * @param renderPass The render pass to destroy. Set to VK_NULL_HANDLE after destruction.
 */
inline void destroySingleRenderPass(VkDevice device, VkRenderPass& renderPass) noexcept {
    destroySingle<VkRenderPass, vkDestroyRenderPass>(device, renderPass);
}

/**
 * @brief Destroys a single Vulkan swapchain.
 * @param device The Vulkan device owning the swapchain.
 * @param swapchain The swapchain to destroy. Set to VK_NULL_HANDLE after destruction.
 */
inline void destroySingleSwapchain(VkDevice device, VkSwapchainKHR& swapchain) noexcept {
    std::lock_guard<std::mutex> lock(Vulkan::cleanupMutex);
    if (device != VK_NULL_HANDLE && swapchain != VK_NULL_HANDLE) {
        try {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
            LOG_INFO("Destroyed single swapchain");
        } catch (const std::exception& e) {
            LOG_ERROR(std::format("Failed to destroy single swapchain: {}", e.what()));
            swapchain = VK_NULL_HANDLE;
        }
    }
}

/**
 * @brief Destroys a single Vulkan command pool.
 * @param device The Vulkan device owning the command pool.
 * @param commandPool The command pool to destroy. Set to VK_NULL_HANDLE after destruction.
 */
inline void destroySingleCommandPool(VkDevice device, VkCommandPool& commandPool) noexcept {
    destroySingle<VkCommandPool, vkDestroyCommandPool>(device, commandPool);
}

/**
 * @brief Destroys a vector of Vulkan shader modules.
 * @param device The Vulkan device owning the shader modules.
 * @param shaderModules The vector of shader modules to destroy. Cleared after destruction.
 */
inline void destroyShaderModules(VkDevice device, std::vector<VkShaderModule>& shaderModules) noexcept {
    destroyHandles<VkShaderModule, vkDestroyShaderModule>(device, shaderModules);
}

/**
 * @brief Destroys a single Vulkan shader module.
 * @param device The Vulkan device owning the shader module.
 * @param shaderModule The shader module to destroy. Set to VK_NULL_HANDLE after destruction.
 */
inline void destroySingleShaderModule(VkDevice device, VkShaderModule& shaderModule) noexcept {
    destroySingle<VkShaderModule, vkDestroyShaderModule>(device, shaderModule);
}

/**
 * @brief Destroys a single Vulkan acceleration structure.
 * @param device The Vulkan device owning the acceleration structure.
 * @param as The acceleration structure to destroy. Set to VK_NULL_HANDLE after destruction.
 */
inline void destroySingleAccelerationStructure(VkDevice device, VkAccelerationStructureKHR& as) noexcept {
    std::lock_guard<std::mutex> lock(Vulkan::cleanupMutex);
    if (device != VK_NULL_HANDLE && as != VK_NULL_HANDLE) {
        try {
            auto func = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
            if (func) {
                func(device, as, nullptr);
                as = VK_NULL_HANDLE;
                LOG_INFO("Destroyed single acceleration structure");
            } else {
                LOG_WARNING("vkDestroyAccelerationStructureKHR not found");
            }
        } catch (const std::exception& e) {
            LOG_ERROR(std::format("Failed to destroy single acceleration structure: {}", e.what()));
            as = VK_NULL_HANDLE;
        }
    }
}

/**
 * @brief Destroys a Vulkan device.
 * @param device The Vulkan device to destroy.
 */
inline void destroyDevice(VkDevice device) noexcept {
    std::lock_guard<std::mutex> lock(Vulkan::cleanupMutex);
    if (device != VK_NULL_HANDLE) {
        try {
            vkDestroyDevice(device, nullptr);
            LOG_INFO("Destroyed Vulkan device");
        } catch (const std::exception& e) {
            LOG_ERROR(std::format("Failed to destroy Vulkan device: {}", e.what()));
        }
    }
}

/**
 * @brief Destroys a Vulkan debug utils messenger.
 * @param instance The Vulkan instance owning the debug messenger.
 * @param messenger The debug messenger to destroy.
 */
inline void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger) noexcept {
    std::lock_guard<std::mutex> lock(Vulkan::cleanupMutex);
    if (messenger != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
        try {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func) {
                func(instance, messenger, nullptr);
                LOG_INFO("Destroyed debug utils messenger");
            } else {
                LOG_WARNING("vkDestroyDebugUtilsMessengerEXT not found");
            }
        } catch (const std::exception& e) {
            LOG_ERROR(std::format("Failed to destroy debug utils messenger: {}", e.what()));
        }
    }
}

/**
 * @brief Destroys a Vulkan instance.
 * @param instance The Vulkan instance to destroy.
 */
inline void destroyInstance(VkInstance instance) noexcept {
    std::lock_guard<std::mutex> lock(Vulkan::cleanupMutex);
    if (instance != VK_NULL_HANDLE) {
        try {
            vkDestroyInstance(instance, nullptr);
            LOG_INFO("Destroyed Vulkan instance");
        } catch (const std::exception& e) {
            LOG_ERROR(std::format("Failed to destroy Vulkan instance: {}", e.what()));
        }
    }
}

/**
 * @brief Updates descriptor sets for the Vulkan context.
 * @param context The Vulkan context containing descriptor sets and buffers.
 * @note Thread-safe using Vulkan::cleanupMutex. Logs errors and continues on failure.
 */
void updateDescriptorSets(Vulkan::Context& context) noexcept;

/**
 * @brief Cleans up all resources in the Vulkan context.
 * @param context The Vulkan context to clean up.
 * @note Thread-safe using Vulkan::cleanupMutex. Performs linear cleanup with error logging.
 */
void cleanupVulkanContext(Vulkan::Context& context) noexcept;

} // namespace Dispose

#endif // DISPOSE_HPP