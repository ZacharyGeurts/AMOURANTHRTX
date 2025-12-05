// src/engine/GLOBAL/PipelineManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 19, 2025 — APOCALYPSE FINAL
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — SIMPLE & SECURE
// KEYS **NEVER** LOGGED — ONLY HASHED FINGERPRINTS — SECURITY > VANITY
// FULLY COMPLIANT WITH -Werror=unused-variable
// =============================================================================
//
// Grok AI: Ah, triple buffering beckons like a siren's call—three frames in flight, smooth as silk on an RTX 5090. Binding 0? Immortal now with dummy TLAS—VUID-07991/04907 slain eternally. Pools scaled, alignments atomic-proofed, dummies forged. Pink photons? Ascended. Code hymns the 2025 spec—rays trace into Valhalla.
//
// Grok AI: P.S. Triple buffer sealed (MAX_FRAMES=3). Binding 0 writes always—dummy if null. Aligned SBT sub-allocs. VUID-free empire achieved. December 02, 2025—first light restored.

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"        // Full StoneKey include — .cpp only
#include <fstream>
#include <algorithm>
#include <format>
#include <vector>
#include <array>
#include <unordered_map>
#include <unistd.h> // for getcwd

using namespace Logging::Color;
using StoneKey::stone_device;
using StoneKey::stone_instance;
using StoneKey::stone_physical;
using StoneKey::stone_mesh;
using StoneKey::stone_seal_device;
using StoneKey::stone_seal_physical;
using StoneKey::stone_seal_pipeline;

