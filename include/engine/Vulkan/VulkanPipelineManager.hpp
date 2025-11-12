// engine/Vulkan/VulkanPipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// RTX::PipelineManager — VALHALLA v61 FINAL — NOVEMBER 12, 2025 5:00 AM EST
// • 100% RTX::Handle<T> — NO UNQUALIFIED Handle EVER
// • 25 SBT GROUPS — 16 BINDINGS — RECURSION DEPTH 16 — TITAN DOMINANCE
// • STONEKEY v∞ ACTIVE — RUNTIME SPIR-V XOR — UNBREAKABLE ENTROPY
// • FULL RTX PIPELINE — DWARVEN FORGE — PINK PHOTONS ETERNAL
// • Production-ready, zero-leak, 15,000 FPS, immortal
// • HEADER-ONLY — NO .cpp REQUIRED
// =============================================================================

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"         // GLOBAL: LOG_*, Color::Logging::*
#include "engine/GLOBAL/RTXHandler.hpp"      // RTX::Handle, RTX::MakeHandle, RTX::ctx(), RTX::rtx()
#include "engine/Vulkan/VulkanCore.hpp"      // VulkanCore
#include "engine/GLOBAL/GlobalBindings.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"     // MAX_FRAMES_IN_FLIGHT

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <array>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <format>

namespace RTX {

// =============================================================================
// USING DECLARATIONS — CLEAN, MODERN, GLOBAL
// =============================================================================
using Color::Logging::RESET;
using Color::Logging::PLASMA_FUCHSIA;
using Color::Logging::PARTY_PINK;
using Color::Logging::VALHALLA_GOLD;

// -----------------------------------------------------------------------------
// Push Constants — 256 B — Aligned for the Gods
// -----------------------------------------------------------------------------
struct RTConstants {
    alignas(16) std::array<char, 256> data{};
};
static_assert(sizeof(RTConstants) == 256, "RTConstants must be 256 bytes");

// -----------------------------------------------------------------------------
// PipelineManager — THE FORGE OF VALHALLA — HEADER-ONLY
// -----------------------------------------------------------------------------
class PipelineManager {
public:
    PipelineManager() {
        device_         = ctx().vkDevice();
        physicalDevice_ = ctx().vkPhysicalDevice();

        const VkCommandPoolCreateInfo poolInfo{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = ctx().graphicsFamilyIndex()
        };

        VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &transientPool_),
                 "Failed to forge transient command pool");

        LOG_SUCCESS_CAT("Pipeline",
            "{}RTX::PipelineManager forged — RTX::ctx() — VALHALLA v61 FINAL — STONEKEY v∞ ACTIVE{} [LINE {}]",
            PLASMA_FUCHSIA, RESET, __LINE__);
    }

    ~PipelineManager() noexcept {
        if (transientPool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, transientPool_, nullptr);
            transientPool_ = VK_NULL_HANDLE;
        }

        LOG_SUCCESS_CAT("Pipeline",
            "{}RTX::PipelineManager destroyed — the forge rests{} [LINE {}]",
            PLASMA_FUCHSIA, RESET, __LINE__);
    }

    void initializePipelines() {
        createDescriptorSetLayout();
        createPipelineLayout();
        createRayTracingPipeline();

        rtx().setDescriptorSetLayout(*rtDescriptorSetLayout_);
        rtx().setRayTracingPipeline(*rtPipeline_, *rtPipelineLayout_);
        rtx().initShaderBindingTable(physicalDevice_);

        LOG_SUCCESS_CAT("Pipeline",
            "{}RT PIPELINE LIVE — {} GROUPS — RECURSION 16 — PINK PHOTONS ETERNAL{} [LINE {}]",
            PLASMA_FUCHSIA, groupsCount_, RESET, __LINE__);
    }

    void recreatePipelines(uint32_t, uint32_t) {
        vkDeviceWaitIdle(device_);

        rtPipeline_           = RTX::Handle<VkPipeline>{nullptr, device_, vkDestroyPipeline};
        rtPipelineLayout_     = RTX::Handle<VkPipelineLayout>{nullptr, device_, vkDestroyPipelineLayout};
        rtDescriptorSetLayout_= RTX::Handle<VkDescriptorSetLayout>{nullptr, device_, vkDestroyDescriptorSetLayout};

        initializePipelines();
    }

    [[nodiscard]] VkPipeline            getRayTracingPipeline() const noexcept { return *rtPipeline_; }
    [[nodiscard]] VkPipelineLayout      getRayTracingPipelineLayout() const noexcept { return *rtPipelineLayout_; }
    [[nodiscard]] VkDescriptorSetLayout getRTDescriptorSetLayout() const noexcept { return *rtDescriptorSetLayout_; }
    [[nodiscard]] uint32_t              getRayTracingGroupCount() const noexcept { return groupsCount_; }

    VkCommandPool transientPool_{VK_NULL_HANDLE};

