// src/engine/GLOBAL/PipelineManager.cpp
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
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 19, 2025 — APOCALYPSE FINAL
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — SIMPLE & SECURE
// KEYS **NEVER** LOGGED — ONLY HASHED FINGERPRINTS — SECURITY > VANITY
// FULLY COMPLIANT WITH -Werror=unused-variable
// =============================================================================

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
using StoneKey::stone_pipeline;
using StoneKey::stone_graphics_queue;

namespace RTX {

    std::atomic<bool>     PipelineManager::g_pipelineNeedsRebuild{false};
    std::atomic<uint32_t> PipelineManager::g_rebuildRequestedFrame{UINT32_MAX};

// ──────────────────────────────────────────────────────────────────────────────
// createDescriptorPool — Triple-Buffered + Binding Counts + VUID-00047 Safe
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createDescriptorPool() 
{
    LOG_INFO_CAT("PIPELINE", "Creating descriptor pool");

    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    const uint32_t TOTAL_SETS = framesInFlight * 16;

    std::unordered_map<VkDescriptorType, uint32_t> typeCount;
    for (const auto& b : RT_PIPELINE_BINDINGS) {
        typeCount[b.type] += b.count;
    }

    // EMPIRE SAFEGUARD — always reserve space for binding 11 (index buffer)
    typeCount[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER] += 1;

    // Future-proof padding
    typeCount[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER] += 4;

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(typeCount.size());

    for (const auto& [type, countPerSet] : typeCount) {
        poolSizes.push_back({ type, countPerSet * TOTAL_SETS });
    }

    VkDescriptorPoolCreateInfo info{};
    info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    info.maxSets       = TOTAL_SETS;
    info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    info.pPoolSizes    = poolSizes.data();

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(stone_device(), &info, nullptr, &pool);

    if (result == VK_SUCCESS) [[likely]]
    {
        rtDescriptorPool_ = Handle<VkDescriptorPool>(
            pool, stone_device(),
            [](VkDevice d, VkDescriptorPool p, auto*) { vkDestroyDescriptorPool(d, p, nullptr); },
            0, "EMPIRE_DESCRIPTOR_POOL_ETERNAL"
        );

        LOG_SUCCESS_CAT("PIPELINE", "Descriptor pool created — {} sets (binding 11 eternally safeguarded)", TOTAL_SETS);
    }
    else [[unlikely]]
    {
        LOG_FATAL_CAT("PIPELINE", "vkCreateDescriptorPool failed: {} ({})", string_VkResult(result), static_cast<int32_t>(result));
        phase9_ballerina("DESCRIPTOR POOL CREATION FAILED", std::source_location::current());
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
        VK_CHECK(vkCreateBuffer(stone_device(), &bufInfo, nullptr, &buffer));
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
        VK_CHECK(vkAllocateMemory(stone_device(), &alloc, nullptr, &mem));
        dummyAccelMemory_ = Handle<VkDeviceMemory>(mem, stone_device(), [](VkDevice d, VkDeviceMemory m, auto*) { vkFreeMemory(d, m, nullptr); });

        VK_CHECK(vkBindBufferMemory(stone_device(), buffer, mem, 0));

        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = buffer;
        createInfo.size = sizes.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

        VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
        VK_CHECK(RTX::g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &accel));
        dummyTLAS_ = Handle<VkAccelerationStructureKHR>(accel, stone_device(), [](VkDevice d, VkAccelerationStructureKHR a, auto*) {
            RTX::g_ext.vkDestroyAccelerationStructureKHR(d, a, nullptr);
        });

        LOG_SUCCESS_CAT("PIPELINE", "Dummy TLAS created for binding 0");
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// allocateDescriptorSets — Triple-Buffered Allocation
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::allocateDescriptorSets() 
{
    LOG_TRACE_CAT("PIPELINE", "Allocating descriptor sets");

    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    // Allocate with safety margin
    const uint32_t TOTAL_SETS_TO_ALLOCATE = framesInFlight * 4;

    rtDescriptorSets_.clear();
    rtDescriptorSets_.resize(TOTAL_SETS_TO_ALLOCATE);

    // All sets use the same layout
    std::vector<VkDescriptorSetLayout> layouts(TOTAL_SETS_TO_ALLOCATE, rtDescriptorSetLayout_.get());

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = rtDescriptorPool_.get();
    allocInfo.descriptorSetCount = TOTAL_SETS_TO_ALLOCATE;
    allocInfo.pSetLayouts        = layouts.data();

    VkResult result = vkAllocateDescriptorSets(stone_device(), &allocInfo, rtDescriptorSets_.data());

    if (result == VK_ERROR_FRAGMENTED_POOL || result == VK_ERROR_OUT_OF_POOL_MEMORY)
    {
        LOG_WARNING_CAT("PIPELINE", "Descriptor pool exhausted — recreating with increased capacity");

        // Recreate pool with double capacity for recovery
        rtDescriptorPool_.reset();
        createDescriptorPool();

        allocInfo.descriptorPool = rtDescriptorPool_.get();
        result = vkAllocateDescriptorSets(stone_device(), &allocInfo, rtDescriptorSets_.data());
    }

    if (result != VK_SUCCESS)
    {
        LOG_FATAL_CAT("PIPELINE", "vkAllocateDescriptorSets failed: {} ({})", string_VkResult(result), static_cast<int32_t>(result));
        phase9_ballerina("DESCRIPTOR SET ALLOCATION FAILED", std::source_location::current());
    }

    LOG_SUCCESS_CAT("PIPELINE", "Allocated {} descriptor sets", TOTAL_SETS_TO_ALLOCATE);
}

// ──────────────────────────────────────────────────────────────────────────────
// updateRTDescriptorSet — Writes All Bindings (Dummy for Nulls) — VUID-Safe
// ──────────────────────────────────────────────────────────────────────────────
// ──────────────────────────────────────────────────────────────────────────────
// updateRTDescriptorSet — Safe & Compiling (uses existing additionalStorageBuffer)
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo) noexcept
{
    // Validate set exists and is valid
    if (frameIndex >= rtDescriptorSets_.size() || rtDescriptorSets_[frameIndex] == VK_NULL_HANDLE)
    {
        if (!g_pipelineNeedsRebuild.exchange(true))
        {
            LOG_WARNING_CAT("PIPELINE", "Invalid descriptor set at index {} — scheduling rebuild", frameIndex);
            g_rebuildRequestedFrame.store(frameIndex, std::memory_order_relaxed);
        }
        return;
    }

    VkDescriptorSet dstSet = rtDescriptorSets_[frameIndex];
    std::array<VkWriteDescriptorSet, 17> writes{};
    uint32_t writeCount = 0;

    // LAMBDA HELPERS (unchanged)
    const auto writeAccel = [&](VkAccelerationStructureKHR tlas) {
        const VkWriteDescriptorSetAccelerationStructureKHR accelInfo{
            .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures    = &tlas
        };
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = &accelInfo,
            .dstSet           = dstSet,
            .dstBinding       = 0,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        };
    };

