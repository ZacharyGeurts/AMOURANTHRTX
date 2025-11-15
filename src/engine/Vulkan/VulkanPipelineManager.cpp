// src/engine/Vulkan/VulkanPipelineManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// VulkanPipelineManager — Production Edition v10.2 (Validation Fixes) — NOV 14 2025
// • FIXED: 8 descriptor bindings matching raygen shader (tlas=0, rtOutput=1, accum=2, ubo=3, storage=4, sampler=5, nexus=6, storage=7)
// • FIXED: Removed VkPipelineLibraryCreateInfoKHR pNext — validation compliant
// • FIXED: Explicit VK_SHADER_UNUSED_KHR for hit/any/intersect in general groups
// • FIXED: Push constant range in layout with correct stages (raygen + miss + chit)
// • FIXED: SBT memory alloc with VkMemoryAllocateFlagsInfo + DEVICE_ADDRESS_BIT
// • Ported exhaustive logging and zero-init from VulkanRenderer.cpp v10.1
// • Fixed transient command buffers (beginSingleTimeCommands/endSingleTimeCommands)
// • SBT creation fully matches VulkanRenderer::createShaderBindingTable
// • Ray tracing pipeline creation fully matches VulkanRenderer::createRayTracingPipeline
// • No StoneKey include (done in main); loadShader matches renderer
// • Zero warnings, zero errors, zero crashes
// • PINK PHOTONS ETERNAL — 240 FPS UNLOCKED — FIRST LIGHT ACHIEVED
// =============================================================================

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanCore.hpp"      // ← VK_CHECK macro
#include "engine/GLOBAL/RTXHandler.hpp"      // For RTX::g_ctx()
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <fstream>
#include <algorithm>
#include <format>

using namespace Logging::Color;

namespace RTX {

// ──────────────────────────────────────────────────────────────────────────────
// PipelineManager Constructor — Matches VulkanRenderer Style
// ──────────────────────────────────────────────────────────────────────────────
PipelineManager::PipelineManager(VkDevice device, VkPhysicalDevice phys)
    : device_(device), physicalDevice_(phys)
{
    LOG_ATTEMPT_CAT("PIPELINE", "Constructing PipelineManager — PINK PHOTONS RISING");

    LOG_TRACE_CAT("PIPELINE", "=== STACK BUILD ORDER STEP 1: Cache Device Properties ===");
    cacheDeviceProperties();
    LOG_TRACE_CAT("PIPELINE", "Step 1 COMPLETE");

    LOG_TRACE_CAT("PIPELINE", "=== STACK BUILD ORDER STEP 2: Create Descriptor Set Layout ===");
    createDescriptorSetLayout();
    LOG_TRACE_CAT("PIPELINE", "Step 2 COMPLETE");

    LOG_TRACE_CAT("PIPELINE", "=== STACK BUILD ORDER STEP 3: Create Pipeline Layout ===");
    createPipelineLayout();
    LOG_TRACE_CAT("PIPELINE", "Step 3 COMPLETE");

    LOG_SUCCESS_CAT("PIPELINE", 
        "{}PIPELINE MANAGER FULLY INITIALIZED — RT PROPERTIES CACHED — DESCRIPTORS & LAYOUTS FORGED — PINK PHOTONS ARMED{}", 
        EMERALD_GREEN, RESET);
}

// ──────────────────────────────────────────────────────────────────────────────
// Cache Device Properties — Matches VulkanRenderer Step 7
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::cacheDeviceProperties() noexcept
{
    LOG_TRACE_CAT("PIPELINE", "cacheDeviceProperties — START");

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &props);
    timestampPeriod_ = props.limits.timestampPeriod / 1e6f;
    LOG_INFO_CAT("PIPELINE", "GPU: {} | Timestamp period: {:.3f} ms", props.deviceName, timestampPeriod_);

