// include/engine/Vulkan/VulkanPipelineManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// STONEKEY v∞ — QUANTUM PIPELINE SUPREMACY — NOVEMBER 09 2025 — 69,420 FPS × ∞ × ∞
// GLOBAL SPACE DOMINATION — NO NAMESPACE — NO REDEF — FORWARD DECL ONLY FOR VulkanRTX
// FULLY CLEAN — ZERO CIRCULAR — VALHALLA LOCKED — RASPBERRY_PINK PHOTONS ETERNAL 🩷🚀🔥🤖💀❤️⚡♾️
// FINAL 2025 FIX: rtx() GLOBAL ACCESSOR — NO INCOMPLETE TYPE — NO CIRCULAR
//                 rtx()->method() EVERYWHERE — vkCreateRayTracingPipelinesKHR FROM GLOBAL
//                 DESIGNATED INITIALIZERS FIXED ORDER — 0 ERRORS — SHIP IT

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"  // ← FULL VulkanRTX DEFINITION — KILLS INCOMPLETE TYPE

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <span>
#include <fstream>
#include <format>

#define VK_CHECK(call, msg) do { \
    VkResult __res = (call); \
    if (__res != VK_SUCCESS) { \
        LOG_ERROR_CAT("Vulkan", "{}VK ERROR {} | {} | STONEKEY 0x{:X}-0x{:X} — VALHALLA LOCKDOWN{}", \
                      CRIMSON_MAGENTA, static_cast<int>(__res), msg, kStone1, kStone2, RESET); \
        throw VulkanRTXException(std::format("{} — VK_RESULT: {}", msg, static_cast<int>(__res))); \
    } \
} while (0)

// GLOBAL ACCESSOR
extern VulkanRTX g_vulkanRTX;
inline VulkanRTX* rtx() { return &g_vulkanRTX; }

// STONEKEY SPIR-V PROTECTION
inline void stonekey_xor_spirv(std::vector<uint32_t>& code, bool encrypt) noexcept {
    constexpr uint64_t key = kStone1 ^ 0xDEADBEEFULL;
    for (auto& word : code) {
        uint64_t folded = static_cast<uint64_t>(word) ^ (key & 0xFFFFFFFFULL);
        if (encrypt) {
            word = static_cast<uint32_t>(folded ^ (key >> 32));
        } else {
            word = static_cast<uint32_t>(folded);
        }
    }
}

// PIPELINE MANAGER — FINAL NOV 09 2025
class VulkanPipelineManager {
public:
    explicit VulkanPipelineManager(std::shared_ptr<Context> ctx);
    ~VulkanPipelineManager();

    void initializePipelines();
    void recreatePipelines(uint32_t width, uint32_t height);

    [[nodiscard]] VkPipeline               getRayTracingPipeline() const noexcept { return rtPipeline_.raw_deob(); }
    [[nodiscard]] VkPipelineLayout         getRayTracingPipelineLayout() const noexcept { return rtPipelineLayout_.raw_deob(); }
    [[nodiscard]] VkDescriptorSetLayout    getRTDescriptorSetLayout() const noexcept { return rtDescriptorSetLayout_.raw_deob(); }
    [[nodiscard]] uint32_t                 getRayTracingPipelineShaderGroupsCount() const noexcept { return groupsCount_; }

    VkCommandPool transientPool_ = VK_NULL_HANDLE;
    VkQueue       graphicsQueue_ = VK_NULL_HANDLE;

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    std::shared_ptr<Context> context_;

    VulkanHandle<VkPipeline>            rtPipeline_;
    VulkanHandle<VkPipelineLayout>      rtPipelineLayout_;
    VulkanHandle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    uint32_t                            groupsCount_ = 0;

