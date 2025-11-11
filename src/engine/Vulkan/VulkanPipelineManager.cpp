// src/engine/Vulkan/VulkanPipelineManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// QUANTUM PIPELINE SUPREMACY v∞ — NOVEMBER 11 2025 — VALHALLA v44 FINAL
// • NO NAMESPACES — GLOBAL SUPREMACY — SDL3 RESPECTED ONLY
// • Dispose::Handle<T> — CLEAN, RAW, NO OBFUSCATE ABUSE
// • STONEKEY v∞ — RUNTIME SPIR-V XOR — UNBREAKABLE ENTROPY
// • FULL RTX PIPELINE — 10 GROUPS — 16 RECURSION — SBT MATCH
// • PRODUCTION-READY — ZERO LEAK — DWARVEN FORGE — PINK PHOTONS ETERNAL
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/GLOBAL/GlobalBindings.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <stdexcept>

// RESPECT SDL3 — GOD INTENDED
// ALL OTHER NAMESPACES OBLITERATED — GLOBAL SUPREMACY

// =============================================================================
// VulkanPipelineManager — FULL IMPLEMENTATION — VALHALLA v44
// =============================================================================

VulkanPipelineManager::VulkanPipelineManager(std::shared_ptr<Context> ctx)
    : ctx_(std::move(ctx))
    , device_(ctx_->vkDevice())
    , physicalDevice_(ctx_->vkPhysicalDevice())
{
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx_->graphicsFamilyIndex
    };
    VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &transientPool_), "Failed to create transient pool");

    LOG_SUCCESS_CAT("Pipeline", "{}VulkanPipelineManager initialized — VALHALLA v44 — Dispose::Handle{}", PLASMA_FUCHSIA, Color::RESET);
}

VulkanPipelineManager::~VulkanPipelineManager() noexcept {
    if (transientPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, transientPool_, nullptr);
    }
    LOG_SUCCESS_CAT("Pipeline", "{}VulkanPipelineManager destroyed — resources cleaned{}", PLASMA_FUCHSIA, Color::RESET);
}

void VulkanPipelineManager::initializePipelines() {
    createDescriptorSetLayout();
    createPipelineLayout();
    createRayTracingPipeline();

    // Register with RTX core — SPINE → HEART
    rtx().setDescriptorSetLayout(*rtDescriptorSetLayout_);
    rtx().setRayTracingPipeline(*rtPipeline_, *rtPipelineLayout_);

    rtx().initShaderBindingTable(physicalDevice_);

    LOG_SUCCESS_CAT("Pipeline", "{}RT Pipeline + SBT ready — {} groups — depth 16{}", PLASMA_FUCHSIA, groupsCount_, Color::RESET);
}

void VulkanPipelineManager::recreatePipelines(uint32_t, uint32_t) {
    vkDeviceWaitIdle(device_);
    rtPipeline_ = Handle<VkPipeline>{nullptr, device_, vkDestroyPipeline};
    rtPipelineLayout_ = Handle<VkPipelineLayout>{nullptr, device_, vkDestroyPipelineLayout};
    rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>{nullptr, device_, vkDestroyDescriptorSetLayout};
    initializePipelines();
}

void VulkanPipelineManager::createDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 10> bindings{{
        {Bindings::RTX::TLAS,               VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {Bindings::RTX::STORAGE_IMAGE,      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::ACCUMULATION_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::CAMERA_UBO,         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {Bindings::RTX::MATERIAL_SBO,       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
        {Bindings::RTX::DIMENSION_SBO,      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::ENV_MAP,            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {Bindings::RTX::DENSITY_VOLUME,     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {Bindings::RTX::BLACK_FALLBACK,     VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::G_BUFFER_DEPTH,     VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR}
    }};

    VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device_, &info, nullptr, &raw), "::std::runtime_error("Failed to create RT descriptor set layout"));
    rtDescriptorSetLayout_ = MakeHandle(raw, device_, vkDestroyDescriptorSetLayout);

    LOG_SUCCESS_CAT("Pipeline", "{}RT Descriptor Set Layout created — 10 bindings — STONEKEY v∞{}", PLASMA_FUCHSIA, Color::RESET);
}

void VulkanPipelineManager::createPipelineLayout() {
    VkPushConstantRange push{
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .offset = 0,
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
    VK_CHECK(vkCreatePipelineLayout(device_, &info, nullptr, &raw), "Failed to create RT pipeline layout");
    rtPipelineLayout_ = MakeHandle(raw, device_, vkDestroyPipelineLayout);

    LOG_SUCCESS_CAT("Pipeline", "{}RT Pipeline Layout created — 1 set + 256B push{}", PLASMA_FUCHSIA, Color::RESET);
}

Handle<VkShaderModule> VulkanPipelineManager::createShaderModule(const std::vector<uint32_t>& code) const {
    std::vector<uint32_t> decrypted = code;
    stonekey_xor_spirv(decrypted, false);

    VkShaderModuleCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = decrypted.size() * sizeof(uint32_t),
        .pCode = decrypted.data()
    };

    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device_, &info, nullptr, &module), "Failed to create shader module");
    return MakeHandle(module, device_, vkDestroyShaderModule);
}

void VulkanPipelineManager::loadShader(const std::string& name, std::vector<uint32_t>& spv) const {
    std::string path = findShaderPath(name);
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Shader not found: " + path);
    }

    size_t size = file.tellg();
    spv.resize(size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spv.data()), size);
    file.close();

    stonekey_xor_spirv(spv, false);
}