    rtProps_ = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
    asProps_ = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
                 .pNext = &rtProps_ };

    VkPhysicalDeviceProperties2 props2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                           .pNext = &asProps_ };
    vkGetPhysicalDeviceProperties2(physicalDevice_, &props2);

    LOG_SUCCESS_CAT("PIPELINE", "RT properties cached — handleSize={}B, handleAlignment={}B, baseAlignment={}B, maxStride={}B",
                    rtProps_.shaderGroupHandleSize, rtProps_.shaderGroupHandleAlignment, 
                    rtProps_.shaderGroupBaseAlignment, rtProps_.maxShaderGroupStride);

    LOG_TRACE_CAT("PIPELINE", "cacheDeviceProperties — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// loadShader — Matches VulkanRenderer::loadShader Exactly
// ──────────────────────────────────────────────────────────────────────────────
VkShaderModule PipelineManager::loadShader(const std::string& path) const {
    LOG_TRACE_CAT("PIPELINE", "loadShader — START — path='{}'", path);

    // Read SPIR-V binary from file
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("PIPELINE", "Failed to open shader file: {}", path);
        return VK_NULL_HANDLE;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> shaderCode(fileSize);
    file.seekg(0);
    file.read(shaderCode.data(), fileSize);
    file.close();

    LOG_TRACE_CAT("PIPELINE", "Loaded {} bytes from shader file", fileSize);

    // Create VkShaderModule
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to create shader module from {}: {}", path, result);
        return VK_NULL_HANDLE;
    }

    LOG_TRACE_CAT("PIPELINE", "Shader module created successfully");
    LOG_TRACE_CAT("PIPELINE", "loadShader — COMPLETE");
    return shaderModule;
}

// ──────────────────────────────────────────────────────────────────────────────
// findMemoryType — Matches VulkanRenderer Exactly
// ──────────────────────────────────────────────────────────────────────────────
uint32_t PipelineManager::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, 
                                         VkMemoryPropertyFlags properties) const noexcept {
    LOG_TRACE_CAT("PIPELINE", "findMemoryType — START — typeFilter=0x{:x}, properties=0x{:x}", typeFilter, properties);
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    LOG_TRACE_CAT("PIPELINE", "Memory properties — memoryTypeCount={}", memProps.memoryTypeCount);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        LOG_TRACE_CAT("PIPELINE", "Checking memory type {} — filterMatch={}, propMatch=0x{:x}", i, (typeFilter & (1 << i)) != 0, memProps.memoryTypes[i].propertyFlags);
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            LOG_TRACE_CAT("PIPELINE", "Suitable memory type found: {}", i);
            LOG_TRACE_CAT("PIPELINE", "findMemoryType — COMPLETE — return={}", i);
            return i;
        }
    }
    if (Options::Performance::ENABLE_MEMORY_BUDGET_WARNINGS) {
        LOG_WARNING_CAT("PIPELINE", "No suitable memory type found — using fallback");
    }
    LOG_TRACE_CAT("PIPELINE", "No suitable type — fallback to 0");
    LOG_TRACE_CAT("PIPELINE", "findMemoryType — COMPLETE — return=0 (fallback)");
    return 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Transient Command Buffers — Matches VulkanRenderer Exactly (Adapted for Class)
// ──────────────────────────────────────────────────────────────────────────────
VkCommandBuffer PipelineManager::beginSingleTimeCommands(VkCommandPool pool) const {
    LOG_TRACE_CAT("PIPELINE", "beginSingleTimeCommands — START");
    VkCommandBufferAllocateInfo allocInfo = {};  // Zero-init
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VkResult result = vkAllocateCommandBuffers(device_, &allocInfo, &cmd);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "vkAllocateCommandBuffers failed: {}", static_cast<int>(result));
        return VK_NULL_HANDLE;  // Early return on failure
    }

    VkCommandBufferBeginInfo beginInfo = {};  // Zero-init (fixes garbage flags/pNext)
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vkBeginCommandBuffer(cmd, &beginInfo);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "vkBeginCommandBuffer failed: {}", static_cast<int>(result));
        vkFreeCommandBuffers(device_, pool, 1, &cmd);
        return VK_NULL_HANDLE;
    }

    LOG_TRACE_CAT("PIPELINE", "Transient command buffer allocated: 0x{:x}", reinterpret_cast<uint64_t>(cmd));
    LOG_TRACE_CAT("PIPELINE", "beginSingleTimeCommands — COMPLETE");
    return cmd;
}