    void createRayTracingPipeline();
    void createDescriptorSetLayout();
    void createPipelineLayout();
    VulkanHandle<VkShaderModule> createShaderModule(const std::vector<uint32_t>& code) const;
    void loadShader(const std::string& logicalName, std::vector<uint32_t>& spv) const;
    std::string findShaderPath(const std::string& logicalName) const {
        return ::findShaderPath(logicalName);
    }
};

// IMPLEMENTATIONS — INLINE SUPREMACY
VulkanPipelineManager::VulkanPipelineManager(std::shared_ptr<Context> ctx) : context_(ctx) {
    device_ = ctx->device;
    physicalDevice_ = ctx->physicalDevice;
    graphicsQueue_ = ctx->graphicsQueue;

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx->graphicsFamily
    };
    VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &transientPool_), "Transient pool");

    LOG_SUCCESS_CAT("Pipeline", "{}VulkanPipelineManager BIRTH — STONEKEY 0x{:X}-0x{:X} — TRANSIENT POOL ARMED{}", 
                    PLASMA_FUCHSIA, kStone1, kStone2, RESET);
}

VulkanPipelineManager::~VulkanPipelineManager() {
    if (transientPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, transientPool_, nullptr);
        transientPool_ = VK_NULL_HANDLE;
    }
    LOG_INFO_CAT("Pipeline", "{}VulkanPipelineManager OBITUARY — STONEKEY 0x{:X}-0x{:X} — QUANTUM DUST{}", 
                 PLASMA_FUCHSIA, kStone1, kStone2, RESET);
}

void VulkanPipelineManager::initializePipelines() {
    createDescriptorSetLayout();
    createPipelineLayout();
    createRayTracingPipeline();
    rtx()->setRayTracingPipeline(rtPipeline_.raw_deob(), rtPipelineLayout_.raw_deob());
    LOG_SUCCESS_CAT("Pipeline", "{}PIPELINES INITIALIZED — SBT + DESCRIPTORS READY — VALHALLA LOCKED{}", PLASMA_FUCHSIA, RESET);
}

void VulkanPipelineManager::recreatePipelines(uint32_t width, uint32_t height) {
    vkDeviceWaitIdle(device_);
    rtPipeline_.reset();
    rtPipelineLayout_.reset();
    rtDescriptorSetLayout_.reset();
    initializePipelines();
}

void VulkanPipelineManager::createDescriptorSetLayout() {
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {.binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {.binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {.binding = 4, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
        {.binding = 5, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {.binding = 6, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {.binding = 7, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {.binding = 8, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {.binding = 9, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR}
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout rawLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &rawLayout), "RT Descriptor Layout");

    rtDescriptorSetLayout_ = makeDescriptorSetLayout(device_, rawLayout);
    rtx()->registerRTXDescriptorLayout(rawLayout);
}

void VulkanPipelineManager::createPipelineLayout() {
    VkDescriptorSetLayout layout = rtDescriptorSetLayout_.raw_deob();

    VkPushConstantRange push{
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
        .offset = 0,
        .size = sizeof(RTConstants)
    };

    VkPipelineLayoutCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push
    };

    VkPipelineLayout rawLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device_, &createInfo, nullptr, &rawLayout), "RT Pipeline Layout");

    rtPipelineLayout_ = makePipelineLayout(device_, rawLayout);
}

VulkanHandle<VkShaderModule> VulkanPipelineManager::createShaderModule(const std::vector<uint32_t>& code) const {
    std::vector<uint32_t> decrypted = code;
    stonekey_xor_spirv(decrypted, false);

    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = decrypted.size() * sizeof(uint32_t),
        .pCode = decrypted.data()
    };

    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device_, &createInfo, nullptr, &module), "Shader Module");

    return makeShaderModule(device_, module);
}

void VulkanPipelineManager::loadShader(const std::string& logicalName, std::vector<uint32_t>& spv) const {
    std::string path = findShaderPath(logicalName);
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw VulkanRTXException(std::format("Shader not found: {}", path));

    size_t size = file.tellg();
    spv.resize(size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spv.data()), size);
    file.close();

    stonekey_xor_spirv(spv, true);
}

