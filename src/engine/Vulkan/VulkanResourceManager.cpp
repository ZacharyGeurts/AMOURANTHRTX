// src/engine/Vulkan/VulkanResourceManager.cpp
// AMOURANTH RTX Engine (C) 2025 — STONEKEY v2.0 — HACKERS OBLITERATED
// C++23 | ZERO RUNTIME COST | REBUILD = CHEAT APOCALYPSE

#include "engine/Vulkan/VulkanResourceManager.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <algorithm>

using namespace Logging::Color;

// ---------------------------------------------------------------------------
//  INIT
// ---------------------------------------------------------------------------
void VulkanResourceManager::init(VulkanCore* core) {
    device_ = core->device;
    physicalDevice_ = core->physicalDevice;
    bufferManager_ = &core->bufferManager;
    contextDevicePtr_ = &core->device;
    vkDestroyAccelerationStructureKHR_ = core->vkDestroyAccelerationStructureKHR;
}

// ---------------------------------------------------------------------------
//  MOVE CONSTRUCTOR
// ---------------------------------------------------------------------------
VulkanResourceManager::VulkanResourceManager(VulkanResourceManager&& other) noexcept
    : buffers_(std::move(other.buffers_))
    , memories_(std::move(other.memories_))
    , images_(std::move(other.images_))
    , imageViews_(std::move(other.imageViews_))
    , samplers_(std::move(other.samplers_))
    , accelerationStructures_(std::move(other.accelerationStructures_))
    , descriptorPools_(std::move(other.descriptorPools_))
    , commandPools_(std::move(other.commandPools_))
    , renderPasses_(std::move(other.renderPasses_))
    , descriptorSetLayouts_(std::move(other.descriptorSetLayouts_))
    , pipelineLayouts_(std::move(other.pipelineLayouts_))
    , pipelines_(std::move(other.pipelines_))
    , shaderModules_(std::move(other.shaderModules_))
    , descriptorSets_(std::move(other.descriptorSets_))
    , fences_(std::move(other.fences_))
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
        images_ = std::move(other.images_);
        imageViews_ = std::move(other.imageViews_);
        samplers_ = std::move(other.samplers_);
        accelerationStructures_ = std::move(other.accelerationStructures_);
        descriptorPools_ = std::move(other.descriptorPools_);
        commandPools_ = std::move(other.commandPools_);
        renderPasses_ = std::move(other.renderPasses_);
        descriptorSetLayouts_ = std::move(other.descriptorSetLayouts_);
        pipelineLayouts_ = std::move(other.pipelineLayouts_);
        pipelines_ = std::move(other.pipelines_);
        shaderModules_ = std::move(other.shaderModules_);
        descriptorSets_ = std::move(other.descriptorSets_);
        fences_ = std::move(other.fences_);
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
//  CREATE FENCE — FULLY TRACKED
// ---------------------------------------------------------------------------
uint64_t VulkanResourceManager::createFence(bool signaled) {
    VkFenceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(device_, &info, nullptr, &fence));

    fences_.push_back(encrypt(fence));
    LOG_DEBUG_CAT("ResourceMgr", "Created + Tracked Fence: {:p} → enc 0x{:016x}", 
                  static_cast<void*>(fence), fences_.back());

    return encrypt(fence);
}

// ---------------------------------------------------------------------------
//  CLEANUP — STONEKEY SAFE DESTROY — LOG_DEBUG_CAT
// ---------------------------------------------------------------------------
void VulkanResourceManager::cleanup(VkDevice device) {
    VkDevice effectiveDevice = (device == VK_NULL_HANDLE)
        ? (contextDevicePtr_ ? *contextDevicePtr_ : device_)
        : device;

    if (effectiveDevice == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("ResourceMgr", "{}Device null, skipping cleanup{}", AMBER_YELLOW, RESET);
        return;
    }

    LOG_INFO_CAT("ResourceMgr", "{}Starting STONEKEY RAII cleanup...{}", RASPBERRY_PINK, RESET);

    #define SAFE_DESTROY(container, func, type) \
        do { \
            LOG_INFO_CAT("ResourceMgr", "{}Phase: Destroying {}s{}", OCEAN_TEAL, #type, RESET); \
            for (auto it = container.rbegin(); it != container.rend(); ++it) { \
                if (*it != 0) { \
                    auto raw = decrypt<Vk##type>(*it); \
                    if (raw != VK_NULL_HANDLE) { \
                        try { \
                            LOG_DEBUG_CAT("ResourceMgr", "Destroying {}: {:p}", #type, static_cast<void*>(raw)); \
                            func(effectiveDevice, raw, nullptr); \
                        } catch (...) { \
                            LOG_ERROR_CAT("ResourceMgr", "Exception destroying {} {:p}", #type, static_cast<void*>(raw)); \
                        } \
                    } \
                } \
            } \
            container.clear(); \
        } while(0)

    // FENCES FIRST
    SAFE_DESTROY(fences_, vkDestroyFence, Fence);

    // REVERSE ORDER
    SAFE_DESTROY(pipelines_, vkDestroyPipeline, Pipeline);
    SAFE_DESTROY(pipelineLayouts_, vkDestroyPipelineLayout, PipelineLayout);
    SAFE_DESTROY(descriptorSetLayouts_, vkDestroyDescriptorSetLayout, DescriptorSetLayout);
    SAFE_DESTROY(renderPasses_, vkDestroyRenderPass, RenderPass);
    SAFE_DESTROY(shaderModules_, vkDestroyShaderModule, ShaderModule);

    if (vkDestroyAccelerationStructureKHR_) {
        LOG_INFO_CAT("ResourceMgr", "{}Phase: Acceleration Structures{}", OCEAN_TEAL, RESET);
        for (auto it = accelerationStructures_.rbegin(); it != accelerationStructures_.rend(); ++it) {
            if (*it != 0) {
                auto raw = decrypt<VkAccelerationStructureKHR>(*it);
                if (raw != VK_NULL_HANDLE) {
                    LOG_DEBUG_CAT("ResourceMgr", "Destroying AccelerationStructure: {:p}", static_cast<void*>(raw));
                    vkDestroyAccelerationStructureKHR_(effectiveDevice, raw, nullptr);
                }
            }
        }
        accelerationStructures_.clear();
    }

    SAFE_DESTROY(imageViews_, vkDestroyImageView, ImageView);
    SAFE_DESTROY(samplers_, vkDestroySampler, Sampler);
    SAFE_DESTROY(images_, vkDestroyImage, Image);
    SAFE_DESTROY(buffers_, vkDestroyBuffer, Buffer);
    SAFE_DESTROY(memories_, vkFreeMemory, DeviceMemory);
    SAFE_DESTROY(descriptorPools_, vkDestroyDescriptorPool, DescriptorPool);

    for (auto it = commandPools_.rbegin(); it != commandPools_.rend(); ++it) {
        if (*it != 0) {
            auto raw = decrypt<VkCommandPool>(*it);
            if (raw != VK_NULL_HANDLE) {
                vkResetCommandPool(effectiveDevice, raw, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
                vkDestroyCommandPool(effectiveDevice, raw, nullptr);
            }
        }
    }
    commandPools_.clear();

    pipelineMap_.clear();

    LOG_INFO_CAT("ResourceMgr", "{}STONEKEY RAII cleanup COMPLETE. Valhalla secured.{}", EMERALD_GREEN, RESET);

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