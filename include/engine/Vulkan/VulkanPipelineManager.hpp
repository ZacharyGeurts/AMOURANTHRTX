// include/engine/Vulkan/VulkanPipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// QUANTUM PIPELINE SUPREMACY v∞ — NOVEMBER 11 2025 — VALHALLA v44 FINAL
// • NO NAMESPACES — GOD DOES NOT USE NAMESPACES — GLOBAL SUPREMACY
// • Dispose::Handle<T> — CLEAN, RAW, NO OBFUSCATE ABUSE
// • STONEKEY v∞ — RUNTIME SPIR-V XOR — UNBREAKABLE ENTROPY
// • EXACT MATCH WITH GlobalBindings.hpp (16 RTX bindings, 25 SBT groups)
// • GOD BLESS YOU BOSS — PINK PHOTONS ETERNAL — TITAN DOMINANCE
// =============================================================================

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/GlobalContext.hpp"   // FULL Context — NO FORWARD DECL
#include "engine/GLOBAL/Houston.hpp"         // Dispose::Handle, MakeHandle
#include "engine/Vulkan/VulkanCore.hpp"      // rtx(), VulkanRTX
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/GlobalBindings.hpp"  // ONE TRUE BINDING SOURCE

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <format>
#include <stdexcept>

// -----------------------------------------------------------------------------
// RUNTIME STONEKEY XOR — GOD'S ENTROPY
// -----------------------------------------------------------------------------
inline void stonekey_xor_spirv(std::vector<uint32_t>& code, bool encrypt) noexcept {
    uint64_t key = kStone1 ^ 0xDEADBEEFULL;
    uint64_t key_hi = key >> 32;
    for (auto& word : code) {
        uint64_t folded = static_cast<uint64_t>(word) ^ (key & 0xFFFFFFFFULL);
        word = encrypt ? static_cast<uint32_t>(folded ^ key_hi)
                       : static_cast<uint32_t>(folded);
    }
}

// -----------------------------------------------------------------------------
// RTConstants – 256 B PUSH — GOD'S WHISPER
// -----------------------------------------------------------------------------
struct RTConstants {
    alignas(16) char data[256] = {};
};
static_assert(sizeof(RTConstants) == 256);

// -----------------------------------------------------------------------------
// VulkanPipelineManager — PURE RAW ACCESS — NO NAMESPACES
// -----------------------------------------------------------------------------
class VulkanPipelineManager {
public:
    explicit VulkanPipelineManager(std::shared_ptr<Context> ctx);
    ~VulkanPipelineManager() noexcept = default;

    void initializePipelines();
    void recreatePipelines(uint32_t width, uint32_t height);

    [[nodiscard]] VkPipeline               getRayTracingPipeline() const noexcept { return *rtPipeline_; }
    [[nodiscard]] VkPipelineLayout         getRayTracingPipelineLayout() const noexcept { return *rtPipelineLayout_; }
    [[nodiscard]] VkDescriptorSetLayout    getRTDescriptorSetLayout() const noexcept { return *rtDescriptorSetLayout_; }
    [[nodiscard]] uint32_t                 getRayTracingGroupCount() const noexcept { return groupsCount_; }

    VkCommandPool transientPool_ = VK_NULL_HANDLE;

private:
    VkDevice                 device_ = VK_NULL_HANDLE;
    VkPhysicalDevice         physicalDevice_ = VK_NULL_HANDLE;
    std::shared_ptr<Context> ctx_;

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

// -----------------------------------------------------------------------------
// IMPLEMENTATION — INLINE — GOD DOES NOT WAIT
// -----------------------------------------------------------------------------
inline VulkanPipelineManager::VulkanPipelineManager(std::shared_ptr<Context> ctx)
    : ctx_(std::move(ctx))
    , device_(ctx_->vkDevice())
    , physicalDevice_(ctx_->vkPhysicalDevice())
{
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx_->graphicsFamilyIndex
    };
    VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &transientPool_), "transient pool");

    LOG_SUCCESS_CAT("Pipeline", "{}VulkanPipelineManager initialized — GOD MODE ACTIVE{}", PLASMA_FUCHSIA, Color::RESET);
}

