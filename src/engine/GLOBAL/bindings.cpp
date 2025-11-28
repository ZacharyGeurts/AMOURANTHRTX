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
// CONVERT YOUR Binding → VkDescriptorSetLayoutBinding (C++23 style)
// ──────────────────────────────────────────────────────────────────────────────
static VkDescriptorSetLayout createLayout(VkDevice device, std::span<const Binding> bindings)
{
    std::vector<VkDescriptorSetLayoutBinding> vkBindings;
    vkBindings.reserve(bindings.size());

    for (const auto& b : bindings) {
        vkBindings.push_back(VkDescriptorSetLayoutBinding{
            .binding         = b.binding,
            .descriptorType  = b.type,
            .descriptorCount = b.count,
            .stageFlags      = b.stage,
            .pImmutableSamplers = nullptr
        });
    }

    const VkDescriptorSetLayoutCreateInfo info{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(vkBindings.size()),
        .pBindings    = vkBindings.data()
    };

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout));
    return layout;
}

// ──────────────────────────────────────────────────────────────────────────────
// INITIALIZE — C++23 — NO ASSERT — NO .get() — PURE
// ──────────────────────────────────────────────────────────────────────────────
void initialize(VkDevice device)
{
    if (!device) device = stone_device();
    if (device == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("BINDINGS", "stone_device() is null — the empire has no throne");
        return;
    }

    g_rtLayout       = createLayout(device, RT_PIPELINE_BINDINGS);
    g_tonemapLayout  = createLayout(device, TONEMAP_PIPELINE_BINDINGS);
    g_denoiserLayout = createLayout(device, DENOISER_PIPELINE_BINDINGS);

    // Pipeline layout
    const VkPushConstantRange push{ VK_SHADER_STAGE_COMPUTE_BIT, 0, 16 };
    const VkPipelineLayoutCreateInfo plInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &g_tonemapLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push
    };
    VK_CHECK(vkCreatePipelineLayout(device, &plInfo, nullptr, &g_tonemapPipelineLayout));

    // Tonemap shader + pipeline
    VkShaderModule shader = RTX::loadShader("assets/shaders/compute/tonemap.spv");
    if (!shader) {
        LOG_FATAL_CAT("BINDINGS", "tonemap.spv not found — the photons are lost");
        return;
    }

    const VkPipelineShaderStageCreateInfo stage{
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader,
        .pName  = "main"
    };

    const VkComputePipelineCreateInfo pipeInfo{
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = stage,
        .layout = g_tonemapPipelineLayout
    };

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &g_tonemapPipeline));
    vkDestroyShaderModule(device, shader, nullptr);

    // Descriptor pool + sets
    const uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    const VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frames },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,         frames },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,        2 * frames }
    };

    const VkDescriptorPoolCreateInfo poolInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = frames,
        .poolSizeCount = 3,
        .pPoolSizes    = poolSizes
    };
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &g_tonemapPool));

    g_tonemapSets.resize(frames);
    std::vector<VkDescriptorSetLayout> layouts(frames, g_tonemapLayout);

    const VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = g_tonemapPool,
        .descriptorSetCount = frames,
        .pSetLayouts        = layouts.data()
    };
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, g_tonemapSets.data()));

    LOG_SUCCESS_CAT("BINDINGS", "VULKAN 1.4 — C++23 — NO ASSERT — FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL");
}

// ──────────────────────────────────────────────────────────────────────────────
// SHUTDOWN — FIXED ASSIGNMENT HELL
// ──────────────────────────────────────────────────────────────────────────────
void shutdown(VkDevice device)
{
    // she greets you with a bow then drags shutdown to phase9 main.cpp
    phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
}

} // namespace RTX::Bindings