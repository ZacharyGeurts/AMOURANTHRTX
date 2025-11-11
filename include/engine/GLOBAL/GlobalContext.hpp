// include/engine/GLOBAL/GlobalContext.hpp
#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <memory>
#include <vector>
#include <string>
#include <mutex>

class VulkanRenderer;

class GlobalRTXContext {
public:
    static GlobalRTXContext& get() noexcept {
        static GlobalRTXContext instance;
        return instance;
    }

    GlobalRTXContext(const GlobalRTXContext&) = delete;
    GlobalRTXContext& operator=(const GlobalRTXContext&) = delete;
    ~GlobalRTXContext() noexcept { cleanup(); }

    // Creation
    bool createInstance(const std::vector<const char*>& extra = {}) noexcept;
    bool createSurface(SDL_Window* window, VkInstance inst) noexcept;
    bool pickPhysicalDevice(VkSurfaceKHR surf, bool preferNvidia = false) noexcept;
    bool createDevice(VkSurfaceKHR surf, bool enableRT = true) noexcept;
    bool createQueuesAndPools() noexcept;

    // Accessors
    VkInstance vkInstance() const noexcept { return instance_; }
    VkPhysicalDevice vkPhysicalDevice() const noexcept { return physicalDevice_; }
    VkDevice vkDevice() const noexcept { return device_; }
    VkSurfaceKHR vkSurface() const noexcept { return surface_; }
    VkQueue graphicsQueue() const noexcept { return graphicsQueue_; }
    VkQueue presentQueue() const noexcept { return presentQueue_; }
    VkCommandPool commandPool() const noexcept { return commandPool_; }

    void cleanup() noexcept;

private:
    GlobalRTXContext() = default;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    uint32_t graphicsFamily_ = UINT32_MAX;
    uint32_t presentFamily_ = UINT32_MAX;

    struct RTX {
        VkPhysicalDeviceBufferDeviceAddressFeatures addr = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
        VkPhysicalDeviceAccelerationStructureFeaturesKHR as = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
        VkPhysicalDeviceRayQueryFeaturesKHR rq = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };

        void chain() noexcept {
            addr.pNext = &as;
            as.pNext = &rt;
            rt.pNext = &rq;
        }
    } rtx_;
};