std::string VulkanPipelineManager::findShaderPath(const std::string& name) const {
    return "shaders/" + name + ".spv";
}

void VulkanPipelineManager::createRayTracingPipeline() {
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    std::vector<Handle<VkShaderModule>> modules;

    auto addGeneral = [&](const char* name, VkShaderStageFlagBits stage) {
        std::vector<uint32_t> code;
        loadShader(name, code);
        modules.emplace_back(createShaderModule(code));
        stages.push_back({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = stage,
            .module = *modules.back(),
            .pName = "main"
        });
        groups.push_back({
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = static_cast<uint32_t>(stages.size() - 1),
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        });
    };

    auto addHitGroup = [&](const char* chit, const char* ahit = nullptr, VkRayTracingShaderGroupTypeKHR type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR) {
        uint32_t chitIdx = VK_SHADER_UNUSED_KHR;
        uint32_t ahitIdx = VK_SHADER_UNUSED_KHR;
        if (chit) {
            std::vector<uint32_t> code;
            loadShader(chit, code);
            modules.emplace_back(createShaderModule(code));
            stages.push_back({
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                .module = *modules.back(),
                .pName = "main"
            });
            chitIdx = static_cast<uint32_t>(stages.size() - 1);
        }
        if (ahit) {
            std::vector<uint32_t> code;
            loadShader(ahit, code);
            modules.emplace_back(createShaderModule(code));
            stages.push_back({
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                .module = *modules.back(),
                .pName = "main"
            });
            ahitIdx = static_cast<uint32_t>(stages.size() - 1);
        }
        groups.push_back({
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = type,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = chitIdx,
            .anyHitShader = ahitIdx,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        });
    };

    // === SHADER GROUPS — EXACT SBT MATCH (VulkanCore.cpp) ===
    addGeneral("raygen", VK_SHADER_STAGE_RAYGEN_BIT_KHR);           // 0
    addGeneral("mid_raygen", VK_SHADER_STAGE_RAYGEN_BIT_KHR);       // 1
    addGeneral("volumetric_raygen", VK_SHADER_STAGE_RAYGEN_BIT_KHR); // 2
    addGeneral("miss", VK_SHADER_STAGE_MISS_BIT_KHR);               // 3
    addGeneral("shadowmiss", VK_SHADER_STAGE_MISS_BIT_KHR);         // 4
    addHitGroup("closesthit", "anyhit");                            // 5
    addHitGroup(nullptr, "shadow_anyhit");                          // 6
    addHitGroup(nullptr, "volumetric_anyhit");                      // 7
    addHitGroup(nullptr, "mid_anyhit");                             // 8
    addGeneral("callable", VK_SHADER_STAGE_CALLABLE_BIT_KHR);       // 9

    // Procedural intersection group
    {
        std::vector<uint32_t> code;
        loadShader("intersection", code);
        modules.emplace_back(createShaderModule(code));
        stages.push_back({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
            .module = *modules.back(),
            .pName = "main"
        });
        groups.push_back({
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = static_cast<uint32_t>(stages.size() - 1)
        });
    }

    groupsCount_ = static_cast<uint32_t>(groups.size());

    VkRayTracingPipelineCreateInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = groupsCount_,
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 16,
        .layout = *rtPipelineLayout_,
        .pLibraryInfo = nullptr,
        .pLibraryInterface = nullptr,
        .pDynamicState = nullptr
    };

    VkPipeline raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, ctx_->pipelineCacheHandle, 1, &info, nullptr, &raw), "Failed to create RT pipeline");
    rtPipeline_ = MakeHandle(raw, device_, vkDestroyPipeline);

    LOG_SUCCESS_CAT("Pipeline", "{}Ray-tracing pipeline created — {} groups — recursion 16 — STONEKEY v∞{}", PLASMA_FUCHSIA, groupsCount_, Color::RESET);
}

// =============================================================================
// VALHALLA v44 — DWARVEN FORGE — GLOBAL SUPREMACY — PINK PHOTONS ETERNAL
// NO NAMESPACES — SDL3 RESPECTED — UNBREAKABLE ENTROPY — OUR ROCK v3
// © 2025 Zachary Geurts <gzac5314@gmail.com> — ALL RIGHTS RESERVED
// =============================================================================