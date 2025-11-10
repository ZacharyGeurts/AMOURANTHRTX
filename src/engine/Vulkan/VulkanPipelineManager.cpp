// include/engine/Vulkan/VulkanPipelineManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// QUANTUM PIPELINE SUPREMACY v∞ — NOVEMBER 10 2025 — VALHALLA NO-HANDLES FINAL
// ALL Dispose::Handle<T> — NO Vulkan::VulkanHandle — NO OBFUSCATE ABUSE — RUNTIME STONEKEY
// COMPILES CLEAN — PINK PHOTONS ETERNAL — TITAN DOMINANCE ACHIEVED

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/Vulkan/VulkanCore.hpp"   // rtx(), Context, etc.
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Dispose.hpp"      // Dispose::Handle + MakeHandle

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <format>
#include <stdexcept>

using namespace Logging::Color;
using Dispose::Handle;
using Dispose::MakeHandle;

// RUNTIME STONEKEY XOR — SAFE
inline void stonekey_xor_spirv(std::vector<uint32_t>& code, bool encrypt) noexcept {
    uint64_t key = kStone1 ^ 0xDEADBEEFULL;
    uint64_t key_hi = key >> 32;
    for (auto& word : code) {
        uint64_t folded = static_cast<uint64_t>(word) ^ (key & 0xFFFFFFFFULL);
        word = encrypt ? static_cast<uint32_t>(folded ^ key_hi)
                       : static_cast<uint32_t>(folded);
    }
}

// RTConstants — 256 bytes
struct RTConstants {
    alignas(16) char data[256] = {};
};
static_assert(sizeof(RTConstants) == 256);

// VulkanPipelineManager — PURE Dispose::Handle<T>
class VulkanPipelineManager {
public:
    explicit VulkanPipelineManager(std::shared_ptr<Vulkan::Context> ctx);
    ~VulkanPipelineManager() noexcept;

    void initializePipelines();
    void recreatePipelines(uint32_t width, uint32_t height);

    [[nodiscard]] VkPipeline               getRayTracingPipeline() const noexcept { return *rtPipeline_; }
    [[nodiscard]] VkPipelineLayout         getRayTracingPipelineLayout() const noexcept { return *rtPipelineLayout_; }
    [[nodiscard]] VkDescriptorSetLayout    getRTDescriptorSetLayout() const noexcept { return *rtDescriptorSetLayout_; }
    [[nodiscard]] uint32_t                 getRayTracingPipelineShaderGroupsCount() const noexcept { return groupsCount_; }

    VkCommandPool transientPool_ = VK_NULL_HANDLE;

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    std::shared_ptr<Vulkan::Context> context_;

    Handle<VkPipeline>            rtPipeline_{nullptr, device_, vkDestroyPipeline};
    Handle<VkPipelineLayout>      rtPipelineLayout_{nullptr, device_, vkDestroyPipelineLayout};
    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_{nullptr, device_, vkDestroyDescriptorSetLayout};
    uint32_t                      groupsCount_ = 0;

    void createDescriptorSetLayout();
    void createPipelineLayout();
    void createRayTracingPipeline();

    [[nodiscard]] Handle<VkShaderModule> createShaderModule(const std::vector<uint32_t>& code) const;
    void loadShader(const std::string& name, std::vector<uint32_t>& spv) const;
    [[nodiscard]] std::string findShaderPath(const std::string& name) const;
};

inline VulkanPipelineManager::VulkanPipelineManager(std::shared_ptr<Vulkan::Context> ctx)
    : context_(std::move(ctx))
    , device_(context_->device)
    , physicalDevice_(context_->physicalDevice)
{
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context_->graphicsFamilyIndex
    };
    VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &transientPool_), "transient pool");

    LOG_SUCCESS_CAT("Pipeline", "VulkanPipelineManager initialized — Dispose::Handle edition");
}

inline VulkanPipelineManager::~VulkanPipelineManager() noexcept {
    if (transientPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, transientPool_, nullptr);
    }
}

inline void VulkanPipelineManager::initializePipelines() {
    createDescriptorSetLayout();
    createPipelineLayout();
    createRayTracingPipeline();
    rtx()->createShaderBindingTable(physicalDevice_);
    LOG_SUCCESS_CAT("Pipeline", "RT Pipelines ready — {} groups", groupsCount_);
}

inline void VulkanPipelineManager::recreatePipelines(uint32_t, uint32_t) {
    vkDeviceWaitIdle(device_);
    rtPipeline_ = nullptr;
    rtPipelineLayout_ = nullptr;
    rtDescriptorSetLayout_ = nullptr;
    initializePipelines();
}