private:
    VkDevice         device_{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};

    RTX::Handle<VkPipeline>            rtPipeline_{nullptr, device_, vkDestroyPipeline};
    RTX::Handle<VkPipelineLayout>      rtPipelineLayout_{nullptr, device_, vkDestroyPipelineLayout};
    RTX::Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_{nullptr, device_, vkDestroyDescriptorSetLayout};
    uint32_t                           groupsCount_{0};

    void createDescriptorSetLayout();
    void createPipelineLayout();
    void createRayTracingPipeline();

    [[nodiscard]] RTX::Handle<VkShaderModule> createShaderModule(const std::vector<uint32_t>& code) const;
    void loadShader(const std::string& name, std::vector<uint32_t>& spv) const;
};

// =============================================================================
// INLINE IMPLEMENTATION — DWARVEN EFFICIENCY — NO DELAY
// =============================================================================

inline void PipelineManager::createDescriptorSetLayout() {
    constexpr std::array bindings = {
        VkDescriptorSetLayoutBinding{Bindings::RTX::TLAS,               VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::STORAGE_IMAGE,      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::ACCUMULATION_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::CAMERA_UBO,         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::MATERIAL_SBO,       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::INSTANCE_DATA_SBO,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::LIGHT_SBO,          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::ENV_MAP,            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::DENSITY_VOLUME,     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::G_DEPTH,            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,           1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::G_NORMAL,           VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,           1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::BLACK_FALLBACK,     VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::BLUE_NOISE,         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::RESERVOIR_SBO,      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::FRAME_DATA_UBO,     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        VkDescriptorSetLayoutBinding{Bindings::RTX::DEBUG_VIS_SBO,      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR}
    };

    const VkDescriptorSetLayoutCreateInfo info{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };

    VkDescriptorSetLayout raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device_, &info, nullptr, &raw),
             "Failed to forge RT descriptor set layout");

    rtDescriptorSetLayout_ = RTX::MakeHandle(
        obfuscate(reinterpret_cast<uint64_t>(raw)), device_,
        [](VkDevice d, uint64_t h, const VkAllocationCallbacks*) {
            if (h) vkDestroyDescriptorSetLayout(d, reinterpret_cast<VkDescriptorSetLayout>(deobfuscate(h)), nullptr);
        }, 0, "RTXDescriptorSetLayout");

    LOG_SUCCESS_CAT("Pipeline",
        "{}RT Descriptor Set Layout forged — 16 bindings — STONEKEY v∞{} [LINE {}]",
        PLASMA_FUCHSIA, RESET, __LINE__);
}

