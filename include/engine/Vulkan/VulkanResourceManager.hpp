// src/engine/Vulkan/VulkanResourceManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FINAL: Full header — matches .cpp, singleton-safe, owned by Context

#pragma once
#ifndef VULKAN_RESOURCE_MANAGER_HPP
#define VULKAN_RESOURCE_MANAGER_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <format>
#include "engine/logging.hpp"

class VulkanBufferManager;

// ====================================================================
// VulkanResourceManager – SINGLETON + OWNED BY Vulkan::Context
// ====================================================================
class VulkanResourceManager {
    std::vector<VkBuffer> buffers_;
    std::vector<VkDeviceMemory> memories_;
    std::vector<VkImageView> imageViews_;
    std::vector<VkImage> images_;
    std::vector<VkAccelerationStructureKHR> accelerationStructures_;
    std::vector<VkDescriptorPool> descriptorPools_;
    std::vector<VkCommandPool> commandPools_;
    std::vector<VkRenderPass> renderPasses_;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts_;
    std::vector<VkPipelineLayout> pipelineLayouts_;
    std::vector<VkPipeline> pipelines_;
    std::vector<VkShaderModule> shaderModules_;
    std::unordered_map<std::string, VkPipeline> pipelineMap_;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VulkanBufferManager* bufferManager_ = nullptr;

public:
    // -------------------------------------------------------------------------
    // SINGLETON ACCESS
    // -------------------------------------------------------------------------
    static VulkanResourceManager& instance() {
        static VulkanResourceManager inst;
        return inst;
    }

    VulkanResourceManager() = default;

    // DELETE COPY
    VulkanResourceManager(const VulkanResourceManager&) = delete;
    VulkanResourceManager& operator=(const VulkanResourceManager&) = delete;

    // MOVE CONSTRUCTOR
    VulkanResourceManager(VulkanResourceManager&& other) noexcept
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
        , pipelineMap_(std::move(other.pipelineMap_))
        , device_(other.device_)
        , physicalDevice_(other.physicalDevice_)
        , bufferManager_(other.bufferManager_)
    {
        other.device_ = VK_NULL_HANDLE;
        other.physicalDevice_ = VK_NULL_HANDLE;
        other.bufferManager_ = nullptr;
    }