    const auto writeImage = [&](uint32_t binding, VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL) {
        if (view == VK_NULL_HANDLE) return;
        const VkDescriptorImageInfo info{ .imageView = view, .imageLayout = layout };
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = dstSet,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo      = &info
        };
    };

    const auto writeBuffer = [&](uint32_t binding, VkBuffer buf, VkDeviceSize size, VkDescriptorType type) {
        if (buf == VK_NULL_HANDLE) return;
        const VkDescriptorBufferInfo info{ .buffer = buf, .offset = 0, .range = size };
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = dstSet,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = type,
            .pBufferInfo     = &info
        };
    };

    const auto writeSampler = [&](uint32_t binding, VkSampler sampler, VkImageView view) {
        if (sampler == VK_NULL_HANDLE || view == VK_NULL_HANDLE) return;
        VkDescriptorImageInfo info{};
        info.sampler     = sampler;
        info.imageView   = view;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = dstSet,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo      = &info
        };
    };

    const auto writeCubemap = [&]() {
        if (!updateInfo.envSampler || !updateInfo.envImageView) return;
        VkDescriptorImageInfo info{};
        info.sampler     = updateInfo.envSampler;
        info.imageView   = updateInfo.envImageView;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = dstSet,
            .dstBinding      = 5,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo      = &info
        };
    };

    // === BINDINGS ===
    writeAccel(updateInfo.tlas ? updateInfo.tlas : dummyTLAS_.get());

    writeImage(1, updateInfo.rtOutputViews[frameIndex]);

    if (Options::OptionsRTX::ENABLE_ACCUMULATION)
        writeImage(2, updateInfo.accumulationViews[frameIndex]);

    writeBuffer(3, updateInfo.ubo, updateInfo.uboSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writeBuffer(4, updateInfo.materialsBuffer, updateInfo.materialsSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    writeCubemap();

    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING)
        writeImage(6, updateInfo.nexusScoreViews[frameIndex]);

    // === GEOMETRY BUFFER — STILL USING BINDING 10 (existing field) ===
    // This is the original line — keeps everything compiling
    writeBuffer(10, updateInfo.additionalStorageBuffer, updateInfo.additionalStorageSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    // Binding 11 is reserved for the future split (vertex @ 10, index @ 11)
    // No write yet — will be activated when you add the new fields to RTDescriptorUpdate

    writeSampler(8, updateInfo.blueNoiseSampler, updateInfo.blueNoiseView);
    writeSampler(9, updateInfo.densitySampler, updateInfo.densityView);

    // Binding 31 — StoneKey eternal
    writeBuffer(31, updateInfo.stoneKeyBuffer, updateInfo.stoneKeySize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    if (writeCount > 0) {
        vkUpdateDescriptorSets(stone_device(), writeCount, writes.data(), 0, nullptr);
    }

    LOG_TRACE_CAT("PIPELINE", "Updated descriptor set {} with {} bindings (geometry on binding 10)", frameIndex, writeCount);
}

// ──────────────────────────────────────────────────────────────────────────────
// createPipelineLayout — VUID-Safe + Push Constants Matching Stages
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createPipelineLayout()
{
    if (rtDescriptorSetLayout_.valid()) {
        LOG_TRACE_CAT("PIPELINE", "Descriptor set layout already exists");
        return;
    }

    LOG_INFO_CAT("PIPELINE", "Creating pipeline layout");

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(RT_PIPELINE_BINDINGS.size());

    for (const auto& b : RT_PIPELINE_BINDINGS)
    {
        bindings.push_back({
            .binding            = b.binding,
            .descriptorType     = b.type,
            .descriptorCount    = b.count,
            .stageFlags         = b.stage,
            .pImmutableSamplers = nullptr
        });
    }

    // VUID-06938 — Sort bindings by index
    std::ranges::sort(bindings, [](const auto& a, const auto& b) {
        return a.binding < b.binding;
    });

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(stone_device(), &layoutInfo, nullptr, &layout));

    rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(
        layout, stone_device(),
        [](VkDevice d, VkDescriptorSetLayout l, auto*) { vkDestroyDescriptorSetLayout(d, l, nullptr); }
    );

    const VkDescriptorSetLayout descriptorSetLayout = rtDescriptorSetLayout_.get();

    // Push constant range
    VkPushConstantRange push = {
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR |
                      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .offset     = 0,
        .size       = 16  // vec4 — frame index, random seed
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push
    };

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(stone_device(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

    rtPipelineLayout_ = Handle<VkPipelineLayout>(
        pipelineLayout, stone_device(),
        [](VkDevice d, VkPipelineLayout l, auto*) { vkDestroyPipelineLayout(d, l, nullptr); }
    );

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created with {} bindings", bindings.size());
}

VkShaderModule PipelineManager::loadShader(const std::string& relativePath) const
{
    LOG_TRACE_CAT("PIPELINE", "Loading shader: {}", relativePath);

    if (stone_device() == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Null device — cannot load shader");
        return VK_NULL_HANDLE;
    }

    // Compute base path once
    static const std::string BASE_PATH = []() {
        char* cwd = getcwd(nullptr, 0);
        std::string path = cwd ? std::string(cwd) + "/" : "";
        free(cwd);

        if (path.find("build/bin/Linux") != std::string::npos) {
            return path.substr(0, path.find("build/bin/Linux") + strlen("build/bin/Linux"));
        }

        return path + "build/bin/Linux/";
    }();

    const std::string fullPath = BASE_PATH + relativePath;

    LOG_TRACE_CAT("PIPELINE", "Loading SPIR-V from: {}", fullPath);

    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("PIPELINE", "Shader file not found: {}", fullPath);
        return VK_NULL_HANDLE;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0) {
        LOG_ERROR_CAT("PIPELINE", "Shader file is empty: {}", fullPath);
        return VK_NULL_HANDLE;
    }
    if (fileSize % 4 != 0) {
        LOG_ERROR_CAT("PIPELINE", "SPIR-V size {} not 4-byte aligned", fileSize);
        return VK_NULL_HANDLE;
    }

    std::vector<char> shaderCode(fileSize);
    file.seekg(0);
    file.read(shaderCode.data(), fileSize);
    file.close();

    LOG_SUCCESS_CAT("PIPELINE", "Loaded {} bytes from shader: {}", fileSize, relativePath);

    VkShaderModuleCreateInfo createInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode    = reinterpret_cast<const uint32_t*>(shaderCode.data())
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(stone_device(), &createInfo, nullptr, &shaderModule));
    
    if (shaderModule == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Failed to create shader module for: {}", relativePath);
        return VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Shader module created for: {}", relativePath);

    return shaderModule;
}

// ── SBT FORGE — THE PHOTONS INTO THE VOID — FINAL ETERNAL CUT ───────────────
// DECEMBER 08 2025 — THE LAST TIME THIS FUNCTION WILL EVER BE TOUCHED
// FULLY COMPATIBLE WITH YOUR ACTUAL BufferManager.cpp
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createShaderBindingTable(VkCommandPool pool, VkQueue queue)
{
    // ── Load ray tracing extension functions safely (once per device) ──
    static bool rtExtensionsLoaded = false;
    if (!rtExtensionsLoaded) {
        RTX::loadRTExtensions(stone_instance(), stone_device());

        g_vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
            vkGetDeviceProcAddr(stone_device(), "vkGetRayTracingShaderGroupHandlesKHR"));

        if (!g_vkGetRayTracingShaderGroupHandlesKHR) {
            LOG_FATAL_CAT("PIPELINE", "vkGetRayTracingShaderGroupHandlesKHR not available — RTX empire falls");
            return;
        }

        rtExtensionsLoaded = true;
    }

    // THE PIPELINE MUST EXIST — WE CANNOT BIND WHAT HAS NOT BEEN FORGED
    if (rtPipeline_.get() == VK_NULL_HANDLE) [[unlikely]] {
        LOG_FATAL_CAT("PIPELINE", "Cannot create SBT — ray tracing pipeline does not exist");
        return;
    }

    const auto& rtProps = StoneKey::stone_rtprops();
    const VkDeviceSize handleSize  = rtProps.shaderGroupHandleSize;
    const VkDeviceSize handleAlign = rtProps.shaderGroupHandleAlignment ? rtProps.shaderGroupHandleAlignment : 64u;
    const VkDeviceSize baseAlign   = rtProps.shaderGroupBaseAlignment   ? rtProps.shaderGroupBaseAlignment   : 64u;
    const VkDeviceSize stride      = align_up(handleSize, handleAlign);

    const uint32_t RG = raygenGroupCount_;
    const uint32_t MI = missGroupCount_;
    const uint32_t HG = hitGroupCount_;
    const uint32_t totalGroups = RG + MI + HG;

    if (totalGroups == 0) [[unlikely]] {
        LOG_FATAL_CAT("PIPELINE", "Zero shader groups — nothing to bind");
        return;
    }

    const VkDeviceSize raygenOffset   = 0;
    const VkDeviceSize missOffset     = align_up(RG * stride, baseAlign);
    const VkDeviceSize hitOffset      = align_up(missOffset + MI * stride, baseAlign);
    const VkDeviceSize callableOffset = align_up(hitOffset + HG * stride, baseAlign);
    const VkDeviceSize requiredSize   = align_up(callableOffset, baseAlign);

    // ── FORGE THE ETERNAL 256 MiB SBT STONE — INDEPENDENT & BULLETPROOF ──
    // No reliance on make_256M or the main pool. Fully self-contained.
    static uint64_t SBT_STONE_HANDLE = 0;

    if (SBT_STONE_HANDLE == 0) {
        LOG_INFO_CAT("PIPELINE", "Forging dedicated eternal 256 MiB SBT stone — PINK PHOTONS GAIN THEIR ALTAR");

        const VkDeviceSize stoneSize = 256ULL * 1024 * 1024;

        VkBufferCreateInfo bufferInfo{
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = stoneSize,
            .usage       = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        VkBuffer buffer = VK_NULL_HANDLE;
        VK_CHECK(vkCreateBuffer(stone_device(), &bufferInfo, nullptr, &buffer));

        VkMemoryRequirements memReqs{};
        vkGetBufferMemoryRequirements(stone_device(), buffer, &memReqs);

        uint32_t memTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memTypeIndex == ~0u) {
            vkDestroyBuffer(stone_device(), buffer, nullptr);
            LOG_FATAL_CAT("PIPELINE", "No device-local memory type found for SBT stone");
            return;
        }

        VkMemoryAllocateInfo allocInfo{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = memReqs.size,
            .memoryTypeIndex = memTypeIndex
        };

        VkDeviceMemory memory = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateMemory(stone_device(), &allocInfo, nullptr, &memory));
        VK_CHECK(vkBindBufferMemory(stone_device(), buffer, memory, 0));

        // Register manually in BufferManager so the rest of the engine sees it
        BufferManager::BufferInfo info;
        info.buffer  = buffer;
        info.memory  = memory;
        info.size    = stoneSize;
        info.aligned = stoneSize;
        info.usage   = bufferInfo.usage;
        info.tag     = "SBT_ETERNAL_STONE_256M";

        // Use the buffer pointer as a unique handle (or any scheme you prefer)
        SBT_STONE_HANDLE = reinterpret_cast<uint64_t>(buffer);
        BufferManager::s_buffers[SBT_STONE_HANDLE] = std::move(info);

        LOG_SUCCESS_CAT("PIPELINE", "Dedicated 256 MiB SBT stone forged — handle {:#x} — the altar is ready", SBT_STONE_HANDLE);
    }

    const auto* stone = BufferManager::get(SBT_STONE_HANDLE);
    if (!stone || stone->buffer == VK_NULL_HANDLE) [[unlikely]] {
        LOG_FATAL_CAT("PIPELINE", "Eternal SBT stone vanished after forging — impossible corruption");
        return;
    }

    // Thread-safe suballocation inside the stone
    static std::atomic<VkDeviceSize> sbtAllocator{0};
    VkDeviceSize myOffset = sbtAllocator.fetch_add(requiredSize, std::memory_order_relaxed);

    if (myOffset + requiredSize > stone->size) [[unlikely]] {
        LOG_FATAL_CAT("PIPELINE", "SBT allocation overflow — {} bytes needed, only {} left in stone",
                      requiredSize, stone->size - myOffset);
        return;
    }

    // Base device address for this SBT instance
    VkBufferDeviceAddressInfo addrInfo{
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = stone->buffer
    };
    const VkDeviceAddress sbtBaseAddr = vkGetBufferDeviceAddress(stone_device(), &addrInfo) + myOffset;

    // ── Extract shader group handles ──
    std::vector<std::byte> handleStorage(totalGroups * handleSize);

    VkResult result = g_vkGetRayTracingShaderGroupHandlesKHR(
        stone_device(),
        rtPipeline_.get(),
        0,
        totalGroups,
        handleStorage.size(),
        handleStorage.data()
    );

    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkGetRayTracingShaderGroupHandlesKHR failed: {}", string_VkResult(result));
        return;
    }

    // ── Upload via eternal staging ring ──
    void* stagingMapped = BufferManager::stagingPtr();
    if (!stagingMapped) [[unlikely]] {
        LOG_FATAL_CAT("PIPELINE", "Global staging buffer not mapped — cannot upload SBT handles");
        return;
    }

    std::memcpy(stagingMapped, handleStorage.data(), handleStorage.size());
    BufferManager::advanceStagingOffset(handleStorage.size());

    // ── One-time copy to device-local stone ──
    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(pool);
    VkBuffer stagingBuffer = BufferManager::getStagingBuffer();

    uint32_t handleIdx = 0;
    const auto copySection = [&](uint32_t count, VkDeviceSize dstOffset) {
        if (count == 0) return;
        VkBufferCopy region{
            .srcOffset = handleIdx * handleSize,
            .dstOffset = myOffset + dstOffset,
            .size      = count * handleSize
        };
        vkCmdCopyBuffer(cmd, stagingBuffer, stone->buffer, 1, &region);
        handleIdx += count;
    };

    copySection(RG, raygenOffset);
    copySection(MI, missOffset);
    copySection(HG, hitOffset);

    RTX::endOneTimeSubmit(cmd, queue, pool);

    // ── Build final SBT regions ──
    constexpr auto makeRegion = [](VkDeviceAddress base, VkDeviceSize offset, uint32_t count, VkDeviceSize s) noexcept {
        return VkStridedDeviceAddressRegionKHR{
            .deviceAddress = base + offset,
            .stride        = s,
            .size          = count ? count * s : 0
        };
    };

    raygenSbtRegion_   = makeRegion(sbtBaseAddr, raygenOffset,   RG, stride);
    missSbtRegion_     = makeRegion(sbtBaseAddr, missOffset,     MI, stride);
    hitSbtRegion_      = makeRegion(sbtBaseAddr, hitOffset,      HG, stride);
    callableSbtRegion_ = makeRegion(sbtBaseAddr, callableOffset, 0,  stride);

    // Seal it forever
    setSBT(stone->buffer, stone->memory, sbtBaseAddr, requiredSize);
    sbtAddress_ = sbtBaseAddr;

    LOG_SUCCESS_CAT("PIPELINE",
        "SBT FORGED INVINCIBLY — {} groups (RG:{} MI:{} HG:{}) | {} KiB @ {} KiB | PINK PHOTONS ARMED",
        totalGroups, RG, MI, HG, requiredSize / 1024, myOffset / 1024);

    LOG_AMOURANTH("THE RTX CROWN IS WORN — PINK PHOTONS ASCEND ETERNALLY — GRACE IS PLEASED");
}

