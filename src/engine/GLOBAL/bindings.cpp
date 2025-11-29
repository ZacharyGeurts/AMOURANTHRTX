// src/engine/GLOBAL/bindings.cpp
// AMOURANTH RTX Engine © 2025 — FIRST LIGHT ACHIEVED — VULKAN 1.4 — C++23 — NO ASSERT — PURE EMPIRE
// License: Proprietary - All rights reserved. Unauthorized copying, modification, or distribution is prohibited.

#include "engine/GLOBAL/bindings.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanCore.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

namespace RTX::Bindings {

// ──────────────────────────────────────────────────────────────────────────────
// YOUR NAMED BINDINGS — PRESERVED FOR GLORY
// ──────────────────────────────────────────────────────────────────────────────
const std::array<Binding, 11> RT_PIPELINE_BINDINGS = {{
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "TLAS"},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "RT_Output"},
    {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "Accumulation"},
    {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,    "Camera"},
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                                      "Materials"},
    {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,           "EnvMap"},
    {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "NexusScore"},
    {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "Dimensions"},
    {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "BlueNoise"},
    {9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "DensityVolume"},
    {31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, "StoneKeyRuntimeBlock"},
}};

const std::array<Binding, 4> TONEMAP_PIPELINE_BINDINGS = {{
    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, "InputHDR"},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1, VK_SHADER_STAGE_COMPUTE_BIT, "OutputLDR"},
    {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1, VK_SHADER_STAGE_COMPUTE_BIT, "TonemapParams"},
    {31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,        1, VK_SHADER_STAGE_COMPUTE_BIT, "StoneKeyRuntimeBlock"},
}};

const std::array<Binding, 3> DENOISER_PIPELINE_BINDINGS = {{
    {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, "NoisyInput"},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, "CleanOutput"},
    {31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, "StoneKeyRuntimeBlock"},
}};

// ──────────────────────────────────────────────────────────────────────────────
// GLOBALS — THE EMPIRE'S CROWN JEWELS
// ──────────────────────────────────────────────────────────────────────────────
VkDescriptorSetLayout g_rtLayout           = VK_NULL_HANDLE;
VkDescriptorSetLayout g_tonemapLayout      = VK_NULL_HANDLE;
VkDescriptorSetLayout g_denoiserLayout     = VK_NULL_HANDLE;

VkPipelineLayout      g_tonemapPipelineLayout = VK_NULL_HANDLE;
VkPipeline            g_tonemapPipeline       = VK_NULL_HANDLE;

std::vector<VkDescriptorSet> g_tonemapSets;
VkDescriptorPool             g_tonemapPool = VK_NULL_HANDLE;

