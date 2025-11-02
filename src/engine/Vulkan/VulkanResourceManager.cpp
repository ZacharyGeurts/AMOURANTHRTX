// src/engine/Vulkan/VulkanResourceManager.cpp
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"
#include <algorithm>

using namespace Logging::Color;

// ---------------------------------------------------------------------------
//  MOVE CONSTRUCTOR
// ---------------------------------------------------------------------------
VulkanResourceManager::VulkanResourceManager(VulkanResourceManager&& other) noexcept
    : buffers_(std::move(other.buffers_))
    , memories_(std::move(other.memories_))
    , imageViews_(std::move(other.imageViews_))
    , images_(std::move(other.images_))
    , accelerationStructures_(std::move(other.accelerationStructures_))
    , descriptorPools_(std::move(other.descriptorPools_))
    , commandPools_(std::move(other.commandPools_))
    , renderPasses_(std::move(other.renderPasses_))
    , descriptorSetLayouts_(std::move(other.descriptorSetLayouts_))
    , pipelineLayouts_(std::move(other.pipelineLayouts_))
    , pipelines_(std::move(other.pipelines_))
    , shaderModules_(std::move(other.shaderModules_))
    , descriptorSets_(std::move(other.descriptorSets_))
    , pipelineMap_(std::move(other.pipelineMap_))
    , device_(other.device_)
    , physicalDevice_(other.physicalDevice_)
    , bufferManager_(other.bufferManager_)
    , contextDevicePtr_(other.contextDevicePtr_)
    , vkDestroyAccelerationStructureKHR_(other.vkDestroyAccelerationStructureKHR_)
{
    other.device_ = VK_NULL_HANDLE;
    other.physicalDevice_ = VK_NULL_HANDLE;
    other.bufferManager_ = nullptr;
    other.contextDevicePtr_ = nullptr;
    other.vkDestroyAccelerationStructureKHR_ = nullptr;
}

// ---------------------------------------------------------------------------
//  MOVE ASSIGNMENT
// ---------------------------------------------------------------------------
VulkanResourceManager& VulkanResourceManager::operator=(VulkanResourceManager&& other) noexcept {
    if (this != &other) {
        cleanup(device_);
        buffers_ = std::move(other.buffers_);
        memories_ = std::move(other.memories_);
        imageViews_ = std::move(other.imageViews_);
        images_ = std::move(other.images_);
        accelerationStructures_ = std::move(other.accelerationStructures_);
        descriptorPools_ = std::move(other.descriptorPools_);
        commandPools_ = std::move(other.commandPools_);
        renderPasses_ = std::move(other.renderPasses_);
        descriptorSetLayouts_ = std::move(other.descriptorSetLayouts_);
        pipelineLayouts_ = std::move(other.pipelineLayouts_);
        pipelines_ = std::move(other.pipelines_);
        shaderModules_ = std::move(other.shaderModules_);
        descriptorSets_ = std::move(other.descriptorSets_);
        pipelineMap_ = std::move(other.pipelineMap_);
        device_ = other.device_;
        physicalDevice_ = other.physicalDevice_;
        bufferManager_ = other.bufferManager_;
        contextDevicePtr_ = other.contextDevicePtr_;
        vkDestroyAccelerationStructureKHR_ = other.vkDestroyAccelerationStructureKHR_;

        other.device_ = VK_NULL_HANDLE;
        other.physicalDevice_ = VK_NULL_HANDLE;
        other.bufferManager_ = nullptr;
        other.contextDevicePtr_ = nullptr;
        other.vkDestroyAccelerationStructureKHR_ = nullptr;
    }
    return *this;
}

// ---------------------------------------------------------------------------
//  DESTRUCTOR
// ---------------------------------------------------------------------------
VulkanResourceManager::~VulkanResourceManager() {
    if (contextDevicePtr_ && *contextDevicePtr_ != VK_NULL_HANDLE) {
        cleanup(*contextDevicePtr_);
    } else if (device_ != VK_NULL_HANDLE) {
        cleanup(device_);
    }
}

