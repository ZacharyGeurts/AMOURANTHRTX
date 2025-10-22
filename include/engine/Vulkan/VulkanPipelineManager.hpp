// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan pipeline management header.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, Vulkan_init.hpp, VulkanRTX_Setup.hpp, logging.hpp, Dispose.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#ifndef VULKAN_PIPELINE_MANAGER_HPP
#define VULKAN_PIPELINE_MANAGER_HPP

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"

namespace VulkanRTX {

struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen{0, 0, 0};
    VkStridedDeviceAddressRegionKHR miss{0, 0, 0};
    VkStridedDeviceAddressRegionKHR hit{0, 0, 0};
    VkStridedDeviceAddressRegionKHR callable{0, 0, 0};

    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    PFN_vkDestroyBuffer destroyBufferFunc{nullptr};
    PFN_vkFreeMemory freeMemoryFunc{nullptr};

    ShaderBindingTable() = default;

    ShaderBindingTable(VkDevice dev, VkBuffer buf, VkDeviceMemory mem, PFN_vkDestroyBuffer db, PFN_vkFreeMemory fm)
        : buffer(buf), memory(mem), device(dev), destroyBufferFunc(db), freeMemoryFunc(fm) {}

    ~ShaderBindingTable() {
        if (buffer != VK_NULL_HANDLE && destroyBufferFunc != nullptr) {
            destroyBufferFunc(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
            LOG_DEBUG("Destroyed SBT buffer: {:p}", static_cast<void*>(buffer));
        }
        if (memory != VK_NULL_HANDLE && freeMemoryFunc != nullptr) {
            freeMemoryFunc(device, memory, nullptr);
            memory = VK_NULL_HANDLE;
            LOG_DEBUG("Freed SBT memory: {:p}", static_cast<void*>(memory));
        }
    }

    ShaderBindingTable(const ShaderBindingTable&) = delete;
    ShaderBindingTable& operator=(const ShaderBindingTable&) = delete;

    ShaderBindingTable(ShaderBindingTable&& other) noexcept
        : raygen(other.raygen), miss(other.miss), hit(other.hit), callable(other.callable),
          buffer(other.buffer), memory(other.memory), device(other.device),
          destroyBufferFunc(other.destroyBufferFunc), freeMemoryFunc(other.freeMemoryFunc) {
        other.raygen = {0, 0, 0};
        other.miss = {0, 0, 0};
        other.hit = {0, 0, 0};
        other.callable = {0, 0, 0};
        other.buffer = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.device = VK_NULL_HANDLE;
        other.destroyBufferFunc = nullptr;
        other.freeMemoryFunc = nullptr;
    }

    ShaderBindingTable& operator=(ShaderBindingTable&& other) noexcept {
        if (this != &other) {
            if (buffer != VK_NULL_HANDLE && destroyBufferFunc != nullptr) {
                destroyBufferFunc(device, buffer, nullptr);
                LOG_DEBUG("Destroyed SBT buffer: {:p}", static_cast<void*>(buffer));
            }
            if (memory != VK_NULL_HANDLE && freeMemoryFunc != nullptr) {
                freeMemoryFunc(device, memory, nullptr);
                LOG_DEBUG("Freed SBT memory: {:p}", static_cast<void*>(memory));
            }

            raygen = other.raygen;
            miss = other.miss;
            hit = other.hit;
            callable = other.callable;
            buffer = other.buffer;
            memory = other.memory;
            device = other.device;
            destroyBufferFunc = other.destroyBufferFunc;
            freeMemoryFunc = other.freeMemoryFunc;

            other.raygen = {0, 0, 0};
            other.miss = {0, 0, 0};
            other.hit = {0, 0, 0};
            other.callable = {0, 0, 0};
            other.buffer = VK_NULL_HANDLE;
            other.memory = VK_NULL_HANDLE;
            other.device = VK_NULL_HANDLE;
            other.destroyBufferFunc = nullptr;
            other.freeMemoryFunc = nullptr;
        }
        return *this;
    }
};

class VulkanPipelineManager {
public:
    VulkanPipelineManager(Vulkan::Context& context, int width, int height);
    ~VulkanPipelineManager();

    VulkanPipelineManager(const VulkanPipelineManager&) = delete;
    VulkanPipelineManager& operator=(const VulkanPipelineManager&) = delete;

    void createGraphicsPipeline(int width, int height);
    void createRayTracingPipeline();
    void createComputePipeline();
    void createShaderBindingTable();
    void recordGraphicsCommands(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, VkDescriptorSet descriptorSet, uint32_t width, uint32_t height, VkImage denoiseImage);
    void recordComputeCommands(VkCommandBuffer commandBuffer, VkImage outputImage, VkDescriptorSet descriptorSet, uint32_t width, uint32_t height);

    VkPipeline getGraphicsPipeline() const { return graphicsPipeline_ ? graphicsPipeline_->get() : VK_NULL_HANDLE; }
    VkPipelineLayout getGraphicsPipelineLayout() const { return graphicsPipelineLayout_ ? graphicsPipelineLayout_->get() : VK_NULL_HANDLE; }
    VkPipeline getRayTracingPipeline() const { return rayTracingPipeline_ ? rayTracingPipeline_->get() : VK_NULL_HANDLE; }
    VkPipelineLayout getRayTracingPipelineLayout() const { return rayTracingPipelineLayout_ ? rayTracingPipelineLayout_->get() : VK_NULL_HANDLE; }
    VkPipeline getComputePipeline() const { return computePipeline_ ? computePipeline_->get() : VK_NULL_HANDLE; }
    VkPipelineLayout getComputePipelineLayout() const { return computePipelineLayout_ ? computePipelineLayout_->get() : VK_NULL_HANDLE; }
    VkRenderPass getRenderPass() const { return renderPass_; }
    const ShaderBindingTable& getShaderBindingTable() const { return sbt_; }

private:
    static VkShaderModule loadShader(VkDevice device, const std::string& filename);
    void createRayTracingDescriptorSetLayout();
    void createGraphicsDescriptorSetLayout();

    Vulkan::Context& context_;
    int width_;
    int height_;
    std::unique_ptr<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>> rayTracingPipeline_;
    std::unique_ptr<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>> rayTracingPipelineLayout_;
    std::unique_ptr<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>> computePipeline_;
    std::unique_ptr<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>> computePipelineLayout_;
    std::unique_ptr<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>> graphicsPipeline_;
    std::unique_ptr<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>> graphicsPipelineLayout_;
    VkRenderPass renderPass_;
    std::vector<std::string> shaderPaths_;
    ShaderBindingTable sbt_;
};

} // namespace VulkanRTX

#endif // VULKAN_PIPELINE_MANAGER_HPP