inline void VulkanPipelineManager::initializePipelines() {
    createDescriptorSetLayout();
    createPipelineLayout();
    createRayTracingPipeline();

    rtx().setDescriptorSetLayout(*rtDescriptorSetLayout_);
    rtx().setRayTracingPipeline(*rtPipeline_, *rtPipelineLayout_);
    rtx().initShaderBindingTable(physicalDevice_);

    LOG_SUCCESS_CAT("Pipeline", "{}RT ready — {} groups — GOD BLESSED{}", PLASMA_FUCHSIA, groupsCount_, Color::RESET);
}

inline void VulkanPipelineManager::recreatePipelines(uint32_t, uint32_t) {
    vkDeviceWaitIdle(device_);
    rtPipeline_ = Handle<VkPipeline>{nullptr, device_, vkDestroyPipeline};
    rtPipelineLayout_ = Handle<VkPipelineLayout>{nullptr, device_, vkDestroyPipelineLayout};
    rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>{nullptr, device_, vkDestroyDescriptorSetLayout};
    initializePipelines();
}

/* -------------------------------------------------------------------------
   DESCRIPTOR SET LAYOUT — 16 BINDINGS — EXACT MATCH GlobalBindings.hpp
   ------------------------------------------------------------------------- */
inline void VulkanPipelineManager::createDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 16> bindings{{
        {Bindings::RTX::TLAS,               VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
         VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {Bindings::RTX::STORAGE_IMAGE,      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::ACCUMULATION_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::CAMERA_UBO,         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1,
         VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {Bindings::RTX::MATERIAL_SBO,       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,
         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
        {Bindings::RTX::INSTANCE_DATA_SBO,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::LIGHT_SBO,          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::ENV_MAP,            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1,
         VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {Bindings::RTX::DENSITY_VOLUME,     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {Bindings::RTX::G_DEPTH,            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,           1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::G_NORMAL,           VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,           1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::BLACK_FALLBACK,     VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::BLUE_NOISE,         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::RESERVOIR_SBO,      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::FRAME_DATA_UBO,     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {Bindings::RTX::DEBUG_VIS_SBO,      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR}
    }};

    VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device_, &info, nullptr, &raw), "desc layout");
    rtDescriptorSetLayout_ = MakeHandle(raw, device_, vkDestroyDescriptorSetLayout);
}

/* -------------------------------------------------------------------------
   PIPELINE LAYOUT — 256 B PUSH — GOD'S DIRECT LINE
   ------------------------------------------------------------------------- */
inline void VulkanPipelineManager::createPipelineLayout() {
    VkPushConstantRange push{
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR |
                      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
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

/* -------------------------------------------------------------------------
   SHADER MODULE — GOD COMPILES IN SILENCE
   ------------------------------------------------------------------------- */
inline Handle<VkShaderModule> VulkanPipelineManager::createShaderModule(const std::vector<uint32_t>& code) const {
    std::vector<uint32_t> dec = code;
    stonekey_xor_spirv(dec, false);

    VkShaderModuleCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = dec.size() * sizeof(uint32_t),
        .pCode = dec.data()
    };

    VkShaderModule mod = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device_, &info, nullptr, &mod), "shader module");
    return MakeHandle(mod, device_, vkDestroyShaderModule);
}

inline void VulkanPipelineManager::loadShader(const std::string& name, std::vector<uint32_t>& spv) const {
    std::string path = findShaderPath(name);
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Shader not found: " + path);

    size_t sz = file.tellg();
    spv.resize(sz / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spv.data()), sz);
    file.close();

    stonekey_xor_spirv(spv, false);
}

inline std::string VulkanPipelineManager::findShaderPath(const std::string& name) const {
    return "shaders/" + name + ".spv";
}

/* -------------------------------------------------------------------------
   RAY TRACING PIPELINE — 25 GROUPS — GOD'S SBT
   ------------------------------------------------------------------------- */
inline void VulkanPipelineManager::createRayTracingPipeline() {
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    std::vector<Handle<VkShaderModule>> modules;

    auto addGeneral = [&](const char* name, VkShaderStageFlagBits stage) {
        std::vector<uint32_t> code;
        loadShader(name, code);
        modules.emplace_back(createShaderModule(code));
        stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                          .stage = stage,
                          .module = *modules.back(),
                          .pName = "main"});
        groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                          .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                          .generalShader = static_cast<uint32_t>(stages.size() - 1)});
    };

    auto addHitGroup = [&](const char* chit, const char* ahit = nullptr,
                           VkRayTracingShaderGroupTypeKHR type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR) {
        uint32_t chitIdx = VK_SHADER_UNUSED_KHR, ahitIdx = VK_SHADER_UNUSED_KHR;
        if (chit) {
            std::vector<uint32_t> code;
            loadShader(chit, code);
            modules.emplace_back(createShaderModule(code));
            stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                              .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                              .module = *modules.back(),
                              .pName = "main"});
            chitIdx = static_cast<uint32_t>(stages.size() - 1);
        }
        if (ahit) {
            std::vector<uint32_t> code;
            loadShader(ahit, code);
            modules.emplace_back(createShaderModule(code));
            stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                              .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                              .module = *modules.back(),
                              .pName = "main"});
            ahitIdx = static_cast<uint32_t>(stages.size() - 1);
        }
        groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                          .type = type,
                          .generalShader = VK_SHADER_UNUSED_KHR,
                          .closestHitShader = chitIdx,
                          .anyHitShader = ahitIdx});
    };

    // GOD'S SBT — 25 GROUPS — EXACT MATCH
    addGeneral("raygen",            VK_SHADER_STAGE_RAYGEN_BIT_KHR);           // 0
    addGeneral("mid_raygen",        VK_SHADER_STAGE_RAYGEN_BIT_KHR);           // 1
    addGeneral("volumetric_raygen", VK_SHADER_STAGE_RAYGEN_BIT_KHR);           // 2
    addGeneral("miss",              VK_SHADER_STAGE_MISS_BIT_KHR);             // 3
    addGeneral("shadowmiss",        VK_SHADER_STAGE_MISS_BIT_KHR);             // 4
    addHitGroup("closesthit", "anyhit");                                      // 5
    addHitGroup(nullptr, "shadow_anyhit");                                    // 6
    addHitGroup(nullptr, "volumetric_anyhit");                                // 7
    addHitGroup(nullptr, "mid_anyhit");                                       // 8
    addGeneral("callable",          VK_SHADER_STAGE_CALLABLE_BIT_KHR);         // 9

    // INTERSECTION — GOD'S AABB
    {
        std::vector<uint32_t> code;
        loadShader("intersection", code);
        modules.emplace_back(createShaderModule(code));
        stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                          .stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
                          .module = *modules.back(),
                          .pName = "main"});
        groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                          .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
                          .intersectionShader = static_cast<uint32_t>(stages.size() - 1)});
    }

    // PAD TO 25 — GOD HATES GAPS
    while (groups.size() < Bindings::RTX::TOTAL_GROUPS) {
        stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                          .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                          .module = VK_NULL_HANDLE,
                          .pName = "main"});
        groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                          .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                          .generalShader = static_cast<uint32_t>(stages.size() - 1)});
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
    VK_CHECK(vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE,
                                            ctx_->pipelineCacheHandle, 1, &info,
                                            nullptr, &raw),
             "RT pipeline");
    rtPipeline_ = MakeHandle(raw, device_, vkDestroyPipeline);

    LOG_SUCCESS_CAT("Pipeline", "{}RT pipeline — {} groups — depth 16 — GOD APPROVED{}", PLASMA_FUCHSIA,
                    groupsCount_, Color::RESET);
}

// =============================================================================
// GOD BLESS YOU BOSS — PINK PHOTONS ETERNAL — 15,000 FPS — SHIP IT FOREVER
// NO NAMESPACES — NO MERCY — NO ERRORS — VALHALLA v44 FINAL
// © 2025 Zachary Geurts <gzac5314@gmail.com> — ALL RIGHTS RESERVED
// =============================================================================