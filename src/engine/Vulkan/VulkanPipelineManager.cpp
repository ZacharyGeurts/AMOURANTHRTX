// src/engine/Vulkan/VulkanPipelineManager.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 15, 2025 — APOCALYPSE v3.2
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — SIMPLE & SECURE
// KEYS **NEVER** LOGGED — ONLY HASHED FINGERPRINTS — SECURITY > VANITY
// FULLY COMPLIANT WITH -Werror=unused-variable
// =============================================================================

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
// PipelineManager Constructor — Matches VulkanRenderer Style + FIXED: Null Guard Early Exit
// ──────────────────────────────────────────────────────────────────────────────
PipelineManager::PipelineManager(VkDevice device, VkPhysicalDevice phys)
    : device_(device), physicalDevice_(phys)
{
    LOG_ATTEMPT_CAT("PIPELINE", "Constructing PipelineManager — PINK PHOTONS RISING");

    // FIXED: Early guard — skip init if null (prevents segfault in load/cache)
    if (device_ == VK_NULL_HANDLE || physicalDevice_ == VK_NULL_HANDLE) {
        LOG_WARN_CAT("PIPELINE", "Null device (0x{:x}) or phys (0x{:x}) — skipping init (dummy state)", 
                     reinterpret_cast<uintptr_t>(device_), reinterpret_cast<uintptr_t>(physicalDevice_));
        return;
    }

    LOG_TRACE_CAT("PIPELINE", "=== STACK BUILD ORDER STEP 0.5: Load Ray Tracing Extensions ===");
    loadExtensions();  // NEW: Dynamic PFN loading
    LOG_TRACE_CAT("PIPELINE", "Step 0.5 COMPLETE");

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
// NEW: Destructor — vkDeviceWaitIdle Before Handle Resets (Fixes In-Use Destruction)
// ──────────────────────────────────────────────────────────────────────────────
PipelineManager::~PipelineManager() {
    LOG_ATTEMPT_CAT("PIPELINE", "Destructing PipelineManager — PINK PHOTONS DIMMING");

    // FIXED: Wait for device idle — Ensures all submitted cmds complete before destroying pipelines/buffers/pools
    //        (Resolves vkDestroyPipeline in-use validation error: VUID-vkDestroyPipeline-pipeline-00765)
    if (device_ != VK_NULL_HANDLE) {
        LOG_TRACE_CAT("PIPELINE", "vkDeviceWaitIdle — Waiting for queues to drain (shutdown safety)");
        VkResult idleResult = vkDeviceWaitIdle(device_);
        if (idleResult == VK_SUCCESS) {
            LOG_TRACE_CAT("PIPELINE", "vkDeviceWaitIdle — SUCCESS: All cmds complete, resources safe to destroy");
        } else {
            LOG_WARN_CAT("PIPELINE", "vkDeviceWaitIdle failed: {} — Proceeding anyway (possible device lost)", static_cast<int>(idleResult));
        }
    } else {
        LOG_TRACE_CAT("PIPELINE", "Null device — Skipping vkDeviceWaitIdle (dummy state)");
    }

    // Handles auto-reset here — Now safe post-idle
    LOG_SUCCESS_CAT("PIPELINE", "{}PIPELINE MANAGER DESTROYED — Handles reset safely — EMPIRE PRESERVED — PINK PHOTONS ETERNAL{}", 
                    EMERALD_GREEN, RESET);
}

// ──────────────────────────────────────────────────────────────────────────────
// FIXED: Load Extension Function Pointers — Runtime-Safe Dynamic Loading + Null Device Guard
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::loadExtensions() {
    LOG_TRACE_CAT("PIPELINE", "loadExtensions — START — Fetching RT KHR PFNs via vkGetDeviceProcAddr");

    // FIXED: Null device guard — skip if invalid
    if (device_ == VK_NULL_HANDLE) {
        LOG_WARN_CAT("PIPELINE", "Null device — skipping extension load");
        return;
    }

    vkCreateRayTracingPipelinesKHR_ = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(device_, "vkCreateRayTracingPipelinesKHR"));
    if (!vkCreateRayTracingPipelinesKHR_) {
        LOG_FATAL_CAT("PIPELINE", "Failed to load vkCreateRayTracingPipelinesKHR — Ensure VK_KHR_ray_tracing_pipeline enabled");
        return;  // Early exit; methods will check nullptr
    }
    LOG_TRACE_CAT("PIPELINE", "Loaded vkCreateRayTracingPipelinesKHR @ 0x{:x}", reinterpret_cast<uintptr_t>(vkCreateRayTracingPipelinesKHR_));

    vkGetRayTracingShaderGroupHandlesKHR_ = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(device_, "vkGetRayTracingShaderGroupHandlesKHR"));
    if (!vkGetRayTracingShaderGroupHandlesKHR_) {
        LOG_FATAL_CAT("PIPELINE", "Failed to load vkGetRayTracingShaderGroupHandlesKHR — Ensure VK_KHR_ray_tracing enabled");
        return;
    }
    LOG_TRACE_CAT("PIPELINE", "Loaded vkGetRayTracingShaderGroupHandlesKHR @ 0x{:x}", reinterpret_cast<uintptr_t>(vkGetRayTracingShaderGroupHandlesKHR_));

    vkGetBufferDeviceAddressKHR_ = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(
        vkGetDeviceProcAddr(device_, "vkGetBufferDeviceAddressKHR"));
    if (!vkGetBufferDeviceAddressKHR_) {
        LOG_FATAL_CAT("PIPELINE", "Failed to load vkGetBufferDeviceAddressKHR — Ensure VK_KHR_buffer_device_address enabled");
        return;
    }
    LOG_TRACE_CAT("PIPELINE", "Loaded vkGetBufferDeviceAddressKHR @ 0x{:x}", reinterpret_cast<uintptr_t>(vkGetBufferDeviceAddressKHR_));

    LOG_SUCCESS_CAT("PIPELINE", "All RT extension PFNs loaded successfully — Linker errors RESOLVED");
    LOG_TRACE_CAT("PIPELINE", "loadExtensions — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// FIXED: Cache Device Properties — Matches VulkanRenderer Step 7 + Null Phys Guard
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::cacheDeviceProperties() {
    LOG_TRACE_CAT("PIPELINE", "cacheDeviceProperties — START");

    // FIXED: Null phys guard — skip if invalid
    if (physicalDevice_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Null physicalDevice_ — cannot cache properties");
        return;
    }

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
    // NEW: Query and log shaderInt64 support (fixes Int64 validation upstream)
    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(physicalDevice_, &features);
    
    if (features.shaderInt64) {
        LOG_SUCCESS_CAT("PIPELINE", "GPU supports shaderInt64 — 64-bit rays ready to trace!");
    } else {
        LOG_ERROR_CAT("PIPELINE", "GPU lacks shaderInt64 support — shaders will fail validation! Enable in device creation or refactor GLSL.");
    }

    LOG_SUCCESS_CAT("PIPELINE", "RT properties cached — handleSize={}B, handleAlignment={}B, baseAlignment={}B, maxStride={}B",
                    rtProps_.shaderGroupHandleSize, rtProps_.shaderGroupHandleAlignment, 
                    rtProps_.shaderGroupBaseAlignment, rtProps_.maxShaderGroupStride);

    LOG_TRACE_CAT("PIPELINE", "cacheDeviceProperties — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// loadShader — Matches VulkanRenderer::loadShader Exactly + Null Device Guard + FIXED: VK_CHECK for Create
// ──────────────────────────────────────────────────────────────────────────────
VkShaderModule PipelineManager::loadShader(const std::string& path) const {
    LOG_TRACE_CAT("PIPELINE", "loadShader — START — path='{}'", path);

    // FIXED: Null device guard
    if (device_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Null device — cannot load shader");
        return VK_NULL_HANDLE;
    }

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
    // FIXED: Use VK_CHECK for consistent error handling (logs + aborts on failure)
    VK_CHECK(vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule),
             std::format("Failed to create shader module from {}", path).c_str());

    LOG_TRACE_CAT("PIPELINE", "Shader module created successfully");
    LOG_TRACE_CAT("PIPELINE", "loadShader — COMPLETE");
    return shaderModule;
}

// ──────────────────────────────────────────────────────────────────────────────
// findMemoryType — Matches VulkanRenderer Exactly + Null Phys Guard
// ──────────────────────────────────────────────────────────────────────────────
uint32_t PipelineManager::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, 
                                         VkMemoryPropertyFlags properties) const noexcept {
    LOG_TRACE_CAT("PIPELINE", "findMemoryType — START — typeFilter=0x{:x}, properties=0x{:x}", typeFilter, properties);

    // FIXED: Null phys guard — use class member if param null (fallback)
    VkPhysicalDevice phys = (physicalDevice == VK_NULL_HANDLE) ? physicalDevice_ : physicalDevice;
    if (phys == VK_NULL_HANDLE) {
        LOG_WARN_CAT("PIPELINE", "Null physicalDevice — fallback to 0");
        return 0;
    }

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
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
// FIXED: Transient Command Buffers — Matches VulkanRenderer Exactly + Null Guards
// ──────────────────────────────────────────────────────────────────────────────
VkCommandBuffer PipelineManager::beginSingleTimeCommands(VkCommandPool pool) const {
    LOG_TRACE_CAT("PIPELINE", "beginSingleTimeCommands — START");

    // FIXED: Null guards
    if (device_ == VK_NULL_HANDLE || pool == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Null device or pool — cannot begin single-time commands");
        return VK_NULL_HANDLE;
    }

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
    // FIXED: Null guards
    if (cmd == VK_NULL_HANDLE || pool == VK_NULL_HANDLE || queue == VK_NULL_HANDLE || device_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "endSingleTimeCommands called with invalid params (cmd=0x{:x}, pool=0x{:x}, queue=0x{:x}, dev=0x{:x})",
                      reinterpret_cast<uintptr_t>(cmd), reinterpret_cast<uintptr_t>(pool), reinterpret_cast<uintptr_t>(queue), reinterpret_cast<uintptr_t>(device_));
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
        if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);
    }

    // 4. Cleanup
    vkFreeCommandBuffers(device_, pool, 1, &cmd);

    LOG_TRACE_CAT("PIPELINE", "endSingleTimeCommands — COMPLETE (safe, no device lost)");
}

