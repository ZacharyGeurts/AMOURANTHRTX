// src/engine/Vulkan/VulkanResourceManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FINAL: Full destructor + cleanup() + findMemoryType() — NO DOUBLE-FREE, NO UNDEFINED
//        FIXED: Dedup handles with unordered_set before destroy (prevents NVIDIA driver crash on duplicates/stale)

#include "engine/Vulkan/VulkanResourceManager.hpp"
#include "engine/logging.hpp"
#include <format>
#include <stdexcept>
#include <unordered_set>

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
    {
        std::unordered_set<VkPipeline> uniquePipelines(pipelines_.begin(), pipelines_.end());
        for (auto p : uniquePipelines) {
            if (p != VK_NULL_HANDLE) {
                vkDestroyPipeline(effectiveDevice, p, nullptr);
                LOG_INFO_CAT("ResourceMgr", "Destroyed pipeline: {:p}", static_cast<void*>(p));
            }
        }
        pipelines_.clear();
    }
    pipelineMap_.clear();

    // === PIPELINE LAYOUTS ===
    {
        std::unordered_set<VkPipelineLayout> uniqueLayouts(pipelineLayouts_.begin(), pipelineLayouts_.end());
        for (auto l : uniqueLayouts) {
            if (l != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(effectiveDevice, l, nullptr);
                LOG_INFO_CAT("ResourceMgr", "Destroyed pipeline layout: {:p}", static_cast<void*>(l));
            }
        }
        pipelineLayouts_.clear();
    }

    // === DESCRIPTOR SET LAYOUTS ===
    {
        std::unordered_set<VkDescriptorSetLayout> uniqueLayouts(descriptorSetLayouts_.begin(), descriptorSetLayouts_.end());
        for (auto l : uniqueLayouts) {
            if (l != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(effectiveDevice, l, nullptr);
                LOG_INFO_CAT("ResourceMgr", "Destroyed descriptor set layout: {:p}", static_cast<void*>(l));
            }
        }
        descriptorSetLayouts_.clear();
    }

    // === RENDER PASSES ===
    {
        std::unordered_set<VkRenderPass> uniqueRenderPasses(renderPasses_.begin(), renderPasses_.end());
        for (auto rp : uniqueRenderPasses) {
            if (rp != VK_NULL_HANDLE) {
                vkDestroyRenderPass(effectiveDevice, rp, nullptr);
                LOG_INFO_CAT("ResourceMgr", "Destroyed render pass: {:p}", static_cast<void*>(rp));
            }
        }
        renderPasses_.clear();
    }

    // === SHADER MODULES ===
    {
        std::unordered_set<VkShaderModule> uniqueModules(shaderModules_.begin(), shaderModules_.end());
        for (auto m : uniqueModules) {
            if (m != VK_NULL_HANDLE) {
                vkDestroyShaderModule(effectiveDevice, m, nullptr);
                LOG_INFO_CAT("ResourceMgr", "Destroyed shader module: {:p}", static_cast<void*>(m));
            }
        }
        shaderModules_.clear();
    }

    // === ACCELERATION STRUCTURES (KHR) ===
    {
        std::unordered_set<VkAccelerationStructureKHR> uniqueAS(accelerationStructures_.begin(), accelerationStructures_.end());
        for (auto as : uniqueAS) {
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
    }

    // === IMAGE VIEWS ===
    {
        std::unordered_set<VkImageView> uniqueViews(imageViews_.begin(), imageViews_.end());
        for (auto iv : uniqueViews) {
            if (iv != VK_NULL_HANDLE) {
                vkDestroyImageView(effectiveDevice, iv, nullptr);
                LOG_INFO_CAT("ResourceMgr", "Destroyed image view: {:p}", static_cast<void*>(iv));
            }
        }
        imageViews_.clear();
    }

    // === IMAGES ===
    {
        std::unordered_set<VkImage> uniqueImages(images_.begin(), images_.end());
        for (auto img : uniqueImages) {
            if (img != VK_NULL_HANDLE) {
                vkDestroyImage(effectiveDevice, img, nullptr);
                LOG_INFO_CAT("ResourceMgr", "Destroyed image: {:p}", static_cast<void*>(img));
            }
        }
        images_.clear();
    }

    // === BUFFERS ===
    {
        std::unordered_set<VkBuffer> uniqueBuffers(buffers_.begin(), buffers_.end());
        for (auto b : uniqueBuffers) {
            if (b != VK_NULL_HANDLE) {
                vkDestroyBuffer(effectiveDevice, b, nullptr);
                LOG_INFO_CAT("ResourceMgr", "Destroyed buffer: {:p}", static_cast<void*>(b));
            }
        }
        buffers_.clear();
    }

    // === MEMORY ===
    {
        std::unordered_set<VkDeviceMemory> uniqueMemories(memories_.begin(), memories_.end());
        for (auto mem : uniqueMemories) {
            if (mem != VK_NULL_HANDLE) {
                vkFreeMemory(effectiveDevice, mem, nullptr);
                LOG_INFO_CAT("ResourceMgr", "Freed memory: {:p}", static_cast<void*>(mem));
            }
        }
        memories_.clear();
    }

    // === DESCRIPTOR POOLS ===
    {
        std::unordered_set<VkDescriptorPool> uniquePools(descriptorPools_.begin(), descriptorPools_.end());
        for (auto dp : uniquePools) {
            if (dp != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(effectiveDevice, dp, nullptr);
                LOG_INFO_CAT("ResourceMgr", "Destroyed descriptor pool: {:p}", static_cast<void*>(dp));
            }
        }
        descriptorPools_.clear();
    }

    // === COMMAND POOLS ===
    {
        std::unordered_set<VkCommandPool> uniqueCommandPools(commandPools_.begin(), commandPools_.end());
        for (auto cp : uniqueCommandPools) {
            if (cp != VK_NULL_HANDLE) {
                vkDestroyCommandPool(effectiveDevice, cp, nullptr);
                LOG_INFO_CAT("ResourceMgr", "Destroyed command pool: {:p}", static_cast<void*>(cp));
            }
        }
        commandPools_.clear();
    }

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