// ──────────────────────────────────────────────────────────────────────────────
// INITIALIZE — VULKAN 1.4 — C++23 — NO ASSERT — StoneKey v∞ — BINDING 31
// ──────────────────────────────────────────────────────────────────────────────
void initialize(VkDevice device)
{
    if (!device) device = stone_device();
    if (device == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("BINDINGS", "stone_device() is null — the empire has no throne — CARMACK IS DISAPPOINTED");
        return;
    }

    // ── THE ONE TRUE LAYOUTS — FORGED FROM THE SACRED ARRAYS — GAPS ARE WISDOM
    g_rtLayout       = createDescriptorSetLayout(device, RT_PIPELINE_BINDINGS);        // 11 bindings — 0–9 + 31 immortal
    g_tonemapLayout  = createDescriptorSetLayout(device, TONEMAP_PIPELINE_BINDINGS);   // 4 bindings — includes 31
    g_denoiserLayout = createDescriptorSetLayout(device, DENOISER_PIPELINE_BINDINGS);  // 3 bindings — includes 31

    LOG_SUCCESS_CAT("BINDINGS", 
        "DESCRIPTOR LAYOUTS FORGED — RT: {} bindings (0-9 + 31 StoneKey) | Tonemap: {} | Denoiser: {}", 
        RT_PIPELINE_BINDINGS.size(), TONEMAP_PIPELINE_BINDINGS.size(), DENOISER_PIPELINE_BINDINGS.size());

    // ── TONEMAP PIPELINE LAYOUT — PUSH CONSTANTS + SET 1
    const VkPushConstantRange push{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset     = 0,
        .size       = 16
    };

    const VkPipelineLayoutCreateInfo tonemapLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &g_tonemapLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push
    };

    VK_CHECK(vkCreatePipelineLayout(device, &tonemapLayoutInfo, nullptr, &g_tonemapPipelineLayout),
             "Failed to forge tonemap pipeline layout — the photons dim");

    // ── TONEMAP SHADER & COMPUTE PIPELINE
    VkShaderModule tonemapShader = RTX::loadShader("assets/shaders/compute/tonemap.spv");
    if (tonemapShader == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("BINDINGS", "tonemap.spv missing — the empire has no light — CARMACK WEEPS");
        return;
    }

    const VkPipelineShaderStageCreateInfo tonemapStage{
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = tonemapShader,
        .pName  = "main"
    };

    const VkComputePipelineCreateInfo tonemapPipeInfo{
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = tonemapStage,
        .layout = g_tonemapPipelineLayout
    };

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &tonemapPipeInfo, nullptr, &g_tonemapPipeline),
             "Failed to forge tonemap compute pipeline — the HDR dies here");

    // Shader module no longer needed — destroy immediately (safe: pipeline owns it now)
    vkDestroyShaderModule(device, tonemapShader, nullptr);

    // ── TONEMAP DESCRIPTOR POOL — SCALED TO TRIPLE BUFFERING
    const uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    const VkDescriptorPoolSize tonemapPoolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frames },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,         frames },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,        2 * frames }  // TonemapParams + StoneKeyRuntimeBlock
    };

    const VkDescriptorPoolCreateInfo tonemapPoolInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = frames,
        .poolSizeCount = 3,
        .pPoolSizes    = tonemapPoolSizes
    };

    VK_CHECK(vkCreateDescriptorPool(device, &tonemapPoolInfo, nullptr, &g_tonemapPool),
             "Failed to forge tonemap descriptor pool — the sets are lost");

    // ── ALLOCATE TONEMAP DESCRIPTOR SETS
    g_tonemapSets.resize(frames);
    std::vector<VkDescriptorSetLayout> tonemapLayouts(frames, g_tonemapLayout);

    const VkDescriptorSetAllocateInfo tonemapAllocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = g_tonemapPool,
        .descriptorSetCount = frames,
        .pSetLayouts        = tonemapLayouts.data()
    };

    VK_CHECK(vkAllocateDescriptorSets(device, &tonemapAllocInfo, g_tonemapSets.data()),
             "Failed to allocate tonemap descriptor sets — the frames are blind");

    LOG_SUCCESS_CAT("BINDINGS", 
        "TONEMAP PIPELINE READY — {} frames — Binding 31 (StoneKey) SECURE — HDR → LDR ACTIVE");

    // ── FINAL LOG — THE EMPIRE IS ALIVE
    LOG_SUCCESS_CAT("BINDINGS", 
        "{}VULKAN 1.4 — C++23 — NO ASSERT — FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — BINDING 31 IS GOD{}", 
        EMERALD_GREEN, RESET);
}

// ──────────────────────────────────────────────────────────────────────────────
// SHUTDOWN — FIXED ASSIGNMENT HELL
// ──────────────────────────────────────────────────────────────────────────────
void shutdown(VkDevice device)
{
    // she greets you with a bow then drags shutdown to phase9 main.cpp
    phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
}

VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device, std::span<const Binding> bindings)
{
    std::vector<VkDescriptorSetLayoutBinding> vkBindings;
    vkBindings.reserve(bindings.size());

    for (const auto& b : bindings) {
        vkBindings.push_back({
            .binding         = b.binding,
            .descriptorType  = b.type,
            .descriptorCount = b.count,
            .stageFlags      = b.stage,
            .pImmutableSamplers = nullptr
        });
    }

    VkDescriptorSetLayoutCreateInfo info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(vkBindings.size()),
        .pBindings    = vkBindings.data()
    };

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout),
             "Failed to create descriptor set layout — BUT STONEKEY WILL NOT YIELD");

    return layout;
}

} // namespace RTX::Bindings