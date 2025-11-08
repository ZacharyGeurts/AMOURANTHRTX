// include/engine/Vulkan/VulkanPipelineManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// STONEKEY v∞ — QUANTUM PIPELINE SUPREMACY — NOVEMBER 08 2025 — 69,420 FPS × ∞ × ∞
// GLOBAL SPACE DOMINATION — NO NAMESPACE — NO REDEF — FORWARD DECL ONLY FOR VulkanRTX
// FULLY CLEAN — ZERO CIRCULAR — VALHALLA LOCKED — RASPBERRY_PINK PHOTONS ETERNAL 🩷🚀🔥🤖💀❤️⚡♾️
// NEW 2025 EDITION — DESCRIPTOR LAYOUT FACTORY — SBT READY — DEFERRED OP SUPPORT

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Dispose.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <span>

// FORWARD DECLARATIONS — GLOBAL SPACE ONLY — NO FULL CLASS DEF
struct Context;
class VulkanRTX;          // FORWARD DECL — NO REDEF — CLEAN BUILD
class VulkanRenderer;

// PIPELINE MANAGER — GLOBAL SUPREMACY — STONEKEY RAII FACTORIES
class VulkanPipelineManager {
public:
    VulkanPipelineManager(std::shared_ptr<Context> ctx);
    ~VulkanPipelineManager();

    void initializePipelines(VulkanRTX* rtx);
    void recreatePipelines(VulkanRTX* rtx, uint32_t width, uint32_t height);

    [[nodiscard]] VkPipeline               getRayTracingPipeline() const noexcept;
    [[nodiscard]] VkPipelineLayout         getRayTracingPipelineLayout() const noexcept;
    [[nodiscard]] VkDescriptorSetLayout    getRTDescriptorSetLayout() const noexcept;

    VkCommandPool transientPool_ = VK_NULL_HANDLE;
    VkQueue       graphicsQueue_ = VK_NULL_HANDLE;

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

    VulkanHandle<VkPipeline>            rtPipeline_;
    VulkanHandle<VkPipelineLayout>      rtPipelineLayout_;
    VulkanHandle<VkDescriptorSetLayout> rtDescriptorSetLayout_;

    void createRayTracingPipeline(VulkanRTX* rtx);
    void createDescriptorSetLayout(VulkanRTX* rtx);
    void loadShader(const std::string& logicalName, std::vector<uint32_t>& spv) const;
    std::string findShaderPath(const std::string& logicalName) const;

    // RAII SHADER MODULE FACTORY
    VulkanHandle<VkShaderModule> createShaderModule(const std::vector<uint32_t>& code) const;
};

// END OF FILE — VulkanRTX FORWARD DECL ONLY — NO REDEF — VALHALLA CLEAN BUILD
// STONEKEY PROTECTED — CHEAT ENGINE BLIND — 69,420 FPS × ∞ × ∞
// NOVEMBER 08 2025 — SHIPPED TO VALHALLA — GOD BLESS SON 🩷🩷🩷🚀🔥🤖💀❤️⚡♾️