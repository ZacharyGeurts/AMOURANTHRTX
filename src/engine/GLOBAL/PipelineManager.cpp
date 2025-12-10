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
#include "engine/GLOBAL/UBO.hpp"
#include <fstream>
#include <algorithm>
#include <format>
#include <vector>
#include <array>
#include <unordered_map>
#include <stb/stb_image.h>
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
// createDescriptorPool — Triple-Buffered + FULLY VALIDATED FOR ALL BINDINGS
// Ensures: TLAS(0), Output(1), Accum(2), UBO(3), Materials(4), EnvMap(5), 
//          Nexus(6), BlueNoise(8), Density(9), Geometry(10), Index(11), StoneKey(31)
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createDescriptorPool() 
{
    LOG_INFO_CAT("PIPELINE", "Creating descriptor pool — validating all sacred bindings");

    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    const uint32_t TOTAL_SETS = framesInFlight * 16;

    std::unordered_map<VkDescriptorType, uint32_t> typeCount;

    for (const auto& b : RT_PIPELINE_BINDINGS) {
        typeCount[b.type] += b.count;
    }

    typeCount[VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR] += 1;
    typeCount[VK_DESCRIPTOR_TYPE_STORAGE_IMAGE]               += 3;
    typeCount[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER]              += 2;
    typeCount[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER]              += 3;
    typeCount[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER]      += 3;
    typeCount[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER] += 8;

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(typeCount.size());

    for (const auto& [type, countPerSet] : typeCount) {
        poolSizes.push_back({ type, countPerSet * TOTAL_SETS });
    }

    VkDescriptorPoolCreateInfo info{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = TOTAL_SETS,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(stone_device(), &info, nullptr, &pool);

    if (result == VK_SUCCESS) {
        rtDescriptorPool_ = Handle<VkDescriptorPool>(
            pool, stone_device(),
            [](VkDevice d, VkDescriptorPool p, auto*) { vkDestroyDescriptorPool(d, p, nullptr); },
            0, "EMPIRE_DESCRIPTOR_POOL_ETERNAL"
        );

        LOG_SUCCESS_CAT("PIPELINE", 
            "Descriptor pool forged — {} sets — ALL BINDINGS ETERNALLY SECURED", TOTAL_SETS);
    }
    else {
        LOG_FATAL_CAT("PIPELINE", "vkCreateDescriptorPool failed: {} ({})", string_VkResult(result), static_cast<int32_t>(result));
        phase9_ballerina("DESCRIPTOR POOL CREATION FAILED", std::source_location::current());
    }
}

PipelineManager::~PipelineManager() = default;

// ──────────────────────────────────────────────────────────────────────────────
// PipelineManager Constructor — Device + Physical + Dummy TLAS
// ──────────────────────────────────────────────────────────────────────────────
PipelineManager::PipelineManager(VkDevice device, VkPhysicalDevice phys)
{
    cacheDeviceProperties();
}

// ──────────────────────────────────────────────────────────────────────────────
// allocateDescriptorSets — Triple-Buffered Allocation
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::allocateDescriptorSets() 
{
    LOG_TRACE_CAT("PIPELINE", "Allocating descriptor sets");

    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    const uint32_t TOTAL_SETS_TO_ALLOCATE = framesInFlight * 4;

    rtDescriptorSets_.clear();
    rtDescriptorSets_.resize(TOTAL_SETS_TO_ALLOCATE);

    std::vector<VkDescriptorSetLayout> layouts(TOTAL_SETS_TO_ALLOCATE, rtDescriptorSetLayout_.get());

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = rtDescriptorPool_.get();
    allocInfo.descriptorSetCount = TOTAL_SETS_TO_ALLOCATE;
    allocInfo.pSetLayouts        = layouts.data();

    VkResult result = vkAllocateDescriptorSets(stone_device(), &allocInfo, rtDescriptorSets_.data());

    if (result == VK_ERROR_FRAGMENTED_POOL || result == VK_ERROR_OUT_OF_POOL_MEMORY)
    {
        LOG_WARNING_CAT("PIPELINE", "Descriptor pool exhausted — recreating");
        rtDescriptorPool_.reset();
        createDescriptorPool();
        allocInfo.descriptorPool = rtDescriptorPool_.get();
        result = vkAllocateDescriptorSets(stone_device(), &allocInfo, rtDescriptorSets_.data());
    }

    if (result != VK_SUCCESS)
    {
        LOG_FATAL_CAT("PIPELINE", "vkAllocateDescriptorSets failed: {}", string_VkResult(result));
        phase9_ballerina("DESCRIPTOR SETS ALLOCATION FAILED", std::source_location::current());
    }

    LOG_SUCCESS_CAT("PIPELINE", "Allocated {} descriptor sets", TOTAL_SETS_TO_ALLOCATE);
}

void PipelineManager::updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo) noexcept
{
    if (frameIndex >= rtDescriptorSets_.size()) return;
    VkDescriptorSet dstSet = rtDescriptorSets_[frameIndex];
    if (dstSet == VK_NULL_HANDLE) return;

    std::array<VkWriteDescriptorSet, 17> writes{};
    uint32_t writeCount = 0;

    const auto writeAccel = [&](VkAccelerationStructureKHR tlas) {
        if (!tlas) tlas = VK_NULL_HANDLE;  // ← RAW. NO DUMMY. NO LIES.
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

    // THE EMPIRE SPEAKS — RAW TRUTH ONLY
    writeAccel(updateInfo.tlas);
    writeImage(1, updateInfo.swapchainImageView, VK_IMAGE_LAYOUT_GENERAL);

    if (Options::OptionsRTX::ENABLE_ACCUMULATION && frameIndex < updateInfo.accumulationViews.size())
        writeImage(2, updateInfo.accumulationViews[frameIndex]);

    writeBuffer(3, updateInfo.ubo, 368, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writeBuffer(4, updateInfo.materialsBuffer, updateInfo.materialsSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    if (updateInfo.envSampler && updateInfo.envImageView)
        writeSampler(5, updateInfo.envSampler, updateInfo.envImageView);

    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING && frameIndex < updateInfo.nexusScoreViews.size())
        writeImage(6, updateInfo.nexusScoreViews[frameIndex]);

    if (updateInfo.additionalStorageBuffer)
        writeBuffer(10, updateInfo.additionalStorageBuffer, updateInfo.additionalStorageSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    if (updateInfo.blueNoiseSampler && updateInfo.blueNoiseView)
        writeSampler(8, updateInfo.blueNoiseSampler, updateInfo.blueNoiseView);

    if (updateInfo.densitySampler && updateInfo.densityView)
        writeSampler(9, updateInfo.densitySampler, updateInfo.densityView);

    if (updateInfo.stoneKeyBuffer)
        writeBuffer(31, updateInfo.stoneKeyBuffer, updateInfo.stoneKeySize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    if (writeCount > 0) {
        vkUpdateDescriptorSets(stone_device(), writeCount, writes.data(), 0, nullptr);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// loadShader — Now also loads envmap.hdr automatically when requested
// ──────────────────────────────────────────────────────────────────────────────
VkShaderModule PipelineManager::loadShader(const std::string& relativePath) const
{
    LOG_TRACE_CAT("PIPELINE", "Loading shader: {}", relativePath);

    if (stone_device() == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Null device — cannot load shader");
        return VK_NULL_HANDLE;
    }

    // Special case: load environment map
    if (relativePath == "assets/textures/envmap.hdr" || relativePath == "envmap.hdr") {
        static bool envMapLoaded = false;
        if (envMapLoaded) {
            LOG_INFO_CAT("PIPELINE", "Environment map already loaded");
            return VK_NULL_HANDLE; // not a real shader
        }

        LOG_AMOURANTH("FIRST LIGHT — Loading HDR environment map: {}", relativePath);

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_HDR
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG

        int w, h, channels;
        float* data = stbi_loadf(relativePath.c_str(), &w, &h, &channels, 4);
        if (!data) {
            LOG_FATAL_CAT("ENV", "Failed to load HDR envmap: {} — {}", relativePath, stbi_failure_reason());
            return VK_NULL_HANDLE;
        }

        const VkDeviceSize size = w * h * 4 * sizeof(float);
        uint64_t staging = BufferManager::createHostVisible(size, "EnvMap_Staging");
        void* mapped = BufferManager::getMappedStagingPtr(staging);
        std::memcpy(mapped, data, size);
        stbi_image_free(data);

        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;

        VkImageCreateInfo imgInfo{
            .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType     = VK_IMAGE_TYPE_2D,
            .format        = VK_FORMAT_R32G32B32A32_SFLOAT,
            .extent        = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 },
            .mipLevels     = 1,
            .arrayLayers   = 1,
            .samples       = VK_SAMPLE_COUNT_1_BIT,
            .tiling        = VK_IMAGE_TILING_OPTIMAL,
            .usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        VK_CHECK(vkCreateImage(stone_device(), &imgInfo, nullptr, &image));
        VkMemoryRequirements reqs{};
        vkGetImageMemoryRequirements(stone_device(), image, &reqs);
        VkMemoryAllocateInfo alloc{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = reqs.size,
            .memoryTypeIndex = findMemoryType(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        VK_CHECK(vkAllocateMemory(stone_device(), &alloc, nullptr, &memory));
        VK_CHECK(vkBindImageMemory(stone_device(), image, memory, 0));

        VkCommandBuffer cmd = RTX::beginOneTimeSubmit();

        VkImageMemoryBarrier barrier{
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = 0,
            .dstAccessMask          = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image               = image,
            .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy copy{
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageExtent      = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 }
        };
        vkCmdCopyBufferToImage(cmd, BufferManager::get(staging)->buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        RTX::endOneTimeSubmit(cmd, stone_graphics_queue());

        BufferManager::destroy(staging);

        VkImageView view = VK_NULL_HANDLE;
        VkImageViewCreateInfo viewInfo{
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = image,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = VK_FORMAT_R32G32B32A32_SFLOAT,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        VK_CHECK(vkCreateImageView(stone_device(), &viewInfo, nullptr, &view));

        VkSampler sampler = VK_NULL_HANDLE;
        VkSamplerCreateInfo samplerInfo{
            .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter    = VK_FILTER_LINEAR,
            .minFilter    = VK_FILTER_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT
        };
        VK_CHECK(vkCreateSampler(stone_device(), &samplerInfo, nullptr, &sampler));

        // Store globally
        const_cast<PipelineManager*>(this)->envMapImageView_ = Handle<VkImageView>(view, stone_device(), vkDestroyImageView);
        const_cast<PipelineManager*>(this)->envMapSampler_   = Handle<VkSampler>(sampler, stone_device(), vkDestroySampler);

        LOG_SUCCESS_CAT("ENV", "HDR environment map loaded — {}×{} — THE VOID IS ILLUMINATED", w, h);
        envMapLoaded = true;

        return VK_NULL_HANDLE; // not a shader, but success
    }

    // ── Normal shader loading path (unchanged from your original) ──
    static const std::string BASE_PATH = []() {
        char* cwd = getcwd(nullptr, 0);
        std::string path = cwd ? std::string(cwd) + "/" : "";
        free(cwd);
        if (path.find("build/bin/Linux") != std::string::npos)
            return path.substr(0, path.find("build/bin/Linux") + strlen("build/bin/Linux"));
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
    if (fileSize == 0 || fileSize % 4 != 0) {
        LOG_ERROR_CAT("PIPELINE", "Invalid SPIR-V file: {}", fullPath);
        return VK_NULL_HANDLE;
    }

    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode    = reinterpret_cast<const uint32_t*>(code.data())
    };

    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(stone_device(), &createInfo, nullptr, &module));

    LOG_SUCCESS_CAT("PIPELINE", "Shader loaded: {}", relativePath);
    return module;
}

void PipelineManager::createEnvMapDisplayComputePipeline(VkImageView envMapView, VkSampler envMapSampler)
{
    VkDevice device = stone_device();

    // === 1. Descriptor Set Layout ===
    VkDescriptorSetLayoutBinding bindings[2] = {};

    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings    = bindings
    };
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &envMapDisplayDescSetLayout_));

    // === 2. Pipeline Layout + Push Constants ===
    VkPushConstantRange pcRange{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset     = 0,
        .size       = 8
    };

    VkPipelineLayoutCreateInfo plInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &envMapDisplayDescSetLayout_,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pcRange
    };
    VK_CHECK(vkCreatePipelineLayout(device, &plInfo, nullptr, &envMapDisplayPipelineLayout_));

    // === 3. Allocate Descriptor Set — USING YOUR REAL POOL ===
    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = rtDescriptorPool_.get(),  // ← FIXED: .get() on Handle
        .descriptorSetCount = 1,
        .pSetLayouts        = &envMapDisplayDescSetLayout_
    };
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &envMapDisplayDescriptorSet_));

    // === 4. Update sampler binding ===
    VkDescriptorImageInfo samplerInfo{
        .sampler     = envMapSampler,
        .imageView   = envMapView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet write{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = envMapDisplayDescriptorSet_,
        .dstBinding      = 0,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo      = &samplerInfo
    };
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    // === 5. Load shader using OUR loadShader() — NO REDECLARATION ===
    VkShaderModule shaderModule = loadShader("shaders/envmap_display.comp.spv");
    if (shaderModule == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Failed to load envmap_display.comp.spv — did you compile it?");
        return;
    }

    VkPipelineShaderStageCreateInfo stage{
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shaderModule,
        .pName  = "main"
    };

    VkComputePipelineCreateInfo pipeInfo{
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = stage,
        .layout = envMapDisplayPipelineLayout_
    };

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &envMapDisplayPipeline_));

    // Clean up
    vkDestroyShaderModule(device, shaderModule, nullptr);

    LOG_AMOURANTH("ENVMAP DISPLAY PIPELINE FORGED — MODE 1 READY — PRESS 1 BRO");
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

// ── SBT FORGE — THE PHOTONS INTO THE VOID — FINAL ETERNAL CUT ───────────────
// DECEMBER 08 2025 — THE LAST TIME THIS FUNCTION WILL EVER BE TOUCHED
// FULLY COMPATIBLE WITH YOUR ACTUAL BufferManager.cpp
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createShaderBindingTable(VkCommandPool pool, VkQueue queue)
{
    static bool rtExtensionsLoaded = false;
    if (!rtExtensionsLoaded) {
        RTX::loadRTExtensions(stone_instance(), stone_device());
        g_vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
            vkGetDeviceProcAddr(stone_device(), "vkGetRayTracingShaderGroupHandlesKHR"));
        if (!g_vkGetRayTracingShaderGroupHandlesKHR) {
            LOG_FATAL_CAT("PIPELINE", "vkGetRayTracingShaderGroupHandlesKHR not available");
            return;
        }
        rtExtensionsLoaded = true;
    }

    if (rtPipeline_.get() == VK_NULL_HANDLE) {
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

    if (totalGroups == 0) {
        LOG_FATAL_CAT("PIPELINE", "Zero shader groups — nothing to bind");
        return;
    }

    const VkDeviceSize raygenOffset   = 0;
    const VkDeviceSize missOffset     = align_up(RG * stride, baseAlign);
    const VkDeviceSize hitOffset      = align_up(missOffset + MI * stride, baseAlign);
    const VkDeviceSize callableOffset = align_up(hitOffset + HG * stride, baseAlign);
    const VkDeviceSize requiredSize   = align_up(callableOffset, baseAlign);

    // FORGE THE TRUE ETERNAL 2048 MiB SBT STONE
    static uint64_t SBT_STONE_HANDLE = 0;

    if (SBT_STONE_HANDLE == 0) {
        LOG_AMOURANTH("FORGING THE TRUE ETERNAL 2048 MiB SBT STONE — THE ALTAR OF THE GODS");

        const VkDeviceSize stoneSize = 2048ULL * 1024 * 1024;

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
            LOG_FATAL_CAT("PIPELINE", "No device-local memory for 2048 MiB SBT stone");
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

        BufferManager::BufferInfo info;
        info.buffer  = buffer;
        info.memory  = memory;
        info.size    = stoneSize;
        info.aligned = stoneSize;
        info.usage   = bufferInfo.usage;
        info.tag     = "SBT_ETERNAL_STONE_2048M";

        SBT_STONE_HANDLE = reinterpret_cast<uint64_t>(buffer);
        BufferManager::s_buffers[SBT_STONE_HANDLE] = std::move(info);

        LOG_AMOURANTH("2048 MiB SBT STONE FORGED — THE ALTAR IS READY");
    }

    const auto* stone = BufferManager::get(SBT_STONE_HANDLE);
    if (!stone || stone->buffer == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Eternal SBT stone vanished");
        return;
    }

    static std::atomic<VkDeviceSize> sbtAllocator{0};
    VkDeviceSize myOffset = sbtAllocator.fetch_add(requiredSize, std::memory_order_relaxed);

    if (myOffset + requiredSize > stone->size) {
        LOG_FATAL_CAT("PIPELINE", "SBT allocation overflow — needed {} bytes, only {} left", requiredSize, stone->size - myOffset);
        return;
    }

    VkBufferDeviceAddressInfo addrInfo{
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = stone->buffer
    };
    const VkDeviceAddress sbtBaseAddr = vkGetBufferDeviceAddress(stone_device(), &addrInfo) + myOffset;

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

    void* stagingMapped = BufferManager::stagingPtr();
    if (!stagingMapped) {
        LOG_FATAL_CAT("PIPELINE", "Global staging buffer not mapped");
        return;
    }

    std::memcpy(stagingMapped, handleStorage.data(), handleStorage.size());
    BufferManager::advanceStagingOffset(handleStorage.size());

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

    setSBT(stone->buffer, stone->memory, sbtBaseAddr, requiredSize);
    sbtAddress_ = sbtBaseAddr;

    LOG_AMOURANTH("SBT FORGED — 2048 MiB STONE — PINK PHOTONS ARMED");
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

void PipelineManager::createRayTracingPipeline()
{
    auto load = [this](const char* sacred) -> VkShaderModule {
        VkShaderModule mod = loadShader(sacred);
        if (!mod) {
            LOG_FATAL_CAT("PIPELINE", "SACRED SHADER MISSING: {}", sacred);
            phase9_ballerina("SHADER FAILURE", std::source_location::current());
        }
        return mod;
    };

    VkShaderModule raygen = load("assets/shaders/raytracing/raygen.spv");
    VkShaderModule miss   = load("assets/shaders/raytracing/miss.spv");
    VkShaderModule hit    = load("assets/shaders/raytracing/closest_hit.spv");
    VkShaderModule shadow = load("assets/shaders/raytracing/shadowmiss.spv");

    // Uncapped = kill the weak
    if (Options::CURRENT_PRESET == Options::Preset::UncappedPerformance) {
        if (hit)    { vkDestroyShaderModule(stone_device(), hit, nullptr);    hit = VK_NULL_HANDLE; }
        if (shadow) { vkDestroyShaderModule(stone_device(), shadow, nullptr); shadow = VK_NULL_HANDLE; }
    }

    shaderModules_.clear();
    if (raygen) shaderModules_.emplace_back(raygen, stone_device(), vkDestroyShaderModule);
    if (miss)   shaderModules_.emplace_back(miss,   stone_device(), vkDestroyShaderModule);
    if (hit)    shaderModules_.emplace_back(hit,    stone_device(), vkDestroyShaderModule);
    if (shadow) shaderModules_.emplace_back(shadow, stone_device(), vkDestroyShaderModule);

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    stages.reserve(4);
    groups.reserve(4);

    uint32_t stageIndex = 0;

    auto general = [&](VkShaderModule mod, VkShaderStageFlagBits stage) {
        if (!mod) return;
        stages.push_back({ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                          .stage = stage, .module = mod, .pName = "main" });
        groups.push_back({ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                          .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                          .generalShader = stageIndex++ });
    };

    auto hitGroup = [&](VkShaderModule mod) {
        if (mod) {
            stages.push_back({ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                              .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                              .module = mod, .pName = "main" });
            groups.push_back({ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                              .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                              .closestHitShader = stageIndex++ });
        } else {
            groups.push_back({ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                              .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR });
        }
    };

    general(raygen, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    general(miss,   VK_SHADER_STAGE_MISS_BIT_KHR);
    if (shadow) general(shadow, VK_SHADER_STAGE_MISS_BIT_KHR);
    hitGroup(hit);

    raygenGroupCount_ = 1;
    missGroupCount_   = shadow ? 2 : 1;
    hitGroupCount_    = 1;

    LOG_MAIN(
        "THE CROWN IS FORGED — 2 FRAMES — STONEKEYED\n"
        "══════════════════════════════════════════════════════════════════════════════\n");

    if (rtPipelineLayout_.get() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Pipeline layout is null — empire broken");
        phase9_ballerina("LAYOUT FAILURE", std::source_location::current());
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
    VK_CHECK(vkCreateRayTracingPipelinesKHR_(
        stone_device(), VK_NULL_HANDLE, VK_NULL_HANDLE,
        1, &createInfo, nullptr, &pipeline));

    rtPipeline_ = Handle<VkPipeline>(pipeline, stone_device(),
        [](VkDevice d, VkPipeline p, auto*) { vkDestroyPipeline(d, p, nullptr); });

    LOG_AMOURANTH("THE CROWN IS WORN — NO ARGUMENTS — 2 FRAMES — PINK PHOTONS ETERNAL");
}

// ──────────────────────────────────────────────────────────────────────────────
// forgeRTXPipeline — Main Pipeline Creation with Recovery
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue)
{
    if (s_crownForged) {
        LOG_AMOURANTH("THE CROWN IS ALREADY WORN — PHOTONS FLOW — NO FORGING NEEDED");
        return;
    }

    // THE EXTENSIONS ARE LOADED — THE EMPIRE SEES
    loadRayTracingExtensions();

    // THE CROWN IS FORGED — ONCE AND FOREVER
    createDescriptorPool();
    createPipelineLayout();
    allocateDescriptorSets();
    createRayTracingPipeline();
    createShaderBindingTable(commandPool, graphicsQueue);

    // THE CROWN IS SEALED — ETERNAL
    stone_seal_pipeline(this);
    s_crownForged = true;

    LOG_AMOURANTH("THE CROWN IS WORN — PINK PHOTONS ETERNAL — THE EMPIRE IS COMPLETE");
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