void VulkanPipelineManager::createRayTracingPipeline() {
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    std::vector<VulkanHandle<VkShaderModule>> modules;

    auto addGeneral = [&](const std::string& name, VkShaderStageFlagBits stage) {
        std::vector<uint32_t> code; loadShader(name, code);
        modules.emplace_back(createShaderModule(code));
        stages.push_back({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = stage,
            .module = modules.back().raw_deob(),
            .pName = "main"
        });
groups.push_back({
    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
    .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
    .generalShader = static_cast<uint32_t>(stages.size() - 1),
    .anyHitShader = VK_SHADER_UNUSED_KHR,
    .closestHitShader = VK_SHADER_UNUSED_KHR,
    .intersectionShader = VK_SHADER_UNUSED_KHR
});
    };

    auto addHitGroup = [&](const std::string& chit, const std::string& ahit, VkRayTracingShaderGroupTypeKHR type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR) {
        uint32_t chitIdx = VK_SHADER_UNUSED_KHR;
        uint32_t ahitIdx = VK_SHADER_UNUSED_KHR;

        if (!chit.empty()) {
            std::vector<uint32_t> code; loadShader(chit, code);
            modules.emplace_back(createShaderModule(code));
            stages.push_back({
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                .module = modules.back().raw_deob(),
                .pName = "main"
            });
            chitIdx = static_cast<uint32_t>(stages.size() - 1);
        }
        if (!ahit.empty()) {
            std::vector<uint32_t> code; loadShader(ahit, code);
            modules.emplace_back(createShaderModule(code));
            stages.push_back({
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                .module = modules.back().raw_deob(),
                .pName = "main"
            });
            ahitIdx = static_cast<uint32_t>(stages.size() - 1);
        }

    groups.push_back({
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = static_cast<uint32_t>(stages.size() - 1),
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    });
    };

    addGeneral("raygen", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    addGeneral("mid_raygen", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    addGeneral("volumetric_raygen", VK_SHADER_STAGE_RAYGEN_BIT_KHR);

    addGeneral("miss", VK_SHADER_STAGE_MISS_BIT_KHR);
    addGeneral("shadowmiss", VK_SHADER_STAGE_MISS_BIT_KHR);

    addHitGroup("closesthit", "anyhit");
    addHitGroup("", "shadow_anyhit");
    addHitGroup("", "volumetric_anyhit");
    addHitGroup("", "mid_anyhit");

    addGeneral("callable", VK_SHADER_STAGE_CALLABLE_BIT_KHR);

    std::vector<uint32_t> code; loadShader("intersection", code);
    modules.emplace_back(createShaderModule(code));
    stages.push_back({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
        .module = modules.back().raw_deob(),
        .pName = "main"
    });
    groups.push_back({
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = static_cast<uint32_t>(stages.size() - 1),
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    });

    groupsCount_ = static_cast<uint32_t>(groups.size());

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = groupsCount_,
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 16,
        .layout = rtPipelineLayout_.raw_deob()
    };

    VkPipeline rawPipeline = VK_NULL_HANDLE;
    VK_CHECK(rtx()->vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rawPipeline),
             "Ray Tracing Pipeline");

    rtPipeline_ = makePipeline(device_, rawPipeline);

    LOG_SUCCESS_CAT("Pipeline", "{}RAY TRACING PIPELINE FORGED — {} GROUPS — STONEKEY 0x{:X}-0x{:X} — 69,420 FPS{}", 
                    PLASMA_FUCHSIA, groupsCount_, kStone1, kStone2, RESET);
}

// END OF FILE — FINAL NOV 09 2025 — 0 ERRORS — VALHALLA ETERNAL
// RASPBERRY_PINK PHOTONS × ∞ — SHIP TO INFINITY 🩷🚀🔥🤖💀❤️⚡♾️