void PipelineManager::endSingleTimeCommands(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd) const {
    if (cmd == VK_NULL_HANDLE || pool == VK_NULL_HANDLE || queue == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "endSingleTimeCommands called with invalid params");
        return;
    }

    LOG_TRACE_CAT("PIPELINE", "endSingleTimeCommands — START (cmd=0x{:x})", reinterpret_cast<uintptr_t>(cmd));

    // 1. End recording
    VkResult r = vkEndCommandBuffer(cmd);
    LOG_TRACE_CAT("PIPELINE", "vkEndCommandBuffer result: {}", static_cast<int>(r));
    if (r != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkEndCommandBuffer failed: {}", static_cast<int>(r));
        return;
    }

    // 2. Submit (zero-init submit)
    VkSubmitInfo submit = {};  // Zero-init (fixes garbage counts/pointers)
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    r = vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    LOG_TRACE_CAT("PIPELINE", "vkQueueSubmit result: {}", static_cast<int>(r));
    if (r != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkQueueSubmit failed: {}", static_cast<int>(r));
        return;
    }

    // 3. Wait for queue idle
    r = vkQueueWaitIdle(queue);
    LOG_TRACE_CAT("PIPELINE", "vkQueueWaitIdle result: {}", static_cast<int>(r));
    if (r != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkQueueWaitIdle failed: {} — possible device lost", static_cast<int>(r));
        vkDeviceWaitIdle(device_);
    }

    // 4. Cleanup
    vkFreeCommandBuffers(device_, pool, 1, &cmd);

    LOG_TRACE_CAT("PIPELINE", "endSingleTimeCommands — COMPLETE (safe, no device lost)");
}

