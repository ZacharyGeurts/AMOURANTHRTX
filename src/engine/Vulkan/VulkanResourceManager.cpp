// src/engine/Vulkan/VulkanResourceManager.cpp
// FULL DESTRUCTOR + cleanup() + findMemoryType() IMPLEMENTATION
// LINKS ALL undefined references

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"

VulkanResourceManager::~VulkanResourceManager() {
    if (device_ != VK_NULL_HANDLE) {
        cleanup(device_);
    }
}

void VulkanResourceManager::cleanup(VkDevice device) {
    VkDevice effectiveDevice = (device == VK_NULL_HANDLE) ? device_ : device;
    if (effectiveDevice == VK_NULL_HANDLE) {
        LOG_WARNING("Device is null, skipping resource manager cleanup");
        return;
    }
    vkDeviceWaitIdle(effectiveDevice);
    LOG_DEBUG("Starting VulkanResourceManager cleanup");

    for (auto p : pipelines_) {
        if (p != VK_NULL_HANDLE) {
            vkDestroyPipeline(effectiveDevice, p, nullptr);
            LOG_INFO(std::format("Destroyed pipeline: {:p}", static_cast<void*>(p)));
        }
    }
    pipelines_.clear(); pipelineMap_.clear();

    for (auto l : pipelineLayouts_) {
        if (l != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(effectiveDevice, l, nullptr);
            LOG_INFO(std::format("Destroyed pipeline layout: {:p}", static_cast<void*>(l)));
        }
    }
    pipelineLayouts_.clear();

    for (auto l : descriptorSetLayouts_) {
        if (l != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(effectiveDevice, l, nullptr);
            LOG_INFO(std::format("Destroyed descriptor set layout: {:p}", static_cast<void*>(l)));
        }
    }
    descriptorSetLayouts_.clear();

    for (auto rp : renderPasses_) {
        if (rp != VK_NULL_HANDLE) {
            vkDestroyRenderPass(effectiveDevice, rp, nullptr);
            LOG_INFO(std::format("Destroyed render pass: {:p}", static_cast<void*>(rp)));
        }
    }
    renderPasses_.clear();

    for (auto m : shaderModules_) {
        if (m != VK_NULL_HANDLE) {
            vkDestroyShaderModule(effectiveDevice, m, nullptr);
            LOG_INFO(std::format("Destroyed shader module: {:p}", static_cast<void*>(m)));
        }
    }
    shaderModules_.clear();

    for (auto as : accelerationStructures_) {
        if (as != VK_NULL_HANDLE) {
            auto destroyFn = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
                vkGetDeviceProcAddr(effectiveDevice, "vkDestroyAccelerationStructureKHR"));
            if (destroyFn) {
                destroyFn(effectiveDevice, as, nullptr);
                LOG_INFO(std::format("Destroyed acceleration structure: {:p}", static_cast<void*>(as)));
            }
        }
    }
    accelerationStructures_.clear();

    for (auto iv : imageViews_) {
        if (iv != VK_NULL_HANDLE) {
            vkDestroyImageView(effectiveDevice, iv, nullptr);
            LOG_INFO(std::format("Destroyed image view: {:p}", static_cast<void*>(iv)));
        }
    }
    imageViews_.clear();

    for (auto img : images_) {
        if (img != VK_NULL_HANDLE) {
            vkDestroyImage(effectiveDevice, img, nullptr);
            LOG_INFO(std::format("Destroyed image: {:p}", static_cast<void*>(img)));
        }
    }
    images_.clear();

    for (auto b : buffers_) {
        if (b != VK_NULL_HANDLE) {
            vkDestroyBuffer(effectiveDevice, b, nullptr);
            LOG_INFO(std::format("Destroyed buffer: {:p}", static_cast<void*>(b)));
        }
    }
    buffers_.clear();

    for (auto mem : memories_) {
        if (mem != VK_NULL_HANDLE) {
            vkFreeMemory(effectiveDevice, mem, nullptr);
            LOG_INFO(std::format("Freed memory: {:p}", static_cast<void*>(mem)));
        }
    }
    memories_.clear();

    for (auto dp : descriptorPools_) {
        if (dp != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(effectiveDevice, dp, nullptr);
            LOG_INFO(std::format("Destroyed descriptor pool: {:p}", static_cast<void*>(dp)));
        }
    }
    descriptorPools_.clear();

    for (auto cp : commandPools_) {
        if (cp != VK_NULL_HANDLE) {
            vkDestroyCommandPool(effectiveDevice, cp, nullptr);
            LOG_INFO(std::format("Destroyed command pool: {:p}", static_cast<void*>(cp)));
        }
    }
    commandPools_.clear();

    LOG_INFO("VulkanResourceManager cleanup completed");
}

uint32_t VulkanResourceManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    if (physicalDevice_ == VK_NULL_HANDLE) {
        LOG_ERROR("Physical device not set in resource manager!");
        throw std::runtime_error("Physical device not set");
    }
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOG_ERROR("Failed to find suitable memory type!");
    throw std::runtime_error("Failed to find suitable memory type!");
}