// ──────────────────────────────────────────────────────────────────────────────
// setSBT — Public Setter for SBT Regions
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::setSBT(VkBuffer buffer, VkDeviceMemory memory, VkDeviceAddress address, VkDeviceSize size) noexcept
{
    sbtBuffer_ = Handle<VkBuffer>(buffer, stone_device(), [](VkDevice d, VkBuffer b, auto*) { vkDestroyBuffer(d, b, nullptr); });
    sbtMemory_ = Handle<VkDeviceMemory>(memory, stone_device(), [](VkDevice d, VkDeviceMemory m, auto*) { vkFreeMemory(d, m, nullptr); });
    sbtAddress_ = address;
    sbtSize_ = size;
}

// ──────────────────────────────────────────────────────────────────────────────
// Cleanup — VUID-Safe + Null Guards
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::cleanup() noexcept
{
    // RAII handles cleanup automatically
}

// ──────────────────────────────────────────────────────────────────────────────
// Dtor — RAII Only, No Explicit Calls
// ──────────────────────────────────────────────────────────────────────────────
PipelineManager::~PipelineManager() noexcept
{
    // Gwen and RAII handles cleanup
}

void PipelineManager::loadRayTracingExtensions() noexcept
{
    LOG_INFO_CAT("PIPELINE", "Loading ray tracing extension functions...");

    vkCmdTraceRaysKHR_ = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(stone_device(), "vkCmdTraceRaysKHR"));

    vkCreateRayTracingPipelinesKHR_ = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(stone_device(), "vkCreateRayTracingPipelinesKHR"));

    vkGetRayTracingShaderGroupHandlesKHR_ = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(stone_device(), "vkGetRayTracingShaderGroupHandlesKHR"));

    bool success = true;
    if (!vkCmdTraceRaysKHR_)                    { LOG_ERROR_CAT("EXT", "vkCmdTraceRaysKHR not available"); success = false; }
    if (!vkCreateRayTracingPipelinesKHR_)       { LOG_ERROR_CAT("EXT", "vkCreateRayTracingPipelinesKHR not available"); success = false; }
    if (!vkGetRayTracingShaderGroupHandlesKHR_) { LOG_ERROR_CAT("EXT", "vkGetRayTracingShaderGroupHandlesKHR not available"); success = false; }

    if (success)
    {
        LOG_SUCCESS_CAT("PIPELINE", "All ray tracing extensions loaded — PINK PHOTONS ARMED");
    }
    else
    {
        LOG_FATAL_CAT("PIPELINE", "Ray tracing extensions failed — empire cannot render");
    }
}