// ---------------------------------------------------------------------------
//  CLEANUP — DESTROYS ALL TRACKED RESOURCES (NO DOUBLE FREE)
// ---------------------------------------------------------------------------
void VulkanResourceManager::cleanup(VkDevice device) {
    VkDevice effectiveDevice = (device == VK_NULL_HANDLE)
        ? (contextDevicePtr_ ? *contextDevicePtr_ : device_)
        : device;

    if (effectiveDevice == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("ResourceMgr", "{}Device null, skipping cleanup{}", AMBER_YELLOW, RESET);
        return;
    }

    LOG_INFO_CAT("ResourceMgr", "{}Starting RAII cleanup...{}", OCEAN_TEAL, RESET);

    // --- SAFE DESTROY MACRO ---
    #define SAFE_DESTROY(container, func, type) \
        do { \
            for (auto it = container.rbegin(); it != container.rend(); ++it) { \
                if (*it != VK_NULL_HANDLE) { \
                    try { \
                        LOG_DEBUG_CAT("ResourceMgr", "Destroying {}: {:p}", #type, static_cast<void*>(*it)); \
                        func(effectiveDevice, *it, nullptr); \
                        *it = VK_NULL_HANDLE; \
                    } catch (...) { \
                        LOG_ERROR_CAT("ResourceMgr", "Exception destroying {} {:p}", #type, static_cast<void*>(*it)); \
                    } \
                } \
            } \
            container.clear(); \
        } while(0)

    // --- DESTROY IN REVERSE ORDER ---
    SAFE_DESTROY(pipelines_,           vkDestroyPipeline,           Pipeline);
    SAFE_DESTROY(pipelineLayouts_,     vkDestroyPipelineLayout,     PipelineLayout);
    SAFE_DESTROY(descriptorSetLayouts_,vkDestroyDescriptorSetLayout,DescriptorSetLayout);
    SAFE_DESTROY(renderPasses_,        vkDestroyRenderPass,         RenderPass);
    SAFE_DESTROY(shaderModules_,       vkDestroyShaderModule,       ShaderModule);

    // Acceleration Structures — use stored function pointer
    if (vkDestroyAccelerationStructureKHR_) {
        SAFE_DESTROY(accelerationStructures_, vkDestroyAccelerationStructureKHR_, AccelerationStructureKHR);
    } else {
        LOG_WARN_CAT("ResourceMgr", "vkDestroyAccelerationStructureKHR not set — skipping AS cleanup");
        accelerationStructures_.clear();
    }

    SAFE_DESTROY(imageViews_,          vkDestroyImageView,          ImageView);
    SAFE_DESTROY(images_,              vkDestroyImage,              Image);
    SAFE_DESTROY(buffers_,             vkDestroyBuffer,             Buffer);
    SAFE_DESTROY(memories_,            vkFreeMemory,                DeviceMemory);
    SAFE_DESTROY(descriptorPools_,     vkDestroyDescriptorPool,     DescriptorPool);

    // Command pools — reset first
    for (auto it = commandPools_.rbegin(); it != commandPools_.rend(); ++it) {
        if (*it != VK_NULL_HANDLE) {
            try {
                vkResetCommandPool(effectiveDevice, *it, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
                vkDestroyCommandPool(effectiveDevice, *it, nullptr);
                *it = VK_NULL_HANDLE;
            } catch (...) {
                LOG_ERROR_CAT("ResourceMgr", "Exception destroying CommandPool {:p}", static_cast<void*>(*it));
            }
        }
    }
    commandPools_.clear();

    pipelineMap_.clear();

    LOG_INFO_CAT("ResourceMgr", "{}RAII cleanup complete.{}", EMERALD_GREEN, RESET);

    #undef SAFE_DESTROY
}

// ---------------------------------------------------------------------------
//  MEMORY TYPE FINDER
// ---------------------------------------------------------------------------
uint32_t VulkanResourceManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    if (physicalDevice_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("ResourceMgr", "{}Physical device not set!{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Physical device not set");
    }

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOG_ERROR_CAT("ResourceMgr", "{}No memory type found! filter=0x{:x}, props=0x{:x}{}", CRIMSON_MAGENTA, typeFilter, properties, RESET);
    throw std::runtime_error("Failed to find memory type");
}