    // MOVE ASSIGNMENT
    VulkanResourceManager& operator=(VulkanResourceManager&& other) noexcept {
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
            pipelineMap_ = std::move(other.pipelineMap_);
            device_ = other.device_;
            physicalDevice_ = other.physicalDevice_;
            bufferManager_ = other.bufferManager_;

            other.device_ = VK_NULL_HANDLE;
            other.physicalDevice_ = VK_NULL_HANDLE;
            other.bufferManager_ = nullptr;
        }
        return *this;
    }

    // DESTRUCTOR (defined in .cpp)
    ~VulkanResourceManager();

    // --------------------- PUBLIC API ---------------------
    void cleanup(VkDevice device = VK_NULL_HANDLE);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    // Setters
    void setDevice(VkDevice newDevice, VkPhysicalDevice physicalDevice) {
        if (newDevice == VK_NULL_HANDLE) {
            LOG_ERROR("Cannot set null device to resource manager");
            throw std::invalid_argument("Cannot set null device");
        }
        device_ = newDevice;
        physicalDevice_ = physicalDevice;
        LOG_INFO(std::format("Resource manager device set: {:p}", static_cast<void*>(device_)));
    }
    VkDevice getDevice() const { return device_; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }

    // Resource tracking (add)
    void addBuffer(VkBuffer buffer) {
        if (buffer != VK_NULL_HANDLE) {
            buffers_.push_back(buffer);
            LOG_DEBUG(std::format("Added buffer: {:p}", static_cast<void*>(buffer)));
        }
    }
    void addMemory(VkDeviceMemory memory) {
        if (memory != VK_NULL_HANDLE) {
            memories_.push_back(memory);
            LOG_DEBUG(std::format("Added memory: {:p}", static_cast<void*>(memory)));
        }
    }
    void addImageView(VkImageView view) {
        if (view != VK_NULL_HANDLE) {
            imageViews_.push_back(view);
            LOG_DEBUG(std::format("Added image view: {:p}", static_cast<void*>(view)));
        }
    }
    void addImage(VkImage image) {
        if (image != VK_NULL_HANDLE) {
            images_.push_back(image);
            LOG_DEBUG(std::format("Added image: {:p}", static_cast<void*>(image)));
        }
    }
    void addAccelerationStructure(VkAccelerationStructureKHR as) {
        if (as != VK_NULL_HANDLE) {
            accelerationStructures_.push_back(as);
            LOG_DEBUG(std::format("Added acceleration structure: {:p}", static_cast<void*>(as)));
        }
    }
    void addDescriptorPool(VkDescriptorPool descriptorPool) {
        if (descriptorPool != VK_NULL_HANDLE) {
            descriptorPools_.push_back(descriptorPool);
            LOG_DEBUG(std::format("Added descriptor pool: {:p}", static_cast<void*>(descriptorPool)));
        }
    }
    void addCommandPool(VkCommandPool commandPool) {
        if (commandPool != VK_NULL_HANDLE) {
            commandPools_.push_back(commandPool);
            LOG_DEBUG(std::format("Added command pool: {:p}", static_cast<void*>(commandPool)));
        }
    }
    void addRenderPass(VkRenderPass renderPass) {
        if (renderPass != VK_NULL_HANDLE) {
            renderPasses_.push_back(renderPass);
            LOG_DEBUG(std::format("Added render pass: {:p}", static_cast<void*>(renderPass)));
        }
    }
    void addDescriptorSetLayout(VkDescriptorSetLayout layout) {
        if (layout != VK_NULL_HANDLE) {
            descriptorSetLayouts_.push_back(layout);
            LOG_DEBUG(std::format("Added descriptor set layout: {:p}", static_cast<void*>(layout)));
        }
    }
    void addPipelineLayout(VkPipelineLayout layout) {
        if (layout != VK_NULL_HANDLE) {
            pipelineLayouts_.push_back(layout);
            LOG_DEBUG(std::format("Added pipeline layout: {:p}", static_cast<void*>(layout)));
        }
    }
    void addPipeline(VkPipeline pipeline, const std::string& name = "") {
        if (pipeline != VK_NULL_HANDLE) {
            pipelines_.push_back(pipeline);
            if (!name.empty()) pipelineMap_[name] = pipeline;
            LOG_DEBUG(std::format("Added pipeline: {:p} ({})", static_cast<void*>(pipeline), name));
        }
    }
    void addShaderModule(VkShaderModule module) {
        if (module != VK_NULL_HANDLE) {
            shaderModules_.push_back(module);
            LOG_DEBUG(std::format("Added shader module: {:p}", static_cast<void*>(module)));
        }
    }

    // Resource tracking (remove)
    void removeBuffer(VkBuffer buffer) {
        if (buffer == VK_NULL_HANDLE) return;
        auto it = std::find(buffers_.begin(), buffers_.end(), buffer);
        if (it != buffers_.end()) {
            buffers_.erase(it);
            LOG_DEBUG(std::format("Removed buffer: {:p}", static_cast<void*>(buffer)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent buffer: {:p}", static_cast<void*>(buffer)));
        }
    }
    void removeMemory(VkDeviceMemory memory) {
        if (memory == VK_NULL_HANDLE) return;
        auto it = std::find(memories_.begin(), memories_.end(), memory);
        if (it != memories_.end()) {
            memories_.erase(it);
            LOG_DEBUG(std::format("Removed memory: {:p}", static_cast<void*>(memory)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent memory: {:p}", static_cast<void*>(memory)));
        }
    }
    void removeImageView(VkImageView view) {
        if (view == VK_NULL_HANDLE) return;
        auto it = std::find(imageViews_.begin(), imageViews_.end(), view);
        if (it != imageViews_.end()) {
            imageViews_.erase(it);
            LOG_DEBUG(std::format("Removed image view: {:p}", static_cast<void*>(view)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent image view: {:p}", static_cast<void*>(view)));
        }
    }
    void removeImage(VkImage image) {
        if (image == VK_NULL_HANDLE) return;
        auto it = std::find(images_.begin(), images_.end(), image);
        if (it != images_.end()) {
            images_.erase(it);
            LOG_DEBUG(std::format("Removed image: {:p}", static_cast<void*>(image)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent image: {:p}", static_cast<void*>(image)));
        }
    }
    void removeAccelerationStructure(VkAccelerationStructureKHR as) {
        if (as != VK_NULL_HANDLE) {
            auto it = std::find(accelerationStructures_.begin(), accelerationStructures_.end(), as);
            if (it != accelerationStructures_.end()) {
                accelerationStructures_.erase(it);
                LOG_DEBUG(std::format("Removed acceleration structure: {:p}", static_cast<void*>(as)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent acceleration structure: {:p}", static_cast<void*>(as)));
            }
        }
    }
    void removeDescriptorPool(VkDescriptorPool descriptorPool) {
        if (descriptorPool != VK_NULL_HANDLE) {
            auto it = std::find(descriptorPools_.begin(), descriptorPools_.end(), descriptorPool);
            if (it != descriptorPools_.end()) {
                descriptorPools_.erase(it);
                LOG_DEBUG(std::format("Removed descriptor pool: {:p}", static_cast<void*>(descriptorPool)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent descriptor pool: {:p}", static_cast<void*>(descriptorPool)));
            }
        }
    }
    void removeCommandPool(VkCommandPool commandPool) {
        if (commandPool != VK_NULL_HANDLE) {
            auto it = std::find(commandPools_.begin(), commandPools_.end(), commandPool);
            if (it != commandPools_.end()) {
                commandPools_.erase(it);
                LOG_DEBUG(std::format("Removed command pool: {:p}", static_cast<void*>(commandPool)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent command pool: {:p}", static_cast<void*>(commandPool)));
            }
        }
    }
    void removeRenderPass(VkRenderPass renderPass) {
        if (renderPass != VK_NULL_HANDLE) {
            auto it = std::find(renderPasses_.begin(), renderPasses_.end(), renderPass);
            if (it != renderPasses_.end()) {
                renderPasses_.erase(it);
                LOG_DEBUG(std::format("Removed render pass: {:p}", static_cast<void*>(renderPass)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent render pass: {:p}", static_cast<void*>(renderPass)));
            }
        }
    }
    void removeDescriptorSetLayout(VkDescriptorSetLayout layout) {
        if (layout != VK_NULL_HANDLE) {
            auto it = std::find(descriptorSetLayouts_.begin(), descriptorSetLayouts_.end(), layout);
            if (it != descriptorSetLayouts_.end()) {
                descriptorSetLayouts_.erase(it);
                LOG_DEBUG(std::format("Removed descriptor set layout: {:p}", static_cast<void*>(layout)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent descriptor set layout: {:p}", static_cast<void*>(layout)));
            }
        }
    }
    void removePipelineLayout(VkPipelineLayout layout) {
        if (layout != VK_NULL_HANDLE) {
            auto it = std::find(pipelineLayouts_.begin(), pipelineLayouts_.end(), layout);
            if (it != pipelineLayouts_.end()) {
                pipelineLayouts_.erase(it);
                LOG_DEBUG(std::format("Removed pipeline layout: {:p}", static_cast<void*>(layout)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent pipeline layout: {:p}", static_cast<void*>(layout)));
            }
        }
    }
    void removePipeline(VkPipeline pipeline) {
        if (pipeline != VK_NULL_HANDLE) {
            auto it = std::find(pipelines_.begin(), pipelines_.end(), pipeline);
            if (it != pipelines_.end()) {
                pipelines_.erase(it);
                for (auto mapIt = pipelineMap_.begin(); mapIt != pipelineMap_.end(); ) {
                    if (mapIt->second == pipeline) mapIt = pipelineMap_.erase(mapIt);
                    else ++mapIt;
                }
                LOG_DEBUG(std::format("Removed pipeline: {:p}", static_cast<void*>(pipeline)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent pipeline: {:p}", static_cast<void*>(pipeline)));
            }
        }
    }
    void removeShaderModule(VkShaderModule module) {
        if (module != VK_NULL_HANDLE) {
            auto it = std::find(shaderModules_.begin(), shaderModules_.end(), module);
            if (it != shaderModules_.end()) {
                shaderModules_.erase(it);
                LOG_DEBUG(std::format("Removed shader module: {:p}", static_cast<void*>(module)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent shader module: {:p}", static_cast<void*>(module)));
            }
        }
    }

    // Getters
    const std::vector<VkBuffer>& getBuffers() const { return buffers_; }
    const std::vector<VkDeviceMemory>& getMemories() const { return memories_; }
    const std::vector<VkImageView>& getImageViews() const { return imageViews_; }
    const std::vector<VkImage>& getImages() const { return images_; }
    const std::vector<VkAccelerationStructureKHR>& getAccelerationStructures() const { return accelerationStructures_; }
    const std::vector<VkDescriptorPool>& getDescriptorPools() const { return descriptorPools_; }
    const std::vector<VkCommandPool>& getCommandPools() const { return commandPools_; }
    const std::vector<VkRenderPass>& getRenderPasses() const { return renderPasses_; }
    const std::vector<VkDescriptorSetLayout>& getDescriptorSetLayouts() const { return descriptorSetLayouts_; }
    const std::vector<VkPipelineLayout>& getPipelineLayouts() const { return pipelineLayouts_; }
    const std::vector<VkPipeline>& getPipelines() const { return pipelines_; }
    const std::vector<VkShaderModule>& getShaderModules() const { return shaderModules_; }

    VkPipeline getPipeline(const std::string& name) const {
        auto it = pipelineMap_.find(name);
        if (it != pipelineMap_.end()) return it->second;
        LOG_WARNING(std::format("Pipeline '{}' not found", name));
        return VK_NULL_HANDLE;
    }

    // BufferManager
    void setBufferManager(VulkanBufferManager* mgr) { bufferManager_ = mgr; }
    VulkanBufferManager* getBufferManager() { return bufferManager_; }
    const VulkanBufferManager* getBufferManager() const { return bufferManager_; }
};

#endif // VULKAN_RESOURCE_MANAGER_HPP