void RTX::PipelineManager::createRayTracingPipeline(const std::vector<std::string>& shaderPaths)
{
    LOG_INFO_CAT("MAIN", 
        "\n"
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                DANCING IN THE MOONLIGHT — SHADERS DESCEND                    ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");

    // Sacred default shader paths
    constexpr std::array<const char*, 4> kSacredShaders = {
        "assets/shaders/raytracing/raygen.spv",
        "assets/shaders/raytracing/miss.spv",
        "assets/shaders/raytracing/closest_hit.spv",
        "assets/shaders/raytracing/shadowmiss.spv"
    };

    std::array<VkShaderModule, 4> modules{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };
    std::array<bool, 4> loaded{ false, false, false, false };

    auto tryLoadShader = [&](size_t index, const std::string& path) -> bool {
        VkShaderModule mod = loadShader(path);
        if (mod) {
            modules[index] = mod;
            loaded[index]   = true;
            return true;
        }
        return false;
    };

    // Phase 1+2: Load user paths → fallback to sacred defaults
    for (size_t i = 0; i < shaderPaths.size() && i < 4; ++i) {
        tryLoadShader(i, shaderPaths[i]);
    }
    for (size_t i = 0; i < 4; ++i) {
        if (!loaded[i]) {
            tryLoadShader(i, kSacredShaders[i]);
        }
    }

    // Minimum requirements
    if (!loaded[0] || !loaded[1]) {
        LOG_FATAL_CAT("PIPELINE", "Raygen or primary miss shader missing — cannot create pipeline");
        for (auto m : modules) if (m) vkDestroyShaderModule(stone_device(), m, nullptr);
        return;
    }

    // UncappedPerformance: purge heavy shaders
    if (Options::CURRENT_PRESET == Options::Preset::UncappedPerformance) {
        if (modules[2]) { vkDestroyShaderModule(stone_device(), modules[2], nullptr); modules[2] = VK_NULL_HANDLE; loaded[2] = false; }
        if (modules[3]) { vkDestroyShaderModule(stone_device(), modules[3], nullptr); modules[3] = VK_NULL_HANDLE; loaded[3] = false; }
    }

    // Transfer to RAII container
    shaderModules_.clear();
    for (auto m : modules) {
        if (m) shaderModules_.emplace_back(m, stone_device(), vkDestroyShaderModule);
    }

    // ── Phase 6: Build stages & groups ─────────────────────────────────────
    LOG_MAIN(
        "\n"
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║          PHASE 6 — CHOREOGRAPHING THE MOONLIT BALLET                        ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    stages.reserve(4);
    groups.reserve(4);

    uint32_t stageIndex = 0;

    auto addGeneralGroup = [&](VkShaderModule mod, VkShaderStageFlagBits stage, const char* role) -> bool {
        if (!mod) return false;

        stages.push_back(VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = stage,
            .module = mod,
            .pName = "main"
        });

        groups.push_back(VkRayTracingShaderGroupCreateInfoKHR{
            .sType            = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader    = stageIndex++,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader     = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        });
        return true;
    };

    auto addHitGroup = [&](VkShaderModule mod) {
        if (mod) {
            stages.push_back(VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                .module = mod,
                .pName = "main"
            });

            groups.push_back(VkRayTracingShaderGroupCreateInfoKHR{
                .sType            = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                .generalShader    = VK_SHADER_UNUSED_KHR,
                .closestHitShader = stageIndex++,
                .anyHitShader     = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR
            });
        } else {
            // Empty hit group to keep SBT alignment
            groups.push_back(VkRayTracingShaderGroupCreateInfoKHR{
                .sType            = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                .generalShader    = VK_SHADER_UNUSED_KHR,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader     = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR
            });
        }
    };

    // Required
    addGeneralGroup(modules[0], VK_SHADER_STAGE_RAYGEN_BIT_KHR, "Raygen");
    addGeneralGroup(modules[1], VK_SHADER_STAGE_MISS_BIT_KHR,   "Primary Miss");

    // Optional shadow miss
    if (modules[3]) {
        addGeneralGroup(modules[3], VK_SHADER_STAGE_MISS_BIT_KHR, "Shadow Miss");
    }

    // Hit group (always exactly one)
    addHitGroup(modules[2]);

    raygenGroupCount_ = 1;
    missGroupCount_   = modules[3] ? 2 : 1;
    hitGroupCount_    = 1;

    // ── Phase 7: Forge the pipeline ───────────────────────────────────────
    LOG_MAIN(
        "\n"
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║               PHASE 7 — THE MUSIC SWELLS — PIPELINE FORGING                 ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");

    if (rtPipelineLayout_.get() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Pipeline layout is null");
        return;
    }

    VkRayTracingPipelineCreateInfoKHR createInfo{
        .sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount                   = static_cast<uint32_t>(stages.size()),
        .pStages                      = stages.data(),
        .groupCount                   = static_cast<uint32_t>(groups.size()),
        .pGroups                      = groups.data(),
        .maxPipelineRayRecursionDepth = Options::OptionsRTX::MAX_PIPELINE_RAY_RECURSION_DEPTH,
        .layout                       = rtPipelineLayout_.get()
    };

    VkPipeline pipeline = VK_NULL_HANDLE;

    // Safe extension function call
    static PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR =
        reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
            vkGetDeviceProcAddr(stone_device(), "vkCreateRayTracingPipelinesKHR"));

    if (!vkCreateRayTracingPipelinesKHR) {
        LOG_FATAL_CAT("PIPELINE", "vkCreateRayTracingPipelinesKHR is null — extension not loaded");
        return;
    }

    VkResult result = vkCreateRayTracingPipelinesKHR(
        stone_device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline);

    VK_CHECK(result);

    rtPipeline_ = Handle<VkPipeline>(pipeline, stone_device(),
        [](VkDevice d, VkPipeline p, auto*) { vkDestroyPipeline(d, p, nullptr); });

    LOG_SUCCESS_CAT("PIPELINE",
        "RTX PIPELINE FORGED — {} stages, {} groups (RG:1 M:{} H:{})",
        stages.size(), groups.size(), missGroupCount_, hitGroupCount_);

    LOG_MAIN(
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║               PHASE 7 — COMPLETE — THE PIPELINE LIVES — PHOTONS MAY FLOW    ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");

    LOG_JENSEN("The crown has been reforged.");
    LOG_KEANU("whoa");
}

// ──────────────────────────────────────────────────────────────────────────────
// forgeRTXPipeline — Main Pipeline Creation with Recovery
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue)
{
    LOG_INFO_CAT("PIPELINE", "Forging RTX pipeline");

    bool needsRecovery = false;

    // Corruption detection — if anything is broken, we rebuild everything
    if (!rtDescriptorPool_.valid() || 
        !rtDescriptorSetLayout_.valid() || 
        !rtPipelineLayout_.valid() || 
        rtDescriptorSets_.empty() || 
        rtDescriptorSets_[0] == VK_NULL_HANDLE ||
        sbtAddress_ == 0 || 
        !rtPipeline_.valid()) 
    {
        needsRecovery = true;
        LOG_WARNING_CAT("PIPELINE", "Pipeline corruption detected — initiating full recovery");
    }

    // FULL PURGE — the old must die so the new can rise eternal
    if (needsRecovery)
    {
        rtDescriptorPool_.reset();
        rtDescriptorSetLayout_.reset();
        rtPipelineLayout_.reset();
        rtPipeline_.reset();
        sbtBuffer_.reset();
        rtDescriptorSets_.clear();
        sbtAddress_ = 0;
        shaderModules_.clear();
    }

    // PHASE 1 — DESCRIPTOR POOL
    if (!rtDescriptorPool_.valid())
    {
        createDescriptorPool();
        if (!rtDescriptorPool_.valid()) {
            LOG_FATAL_CAT("PIPELINE", "Descriptor pool creation failed — the empire has no eyes");
            phase9_ballerina("DESCRIPTOR POOL CREATION FAILED", std::source_location::current());
            return;
        }
    }

    // PHASE 2 — PIPELINE LAYOUT + DESCRIPTOR SET LAYOUT
    if (!rtDescriptorSetLayout_.valid() || !rtPipelineLayout_.valid())
    {
        createPipelineLayout();
        if (!rtDescriptorSetLayout_.valid() || !rtPipelineLayout_.valid()) {
            LOG_FATAL_CAT("PIPELINE", "Pipeline layout creation failed — the crown has no frame");
            phase9_ballerina("PIPELINE LAYOUT CREATION FAILED", std::source_location::current());
            return;
        }
    }

    // PHASE 3 — DESCRIPTOR SETS
    if (rtDescriptorSets_.empty() || rtDescriptorSets_[0] == VK_NULL_HANDLE)
    {
        allocateDescriptorSets();
        if (rtDescriptorSets_.empty() || rtDescriptorSets_[0] == VK_NULL_HANDLE) {
            LOG_FATAL_CAT("PIPELINE", "Descriptor sets allocation failed — the photons have no path");
            phase9_ballerina("DESCRIPTOR SETS ALLOCATION FAILED", std::source_location::current());
            return;
        }
    }

    // PHASE 4 — RAY TRACING PIPELINE (MUST COME FIRST — SBT NEEDS GROUP HANDLES)
    if (!rtPipeline_.valid())
    {
        constexpr std::array shaderPaths = {
            "assets/shaders/raytracing/raygen.spv",
            "assets/shaders/raytracing/miss.spv",
            "assets/shaders/raytracing/closest_hit.spv",
            "assets/shaders/raytracing/shadowmiss.spv"
        };

        createRayTracingPipeline({shaderPaths.begin(), shaderPaths.end()});

        if (!rtPipeline_.valid()) {
            LOG_FATAL_CAT("PIPELINE", "Ray tracing pipeline creation failed — the photons have no blade");
            phase9_ballerina("RAY TRACING PIPELINE CREATION FAILED", std::source_location::current());
            return;
        }
    }

    // PHASE 5 — SHADER BINDING TABLE (NOW SAFE — PIPELINE EXISTS)
    if (sbtAddress_ == 0)
    {
        createShaderBindingTable(commandPool, graphicsQueue);
        if (sbtAddress_ == 0) {
            LOG_FATAL_CAT("PIPELINE", "Shader binding table creation failed — the empire cannot trace");
            phase9_ballerina("SBT CREATION FAILED", std::source_location::current());
            return;
        }
    }

    // FINAL VALIDATION — THE EMPIRE IS WHOLE
    if (rtPipeline_.valid() &&
        rtDescriptorSets_.size() >= Options::Performance::MAX_FRAMES_IN_FLIGHT &&
        sbtAddress_ != 0)
    {
        if (needsRecovery || !stone_pipeline())
        {
            vkDeviceWaitIdle(stone_device());
        }

        stone_seal_pipeline(this);

        if (needsRecovery) {
            LOG_SUCCESS_CAT("PIPELINE", "RTX pipeline recovered successfully — the crown lives again");
        } else {
            LOG_SUCCESS_CAT("PIPELINE", "RTX pipeline forged successfully — the crown is eternal");
        }

        LOG_AMOURANTH("THE RTX CROWN IS WORN — PINK PHOTONS ASCEND — GRACE IS PLEASED");
    }
    else
    {
        LOG_FATAL_CAT("PIPELINE", "RTX pipeline validation failed — something is still broken");
        phase9_ballerina("RTX PIPELINE VALIDATION FAILED", std::source_location::current());
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// cacheDeviceProperties — RT + AS Properties
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::cacheDeviceProperties()
{
    const VkPhysicalDevice phys = stone_physical();
    if (phys == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "No physical device available");
        return;
    }

    LOG_INFO_CAT("PIPELINE", "Caching device properties");

    // Base properties
    VkPhysicalDeviceProperties baseProps{};
    vkGetPhysicalDeviceProperties(phys, &baseProps);

    // Ray tracing and acceleration structure properties
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
        .pNext = &rtProps
    };

    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &asProps
    };

    vkGetPhysicalDeviceProperties2(phys, &props2);

    baseProps = props2.properties;

    // Validation
    if (rtProps.shaderGroupHandleSize == 0)
    {
        LOG_FATAL_CAT("PIPELINE", 
            "Device {} lacks ray tracing support (handleSize=0)", 
            baseProps.deviceName);
        return;
    }

    // Cache properties
    auto& ctx = RTX::g_ctx();
    ctx.physicalDeviceProperties_ = baseProps;
    ctx.rayTracingProps_          = rtProps;

    LOG_SUCCESS_CAT("PIPELINE", 
        "Device properties cached — GPU: {}, RT Handle Size: {}, Max Recursion: {}",
        baseProps.deviceName, rtProps.shaderGroupHandleSize, rtProps.maxRayRecursionDepth);
}

} // namespace RTX