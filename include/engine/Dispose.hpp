// AMOURANTH RTX Engine, October 2025 - Centralized resource disposal for SDL3 and Vulkan.
// One-liner functions where possible, assuming nullptr allocator for Vulkan.
// Usage: Call Dispose::destroy* functions in destructors or cleanup methods.
// Thread safety handled externally. All functions log errors and ensure passthrough to prevent freezes.
// Zachary Geurts 2025

#ifndef DISPOSE_HPP
#define DISPOSE_HPP

#include "engine/Vulkan/VulkanCore.hpp"
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <vector>
#include <format>
#include <typeinfo>
#include "engine/logging.hpp"

namespace Dispose {

template <typename HandleType, void (*DestroyFunc)(VkDevice, HandleType, const VkAllocationCallbacks*)>
inline void destroyHandles(VkDevice device, std::vector<HandleType>& handles) noexcept {
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

template <typename HandleType, void (*DestroyFunc)(VkDevice, HandleType, const VkAllocationCallbacks*)>
inline void destroySingle(VkDevice device, HandleType& handle) noexcept {
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

inline void destroyWindow(SDL_Window* window) noexcept {
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

inline void quitSDL() noexcept {
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

inline void destroyFramebuffers(VkDevice device, std::vector<VkFramebuffer>& framebuffers) noexcept {
    destroyHandles<VkFramebuffer, vkDestroyFramebuffer>(device, framebuffers);
}

inline void destroySemaphores(VkDevice device, std::vector<VkSemaphore>& semaphores) noexcept {
    destroyHandles<VkSemaphore, vkDestroySemaphore>(device, semaphores);
}

inline void destroyFences(VkDevice device, std::vector<VkFence>& fences) noexcept {
    destroyHandles<VkFence, vkDestroyFence>(device, fences);
}

inline void freeCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers) noexcept {
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

inline void destroyImageViews(VkDevice device, std::vector<VkImageView>& imageViews) noexcept {
    destroyHandles<VkImageView, vkDestroyImageView>(device, imageViews);
}

inline void destroySingleImageView(VkDevice device, VkImageView& imageView) noexcept {
    destroySingle<VkImageView, vkDestroyImageView>(device, imageView);
}

inline void destroySingleImage(VkDevice device, VkImage& image) noexcept {
    destroySingle<VkImage, vkDestroyImage>(device, image);
}

inline void freeDeviceMemories(VkDevice device, std::vector<VkDeviceMemory>& memories) noexcept {
    destroyHandles<VkDeviceMemory, vkFreeMemory>(device, memories);
}

inline void freeSingleDeviceMemory(VkDevice device, VkDeviceMemory& memory) noexcept {
    destroySingle<VkDeviceMemory, vkFreeMemory>(device, memory);
}

inline void destroyBuffers(VkDevice device, std::vector<VkBuffer>& buffers) noexcept {
    destroyHandles<VkBuffer, vkDestroyBuffer>(device, buffers);
}

inline void destroySingleBuffer(VkDevice device, VkBuffer& buffer) noexcept {
    destroySingle<VkBuffer, vkDestroyBuffer>(device, buffer);
}

inline void destroySingleSampler(VkDevice device, VkSampler& sampler) noexcept {
    destroySingle<VkSampler, vkDestroySampler>(device, sampler);
}

inline void freeSingleDescriptorSet(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSet& descriptorSet) noexcept {
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

inline void destroySingleDescriptorPool(VkDevice device, VkDescriptorPool& descriptorPool) noexcept {
    destroySingle<VkDescriptorPool, vkDestroyDescriptorPool>(device, descriptorPool);
}

inline void destroySingleDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& layout) noexcept {
    destroySingle<VkDescriptorSetLayout, vkDestroyDescriptorSetLayout>(device, layout);
}

inline void destroySinglePipeline(VkDevice device, VkPipeline& pipeline) noexcept {
    destroySingle<VkPipeline, vkDestroyPipeline>(device, pipeline);
}

inline void destroySinglePipelineLayout(VkDevice device, VkPipelineLayout& layout) noexcept {
    destroySingle<VkPipelineLayout, vkDestroyPipelineLayout>(device, layout);
}

inline void destroySingleRenderPass(VkDevice device, VkRenderPass& renderPass) noexcept {
    destroySingle<VkRenderPass, vkDestroyRenderPass>(device, renderPass);
}

inline void destroySingleSwapchain(VkDevice device, VkSwapchainKHR& swapchain) noexcept {
    destroySingle<VkSwapchainKHR, vkDestroySwapchainKHR>(device, swapchain);
}

inline void destroySingleCommandPool(VkDevice device, VkCommandPool& commandPool) noexcept {
    destroySingle<VkCommandPool, vkDestroyCommandPool>(device, commandPool);
}

inline void destroyShaderModules(VkDevice device, std::vector<VkShaderModule>& shaderModules) noexcept {
    destroyHandles<VkShaderModule, vkDestroyShaderModule>(device, shaderModules);
}

inline void destroySingleShaderModule(VkDevice device, VkShaderModule& shaderModule) noexcept {
    destroySingle<VkShaderModule, vkDestroyShaderModule>(device, shaderModule);
}

inline void destroySingleAccelerationStructure(VkDevice device, VkAccelerationStructureKHR& as) noexcept {
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

inline void destroyDevice(VkDevice device) noexcept {
    if (device != VK_NULL_HANDLE) {
        try {
            vkDestroyDevice(device, nullptr);
            LOG_INFO("Destroyed Vulkan device");
        } catch (const std::exception& e) {
            LOG_ERROR(std::format("Failed to destroy Vulkan device: {}", e.what()));
        }
    }
}

inline void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger) noexcept {
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

inline void destroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface) noexcept {
    if (instance != VK_NULL_HANDLE && surface != VK_NULL_HANDLE) {
        try {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            LOG_INFO("Destroyed Vulkan surface");
        } catch (const std::exception& e) {
            LOG_ERROR(std::format("Failed to destroy Vulkan surface: {}", e.what()));
        }
    } else {
        LOG_WARNING("Skipping surface destruction: instance or surface is null");
    }
}

inline void destroyInstance(VkInstance instance) noexcept {
    if (instance != VK_NULL_HANDLE) {
        try {
            vkDestroyInstance(instance, nullptr);
            LOG_INFO("Destroyed Vulkan instance");
        } catch (const std::exception& e) {
            LOG_ERROR(std::format("Failed to destroy Vulkan instance: {}", e.what()));
        }
    }
}

void updateDescriptorSets(Vulkan::Context& context) noexcept;
void cleanupVulkanContext(Vulkan::Context& context) noexcept;

} // namespace Dispose

#endif // DISPOSE_HPP