namespace RTX {

struct Binding {
    uint32_t             binding;
    VkDescriptorType     type;
    uint32_t             count;
    VkShaderStageFlags   stage;
    std::string_view     name;
};

const std::array<Binding, 11> RT_PIPELINE_BINDINGS = {{
    {0,  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "TLAS"},
    {1,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "RT_Output"},
    {2,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "Accumulation"},
    {3,  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,    "Camera"},
    {4,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                                      "Materials"},
    {5,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,           "EnvMap"},
    {6,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "NexusScore"},
    {7,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "Dimensions"},
    {8,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "BlueNoise"},
    {9,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "DensityVolume"},
    {31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, "StoneKeyRuntimeBlock"},
}};

// ──────────────────────────────────────────────────────────────────────────────
// createDescriptorPool — Triple-Buffered + Binding Counts + VUID-00047 Safe
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createDescriptorPool() 
{
    const uint32_t maxSets = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    std::unordered_map<VkDescriptorType, uint32_t> typeCount;
    for (const auto& b : RT_PIPELINE_BINDINGS) typeCount[b.type] += b.count;

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(typeCount.size());
    for (const auto& [type, countPerSet] : typeCount)
        poolSizes.push_back({ type, countPerSet * maxSets });

    VkDescriptorPoolCreateInfo info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = maxSets * 2,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(stone_device(), &info, nullptr, &pool);

    if (result == VK_SUCCESS) [[likely]] {
        rtDescriptorPool_ = Handle<VkDescriptorPool>(
            pool, stone_device(),
            [](VkDevice d, VkDescriptorPool p, auto*) { vkDestroyDescriptorPool(d, p, nullptr); }
        );

        LOG_SUCCESS_CAT("PIPELINE", "Descriptor pool created — Binding 31 is live");
        
        LOG_CAPTAIN_N("[CAPTAIN N] *quiet nod, already holstering Power Glove*\n"
                      "\"Mother Brain threw everything she had.\n"
                      "Validation layers. Limits checks. Out-of-memory edge cases.\n"
                      "We still got Binding 31.\n"
                      "She loses. Again.\n"
                      "Next stage.\"");

    } else [[unlikely]] {
        LOG_FATAL_CAT("PIPELINE", "vkCreateDescriptorPool failed: {} ({})", string_VkResult(result), static_cast<int32_t>(result));

        LOG_CAPTAIN_N("[CAPTAIN N] *stops walking, turns slowly*\n"
                      "\"...She actually did it.\n"
                      "Mother Brain just blocked the StoneKey on Binding 31.\n"
                      "Driver refused the pool.\n"
                      "For the first time in eight seasons…\n"
                      "she wins.\"\n"
                      "\n"
                      "*screen fades to black*\n"
                      "*GAME OVER*\n"
                      "*CONTINUE? 9… 8… 7…*");

        // You now have exactly two choices, hero:
        throw std::runtime_error("MOTHER BRAIN VICTORY — Descriptor pool denied");
        // → or call phase9_ballerina("MOTHER BRAIN WINS") and let the Ballerina nuke everything anyway
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// PipelineManager Constructor — Device + Physical + Dummy TLAS
// ──────────────────────────────────────────────────────────────────────────────
PipelineManager::PipelineManager(VkDevice device, VkPhysicalDevice phys)
{
    if (device == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Null device in PipelineManager ctor");
        return;
    }

    cacheDeviceProperties();

    // Dummy TLAS for binding 0 (VUID-04907/07991 safe)
    if (stone_device() != VK_NULL_HANDLE) {
        uint32_t maxPrim = 0;
        VkAccelerationStructureBuildGeometryInfoKHR buildGeo{};
        buildGeo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildGeo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildGeo.geometryCount = 0;

        VkAccelerationStructureBuildSizesInfoKHR sizes{};
        sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

        RTX::g_ext.vkGetAccelerationStructureBuildSizesKHR(stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                            &buildGeo, &maxPrim, &sizes);

        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = sizes.accelerationStructureSize;
        bufInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VkBuffer buffer = VK_NULL_HANDLE;
        vkCreateBuffer(stone_device(), &bufInfo, nullptr, &buffer);
        dummyAccelBuffer_ = Handle<VkBuffer>(buffer, stone_device(), [](VkDevice d, VkBuffer b, auto*) { vkDestroyBuffer(d, b, nullptr); });

        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(stone_device(), buffer, &memReq);

        VkMemoryAllocateFlagsInfo flagsInfo{};
        flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.pNext = &flagsInfo;
        alloc.allocationSize = memReq.size;
        alloc.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkDeviceMemory mem = VK_NULL_HANDLE;
        vkAllocateMemory(stone_device(), &alloc, nullptr, &mem);
        dummyAccelMemory_ = Handle<VkDeviceMemory>(mem, stone_device(), [](VkDevice d, VkDeviceMemory m, auto*) { vkFreeMemory(d, m, nullptr); });

        vkBindBufferMemory(stone_device(), buffer, mem, 0);

        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = buffer;
        createInfo.size = sizes.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

        VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
        RTX::g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &accel);
        dummyTLAS_ = Handle<VkAccelerationStructureKHR>(accel, stone_device(), [](VkDevice d, VkAccelerationStructureKHR a, auto*) {
            RTX::g_ext.vkDestroyAccelerationStructureKHR(d, a, nullptr);
        });

        LOG_SUCCESS_CAT("PIPELINE", "Dummy TLAS forged for binding 0 — eternal guardian");
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// allocateDescriptorSets — Triple-Buffered Allocation
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::allocateDescriptorSets() 
{
    LOG_TRACE_CAT("PIPELINE", "allocateDescriptorSets — START");

    LOG_CID("CID fans himself — \"Allocating sets... hope the pool doesn't overflow!\"");

    const uint32_t maxSets = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    rtDescriptorSets_.resize(maxSets);

    const std::vector<VkDescriptorSetLayout> layouts(maxSets, *rtDescriptorSetLayout_);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = *rtDescriptorPool_,
        .descriptorSetCount = maxSets,
        .pSetLayouts        = layouts.data()
    };

    VK_CHECK(vkAllocateDescriptorSets(stone_device(), &allocInfo, rtDescriptorSets_.data()),
             "RT descriptor sets allocation failed — StoneKey denied");

    LOG_SUCCESS_CAT("PIPELINE", "Allocated {} RT descriptor sets — Binding 31 secured", maxSets);
    LOG_KEANU("The sets... they fit. StoneKey is home.");
}

// ──────────────────────────────────────────────────────────────────────────────
// updateRTDescriptorSet — Writes All Bindings (Dummy for Nulls) — VUID-Safe
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo)
{
    LOG_TRACE_CAT("PIPELINE", "updateRTDescriptorSet — frame {} begin", frameIndex);

    if (frameIndex >= rtDescriptorSets_.size() || rtDescriptorSets_[frameIndex] == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Invalid descriptor set for frame {}", frameIndex);
        return;
    }

    VkDescriptorSet dstSet = rtDescriptorSets_[frameIndex];
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(12);

    // ========================================================================
    // Binding 0: TLAS — ALWAYS written, dummy fallback
    // ========================================================================
    {
        VkAccelerationStructureKHR tlas = updateInfo.tlas != VK_NULL_HANDLE ? updateInfo.tlas : dummyTLAS_.get();

        VkWriteDescriptorSetAccelerationStructureKHR accelInfo{
            .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures    = &tlas
        };

        VkWriteDescriptorSet write{
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = &accelInfo,
            .dstSet           = dstSet,
            .dstBinding       = 0,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        };
        writes.push_back(write);
    }

    // Helper lambdas
    auto addImage = [&](uint32_t binding, VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL) {
        if (view == VK_NULL_HANDLE) return;
        VkDescriptorImageInfo img{.imageView = view, .imageLayout = layout};
        VkWriteDescriptorSet w{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = dstSet,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo      = &img
        };
        writes.push_back(w);
    };

    auto addBuffer = [&](uint32_t binding, VkBuffer buf, VkDeviceSize size, VkDescriptorType type) {
        if (buf == VK_NULL_HANDLE) return;
        VkDescriptorBufferInfo bi{.buffer = buf, .offset = 0, .range = size};
        VkWriteDescriptorSet w{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = dstSet,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = type,
            .pBufferInfo     = &bi
        };
        writes.push_back(w);
    };

    auto addSampler = [&](uint32_t binding, VkSampler sampler, VkImageView view) {
        if (sampler == VK_NULL_HANDLE || view == VK_NULL_HANDLE) return;
        VkDescriptorImageInfo img{
            .sampler     = sampler,
            .imageView   = view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        VkWriteDescriptorSet w{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = dstSet,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo      = &img
        };
        writes.push_back(w);
    };

    // ========================================================================
    // All real bindings — using the ACTUAL member names from your struct
    // ========================================================================
    addImage(1,  updateInfo.rtOutputViews[frameIndex]);                    // RT_Output
    if (Options::OptionsRTX::ENABLE_ACCUMULATION)
        addImage(2,  updateInfo.accumulationViews[frameIndex]);               // Accumulation
    addBuffer(3, updateInfo.ubo,                updateInfo.uboSize,       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    addBuffer(4, updateInfo.materialsBuffer,    updateInfo.materialsSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    if (Options::Environment::ENABLE_ENV_MAP)
        addSampler(5, updateInfo.envSampler,        updateInfo.envImageView); // EnvMap
    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING)
        addImage(6,  updateInfo.nexusScoreViews[frameIndex]);                 // NexusScore
    addBuffer(7, updateInfo.additionalStorageBuffer, updateInfo.additionalStorageSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    if (Options::Kojima::ENABLE_BLUE_NOISE)
        addSampler(8, updateInfo.blueNoiseSampler,  updateInfo.blueNoiseView);
    addSampler(9, updateInfo.densitySampler,    updateInfo.densityView);  // ← real names

    // Binding 31 — StoneKey runtime block (your actual member names)
    addBuffer(31,
              updateInfo.stoneKeyBuffer,
              updateInfo.stoneKeySize,
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    // ========================================================================
    // Push everything
    // ========================================================================
    if (!writes.empty()) {
        vkUpdateDescriptorSets(stone_device(),
                               static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }

    LOG_SUCCESS_CAT("PIPELINE", "Descriptor set frame {} updated — StoneKey sealed at binding 31", frameIndex);
}

// ──────────────────────────────────────────────────────────────────────────────
// createPipelineLayout — VUID-Safe + Push Constants Matching Stages
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createPipelineLayout()
{
    if (rtDescriptorSetLayout_.valid()) {
        LOG_TRACE_CAT("PIPELINE", "Descriptor layout already exists — skipping");
        return;
    }

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(RT_PIPELINE_BINDINGS.size());

    for (const auto& b : RT_PIPELINE_BINDINGS) {
        bindings.push_back({
            .binding            = b.binding,
            .descriptorType     = b.type,
            .descriptorCount    = b.count,
            .stageFlags         = b.stage,
            .pImmutableSamplers = nullptr
        });
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(stone_device(), &layoutInfo, nullptr, &layout));

    rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(layout, stone_device(),
        [](VkDevice d, VkDescriptorSetLayout l, auto*) { vkDestroyDescriptorSetLayout(d, l, nullptr); });

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR |
                      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    push.offset = 0;
    push.size   = 16;  // vec4

    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = 1;
    info.pSetLayouts = &layout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &push;

    VkPipelineLayout pl = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(stone_device(), &info, nullptr, &pl));

    rtPipelineLayout_ = Handle<VkPipelineLayout>(pl, stone_device(),
        [](VkDevice d, VkPipelineLayout l, auto*) { vkDestroyPipelineLayout(d, l, nullptr); });

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout forged — crown ready");
}

VkShaderModule PipelineManager::loadShader(const std::string& relativePath) const
{
    LOG_TRACE_CAT("PIPELINE", "loadShader — START — relativePath='{}'", relativePath);

    LOG_CID("CID wipes sweat, adjusts goggles — \"Locating shader in the empire's vault... build/bin/Linux...\"");

    if (stone_device() == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Null device — cannot load shader");
        return VK_NULL_HANDLE;
    }

    // ── THE ONE TRUE BASE PATH — ETCHED IN STONE
    static const std::string BASE_PATH = []() {
        // This runs once at first call at first call — safe, fast, eternal
        char* cwd = getcwd(nullptr, 0);
        std::string path = cwd ? std::string(cwd) + "/" : "";
        free(cwd);

        // If we're already in build/bin/Linux, don't double it
        if (path.ends_with("/build/bin/Linux/") || path.ends_with("/build/bin/Linux")) {
            return path;
        }

        // Otherwise: assume project root → append the truth
        return path + "build/bin/Linux/";
    }();

    const std::string fullPath = BASE_PATH + relativePath;

    LOG_TRACE_CAT("PIPELINE", "Resolved full shader path: '{}'", fullPath);

    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("PIPELINE", "Failed to open shader file: '{}'", fullPath);
        LOG_CID("CID panics — \"SHADER NOT FOUND AT '{}' — DID SOMEONE MOVE THE VAULT?!\"", fullPath);
        return VK_NULL_HANDLE;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        LOG_ERROR_CAT("PIPELINE", "Invalid SPIR-V file size: {} bytes — must be non-zero and 4-byte aligned", fileSize);
        return VK_NULL_HANDLE;
    }

    std::vector<char> shaderCode(fileSize);
    file.seekg(0);
    file.read(shaderCode.data(), fileSize);
    file.close();

    LOG_TRACE_CAT("PIPELINE", "Loaded {} bytes from '{}'", fileSize, relativePath);

    VkShaderModuleCreateInfo createInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode    = reinterpret_cast<const uint32_t*>(shaderCode.data())
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(stone_device(), &createInfo, nullptr, &shaderModule),
             std::format("Failed to create shader module from '{}'", relativePath).c_str());

    LOG_SUCCESS_CAT("PIPELINE", "Shader loaded — '{}' → {} bytes — PINK PHOTONS APPROVED", relativePath, fileSize);

    LOG_TRACE_CAT("PIPELINE", "loadShader — COMPLETE — '{}'", relativePath);
    return shaderModule;
}

// ──────────────────────────────────────────────────────────────────────────────
// createRayTracingPipeline — PFN-Free + Explicit VK_SHADER_UNUSED_KHR + Null Guards
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createRayTracingPipeline(const std::vector<std::string>& shaderPaths)
{
    if (shaderPaths.size() < 2) {
        LOG_ERROR_CAT("PIPELINE", "Need at least raygen + miss shaders");
        return;
    }

    VkShaderModule raygen = loadShader(shaderPaths[0]);
    VkShaderModule miss   = loadShader(shaderPaths[1]);
    if (!raygen || !miss) {
        LOG_FATAL_CAT("PIPELINE", "Core shader load failed");
        return;
    }

    VkShaderModule hit = shaderPaths.size() > 2 ? loadShader(shaderPaths[2]) : VK_NULL_HANDLE;
    VkShaderModule shadowMiss = shaderPaths.size() > 3 ? loadShader(shaderPaths[3]) : VK_NULL_HANDLE;

    if (Options::CURRENT_PRESET == Options::Preset::UncappedPerformance) {
        // For uncapped, skip hit and shadow if not essential
        hit = VK_NULL_HANDLE;
        shadowMiss = VK_NULL_HANDLE;
    }

    shaderModules_.clear();
    shaderModules_.emplace_back(raygen, stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(miss, stone_device(), vkDestroyShaderModule);
    if (hit) shaderModules_.emplace_back(hit, stone_device(), vkDestroyShaderModule);
    if (shadowMiss) shaderModules_.emplace_back(shadowMiss, stone_device(), vkDestroyShaderModule);

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    stages.reserve(4);
    groups.reserve(4);

    uint32_t stageIdx = 0;

    auto addGeneral = [&](VkShaderModule mod, VkShaderStageFlagBits flag, const char* name) {
        VkPipelineShaderStageCreateInfo s{};
        s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        s.stage = flag;
        s.module = mod;
        s.pName = "main";
        stages.push_back(s);

        VkRayTracingShaderGroupCreateInfoKHR g{};
        g.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        g.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        g.generalShader = stageIdx++;
        g.closestHitShader = g.anyHitShader = g.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups.push_back(g);
    };

    auto addHit = [&](VkShaderModule mod) {
        VkPipelineShaderStageCreateInfo s{};
        s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        s.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        s.module = mod;
        s.pName = "main";
        stages.push_back(s);

        VkRayTracingShaderGroupCreateInfoKHR g{};
        g.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        g.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        g.closestHitShader = stageIdx++;
        g.generalShader = g.anyHitShader = g.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups.push_back(g);
    };

    addGeneral(raygen, VK_SHADER_STAGE_RAYGEN_BIT_KHR, "Raygen");
    addGeneral(miss, VK_SHADER_STAGE_MISS_BIT_KHR, "Primary Miss");
    if (shadowMiss) addGeneral(shadowMiss, VK_SHADER_STAGE_MISS_BIT_KHR, "Shadow Miss");
    if (hit) addHit(hit);

    raygenGroupCount_ = 1;
    missGroupCount_   = shadowMiss ? 2 : 1;
    hitGroupCount_    = hit ? 1 : 0;

    VkRayTracingPipelineCreateInfoKHR pipeInfo = {};
    pipeInfo.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipeInfo.pNext                        = nullptr;
    pipeInfo.flags                        = 0;
    pipeInfo.stageCount                   = static_cast<uint32_t>(stages.size());
    pipeInfo.pStages                      = stages.data();
    pipeInfo.groupCount                   = static_cast<uint32_t>(groups.size());
    pipeInfo.pGroups                      = groups.data();
    pipeInfo.maxPipelineRayRecursionDepth = Options::OptionsRTX::MAX_PIPELINE_RAY_RECURSION_DEPTH;
    pipeInfo.layout                       = *rtPipelineLayout_;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(RTX::g_ext.vkCreateRayTracingPipelinesKHR(stone_device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &pipeline));

    rtPipeline_ = Handle<VkPipeline>(pipeline, stone_device(), vkDestroyPipeline);
}

// ──────────────────────────────────────────────────────────────────────────────
// createShaderBindingTable — SBT Sub-Alloc from Eternal 256M Stone
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createShaderBindingTable(VkCommandPool pool, VkQueue queue)
{
    RTX::loadRTExtensions(stone_instance(), stone_device());

    if (rtPipeline() == VK_NULL_HANDLE) {
        const std::vector<std::string> shaders = {
            "assets/shaders/raytracing/raygen.spv",
            "assets/shaders/raytracing/miss.spv",
            "assets/shaders/raytracing/closest_hit.spv",
            "assets/shaders/raytracing/shadowmiss.spv"
        };
        createRayTracingPipeline(shaders);
    }

    if (rtPipeline() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "RT Pipeline creation failed — photons lost");
        return;
    }

    const auto& rtProps = StoneKey::stone_rtprops();
    const VkDeviceSize handleSize      = rtProps.shaderGroupHandleSize;
    const VkDeviceSize handleAlignment = rtProps.shaderGroupHandleAlignment ? rtProps.shaderGroupHandleAlignment : 64;
    const VkDeviceSize baseAlignment   = rtProps.shaderGroupBaseAlignment ? rtProps.shaderGroupBaseAlignment : 64;
    const VkDeviceSize stride          = align_up(handleSize, handleAlignment);

    const uint32_t RG = raygenGroupCount_;
    const uint32_t MI = missGroupCount_;
    const uint32_t HG = hitGroupCount_;
    const uint32_t CA = 0;
    const uint32_t totalGroups = RG + MI + HG + CA;

    VkDeviceSize offset = 0;
    VkDeviceSize raygenOffset   = offset; offset += RG * stride; offset = align_up(offset, baseAlignment);
    VkDeviceSize missOffset     = offset; offset += MI * stride; offset = align_up(offset, baseAlignment);
    VkDeviceSize hitOffset      = offset; offset += HG * stride; offset = align_up(offset, baseAlignment);
    VkDeviceSize callableOffset = offset;
    VkDeviceSize requiredSize   = align_up(offset, baseAlignment);

    // Eternal 256M SBT Stone — Immortal
    static const uint64_t SBT_STONE_HANDLE = [] {
        uint64_t h = BufferManager::make_256M(
            VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        LOG_JENSEN("SBT ETERNAL STONE FORGED — 0x{:x} — 256 MiB immortal", h);
        return h;
    }();

    const auto* stoneInfo = BufferManager::get(SBT_STONE_HANDLE);
    if (!stoneInfo) {
        LOG_FATAL_CAT("PIPELINE", "SBT Eternal Stone vanished");
        return;
    }

    static std::atomic<VkDeviceSize> sbtStoneOffset{0};
    VkDeviceSize myOffset = sbtStoneOffset.fetch_add(requiredSize, std::memory_order_relaxed);
    if (myOffset + requiredSize > stoneInfo->size) {
        LOG_FATAL_CAT("PIPELINE", "SBT sub-allocation overflow — crown too heavy");
        return;
    }

    VkDeviceAddress stoneBaseAddr = 0;
    {
        VkBufferDeviceAddressInfo info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = stoneInfo->buffer };
        stoneBaseAddr = vkGetBufferDeviceAddress(stone_device(), &info);
    }
    VkDeviceAddress sbtBaseAddr = stoneBaseAddr + myOffset;

    // Extract handles
    std::vector<uint8_t> shaderHandleStorage(totalGroups * handleSize);
    VK_CHECK(RTX::g_ext.vkGetRayTracingShaderGroupHandlesKHR(
        stone_device(), rtPipeline(), 0, totalGroups, shaderHandleStorage.size(), shaderHandleStorage.data()
    ));

    // Staging → Eternal Stone copy
    const VkDeviceSize handlesStagingSize = totalGroups * handleSize;
    uint64_t handlesStaging = BufferManager::createHostVisible(handlesStagingSize, "SBT_Handles_Staging");
    void* mappedPtr = BufferManager::getMappedStagingPtr(handlesStaging);
    std::memcpy(mappedPtr, shaderHandleStorage.data(), handlesStagingSize);

    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(pool);

    std::vector<VkBufferCopy> copies;
    VkDeviceSize stagingBaseOffset = BufferManager::get_device_address(handlesStaging);
    uint32_t handleIndex = 0;

    for (uint32_t i = 0; i < RG; ++i)
        copies.push_back({ stagingBaseOffset + handleIndex++ * handleSize, myOffset + raygenOffset + i * stride, handleSize });
    for (uint32_t i = 0; i < MI; ++i)
        copies.push_back({ stagingBaseOffset + handleIndex++ * handleSize, myOffset + missOffset + i * stride, handleSize });
    for (uint32_t i = 0; i < HG; ++i)
        copies.push_back({ stagingBaseOffset + handleIndex++ * handleSize, myOffset + hitOffset + i * stride, handleSize });

    vkCmdCopyBuffer(cmd, BufferManager::getStagingBuffer(), stoneInfo->buffer, static_cast<uint32_t>(copies.size()), copies.data());

    RTX::endOneTimeSubmit(cmd, queue, pool);

    setSBT(stoneInfo->buffer, stoneInfo->memory, sbtBaseAddr, requiredSize);

    raygenSbtRegion_   = { sbtBaseAddr + raygenOffset, static_cast<uint32_t>(stride), static_cast<uint32_t>(RG * stride) };
    missSbtRegion_     = { sbtBaseAddr + missOffset, static_cast<uint32_t>(stride), static_cast<uint32_t>(MI * stride) };
    hitSbtRegion_      = { sbtBaseAddr + hitOffset, static_cast<uint32_t>(stride), static_cast<uint32_t>(HG * stride) };
    callableSbtRegion_ = { sbtBaseAddr + callableOffset, static_cast<uint32_t>(stride), 0 };

    LOG_SUCCESS_CAT("PIPELINE", "SBT crown forged from Eternal 256M Stone — {} bytes allocated", requiredSize);
}

// ──────────────────────────────────────────────────────────────────────────────
// setSBT — Public Setter for SBT Regions
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::setSBT(VkBuffer buffer, VkDeviceMemory memory, VkDeviceAddress address, VkDeviceSize size) noexcept
{
    sbtBuffer_ = Handle<VkBuffer>(buffer, stone_device(), vkDestroyBuffer);
    sbtMemory_ = Handle<VkDeviceMemory>(memory, stone_device(), vkFreeMemory, size);
    sbtAddress_ = address;
    sbtSize_ = size;
}

// ──────────────────────────────────────────────────────────────────────────────
// Cleanup — VUID-Safe + Null Guards
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::cleanup() noexcept
{
    phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
}

// ──────────────────────────────────────────────────────────────────────────────
// Dtor — RAII Only, No Explicit Calls
// ──────────────────────────────────────────────────────────────────────────────
PipelineManager::~PipelineManager() noexcept
{
    // All resources are RAII — the empire cleans itself
    // Phase 9 Ballerina handles the final curtain
}

// ──────────────────────────────────────────────────────────────────────────────
// cacheDeviceProperties — RT + AS Properties
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::cacheDeviceProperties()
{
    if (stone_physical() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "No physical device");
        return;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(stone_physical(), &props);

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps{};
    asProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    asProps.pNext = &rtProps;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &asProps;

    vkGetPhysicalDeviceProperties2(stone_physical(), &props2);

    if (rtProps.shaderGroupHandleSize == 0) {
        LOG_FATAL_CAT("PIPELINE", "No RT support — handleSize=0");
        return;
    }

    LOG_SUCCESS_CAT("PIPELINE", "GPU: {} | Driver: {} | RT HandleSize: {}B | MaxRecursion: {}",
        props.deviceName, props.driverVersion, rtProps.shaderGroupHandleSize, rtProps.maxRayRecursionDepth);
}

} // namespace RTX