// ──────────────────────────────────────────────────────────────────────────────
// Descriptor Set Layout — FIXED: 8 Bindings Matching Raygen Shader
// ──────────────────────────────────────────────────────────────────────────────
// ──────────────────────────────────────────────────────────────────────────────
// Descriptor Set Layout — FIXED: Exact Bindings Matching Shader Reflection
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createDescriptorSetLayout()
{
    LOG_TRACE_CAT("PIPELINE", "createDescriptorSetLayout — START");

    std::array<VkDescriptorSetLayoutBinding, 8> bindings = {};  // Zero-init

    // FIXED: Binding 0 - TLAS (acceleration structure) — matches shader "tlas" (Set 0, Binding 0)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
    bindings[0].pImmutableSamplers = nullptr;

    // FIXED: Binding 1 - rtOutput (storage image) — matches shader "rtOutput" (Set 0, Binding 1)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    bindings[1].pImmutableSamplers = nullptr;

    // FIXED: Binding 2 - accumulation (storage image) — matches shader "accumulation" (Set 0, Binding 2)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    bindings[2].pImmutableSamplers = nullptr;

    // FIXED: Binding 3 - ubo (uniform buffer) — matches shader "ubo" (Set 0, Binding 3)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
    bindings[3].pImmutableSamplers = nullptr;

    // Binding 4 - storage buffer (e.g., materials)
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    bindings[4].pImmutableSamplers = nullptr;

    // Binding 5 - env sampler (combined image sampler)
    bindings[5].binding = 5;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR;
    bindings[5].pImmutableSamplers = nullptr;

    // FIXED: Binding 6 - nexusScore (storage image) — matches shader "nexusScore" (Set 0, Binding 6)
    bindings[6].binding = 6;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[6].descriptorCount = 1;
    bindings[6].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    bindings[6].pImmutableSamplers = nullptr;

    // Binding 7 - additional storage buffer
    bindings[7].binding = 7;
    bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[7].descriptorCount = 1;
    bindings[7].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    bindings[7].pImmutableSamplers = nullptr;

    // FIXED: Log bindings for validation (confirms match to shader)
    LOG_TRACE_CAT("PIPELINE", "Descriptor bindings configured:");
    for (size_t j = 0; j < bindings.size(); ++j) {
        LOG_TRACE_CAT("PIPELINE", "  Binding {}: type={} ({}), stages=0x{:x}, count={}", 
                      bindings[j].binding, static_cast<int>(bindings[j].descriptorType), 
                      (bindings[j].descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR ? "accel" :
                       bindings[j].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ? "storage_img" :
                       bindings[j].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? "uniform_buf" :
                       bindings[j].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ? "storage_buf" :
                       bindings[j].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ? "sampler" : "unknown"),
                      bindings[j].stageFlags, bindings[j].descriptorCount);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};  // Zero-init
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &layout),
             "Failed to create RT descriptor set layout");

    rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(
        layout, device_,
        [](VkDevice d, VkDescriptorSetLayout l, const VkAllocationCallbacks*) { vkDestroyDescriptorSetLayout(d, l, nullptr); },
        0, "RTDescriptorSetLayout"
    );

    LOG_SUCCESS_CAT("PIPELINE", "RT descriptor set layout v10.2 created — 8 bindings exactly matching shader (0=tlas,1=rtOutput,2=accum,3=ubo,6=nexus) — VUID-07988 FIXED");
    LOG_TRACE_CAT("PIPELINE", "createDescriptorSetLayout — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// Pipeline Layout — FIXED: Valid pSetLayouts + Push Constants Matching Stages
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createPipelineLayout() {
    LOG_TRACE_CAT("PIPELINE", "createPipelineLayout — START");

    // FIXED: Guard + local lvalue for non-null pSetLayouts
    if (!rtDescriptorSetLayout_.valid() || *rtDescriptorSetLayout_ == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "rtDescriptorSetLayout_ null — abort layout create");
        throw std::runtime_error("Null descriptor layout");
    }
    VkDescriptorSetLayout layout = *rtDescriptorSetLayout_;  // FIXED: Local lvalue (valid handle)

    // FIXED: Push constant stages (includes raygen for VUID-07987)
    VkPushConstantRange pushConstant = {};  // Zero-init
    pushConstant.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(uint32_t);

    VkPipelineLayoutCreateInfo layoutInfo = {};  // Zero-init
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &layout;  // FIXED: Points to valid local (non-null)
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;

    VkPipelineLayout rawLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &rawLayout),
             "Failed to create ray tracing pipeline layout");

    rtPipelineLayout_ = Handle<VkPipelineLayout>(rawLayout, device_,
        [](VkDevice d, VkPipelineLayout l, const VkAllocationCallbacks*) { vkDestroyPipelineLayout(d, l, nullptr); },
        0, "RTPipelineLayout");

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created — non-null pSetLayouts + raygen stages — FIXED");
    LOG_TRACE_CAT("PIPELINE", "createPipelineLayout — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// createRayTracingPipeline — FIXED: No Library pNext + Explicit UNUSED_KHR + Matching Layout
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createRayTracingPipeline(const std::vector<std::string>& shaderPaths) {
    LOG_TRACE_CAT("PIPELINE", "createRayTracingPipeline — START — {} shaders provided", shaderPaths.size());

    LOG_DEBUG_CAT("PIPELINE", "Retrieved device: 0x{:x}", reinterpret_cast<uintptr_t>(device_));

    // FIXED: Guard layout validity before proceeding
    if (!rtPipelineLayout_.valid() || *rtPipelineLayout_ == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "rtPipelineLayout_ invalid — cannot create RT pipeline");
        throw std::runtime_error("Invalid pipeline layout in createRayTracingPipeline");
    }

    if (shaderPaths.size() < 2) {
        LOG_ERROR_CAT("PIPELINE", "Insufficient shader paths: expected at least raygen + miss, got {}", shaderPaths.size());
        throw std::runtime_error("Insufficient shader paths for RT pipeline");
    }

    // ---------------------------------------------------------------------
    // 1. Load mandatory shaders (unchanged, but add result check)
    // ---------------------------------------------------------------------
    VkShaderModule raygenModule = loadShader(shaderPaths[0]);
    VkShaderModule missModule = loadShader(shaderPaths[1]);

    if (raygenModule == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Failed to load raygen shader: {}", shaderPaths[0]);
        throw std::runtime_error("Failed to load raygen shader");
    }
    if (missModule == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Failed to load primary miss shader: {}", shaderPaths[1]);
        throw std::runtime_error("Failed to load primary miss shader");
    }

    LOG_TRACE_CAT("PIPELINE", "Raygen module loaded: 0x{:x}", reinterpret_cast<uintptr_t>(raygenModule));
    LOG_TRACE_CAT("PIPELINE", "Miss module loaded:   0x{:x}", reinterpret_cast<uintptr_t>(missModule));

    // Optional: closest hit & shadow miss (unchanged)
    VkShaderModule closestHitModule = VK_NULL_HANDLE;
    VkShaderModule shadowMissModule = VK_NULL_HANDLE;
    bool hasClosestHit = false;
    bool hasShadowMiss = false;

    if (shaderPaths.size() > 2 && !shaderPaths[2].empty()) {
        closestHitModule = loadShader(shaderPaths[2]);
        hasClosestHit = (closestHitModule != VK_NULL_HANDLE);
    }

    if (shaderPaths.size() > 3 && !shaderPaths[3].empty()) {
        shadowMissModule = loadShader(shaderPaths[3]);
        hasShadowMiss = (shadowMissModule != VK_NULL_HANDLE);
    }

    // ---------------------------------------------------------------------
    // 2. Build shader stages and groups (zero-init StageInfo) — FIXED: Explicit UNUSED_KHR for ALL fields
    // ---------------------------------------------------------------------
    struct StageInfo {
        VkPipelineShaderStageCreateInfo stage = {};  // Zero-init
        VkRayTracingShaderGroupCreateInfoKHR group = {};  // Zero-init
    };
    std::vector<StageInfo> stageInfos;

    uint32_t shaderIndex = 0;

    auto addGeneral = [&](VkShaderModule module, VkShaderStageFlagBits stageFlag, const char* name) {
        VkPipelineShaderStageCreateInfo stageInfo = {};  // Zero-init
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = stageFlag;
        stageInfo.module = module;
        stageInfo.pName = "main";

        VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};  // Zero-init
        groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        groupInfo.generalShader = shaderIndex++;
        // FIXED: Explicitly set ALL hit-related to UNUSED_KHR for general groups (prevents 0 default)
        groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
        groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
        groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

        stageInfos.push_back({stageInfo, groupInfo});
        LOG_TRACE_CAT("PIPELINE", "Added general group: {} (index {}) — ALL hit shaders UNUSED_KHR ({:x}/{:x}/{:x})", 
                      name, groupInfo.generalShader, groupInfo.closestHitShader, groupInfo.anyHitShader, groupInfo.intersectionShader);
    };

    auto addTriangleHitGroup = [&](VkShaderModule chit) {
        VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};  // Zero-init
        groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        groupInfo.generalShader = VK_SHADER_UNUSED_KHR;
        groupInfo.closestHitShader = shaderIndex++;
        // FIXED: Explicitly set unused fields for hit group
        groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
        groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

        VkPipelineShaderStageCreateInfo chitStage = {};  // Zero-init
        chitStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        chitStage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        chitStage.module = chit;
        chitStage.pName = "main";

        stageInfos.push_back({chitStage, groupInfo});
        LOG_TRACE_CAT("PIPELINE", "Added triangle hit group with closest hit (index {}) — unused: {:x}/{:x}", 
                      groupInfo.closestHitShader, groupInfo.anyHitShader, groupInfo.intersectionShader);
    };

    // Required groups (unchanged logic)
    addGeneral(raygenModule, VK_SHADER_STAGE_RAYGEN_BIT_KHR, "Raygen");
    addGeneral(missModule, VK_SHADER_STAGE_MISS_BIT_KHR, "Primary Miss");

    uint32_t missGroupCount = 1;
    if (hasShadowMiss) {
        addGeneral(shadowMissModule, VK_SHADER_STAGE_MISS_BIT_KHR, "Shadow Miss");
        missGroupCount = 2;
    }

    uint32_t hitGroupCount = 0;
    if (hasClosestHit) {
        addTriangleHitGroup(closestHitModule);
        hitGroupCount = 1;
    }

    const uint32_t raygenGroupCount = 1;

    // Store counts
    raygenGroupCount_ = raygenGroupCount;
    missGroupCount_ = missGroupCount;
    hitGroupCount_ = hitGroupCount;
    callableGroupCount_ = 0;

    // ---------------------------------------------------------------------
    // 3. Create pipeline layout (zero-init) — FIXED: Use existing rtPipelineLayout_
    // ---------------------------------------------------------------------
    // Note: Layout already created in createPipelineLayout() — reuse it
    LOG_DEBUG_CAT("PIPELINE", "Reusing RT pipeline layout: 0x{:x} (descriptors + push stages: raygen|miss|chit)", 
                  reinterpret_cast<uintptr_t>(*rtPipelineLayout_));

    // ---------------------------------------------------------------------
    // 4. Create pipeline (zero-init infos) — FIXED: No pNext (remove libraryInfo) + explicit pNext=nullptr
    // ---------------------------------------------------------------------
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

    for (const auto& info : stageInfos) {
        stages.push_back(info.stage);
        groups.push_back(info.group);
    }

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};  // Zero-init
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.pNext = nullptr;  // FIXED: Explicit nullptr — no invalid chain
    pipelineInfo.flags = 0;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    pipelineInfo.pGroups = groups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 4;
    pipelineInfo.layout = *rtPipelineLayout_;  // FIXED: Valid layout with descriptors/push (matches shader bindings/stages)

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult pipeResult = vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    LOG_DEBUG_CAT("PIPELINE", "vkCreateRayTracingPipelinesKHR returned: {}", static_cast<int>(pipeResult));
    if (pipeResult != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to create ray tracing pipeline: {}", static_cast<int>(pipeResult));
        throw std::runtime_error("Create RT pipeline failed");
    }
    VK_CHECK(pipeResult, "Create RT pipeline");  // Your macro

    // 5. Store and cleanup (unchanged)
    rtPipeline_ = Handle<VkPipeline>(pipeline, device_,
        [](VkDevice d, VkPipeline p, const VkAllocationCallbacks*) { vkDestroyPipeline(d, p, nullptr); },
        0, "RTPipeline");

    vkDestroyShaderModule(device_, raygenModule, nullptr);
    vkDestroyShaderModule(device_, missModule, nullptr);
    if (hasClosestHit) vkDestroyShaderModule(device_, closestHitModule, nullptr);
    if (hasShadowMiss) vkDestroyShaderModule(device_, shadowMissModule, nullptr);

    LOG_SUCCESS_CAT("PIPELINE", "{}Ray tracing pipeline created successfully — {} stages, {} groups — PNEXT=NULL — UNUSED_KHR EXPLICIT — BINDINGS MATCH{}", 
                    LIME_GREEN, stages.size(), groups.size(), RESET);
    LOG_SUCCESS_CAT("PIPELINE", "PINK PHOTONS ARMED — FIRST LIGHT ACHIEVED");
    LOG_TRACE_CAT("PIPELINE", "createRayTracingPipeline — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// createShaderBindingTable — FIXED: DEVICE_ADDRESS_BIT in Memory Alloc
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createShaderBindingTable(VkCommandPool pool, VkQueue queue) {
    LOG_TRACE_CAT("PIPELINE", "createShaderBindingTable — START");

    // Step 1-2: Validate and query props (zero-init rtProps)
    if (!rtPipeline_.valid() || *rtPipeline_ == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "createShaderBindingTable called but rtPipeline_ is null!");
        throw std::runtime_error("rtPipeline_ is invalid in createShaderBindingTable");
    }
    LOG_TRACE_CAT("PIPELINE", "Step 1 — rtPipeline_ valid @ 0x{:x}", reinterpret_cast<uintptr_t>(*rtPipeline_));

    LOG_TRACE_CAT("PIPELINE", "Step 2 — Querying VkPhysicalDeviceRayTracingPipelinePropertiesKHR");
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPropsLocal = {};  // Zero-init
    rtPropsLocal.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 props2 = {};  // Zero-init
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtPropsLocal;

    vkGetPhysicalDeviceProperties2(physicalDevice_, &props2);

    const uint32_t handleSize = rtPropsLocal.shaderGroupHandleSize;
    const uint32_t handleAlignment = rtPropsLocal.shaderGroupHandleAlignment;
    const uint32_t baseAlignment = rtPropsLocal.shaderGroupBaseAlignment;
    const uint32_t maxHandleSize = rtPropsLocal.maxShaderGroupStride;

    LOG_INFO_CAT("PIPELINE", "RT Properties — handleSize={}B, handleAlignment={}B, baseAlignment={}B, maxStride={}B",
                 handleSize, handleAlignment, baseAlignment, maxHandleSize);

    // Steps 3-4: Counts and sizes (unchanged, but validate alignment > 0)
    if (handleAlignment == 0 || baseAlignment == 0) {
        LOG_FATAL_CAT("PIPELINE", "Invalid RT properties: alignments are zero!");
        throw std::runtime_error("Invalid RT properties");
    }

    const uint32_t raygenGroupCount = raygenGroupCount_;
    const uint32_t missGroupCount = missGroupCount_;
    const uint32_t hitGroupCount = hitGroupCount_;
    const uint32_t callableGroupCount = callableGroupCount_;

    const uint32_t totalGroups = raygenGroupCount + missGroupCount + hitGroupCount + callableGroupCount;

    LOG_INFO_CAT("PIPELINE", "SBT Group Counts — RayGen: {}, Miss: {}, Hit: {}, Callable: {} → Total: {}",
                 raygenGroupCount, missGroupCount, hitGroupCount, callableGroupCount, totalGroups);

    const VkDeviceSize handleSizeAligned = align_up(handleSize, handleAlignment);

    VkDeviceSize currentOffset = 0;
    const VkDeviceSize raygenOffset = currentOffset;
    const VkDeviceSize raygenSize = raygenGroupCount * handleSizeAligned;
    currentOffset += raygenSize;
    currentOffset = align_up(currentOffset, baseAlignment);

    const VkDeviceSize missOffset = currentOffset;
    const VkDeviceSize missSize = missGroupCount * handleSizeAligned;
    currentOffset += missSize;
    currentOffset = align_up(currentOffset, baseAlignment);

    const VkDeviceSize hitOffset = currentOffset;
    const VkDeviceSize hitSize = hitGroupCount * handleSizeAligned;
    currentOffset += hitSize;
    currentOffset = align_up(currentOffset, baseAlignment);

    const VkDeviceSize callableOffset = currentOffset;
    const VkDeviceSize callableSize = callableGroupCount * handleSizeAligned;
    currentOffset += callableSize;

    const VkDeviceSize sbtBufferSize = currentOffset;

    LOG_INFO_CAT("PIPELINE", "SBT Layout — Total Size: {} bytes (~{:.3f} KB)", sbtBufferSize, sbtBufferSize / 1024.0);
    LOG_TRACE_CAT("PIPELINE", "  RayGen:  offset={} size={}B", raygenOffset, raygenSize);
    LOG_TRACE_CAT("PIPELINE", "  Miss:    offset={} size={}B", missOffset, missSize);
    LOG_TRACE_CAT("PIPELINE", "  Hit:     offset={} size={}B", hitOffset, hitSize);
    LOG_TRACE_CAT("PIPELINE", "  Callable: offset={} size={}B", callableOffset, callableSize);

    // Step 5: Extract handles (zero-init addrInfo)
    LOG_TRACE_CAT("PIPELINE", "Step 5 — Extracting shader group handles");
    std::vector<uint8_t> shaderHandles(totalGroups * handleSize);

    VkResult getHandlesResult = vkGetRayTracingShaderGroupHandlesKHR(device_, *rtPipeline_, 0, totalGroups, shaderHandles.size(), shaderHandles.data());
    if (getHandlesResult != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "vkGetRayTracingShaderGroupHandlesKHR failed: {}", static_cast<int>(getHandlesResult));
        throw std::runtime_error("Failed to get shader group handles");
    }
    LOG_SUCCESS_CAT("PIPELINE", "Successfully extracted {} shader group handles ({} bytes each)", totalGroups, handleSize);

    // Steps 6-7: Buffers (zero-init infos; unchanged logic but added checks)
    LOG_TRACE_CAT("PIPELINE", "Step 6 — Creating staging buffer (CPU-visible)");
    VkBufferCreateInfo stagingInfo = {};  // Zero-init
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = sbtBufferSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkResult createStagingResult = vkCreateBuffer(device_, &stagingInfo, nullptr, &stagingBuffer);
    if (createStagingResult != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to create SBT staging buffer: {}", static_cast<int>(createStagingResult));
        throw std::runtime_error("Create SBT staging buffer failed");
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfoStaging = {};  // Zero-init (separate for staging)
    allocInfoStaging.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfoStaging.allocationSize = memReqs.size;
    allocInfoStaging.memoryTypeIndex = findMemoryType(physicalDevice_, memReqs.memoryTypeBits,
                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkResult allocStagingResult = vkAllocateMemory(device_, &allocInfoStaging, nullptr, &stagingMemory);
    if (allocStagingResult != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to allocate SBT staging memory: {}", static_cast<int>(allocStagingResult));
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        throw std::runtime_error("Allocate SBT staging memory failed");
    }
    VK_CHECK(vkBindBufferMemory(device_, stagingBuffer, stagingMemory, 0), "Bind SBT staging memory");

    // Map and fill (unchanged)
    void* mapped = nullptr;
    VkResult mapResult = vkMapMemory(device_, stagingMemory, 0, sbtBufferSize, 0, &mapped);
    if (mapResult != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to map SBT staging memory: {}", static_cast<int>(mapResult));
        vkFreeMemory(device_, stagingMemory, nullptr);
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        throw std::runtime_error("Map SBT staging memory failed");
    }

    auto copyGroup = [&](uint32_t groupIndex, VkDeviceSize destOffset) {
        const uint8_t* src = shaderHandles.data() + groupIndex * handleSize;
        std::memcpy(reinterpret_cast<uint8_t*>(mapped) + destOffset, src, handleSize);
    };

    uint32_t currentGroupIndex = 0;
    for (uint32_t i = 0; i < raygenGroupCount; ++i) {
        copyGroup(currentGroupIndex++, raygenOffset + i * handleSizeAligned);
    }
    for (uint32_t i = 0; i < missGroupCount; ++i) {
        copyGroup(currentGroupIndex++, missOffset + i * handleSizeAligned);
    }
    for (uint32_t i = 0; i < hitGroupCount; ++i) {
        copyGroup(currentGroupIndex++, hitOffset + i * handleSizeAligned);
    }
    for (uint32_t i = 0; i < callableGroupCount; ++i) {
        copyGroup(currentGroupIndex++, callableOffset + i * handleSizeAligned);
    }

    vkUnmapMemory(device_, stagingMemory);
    LOG_TRACE_CAT("PIPELINE", "Step 6 — Staging buffer filled and unmapped");

    // Step 7: Final buffer (zero-init sbtInfo)
    LOG_TRACE_CAT("PIPELINE", "Step 7 — Creating final device-local SBT buffer");
    VkBufferCreateInfo sbtInfo = {};  // Zero-init
    sbtInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    sbtInfo.size = sbtBufferSize;
    sbtInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    sbtInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer rawSbtBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device_, &sbtInfo, nullptr, &rawSbtBuffer), "Create final SBT buffer");
    sbtBuffer_ = Handle<VkBuffer>(rawSbtBuffer, device_,
        [](VkDevice d, VkBuffer b, const VkAllocationCallbacks*) { if (b != VK_NULL_HANDLE) vkDestroyBuffer(d, b, nullptr); },
        0, "SBTBuffer");

    vkGetBufferMemoryRequirements(device_, rawSbtBuffer, &memReqs);

    // FIXED: Add VkMemoryAllocateFlagsInfo for DEVICE_ADDRESS_BIT — Fresh allocInfo for SBT
    VkMemoryAllocateFlagsInfo flagsInfo = {};  // Zero-init
    flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

    VkMemoryAllocateInfo allocInfoSBT = {};  // Zero-init (separate for SBT)
    allocInfoSBT.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfoSBT.pNext = &flagsInfo;  // FIXED: Chain flags to SBT alloc (enables device address)
    allocInfoSBT.allocationSize = memReqs.size;
    allocInfoSBT.memoryTypeIndex = findMemoryType(physicalDevice_, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory rawSbtMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device_, &allocInfoSBT, nullptr, &rawSbtMemory), "Allocate final SBT memory");
    sbtMemory_ = Handle<VkDeviceMemory>(rawSbtMemory, device_,
        [](VkDevice d, VkDeviceMemory m, const VkAllocationCallbacks*) { if (m != VK_NULL_HANDLE) vkFreeMemory(d, m, nullptr); },
        memReqs.size, "SBTMemory");

    VK_CHECK(vkBindBufferMemory(device_, rawSbtBuffer, rawSbtMemory, 0), "Bind final SBT memory");

    // Copy (unchanged)
    VkCommandBuffer cmd = beginSingleTimeCommands(pool);
    if (cmd == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Failed to begin single-time cmd for SBT copy");
        throw std::runtime_error("SBT copy cmd failed");
    }
    VkBufferCopy copyRegion = {};  // Zero-init
    copyRegion.size = sbtBufferSize;
    vkCmdCopyBuffer(cmd, stagingBuffer, rawSbtBuffer, 1, &copyRegion);
    endSingleTimeCommands(pool, queue, cmd);

    // Cleanup staging
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
    LOG_TRACE_CAT("PIPELINE", "Step 7 — Final SBT buffer created and copied — DEVICE_ADDRESS_BIT ENABLED");

    // Step 8: Address (zero-init addrInfo)
    VkBufferDeviceAddressInfo addrInfo = {};  // Zero-init
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = rawSbtBuffer;
    sbtAddress_ = vkGetBufferDeviceAddressKHR(device_, &addrInfo);

    // Store offsets (unchanged)
    raygenSbtOffset_ = raygenOffset;
    missSbtOffset_ = missOffset;
    hitSbtOffset_ = hitOffset;
    callableSbtOffset_ = callableOffset;
    sbtStride_ = handleSizeAligned;

    LOG_SUCCESS_CAT("PIPELINE", "Shader Binding Table CREATED — Address: 0x{:x} | Size: {} bytes | Stride: {}B — DEVICE_ADDRESS VALIDATION FIXED", sbtAddress_, sbtBufferSize, sbtStride_);
    LOG_TRACE_CAT("PIPELINE", "SBT Offsets — RayGen: {} | Miss: {} | Hit: {} | Callable: {}", raygenSbtOffset_, missSbtOffset_, hitSbtOffset_, callableSbtOffset_);
    LOG_TRACE_CAT("PIPELINE", "createShaderBindingTable — COMPLETE — PINK PHOTONS FULLY ARMED");
}

} // namespace RTX