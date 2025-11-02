// src/engine/Vulkan/VulkanResourceManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FINAL: Full destructor + cleanup() + findMemoryType() — NO DOUBLE-FREE, NO UNDEFINED

#include "engine/Vulkan/VulkanResourceManager.hpp"
#include "engine/logging.hpp"
#include <format>
#include <stdexcept>

// ---------------------------------------------------------------------------
//  DESTRUCTOR – AUTO-CLEANUP
// ---------------------------------------------------------------------------
VulkanResourceManager::~VulkanResourceManager() {
    if (device_ != VK_NULL_HANDLE) {
        cleanup(device_);
    }
}

// ---------------------------------------------------------------------------
//  CLEANUP – FULL RESOURCE DESTRUCTION (ORDER: DEPENDENCIES LAST)
// ---------------------------------------------------------------------------
void VulkanResourceManager::cleanup(VkDevice device) {
    VkDevice effectiveDevice = (device == VK_NULL_HANDLE) ? device_ : device;
    if (effectiveDevice == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("ResourceMgr", "Device is null, skipping cleanup");
        return;
    }

    LOG_DEBUG_CAT("ResourceMgr", "Starting VulkanResourceManager cleanup");

    // Wait for GPU to finish
    vkDeviceWaitIdle(effectiveDevice);

    // === PIPELINES ===
    for (auto p : pipelines_) {
        if (p != VK_NULL_HANDLE) {
            vkDestroyPipeline(effectiveDevice, p, nullptr);
            LOG_INFO_CAT("ResourceMgr", "Destroyed pipeline: {:p}", static_cast<void*>(p));
        }
    }
    pipelines_.clear();
    pipelineMap_.clear();

    // === PIPELINE LAYOUTS ===
    for (auto l : pipelineLayouts_) {
        if (l != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(effectiveDevice, l, nullptr);
            LOG_INFO_CAT("ResourceMgr", "Destroyed pipeline layout: {:p}", static_cast<void*>(l));
        }
    }
    pipelineLayouts_.clear();

    // === DESCRIPTOR SET LAYOUTS ===
    for (auto l : descriptorSetLayouts_) {
        if (l != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(effectiveDevice, l, nullptr);
            LOG_INFO_CAT("ResourceMgr", "Destroyed descriptor set layout: {:p}", static_cast<void*>(l));
        }
    }
    descriptorSetLayouts_.clear();

    // === RENDER PASSES ===
    for (auto rp : renderPasses_) {
        if (rp != VK_NULL_HANDLE) {
            vkDestroyRenderPass(effectiveDevice, rp, nullptr);
            LOG_INFO_CAT("ResourceMgr", "Destroyed render pass: {:p}", static_cast<void*>(rp));
        }
    }
    renderPasses_.clear();

    // === SHADER MODULES ===
    for (auto m : shaderModules_) {
        if (m != VK_NULL_HANDLE) {
            vkDestroyShaderModule(effectiveDevice, m, nullptr);
            LOG_INFO_CAT("ResourceMgr", "Destroyed shader module: {:p}", static_cast<void*>(m));
        }
    }
    shaderModules_.clear();

    // === ACCELERATION STRUCTURES (KHR) ===
    for (auto as : accelerationStructures_) {
        if (as != VK_NULL_HANDLE) {
            auto destroyFn = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
                vkGetDeviceProcAddr(effectiveDevice, "vkDestroyAccelerationStructureKHR"));
            if (destroyFn) {
                destroyFn(effectiveDevice, as, nullptr);
                LOG_INFO_CAT("ResourceMgr", "Destroyed acceleration structure: {:p}", static_cast<void*>(as));
            } else {
                LOG_WARNING_CAT("ResourceMgr", "vkDestroyAccelerationStructureKHR not loaded");
            }
        }
    }
    accelerationStructures_.clear();

    // === IMAGE VIEWS ===
    for (auto iv : imageViews_) {
        if (iv != VK_NULL_HANDLE) {
            vkDestroyImageView(effectiveDevice, iv, nullptr);
            LOG_INFO_CAT("ResourceMgr", "Destroyed image view: {:p}", static_cast<void*>(iv));
        }
    }
    imageViews_.clear();

    // === IMAGES ===
    for (auto img : images_) {
        if (img != VK_NULL_HANDLE) {
            vkDestroyImage(effectiveDevice, img, nullptr);
            LOG_INFO_CAT("ResourceMgr", "Destroyed image: {:p}", static_cast<void*>(img));
        }
    }
    images_.clear();

    // === BUFFERS ===
    for (auto b : buffers_) {
        if (b != VK_NULL_HANDLE) {
            vkDestroyBuffer(effectiveDevice, b, nullptr);
            LOG_INFO_CAT("ResourceMgr", "Destroyed buffer: {:p}", static_cast<void*>(b));
        }
    }
    buffers_.clear();

    // === MEMORY ===
    for (auto mem : memories_) {
        if (mem != VK_NULL_HANDLE) {
            vkFreeMemory(effectiveDevice, mem, nullptr);
            LOG_INFO_CAT("ResourceMgr", "Freed memory: {:p}", static_cast<void*>(mem));
        }
    }
    memories_.clear();

    // === DESCRIPTOR POOLS ===
    for (auto dp : descriptorPools_) {
        if (dp != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(effectiveDevice, dp, nullptr);
            LOG_INFO_CAT("ResourceMgr", "Destroyed descriptor pool: {:p}", static_cast<void*>(dp));
        }
    }
    descriptorPools_.clear();

    // === COMMAND POOLS ===
    for (auto cp : commandPools_) {
        if (cp != VK_NULL_HANDLE) {
            vkDestroyCommandPool(effectiveDevice, cp, nullptr);
            LOG_INFO_CAT("ResourceMgr", "Destroyed command pool: {:p}", static_cast<void*>(cp));
        }
    }
    commandPools_.clear();

    LOG_INFO_CAT("ResourceMgr", "VulkanResourceManager cleanup completed");
}

// ---------------------------------------------------------------------------
//  FIND MEMORY TYPE – ROBUST & LOGGED
// ---------------------------------------------------------------------------
uint32_t VulkanResourceManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    if (physicalDevice_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("ResourceMgr", "Physical device not set in resource manager!");
        throw std::runtime_error("Physical device not set");
    }

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            LOG_DEBUG_CAT("ResourceMgr", "Found memory type: {} (filter: 0x{:x}, props: 0x{:x})", i, typeFilter, properties);
            return i;
        }
    }

    LOG_ERROR_CAT("ResourceMgr", "Failed to find suitable memory type! filter=0x{:x}, props=0x{:x}", typeFilter, properties);
    throw std::runtime_error("Failed to find suitable memory type!");
}