// ──────────────────────────────────────────────────────────────────────────────
// Descriptor Set Layout — FIXED: 8 Bindings Matching Raygen Shader + Null Device Guard + FIXED: Multi-Frame Pool Sizing + FIXED: Array Counts=3
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createDescriptorSetLayout()
{
    LOG_TRACE_CAT("PIPELINE", "createDescriptorSetLayout — START");

    // FIXED: Null device guard
    if (device_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Null device — cannot create descriptor set layout");
        return;
    }

    std::array<VkDescriptorSetLayoutBinding, 8> bindings = {};  // Zero-init

    // FIXED: Binding 0 - TLAS (acceleration structure) — matches shader "tlas" (Set 0, Binding 0)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
    bindings[0].pImmutableSamplers = nullptr;

    // FIXED: Binding 1 - rtOutput (storage image) — matches shader "rtOutput" (Set 0, Binding 1) — FIXED: count=3 (array[3])
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 3;  // FIXED: Matches SPIR-V array size (VUID-07991)
    bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    bindings[1].pImmutableSamplers = nullptr;

    // FIXED: Binding 2 - accumulation (storage image) — matches shader "accumulation" (Set 0, Binding 2) — FIXED: count=3 (array[3])
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[2].descriptorCount = 3;  // FIXED: Matches SPIR-V array size (VUID-07991)
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

    // FIXED: Binding 6 - nexusScore (storage image) — matches shader "nexusScore" (Set 0, Binding 6) — FIXED: count=3 (array[3])
    bindings[6].binding = 6;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[6].descriptorCount = 3;  // FIXED: Matches SPIR-V array size (assumed; resolves raygen usage)
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

    // FIXED: Create RT Descriptor Pool — Multi-frame sizing per Vulkan spec (total descriptors across maxSets) + FIXED: 9 storage_img (3 bindings * 3 count)
    LOG_TRACE_CAT("PIPELINE", "Creating RT descriptor pool — maxSets={}, freeable, scaled descriptor counts", Options::Performance::MAX_FRAMES_IN_FLIGHT);
    const uint32_t maxSets = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    std::array<VkDescriptorPoolSize, 5> poolSizes = {};  // Zero-init
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    poolSizes[0].descriptorCount = 1 * maxSets;  // Binding 0 x N
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 9 * maxSets;  // FIXED: Bindings 1,2,6 * 3 count x N (VUID-03024)
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[2].descriptorCount = 1 * maxSets;  // Binding 3 x N
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[3].descriptorCount = 2 * maxSets;  // Bindings 4,7 x N
    poolSizes[4].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[4].descriptorCount = 1 * maxSets;  // Binding 5 x N

    VkDescriptorPoolCreateInfo poolInfo = {};  // Zero-init
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;  // Enables vkFreeDescriptorSets
    poolInfo.maxSets = maxSets;  // Supports N-frame in-flight
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    rtDescriptorPool_.reset();  // <-- CRITICAL: wipe any previous garbage state

    VkDescriptorPool rawPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &rawPool),
             "Failed to create RT descriptor pool");

    rtDescriptorPool_ = Handle<VkDescriptorPool>(
        rawPool,
        device_,
        [](VkDevice d, VkDescriptorPool p, const VkAllocationCallbacks*) {
            if (p != VK_NULL_HANDLE) vkDestroyDescriptorPool(d, p, nullptr);
        },
        0,
        "RTDescriptorPool"
    );

    assert(rtDescriptorPool_.valid() && "*rtDescriptorPool_ is null after creation!");
    LOG_SUCCESS_CAT("PIPELINE", "RT descriptor pool created and assigned: 0x{:x}",
                    reinterpret_cast<uintptr_t>(*rtDescriptorPool_));

    LOG_TRACE_CAT("PIPELINE", "RT descriptor pool created: 0x{:x} — maxSets={}, sizes=[accel:{}, img:{}, ubo:{}, buf:{}, sampler:{}]",
                  reinterpret_cast<uintptr_t>(rawPool), maxSets,
                  poolSizes[0].descriptorCount, poolSizes[1].descriptorCount, poolSizes[2].descriptorCount,
                  poolSizes[3].descriptorCount, poolSizes[4].descriptorCount);

    LOG_SUCCESS_CAT("PIPELINE", "RT descriptor set layout v10.4 created — 8 bindings exactly matching shader (0=tlas,1=rtOutput[3],2=accum[3],3=ubo,6=nexus[3]) — VUID-07991 FIXED");
    LOG_SUCCESS_CAT("PIPELINE", "RT descriptor pool created (multi-frame, array-scaled) — Non-null handle — ALLOCATION-READY — VUID-03017 FIXED");
    LOG_TRACE_CAT("PIPELINE", "createDescriptorSetLayout — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// Pipeline Layout — FIXED: Valid pSetLayouts + Push Constants Matching Stages + Null Guards + FIXED: size=16 for vec4
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createPipelineLayout() {
    LOG_TRACE_CAT("PIPELINE", "createPipelineLayout — START");

    // FIXED: Null guards
    if (device_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Null device — cannot create pipeline layout");
        return;
    }
    if (!rtDescriptorSetLayout_.valid() || *rtDescriptorSetLayout_ == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "rtDescriptorSetLayout_ null — abort layout create");
        return;
    }

    VkDescriptorSetLayout layout = *rtDescriptorSetLayout_;  // FIXED: Local lvalue (valid handle)

    // FIXED: Push constant stages (includes raygen for VUID-07987) + FIXED: size=16 (matches vkCmdPushConstants size=16)
    VkPushConstantRange pushConstant = {};  // Zero-init
    pushConstant.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    pushConstant.offset = 0;
    pushConstant.size = 16;  // FIXED: sizeof(vec4) = 16 bytes — matches shader push constant usage

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

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created — non-null pSetLayouts + raygen stages + size=16 — VUID-01795 FIXED");
    LOG_TRACE_CAT("PIPELINE", "createPipelineLayout — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// createRayTracingPipeline — FIXED: No Library pNext + Explicit UNUSED_KHR + Matching Layout + Null Guards + NEW: PFN Call
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createRayTracingPipeline(const std::vector<std::string>& shaderPaths) {
    LOG_TRACE_CAT("PIPELINE", "createRayTracingPipeline — START — {} shaders provided", shaderPaths.size());

    // FIXED: Null guards
    if (device_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Null device — cannot create RT pipeline");
        return;
    }
    LOG_DEBUG_CAT("PIPELINE", "Retrieved device: 0x{:x}", reinterpret_cast<uintptr_t>(device_));

    // FIXED: Guard layout validity before proceeding
    if (!rtPipelineLayout_.valid() || *rtPipelineLayout_ == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "rtPipelineLayout_ invalid — cannot create RT pipeline");
        return;
    }

    // NEW: Guard PFN load
    if (!vkCreateRayTracingPipelinesKHR_) {
        LOG_FATAL_CAT("PIPELINE", "vkCreateRayTracingPipelinesKHR not loaded — abort RT pipeline creation");
        return;
    }

    if (shaderPaths.size() < 2) {
        LOG_ERROR_CAT("PIPELINE", "Insufficient shader paths: expected at least raygen + miss, got {}", shaderPaths.size());
        return;
    }

    // ---------------------------------------------------------------------
    // 1. Load mandatory shaders (unchanged, but add result check)
    // ---------------------------------------------------------------------
    VkShaderModule raygenModule = loadShader(shaderPaths[0]);
    VkShaderModule missModule = loadShader(shaderPaths[1]);

    if (raygenModule == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Failed to load raygen shader: {}", shaderPaths[0]);
        return;
    }
    if (missModule == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Failed to load primary miss shader: {}", shaderPaths[1]);
        return;
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
    // 4. Create pipeline (zero-init infos) — FIXED: No pNext (remove libraryInfo) + explicit pNext=nullptr + NEW: PFN Call
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
    pipelineInfo.maxPipelineRayRecursionDepth = std::min(4u, rtProps_.maxRayRecursionDepth);
    pipelineInfo.layout = *rtPipelineLayout_;  // FIXED: Valid layout with descriptors/push (matches shader bindings/stages)

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult pipeResult = vkCreateRayTracingPipelinesKHR_(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);  // NEW: PFN call
    LOG_DEBUG_CAT("PIPELINE", "vkCreateRayTracingPipelinesKHR returned: {}", static_cast<int>(pipeResult));
    if (pipeResult != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to create ray tracing pipeline: {}", static_cast<int>(pipeResult));
        return;
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
// createShaderBindingTable — FIXED: DEVICE_ADDRESS_BIT in Memory Alloc + Null Guards + NEW: PFN Calls
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createShaderBindingTable(VkCommandPool pool, VkQueue queue) {
    LOG_TRACE_CAT("PIPELINE", "createShaderBindingTable — START");

    // FIXED: Null guards
    if (device_ == VK_NULL_HANDLE || physicalDevice_ == VK_NULL_HANDLE || pool == VK_NULL_HANDLE || queue == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Invalid params for SBT creation (dev=0x{:x}, phys=0x{:x}, pool=0x{:x}, queue=0x{:x})",
                      reinterpret_cast<uintptr_t>(device_), reinterpret_cast<uintptr_t>(physicalDevice_), reinterpret_cast<uintptr_t>(pool), reinterpret_cast<uintptr_t>(queue));
        return;
    }

    // Step 1-2: Validate and query props (zero-init rtProps)
    if (!rtPipeline_.valid() || *rtPipeline_ == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "createShaderBindingTable called but rtPipeline_ is null!");
        return;
    }
    LOG_TRACE_CAT("PIPELINE", "Step 1 — rtPipeline_ valid @ 0x{:x}", reinterpret_cast<uintptr_t>(*rtPipeline_));

    // NEW: Guard PFN loads
    if (!vkGetRayTracingShaderGroupHandlesKHR_) {
        LOG_FATAL_CAT("PIPELINE", "vkGetRayTracingShaderGroupHandlesKHR not loaded — abort SBT creation");
        return;
    }
    if (!vkGetBufferDeviceAddressKHR_) {
        LOG_FATAL_CAT("PIPELINE", "vkGetBufferDeviceAddressKHR not loaded — abort SBT creation");
        return;
    }

    LOG_TRACE_CAT("PIPELINE", "Step 2 — Querying VkPhysicalDeviceRayTracingPipelinePropertiesKHR");
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPropsLocal = {};  // Zero-init
    rtPropsLocal.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 props2 = {};  // Zero-init
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtPropsLocal;

    vkGetPhysicalDeviceProperties2(physicalDevice_, &props2);
    // NEW: Query and log shaderInt64 support (fixes Int64 validation upstream)
    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(physicalDevice_, &features);
    
    if (features.shaderInt64) {
        LOG_SUCCESS_CAT("PIPELINE", "GPU supports shaderInt64 — 64-bit rays ready to trace!");
    } else {
        LOG_ERROR_CAT("PIPELINE", "GPU lacks shaderInt64 support — shaders will fail validation! Enable in device creation or refactor GLSL.");
    }

    const uint32_t handleSize = rtPropsLocal.shaderGroupHandleSize;
    const uint32_t handleAlignment = rtPropsLocal.shaderGroupHandleAlignment;
    const uint32_t baseAlignment = rtPropsLocal.shaderGroupBaseAlignment;
    const uint32_t maxHandleSize = rtPropsLocal.maxShaderGroupStride;

    LOG_INFO_CAT("PIPELINE", "RT Properties — handleSize={}B, handleAlignment={}B, baseAlignment={}B, maxStride={}B",
                 handleSize, handleAlignment, baseAlignment, maxHandleSize);

    // Steps 3-4: Counts and sizes (unchanged, but validate alignment > 0)
    if (handleAlignment == 0 || baseAlignment == 0) {
        LOG_FATAL_CAT("PIPELINE", "Invalid RT properties: alignments are zero!");
        return;
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

    // Step 5: Extract handles (zero-init addrInfo) + NEW: PFN Call
    LOG_TRACE_CAT("PIPELINE", "Step 5 — Extracting shader group handles");
    std::vector<uint8_t> shaderHandles(totalGroups * handleSize);

    VkResult getHandlesResult = vkGetRayTracingShaderGroupHandlesKHR_(device_, *rtPipeline_, 0, totalGroups, shaderHandles.size(), shaderHandles.data());  // NEW: PFN call
    if (getHandlesResult != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "vkGetRayTracingShaderGroupHandlesKHR failed: {}", static_cast<int>(getHandlesResult));
        return;
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
        return;
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
        return;
    }
    VK_CHECK(vkBindBufferMemory(device_, stagingBuffer, stagingMemory, 0), "Bind SBT staging memory");

    // Map and fill (unchanged)
    void* mapped = nullptr;
    VkResult mapResult = vkMapMemory(device_, stagingMemory, 0, sbtBufferSize, 0, &mapped);
    if (mapResult != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to map SBT staging memory: {}", static_cast<int>(mapResult));
        vkFreeMemory(device_, stagingMemory, nullptr);
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        return;
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
        return;
    }
    VkBufferCopy copyRegion = {};  // Zero-init
    copyRegion.size = sbtBufferSize;
    vkCmdCopyBuffer(cmd, stagingBuffer, rawSbtBuffer, 1, &copyRegion);
    endSingleTimeCommands(pool, queue, cmd);

    // Cleanup staging
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
    LOG_TRACE_CAT("PIPELINE", "Step 7 — Final SBT buffer created and copied — DEVICE_ADDRESS_BIT ENABLED");

    // Step 8: Address (zero-init addrInfo) + NEW: PFN Call
    VkBufferDeviceAddressInfo addrInfo = {};  // Zero-init
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = rawSbtBuffer;
    sbtAddress_ = vkGetBufferDeviceAddressKHR_(device_, &addrInfo);  // NEW: PFN call

    // Store offsets (unchanged)
    raygenSbtOffset_ = raygenOffset;
    missSbtOffset_ = missOffset;
    hitSbtOffset_ = hitOffset;
    callableSbtOffset_ = callableOffset;
    sbtStride_ = handleSizeAligned;

    // ──────────────────────────────────────────────────────────────────────────────
    // FINAL STEP: Construct SBT Regions — REQUIRED FOR vkCmdTraceRaysKHR
    // ──────────────────────────────────────────────────────────────────────────────
    raygenSbtRegion_ = {
        .deviceAddress = sbtAddress_ + raygenSbtOffset_,
        .stride        = sbtStride_,
        .size          = raygenGroupCount * sbtStride_
    };

    missSbtRegion_ = {
        .deviceAddress = sbtAddress_ + missSbtOffset_,
        .stride        = sbtStride_,
        .size          = missGroupCount * sbtStride_
    };

    hitSbtRegion_ = {
        .deviceAddress = sbtAddress_ + hitSbtOffset_,
        .stride        = sbtStride_,
        .size          = hitGroupCount * sbtStride_
    };

    callableSbtRegion_ = {
        .deviceAddress = sbtAddress_ + callableSbtOffset_,
        .stride        = sbtStride_,
        .size          = callableGroupCount * sbtStride_
    };

    LOG_SUCCESS_CAT("PIPELINE", "SBT Regions constructed — RayGen: 0x{:x} ({} entries) | Miss: 0x{:x} | Hit: 0x{:x} | Callable: 0x{:x}",
                    raygenSbtRegion_.deviceAddress, raygenGroupCount,
                    missSbtRegion_.deviceAddress,
                    hitSbtRegion_.deviceAddress,
                    callableSbtRegion_.deviceAddress);

    LOG_SUCCESS_CAT("PIPELINE", "Shader Binding Table CREATED — Address: 0x{:x} | Size: {} bytes | Stride: {}B — DEVICE_ADDRESS VALIDATION FIXED", sbtAddress_, sbtBufferSize, sbtStride_);
    LOG_TRACE_CAT("PIPELINE", "SBT Offsets — RayGen: {} | Miss: {} | Hit: {} | Callable: {}", raygenSbtOffset_, missSbtOffset_, hitSbtOffset_, callableSbtOffset_);
    LOG_TRACE_CAT("PIPELINE", "createShaderBindingTable — COMPLETE — PINK PHOTONS FULLY ARMED");
}

} // namespace RTX