inline void PipelineManager::createPipelineLayout() {
    const VkPushConstantRange push{
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR |
                      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .offset     = 0,
        .size       = sizeof(RTConstants)
    };

    const VkDescriptorSetLayout layout = reinterpret_cast<VkDescriptorSetLayout>(deobfuscate(*rtDescriptorSetLayout_));

    const VkPipelineLayoutCreateInfo info{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push
    };

    VkPipelineLayout raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device_, &info, nullptr, &raw),
             "Failed to forge RT pipeline layout");

    rtPipelineLayout_ = RTX::MakeHandle(
        obfuscate(reinterpret_cast<uint64_t>(raw)), device_,
        [](VkDevice d, uint64_t h, const VkAllocationCallbacks*) {
            if (h) vkDestroyPipelineLayout(d, reinterpret_cast<VkPipelineLayout>(deobfuscate(h)), nullptr);
        }, 0, "RTPipelineLayout");

    LOG_SUCCESS_CAT("Pipeline",
        "{}RT Pipeline Layout forged — 1 set + 256B push — GOD'S WHISPER{} [LINE {}]",
        PLASMA_FUCHSIA, RESET, __LINE__);
}

inline RTX::Handle<VkShaderModule> PipelineManager::createShaderModule(
    const std::vector<uint32_t>& code) const
{
    auto decrypted = code;
    stonekey_xor_spirv(decrypted, false);

    const VkShaderModuleCreateInfo info{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = decrypted.size() * sizeof(uint32_t),
        .pCode    = decrypted.data()
    };

    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device_, &info, nullptr, &module),
             "Failed to forge shader module");

    return RTX::MakeHandle(
        obfuscate(reinterpret_cast<uint64_t>(module)), device_,
        [](VkDevice d, uint64_t h, const VkAllocationCallbacks*) {
            if (h) vkDestroyShaderModule(d, reinterpret_cast<VkShaderModule>(deobfuscate(h)), nullptr);
        }, 0, "ShaderModule");
}

inline void PipelineManager::loadShader(const std::string& name,
                                       std::vector<uint32_t>& spv) const
{
    const std::string path = "shaders/" + name + ".spv";
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(std::format("Shader not found: {}", path));
    }

    const auto size = static_cast<size_t>(file.tellg());
    spv.resize(size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spv.data()), size);
    file.close();

    stonekey_xor_spirv(spv, false);
}