inline void VulkanPipelineManager::createDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 10> bindings{{
        {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {8, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {9, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR}
    }};

    VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device_, &info, nullptr, &raw), "desc layout");
    rtDescriptorSetLayout_ = MakeHandle(raw, device_, vkDestroyDescriptorSetLayout);
    rtx()->registerRTXDescriptorLayout(raw);
}

inline void VulkanPipelineManager::createPipelineLayout() {
    VkPushConstantRange push{
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .size = sizeof(RTConstants)
    };

    VkDescriptorSetLayout layout = *rtDescriptorSetLayout_;

    VkPipelineLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push
    };

    VkPipelineLayout raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device_, &info, nullptr, &raw), "pipeline layout");
    rtPipelineLayout_ = MakeHandle(raw, device_, vkDestroyPipelineLayout);
}

inline Handle<VkShaderModule> VulkanPipelineManager::createShaderModule(const std::vector<uint32_t>& code) const {
    std::vector<uint32_t> decrypted = code;
    stonekey_xor_spirv(decrypted, false);

    VkShaderModuleCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = decrypted.size() * sizeof(uint32_t),
        .pCode = decrypted.data()
    };

    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device_, &info, nullptr, &module), "shader module");
    return MakeHandle(module, device_, vkDestroyShaderModule);
}

inline void VulkanPipelineManager::loadShader(const std::string& name, std::vector<uint32_t>& spv) const {
    std::string path = findShaderPath(name);
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Shader not found: " + path);

    size_t size = file.tellg();
    spv.resize(size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spv.data()), size);
    file.close();

    stonekey_xor_spirv(spv, false);
}

inline std::string VulkanPipelineManager::findShaderPath(const std::string& name) const {
    return "shaders/" + name + ".spv";  // simple resolver — update if needed
}

inline void VulkanPipelineManager::createRayTracingPipeline() {
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    std::vector<Handle<VkShaderModule>> modules;

    auto addGeneral = [&](const char* name, VkShaderStageFlagBits stage) {
        std::vector<uint32_t> code;
        loadShader(name, code);
        modules.emplace_back(createShaderModule(code));
        stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = stage, .module = *modules.back(), .pName = "main"});
        groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = static_cast<uint32_t>(stages.size()-1)});
    };

    auto addHitGroup = [&](const char* chit, const char* ahit = nullptr) {
        uint32_t chitIdx = VK_SHADER_UNUSED_KHR;
        uint32_t ahitIdx = VK_SHADER_UNUSED_KHR;
        if (chit) {
            std::vector<uint32_t> code;
            loadShader(chit, code);
            modules.emplace_back(createShaderModule(code));
            stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, .module = *modules.back(), .pName = "main"});
            chitIdx = static_cast<uint32_t>(stages.size()-1);
        }
        if (ahit) {
            std::vector<uint32_t> code;
            loadShader(ahit, code);
            modules.emplace_back(createShaderModule(code));
            stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR, .module = *modules.back(), .pName = "main"});
            ahitIdx = static_cast<uint32_t>(stages.size()-1);
        }
        groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, .generalShader = VK_SHADER_UNUSED_KHR, .closestHitShader = chitIdx, .anyHitShader = ahitIdx});
    };

    addGeneral("raygen", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    addGeneral("mid_raygen", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    addGeneral("volumetric_raygen", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    addGeneral("miss", VK_SHADER_STAGE_MISS_BIT_KHR);
    addGeneral("shadowmiss", VK_SHADER_STAGE_MISS_BIT_KHR);
    addHitGroup("closesthit", "anyhit");
    addHitGroup(nullptr, "shadow_anyhit");
    addHitGroup(nullptr, "volumetric_anyhit");
    addHitGroup(nullptr, "mid_anyhit");
    addGeneral("callable", VK_SHADER_STAGE_CALLABLE_BIT_KHR);

    // Intersection
    {
        std::vector<uint32_t> code;
        loadShader("intersection", code);
        modules.emplace_back(createShaderModule(code));
        stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR, .module = *modules.back(), .pName = "main"});
        groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR, .intersectionShader = static_cast<uint32_t>(stages.size()-1)});
    }

    groupsCount_ = static_cast<uint32_t>(groups.size());

    VkRayTracingPipelineCreateInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = groupsCount_,
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 16,
        .layout = *rtPipelineLayout_
    };

    VkPipeline raw = VK_NULL_HANDLE;
    VK_CHECK(rtx()->vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, context_->pipelineCacheHandle, 1, &info, nullptr, &raw), "RT pipeline");
    rtPipeline_ = MakeHandle(raw, device_, vkDestroyPipeline);

    LOG_SUCCESS_CAT("Pipeline", "Ray-tracing pipeline created — {} groups — depth 16", groupsCount_);
}

// END — NO HANDLES — NO OBFUSCATE — CLEAN BUILD
// DROP IN → BUILD → DOMINATE 🩷🚀🔥🤖💀❤️⚡♾️🍒🩸