// src/engine/GLOBAL/bindings.cpp
// =============================================================================
// AMOURANTH RTX — BINDING CENTRAL COMMAND — v∞ APOCALYPSE — 2025
// ALL DESCRIPTOR SET LAYOUTS ARE BORN HERE AND DIE HERE
// =============================================================================

#include "bindings.hpp"
#include "core/Context.hpp"
#include "utils/Logging.hpp"

namespace RTX::Bindings {

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL LAYOUT HANDLES
// ──────────────────────────────────────────────────────────────────────────────
VkDescriptorSetLayout g_rtLayout       = VK_NULL_HANDLE;
VkDescriptorSetLayout g_tonemapLayout  = VK_NULL_HANDLE;
VkDescriptorSetLayout g_denoiserLayout = VK_NULL_HANDLE;

// ──────────────────────────────────────────────────────────────────────────────
// RAY TRACING SET 0 — FINAL ORDER (matches your shader)
// ──────────────────────────────────────────────────────────────────────────────
const std::array<Binding, 10> RT_PIPELINE_BINDINGS = {{
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
}};

// ──────────────────────────────────────────────────────────────────────────────
// TONEMAP SET 1
// ──────────────────────────────────────────────────────────────────────────────
const std::array<Binding, 3> TONEMAP_PIPELINE_BINDINGS = {{
    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, "InputHDR"},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1, VK_SHADER_STAGE_COMPUTE_BIT, "OutputLDR"},
    {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1, VK_SHADER_STAGE_COMPUTE_BIT, "TonemapParams"},
}};

// ──────────────────────────────────────────────────────────────────────────────
// DENOISER SET 2
// ──────────────────────────────────────────────────────────────────────────────
const std::array<Binding, 2> DENOISER_PIPELINE_BINDINGS = {{
    {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, "NoisyInput"},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, "CleanOutput"},
}};

// ──────────────────────────────────────────────────────────────────────────────
// HELPER: Create layout from table
// ──────────────────────────────────────────────────────────────────────────────
static VkDescriptorSetLayout createLayout(VkDevice device, const auto& table)
{
    VkDescriptorSetLayoutCreateInfo info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(table.size()),
        .pBindings    = table.data()
    };

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout));
    return layout;
}

// ──────────────────────────────────────────────────────────────────────────────
// INITIALIZATION — CALL ONCE AFTER DEVICE CREATION
// ──────────────────────────────────────────────────────────────────────────────
void initialize(VkDevice device)
{
    ASSERT(device != VK_NULL_HANDLE, "Cannot initialize bindings with null device");

    g_rtLayout       = createLayout(device, RT_PIPELINE_BINDINGS);
    g_tonemapLayout  = createLayout(device, TONEMAP_PIPELINE_BINDINGS);
    g_denoiserLayout = createLayout(device, DENOISER_PIPELINE_BINDINGS);

    LOG_INFO_CAT("Bindings", "Central descriptor set layouts created — Empire is aligned");
}

void shutdown(VkDevice device)
{
    if (g_rtLayout)       vkDestroyDescriptorSetLayout(device, g_rtLayout, nullptr);
    if (g_tonemapLayout)  vkDestroyDescriptorSetLayout(device, g_tonemapLayout, nullptr);
    if (g_denoiserLayout) vkDestroyDescriptorSetLayout(device, g_denoiserLayout, nullptr);

    g_rtLayout = g_tonemapLayout = g_denoiserLayout = VK_NULL_HANDLE;
}

// VulkanRTX.cpp — recordRayTrace — FINAL, ETERNAL, BINDINGS-COMPLIANT EDITION
void VulkanRTX::recordRayTrace(VkCommandBuffer cmd,
                               VkExtent2D extent,
                               VkImage outputImage,
                               VkImageView /*outputView*/) noexcept
{
    LOG_TRACE_CAT("RTX", "recordRayTrace — {}x{} — cmd=0x{:x}", extent.width, extent.height, reinterpret_cast<uintptr_t>(cmd));

    // ──────────────────────────────────────────────────────────────────────────────
    // 1. Transition output image → GENERAL (ray tracing write target)
    // ──────────────────────────────────────────────────────────────────────────────
    VkImageMemoryBarrier toGeneral = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .image               = outputImage,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP899_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, 0, nullptr, 0, nullptr, 1, &toGeneral);

    // ──────────────────────────────────────────────────────────────────────────────
    // 2. BIND THE ETERNAL PIPELINE + DESCRIPTOR SET (from centralized bindings)
    // ──────────────────────────────────────────────────────────────────────────────
    using namespace RTX::Bindings;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());

    // Set 0 = RT set — from global layout in bindings.cpp
    const VkDescriptorSet rtSet = descriptorSets_[currentFrame_];
    vkCmdBindDescriptorSets(cmd,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        rtPipelineLayout_.get(),
        SET_RAY_TRACING,        // ← NOW FROM BINDINGS
        1, &rtSet,
        0, nullptr);

    // ──────────────────────────────────────────────────────────────────────────────
    // 3. TRACE RAYS — THE PHOTONS OBEY
    // ──────────────────────────────────────────────────────────────────────────────
    rtCmdTraceRaysKHR(cmd,
        &sbt_.raygen,
        &sbt_.miss,
        &sbt_.hit,
        &sbt_.callable,
        extent.width,
        extent.height,
        1);

    // ──────────────────────────────────────────────────────────────────────────────
    // 4. Transition back → PRESENT_SRC (or whatever consumer wants)
    // ──────────────────────────────────────────────────────────────────────────────
    VkImageMemoryBarrier toPresent = toGeneral;
    toPresent.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    toPresent.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
    toPresent.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toPresent);

    LOG_SUCCESS_CAT("RTX", "Ray trace complete — {}x{} — {} SPP — PHOTONS CONVERGED", 
                    extent.width, extent.height, currentSpp_);
}



} // namespace RTX::Bindings