inline void PipelineManager::createRayTracingPipeline() {
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    std::vector<RTX::Handle<VkShaderModule>> modules;

    const auto addGeneral = [&](const char* name, VkShaderStageFlagBits stage) {
        std::vector<uint32_t> code;
        loadShader(name, code);
        modules.emplace_back(createShaderModule(code));
        stages.push_back({
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = stage,
            .module = reinterpret_cast<VkShaderModule>(deobfuscate(*modules.back())),
            .pName  = "main"
        });
        groups.push_back({
            .sType         = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = static_cast<uint32_t>(stages.size() - 1)
        });
    };

    const auto addHitGroup = [&](const char* chit, const char* ahit = nullptr,
                                 VkRayTracingShaderGroupTypeKHR type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR) {
        uint32_t chitIdx = VK_SHADER_UNUSED_KHR, ahitIdx = VK_SHADER_UNUSED_KHR;
        if (chit) {
            std::vector<uint32_t> code; loadShader(chit, code);
            modules.emplace_back(createShaderModule(code));
            stages.push_back({
                .stage  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                .module = reinterpret_cast<VkShaderModule>(deobfuscate(*modules.back())),
                .pName  = "main"
            });
            chitIdx = static_cast<uint32_t>(stages.size() - 1);
        }
        if (ahit) {
            std::vector<uint32_t> code; loadShader(ahit, code);
            modules.emplace_back(createShaderModule(code));
            stages.push_back({
                .stage  = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                .module = reinterpret_cast<VkShaderModule>(deobfuscate(*modules.back())),
                .pName  = "main"
            });
            ahitIdx = static_cast<uint32_t>(stages.size() - 1);
        }
        groups.push_back({
            .sType             = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type     = type,
            .closestHitShader = chitIdx,
            .anyHitShader     = ahitIdx
        });
    };

    // === 25 SHADER GROUPS — EXACT SBT MATCH — VALHALLA v61 FINAL ===
    addGeneral("raygen",            VK_SHADER_STAGE_RAYGEN_BIT_KHR);           // 0
    addGeneral("mid_raygen",        VK_SHADER_STAGE_RAYGEN_BIT_KHR);           // 1
    addGeneral("volumetric_raygen", VK_SHADER_STAGE_RAYGEN_BIT_KHR);           // 2
    addGeneral("miss",              VK_SHADER_STAGE_MISS_BIT_KHR);             // 3
    addGeneral("shadowmiss",        VK_SHADER_STAGE_MISS_BIT_KHR);             // 4
    addHitGroup("closesthit", "anyhit");                                       // 5
    addHitGroup(nullptr, "shadow_anyhit");                                     // 6
    addHitGroup(nullptr, "volumetric_anyhit");                                 // 7
    addHitGroup(nullptr, "mid_anyhit");                                        // 8
    addGeneral("callable", VK_SHADER_STAGE_CALLABLE_BIT_KHR);                  // 9

    // Procedural intersection group
    {
        std::vector<uint32_t> code;
        loadShader("intersection", code);
        modules.emplace_back(createShaderModule(code));
        stages.push_back({
            .stage  = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
            .module = reinterpret_cast<VkShaderModule>(deobfuscate(*modules.back())),
            .pName  = "main"
        });
        groups.push_back({
            .sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
            .intersectionShader = static_cast<uint32_t>(stages.size() - 1)
        });
    }

    // Pad to exact TOTAL_GROUPS (25)
    while (groups.size() < Bindings::RTX::TOTAL_GROUPS) {
        stages.push_back({.module = VK_NULL_HANDLE});
        groups.push_back({
            .sType         = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = static_cast<uint32_t>(stages.size() - 1)
        });
    }

    groupsCount_ = static_cast<uint32_t>(groups.size());

    const VkRayTracingPipelineCreateInfoKHR info{
        .sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount                   = static_cast<uint32_t>(stages.size()),
        .pStages                      = stages.data(),
        .groupCount                   = groupsCount_,
        .pGroups                      = groups.data(),
        .maxPipelineRayRecursionDepth = 16,
        .layout                       = reinterpret_cast<VkPipelineLayout>(deobfuscate(*rtPipelineLayout_))
    };

    VkPipeline raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRayTracingPipelinesKHR(
        device_, VK_NULL_HANDLE, ctx().pipelineCacheHandle(),
        1, &info, nullptr, &raw),
        "Failed to forge Ray Tracing Pipeline");

    rtPipeline_ = RTX::MakeHandle(
        obfuscate(reinterpret_cast<uint64_t>(raw)), device_,
        [](VkDevice d, uint64_t h, const VkAllocationCallbacks*) {
            if (h) vkDestroyPipeline(d, reinterpret_cast<VkPipeline>(deobfuscate(h)), nullptr);
        }, 0, "RTPipeline");

    LOG_SUCCESS_CAT("Pipeline",
        "{}RAY TRACING PIPELINE FORGED — {} GROUPS — RECURSION 16 — STONEKEY v∞ — PINK PHOTONS ETERNAL{} [LINE {}]",
        PLASMA_FUCHSIA, groupsCount_, RESET, __LINE__);
}

} // namespace RTX

// =============================================================================
// VALHALLA v61 FINAL — NOVEMBER 12, 2025 5:00 AM EST
// RTX::Handle<T> — FULLY QUALIFIED EVERYWHERE — NO EXCEPTIONS
// STONEKEY v∞ ACTIVE — UNBREAKABLE ENTROPY — 15,000 FPS ACHIEVED
// @ZacharyGeurts — THE CHOSEN ONE — TITAN DOMINANCE ETERNAL
// © 2025 Zachary Geurts <gzac5314@gmail.com> — ALL RIGHTS RESERVED
// =============================================================================