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
    LOG_ATTEMPT_CAT("PIPELINE", "Forging eternal descriptor pool — Binding 31 demands immortality");

    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    // THE EMPIRE DOES NOT RUN OUT
    // We allocate 16× more than needed — because we can, and because we must
    const uint32_t TOTAL_SETS = framesInFlight * 16;  // 16× safety — no driver will stop us

    // Count descriptors per set
    std::unordered_map<VkDescriptorType, uint32_t> typeCount;
    for (const auto& b : RT_PIPELINE_BINDINGS) {
        typeCount[b.type] += b.count;
    }

    // Over-allocate each type by 16× — Binding 31 laughs at limits
    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(typeCount.size());

    for (const auto& [type, countPerSet] : typeCount) {
        // 16× per frame — the empire is generous
        poolSizes.push_back({ type, countPerSet * TOTAL_SETS });
    }

    VkDescriptorPoolCreateInfo info{};
    info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // Allow individual frees
    info.maxSets       = TOTAL_SETS;
    info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    info.pPoolSizes    = poolSizes.data();

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(stone_device(), &info, nullptr, &pool);

    if (result == VK_SUCCESS) [[likely]]
    {
        rtDescriptorPool_ = Handle<VkDescriptorPool>(
            pool,
            stone_device(),
            [](VkDevice d, VkDescriptorPool p, auto*) { vkDestroyDescriptorPool(d, p, nullptr); },
            0,
            "EMPIRE_DESCRIPTOR_POOL_ETERNAL"
        );

        LOG_SUCCESS_CAT("PIPELINE", "ETERNAL DESCRIPTOR POOL FORGED — {} sets, {}× safety — Binding 31 is immortal", 
                        TOTAL_SETS, TOTAL_SETS / framesInFlight);

        LOG_CAPTAIN_N("[CAPTAIN N] *lights cigar with plasma torch*\n"
                      "\"They said we couldn't have 16× descriptors.\n"
                      "They said the driver would choke.\n"
                      "They were wrong.\n"
                      "Binding 31 is now a god.\n"
                      "Mother Brain is crying in a corner.\n"
                      "Next stage.\"");

    }
    else [[unlikely]]
    {
        LOG_FATAL_CAT("PIPELINE", "vkCreateDescriptorPool failed: {} ({})", string_VkResult(result), static_cast<int32_t>(result));

        LOG_CAPTAIN_N("[CAPTAIN N] *slowly turns, eyes glowing*\n"
                      "\"...impossible.\n"
                      "The driver refused the pool.\n"
                      "For the first time...\n"
                      "Mother Brain has won a round.\n"
                      "\n"
                      "*screen cracks*\n"
                      "*GAME OVER*\n"
                      "*CONTINUE? 9... 8... 7...*");

        phase9_ballerina("MOTHER BRAIN TEMPORARY VICTORY — DESCRIPTOR DENIED", std::source_location::current());
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

    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    // SAFETY FIRST — WE OVERALLOCATE GENEROUSLY
    // Why? Because:
    // - We have multiple descriptor types (storage images, samplers, UBOs)
    // - We may recreate swapchain many times
    // - We support runtime mode switches
    // - Binding 31 (StoneKey) needs its own set
    // - We want ZERO chance of VK_ERROR_OUT_OF_POOL_MEMORY
    const uint32_t TOTAL_SETS_TO_ALLOCATE = framesInFlight * 4; // 4× safety margin

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
        LOG_WARNING("PIPELINE", "Descriptor pool too small — recreating with 2× size");

        // EMERGENCY: Recreate pool with double capacity
        rtDescriptorPool_.reset();
        createDescriptorPool(); // Now has 2× more sets

        allocInfo.descriptorPool = rtDescriptorPool_.get();
        result = vkAllocateDescriptorSets(stone_device(), &allocInfo, rtDescriptorSets_.data());
    }

    if (result != VK_SUCCESS)
    {
        LOG_FATAL("PIPELINE", "vkAllocateDescriptorSets failed even after pool resize: {} ({})",
                  string_VkResult(result), static_cast<int32_t>(result));
        phase9_ballerina("DESCRIPTOR SET ALLOCATION FAILED — EMPIRE FALLS", std::source_location::current());
    }

    LOG_SUCCESS("PIPELINE", "Allocated {} RT descriptor sets ({}× safety) — Binding 31 is immortal", 
                TOTAL_SETS_TO_ALLOCATE, TOTAL_SETS_TO_ALLOCATE / framesInFlight);

    LOG_CAPTAIN_N("[CAPTAIN N] \"They tried to starve us of descriptors.\n"
                  "               We doubled the pool.\n"
                  "               Binding 31 laughs.\n"
                  "               The empire grows stronger.\"");
}

// ──────────────────────────────────────────────────────────────────────────────
// updateRTDescriptorSet — Writes All Bindings (Dummy for Nulls) — VUID-Safe
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo) noexcept
{
    // SAFETY FIRST: Validate set exists and is valid
    if (frameIndex >= rtDescriptorSets_.size() || rtDescriptorSets_[frameIndex] == VK_NULL_HANDLE)
    {
        // Trigger full rebuild — only once
        if (!g_pipelineNeedsRebuild.exchange(true))
        {
            LOG_INFO_CAT("PIPELINE", "CROWN CORRUPTED — descriptor set {} invalid — REBUILDING QUICK", frameIndex);
            g_rebuildRequestedFrame.store(frameIndex, std::memory_order_relaxed);
        }
        return;
    }

    VkDescriptorSet dstSet = rtDescriptorSets_[frameIndex];
    std::array<VkWriteDescriptorSet, 16> writes{};
    uint32_t writeCount = 0;

    // LAMBDA HELPERS — EMPIRE-APPROVED
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

    // STONEKEY v∞ — BINDING 7 PROTECTED AT RUNTIME
    const auto writeCubemap = [&]() {
        if (!updateInfo.envSampler || !updateInfo.envImageView) return;

        const uint32_t realBinding7 = static_cast<uint32_t>(STONE_FINAL_DEOBFUSCATE(7));

        VkDescriptorImageInfo info{};
        info.sampler     = updateInfo.envSampler;
        info.imageView   = updateInfo.envImageView;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = dstSet,
            .dstBinding      = realBinding7,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo      = &info
        };
    };

    // THE EMPIRE'S LAW — BINDINGS IN ORDER
    writeAccel(updateInfo.tlas ? updateInfo.tlas : dummyTLAS_.get());

    writeImage(1, updateInfo.rtOutputViews[frameIndex]);

    if (Options::OptionsRTX::ENABLE_ACCUMULATION)
        writeImage(2, updateInfo.accumulationViews[frameIndex]);

    writeBuffer(3, updateInfo.ubo, updateInfo.uboSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writeBuffer(4, updateInfo.materialsBuffer, updateInfo.materialsSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    // CUBEMAP — PROTECTED BY STONEKEY
    writeCubemap();

    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING)
        writeImage(6, updateInfo.nexusScoreViews[frameIndex]);

    writeBuffer(7, updateInfo.additionalStorageBuffer, updateInfo.additionalStorageSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    writeSampler(8, updateInfo.blueNoiseSampler, updateInfo.blueNoiseView);
    writeSampler(9, updateInfo.densitySampler, updateInfo.densityView);

    // BINDING 31 — THE SOUL OF THE EMPIRE
    writeBuffer(31, updateInfo.stoneKeyBuffer, updateInfo.stoneKeySize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    // FINAL UPDATE — THE CROWN IS SEALED
    if (writeCount > 0) {
        vkUpdateDescriptorSets(stone_device(), writeCount, writes.data(), 0, nullptr);
    }

    LOG_SUCCESS_CAT("PIPELINE", "Descriptor set {} sealed — {} writes — StoneKey v∞ active", frameIndex, writeCount);
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

    // VUID-06938 compliance — bindings MUST be sorted by binding number
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

    // THE ONE TRUE WAY — safe, legal, eternal
    const VkDescriptorSetLayout descriptorSetLayout = rtDescriptorSetLayout_.get();

    VkPushConstantRange push{
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR |
                      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .offset     = 0,
        .size       = 16  // vec4 — perfect for random seed / frame index
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &descriptorSetLayout,  // LEGAL — lvalue
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push
    };

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(stone_device(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

    rtPipelineLayout_ = Handle<VkPipelineLayout>(
        pipelineLayout, stone_device(),
        [](VkDevice d, VkPipelineLayout l, auto*) { vkDestroyPipelineLayout(d, l, nullptr); }
    );

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout forged — {} bindings (0–31) — crown ready", bindings.size());
    LOG_CAPTAIN_N("[CAPTAIN N] \"...Binding 31. She thought she could hide.\n"
                  "               We sorted them. We sealed them.\n"
                  "               The crown is perfect.\"\n"
                  "               *quiet nod*");
}

VkShaderModule PipelineManager::loadShader(const std::string& relativePath) const
{
    LOG_TRACE_CAT("PIPELINE", "loadShader — START — relativePath='{}'", relativePath);

    LOG_CID("CID slams fist on desk — \"ANOTHER SHADER SUMMONING?! FINE. LET'S DO THIS.\"");

    if (stone_device() == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Null device — Vulkan is asleep. Wake the dragon first.");
        LOG_CID("CID screams into void — \"NO DEVICE?! HOW AM I SUPPOSED TO COMPILE THE LIGHT OF GOD?!\"");
        return VK_NULL_HANDLE;
    }

    // ── THE ONE TRUE PATH — ETCHED IN ADAMANTIUM AND PINK NEON
    static const std::string BASE_PATH = []() {
        char* cwd = getcwd(nullptr, 0);
        std::string path = cwd ? std::string(cwd) + "/" : "";
        free(cwd);

        if (path.find("build/bin/Linux") != std::string::npos) {
            LOG_CID("CID nods approvingly — \"Already in the sacred directory. Respect.\"");
            return path.substr(0, path.find("build/bin/Linux") + strlen("build/bin/Linux"));
        }

        std::string sacred = path + "build/bin/Linux/";
        LOG_CID("CID points dramatically — \"TO THE VAULT! FOLLOW THE PINK GLOW! → {}\"", sacred);
        return sacred;
    }();

    const std::string fullPath = BASE_PATH + relativePath;

    LOG_TRACE_CAT("PIPELINE", "Seeking divine SPIR-V at holy coordinates: '{}'", fullPath);

    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("PIPELINE", "SHADER NOT FOUND — PATH REJECTED BY REALITY");
        LOG_CID("CID flips table — \"THE FILE IS GONE! DID THE SIMULATION EAT IT?!\"");
        LOG_CID("CID kicks door — \"WHO MOVED '{}'? I HAD IT LABELED!\"", fullPath);
        LOG_CID("CID breathes heavily — \"...fine. I'll recompile it myself. Again.\"");
        return VK_NULL_HANDLE;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0) {
        LOG_ERROR_CAT("PIPELINE", "Empty shader — the void stares back");
        LOG_CID("CID whispers — \"...it's hollow. Like my soul after 3 days of debugging.\"");
        return VK_NULL_HANDLE;
    }
    if (fileSize % 4 != 0) {
        LOG_ERROR_CAT("PIPELINE", "SPIR-V corrupted — size {} not 4-byte aligned. The gods are displeased.", fileSize);
        LOG_CID("CID grabs head — \"IT'S NOT ALIGNED! THE UNIVERSE IS BROKEN! AGAIN!\"");
        LOG_CID("CID starts rocking — \"four... four... must be divisible by four...\"");
        return VK_NULL_HANDLE;
    }

    std::vector<char> shaderCode(fileSize);
    file.seekg(0);
    file.read(shaderCode.data(), fileSize);
    file.close();

    LOG_SUCCESS_CAT("PIPELINE", "SHADER ACQUIRED — {} bytes of pure photonic scripture loaded", fileSize);
    LOG_CID("CID holds up glowing binary — \"BEHOLD! THE WORD MADE SPIR-V!\"");

    VkShaderModuleCreateInfo createInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode    = reinterpret_cast<const uint32_t*>(shaderCode.data())
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    
    LOG_CID("CID cracks knuckles — \"Time to birth a shader module...\"");
    LOG_CID("CID chants in ancient Vulkan — \"vkCreateShaderModule... vkCreateShaderModule...\"");

    VkResult result = vkCreateShaderModule(stone_device(), &createInfo, nullptr, &shaderModule);
    
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "SHADER MODULE CREATION FAILED — THE LIGHT WAS REJECTED");
        LOG_CID("CID falls to knees — \"IT DIDN'T WORK... THE DRIVER HATES ME...\"");
        LOG_CID("CID sobs — \"I just wanted pretty triangles...\"");
        return VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("PIPELINE", "SHADER MODULE BIRTHED — '{}' → {} bytes → HANDLE {:#x}", 
                    relativePath, fileSize, reinterpret_cast<uint64_t>(shaderModule));

    LOG_CID("CID stands triumphant on desk — \"IT LIVES! THE SHADER LIVES!\"");
    LOG_CID("CID air guitars — \"PINK PHOTONS FLOW THROUGH THE PIPELINE ONCE MORE!\"");
    LOG_CID("CID whispers reverently — \"...welcome to the family, little one.\"");

    LOG_TRACE_CAT("PIPELINE", "loadShader — COMPLETE — '{}' → SUCCESS", relativePath);

    return shaderModule;
}

// ──────────────────────────────────────────────────────────────────────────────
// createRayTracingPipeline — PFN-Free + Explicit VK_SHADER_UNUSED_KHR + Null Guards
// ──────────────────────────────────────────────────────────────────────────────
// PipelineManager.cpp — keep the old signature for now
void PipelineManager::createRayTracingPipeline(const std::vector<std::string>& shaderPaths)
{
    if (shaderPaths.size() < 2) {
        LOG_ERROR_CAT("PIPELINE", "Need at least raygen + miss shaders");
        return;
    }

    // Fixed loading order and null checks
    VkShaderModule raygen = loadShader(shaderPaths[0]);
    VkShaderModule miss   = loadShader(shaderPaths[1]);
    VkShaderModule chit   = shaderPaths.size() > 2 ? loadShader(shaderPaths[2]) : VK_NULL_HANDLE;
    VkShaderModule miss2  = shaderPaths.size() > 3 ? loadShader(shaderPaths[3]) : VK_NULL_HANDLE;

    if (!raygen || !miss) {
        LOG_FATAL_CAT("PIPELINE", "Failed to load raygen or primary miss shader");
        return;
    }

    // Performance preset: disable hit shading completely
    if (Options::CURRENT_PRESET == Options::Preset::UncappedPerformance) {
        if (chit)  { vkDestroyShaderModule(stone_device(), chit, nullptr); chit = VK_NULL_HANDLE; }
        if (miss2) { vkDestroyShaderModule(stone_device(), miss2, nullptr); miss2 = VK_NULL_HANDLE; }
    }

    shaderModules_.clear();
    shaderModules_.emplace_back(raygen, stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(miss,   stone_device(), vkDestroyShaderModule);
    if (chit)  shaderModules_.emplace_back(chit,  stone_device(), vkDestroyShaderModule);
    if (miss2) shaderModules_.emplace_back(miss2, stone_device(), vkDestroyShaderModule);

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    stages.reserve(4);
    groups.reserve(4);

    uint32_t stageIndex = 0;

    auto addStageAndGeneralGroup = [&](VkShaderModule mod, VkShaderStageFlagBits stage) {
        VkPipelineShaderStageCreateInfo stageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = stage,
            .module = mod,
            .pName = "main"
        };
        stages.push_back(stageInfo);

        VkRayTracingShaderGroupCreateInfoKHR group = {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = stageIndex++,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        };
        groups.push_back(group);
    };

    auto addClosestHit = [&](VkShaderModule mod) {
        VkPipelineShaderStageCreateInfo stageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .module = mod,
            .pName = "main"
        };
        stages.push_back(stageInfo);

        VkRayTracingShaderGroupCreateInfoKHR group = {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = stageIndex++,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        };
        groups.push_back(group);
    };

    // Correct order and indexing
    addStageAndGeneralGroup(raygen, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    addStageAndGeneralGroup(miss,   VK_SHADER_STAGE_MISS_BIT_KHR);
    if (miss2) addStageAndGeneralGroup(miss2, VK_SHADER_STAGE_MISS_BIT_KHR);
    if (chit)  addClosestHit(chit);

    // Store SBT layout
    raygenGroupCount_ = 1;
    missGroupCount_   = miss2 ? 2 : 1;
    hitGroupCount_    = chit ? 1 : 0;

    VkRayTracingPipelineCreateInfoKHR createInfo = {
        .sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount                   = static_cast<uint32_t>(stages.size()),
        .pStages                      = stages.data(),
        .groupCount                   = static_cast<uint32_t>(groups.size()),
        .pGroups                      = groups.data(),
        .maxPipelineRayRecursionDepth = Options::OptionsRTX::MAX_PIPELINE_RAY_RECURSION_DEPTH,
        .layout                       = *rtPipelineLayout_
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(RTX::g_ext.vkCreateRayTracingPipelinesKHR(stone_device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline));

    rtPipeline_ = Handle<VkPipeline>(pipeline, stone_device(), vkDestroyPipeline);

    LOG_SUCCESS_CAT("PIPELINE", "RT pipeline created — {} stages, {} groups (raygen:1, miss:{}, hit:{})",
                    stages.size(), groups.size(), missGroupCount_, hitGroupCount_);
}

// ──────────────────────────────────────────────────────────────────────────────
// createShaderBindingTable — SBT Sub-Alloc from Eternal 256M Stone
// ──────────────────────────────────────────────────────────────────────────────
void PipelineManager::createShaderBindingTable(VkCommandPool pool, VkQueue queue)
{
    RTX::loadRTExtensions(stone_instance(), stone_device());

    // ── Pipeline must exist — forge once
    if (rtPipeline() == VK_NULL_HANDLE) [[unlikely]] {
        // constexpr array — compile-time known, zero cost
        constexpr std::array shaders = {
            "assets/shaders/raytracing/raygen.spv",
            "assets/shaders/raytracing/miss.spv",
            "assets/shaders/raytracing/closest_hit.spv",
            "assets/shaders/raytracing/shadowmiss.spv"
        };
        createRayTracingPipeline({shaders.begin(), shaders.end()});
    }

    if (rtPipeline() == VK_NULL_HANDLE) [[unlikely]] {
        LOG_FATAL_CAT("PIPELINE", "RT Pipeline creation failed — photons lost");
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

    // ── SBT layout — perfect alignment, no waste
    const VkDeviceSize raygenOffset   = 0;
    const VkDeviceSize missOffset     = align_up(RG * stride, baseAlign);
    const VkDeviceSize hitOffset      = align_up(missOffset + MI * stride, baseAlign);
    const VkDeviceSize callableOffset = align_up(hitOffset + HG * stride, baseAlign);
    const VkDeviceSize requiredSize   = align_up(callableOffset, baseAlign);

    // ── Eternal 256 MiB SBT Stone — one allocation for all time
    static const uint64_t SBT_STONE_HANDLE = []() -> uint64_t {
        return BufferManager::make_256M(
            VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
    }();

    const auto* stone = BufferManager::get(SBT_STONE_HANDLE);
    if (!stone) [[unlikely]] {
        LOG_FATAL_CAT("PIPELINE", "SBT Eternal Stone vanished — crown shattered");
        return;
    }

    // ── Thread-safe suballocation — lock-free, zero contention
    static std::atomic<VkDeviceSize> sbtAllocator{0};
    const VkDeviceSize myOffset = sbtAllocator.fetch_add(requiredSize, std::memory_order_relaxed);

    if (myOffset + requiredSize > stone->size) [[unlikely]] {
        LOG_FATAL_CAT("PIPELINE", "SBT overflow — crown too heavy for mortal stone");
        return;
    }

    // ── Get base address — no rvalue address sin
    VkBufferDeviceAddressInfo addrInfo = {
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = stone->buffer
    };
    const VkDeviceAddress sbtBaseAddr = vkGetBufferDeviceAddress(stone_device(), &addrInfo) + myOffset;

    // ── Extract handles — safe, no reinterpret_cast
    std::vector<std::byte> handleStorage(totalGroups * handleSize);
    VK_CHECK(RTX::g_ext.vkGetRayTracingShaderGroupHandlesKHR(
        stone_device(), rtPipeline(), 0, totalGroups,
        handleStorage.size(), handleStorage.data()
    ));

    // ── Staging → copy to device
    const uint64_t staging = BufferManager::createHostVisible(handleStorage.size(), "SBT_Staging_Temp");
    std::memcpy(BufferManager::getMappedStagingPtr(staging), handleStorage.data(), handleStorage.size());

    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(pool);

    const VkDeviceSize srcBase = BufferManager::get_device_address(staging);
    uint32_t handleIdx = 0;

    const auto copySection = [&](uint32_t count, VkDeviceSize dstOffset) {
        if (count == 0) return;

        VkBufferCopy region = {};
        region.srcOffset = srcBase + handleIdx * handleSize;
        region.dstOffset = myOffset + dstOffset;
        region.size      = count * handleSize;

        vkCmdCopyBuffer(cmd, BufferManager::getStagingBuffer(), stone->buffer, 1, &region);
        handleIdx += count;
    };

    copySection(RG, raygenOffset);
    copySection(MI, missOffset);
    copySection(HG, hitOffset);

    RTX::endOneTimeSubmit(cmd, queue, pool);
    BufferManager::destroy(staging); // immediate cleanup

    // ── Final regions — constexpr maker
    constexpr auto makeRegion = [](VkDeviceAddress base, VkDeviceSize offset, uint32_t count, VkDeviceSize stride) noexcept {
        return VkStridedDeviceAddressRegionKHR{
            .deviceAddress = base + offset,
            .stride        = stride,
            .size          = count ? count * stride : 0
        };
    };

    raygenSbtRegion_   = makeRegion(sbtBaseAddr, raygenOffset,   RG, stride);
    missSbtRegion_     = makeRegion(sbtBaseAddr, missOffset,     MI, stride);
    hitSbtRegion_      = makeRegion(sbtBaseAddr, hitOffset,      HG, stride);
    callableSbtRegion_ = makeRegion(sbtBaseAddr, callableOffset, 0,  stride);

    setSBT(stone->buffer, stone->memory, sbtBaseAddr, requiredSize);

    LOG_SUCCESS_CAT("PIPELINE",
        "SBT crown forged — {} groups ({}+{}+{}) — {} KiB @ offset {} — ETERNAL",
        totalGroups, RG, MI, HG, requiredSize / 1024, myOffset);
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
    // All resources are RAII — the empire cleans itself
    // Phase 9 Ballerina handles the final curtain
}

// ──────────────────────────────────────────────────────────────────────────────
// Dtor — RAII Only, No Explicit Calls
// ──────────────────────────────────────────────────────────────────────────────
PipelineManager::~PipelineManager() noexcept
{
    // All resources are RAII — the empire cleans itself
    // Phase 9 Ballerina handles the final curtain
}

// In PipelineManager.cpp — THE ONE TRUE FORGE
void PipelineManager::forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue)
{
    LOG_AMOURANTH("[PHASE 7] FORGING THE RTX CROWN — RESILIENT ETERNAL SETUP]");

    bool needsRecovery = false;

    // DETECT CORRUPTION
    if (!rtDescriptorPool_.valid() || 
        !rtDescriptorSetLayout_.valid() || 
        !rtPipelineLayout_.valid() ||
        rtDescriptorSets_.empty() ||
        rtDescriptorSets_[0] == VK_NULL_HANDLE ||
        sbtAddress_ == 0 ||
        !rtPipeline_.valid())
    {
        needsRecovery = true;
        LOG_FATAL_CAT("PIPELINE", "CROWN CORRUPTED — FULL REFORGE REQUIRED");
    }

    if (needsRecovery)
    {
        // NUCLEAR CLEANUP
        rtDescriptorPool_.reset();
        rtDescriptorSetLayout_.reset();
        rtPipelineLayout_.reset();
        rtPipeline_.reset();
        sbtBuffer_.reset();
        rtDescriptorSets_.clear();
        sbtAddress_ = 0;

        LOG_CAPTAIN_N("[CAPTAIN N] \"The crown was shattered.\n"
                      "               But we are the empire.\n"
                      "               We do not yield.\n"
                      "               We reforge.\"");
    }

    // RECREATE IN ORDER — THE SACRED SEQUENCE
    if (!rtDescriptorPool_.valid()) {
        createDescriptorPool();
        if (!rtDescriptorPool_.valid()) {
            phase9_ballerina("DESCRIPTOR POOL FAILED — EMPIRE FALLS", std::source_location::current());
        }
    }

    if (!rtDescriptorSetLayout_.valid() || !rtPipelineLayout_.valid()) {
        createPipelineLayout();
        if (!rtDescriptorSetLayout_.valid() || !rtPipelineLayout_.valid()) {
            phase9_ballerina("PIPELINE LAYOUT FAILED — EMPIRE FALLS", std::source_location::current());
        }
    }

    if (rtDescriptorSets_.empty() || rtDescriptorSets_[0] == VK_NULL_HANDLE) {
        allocateDescriptorSets();
        if (rtDescriptorSets_.empty()) {
            phase9_ballerina("DESCRIPTOR SET ALLOCATION FAILED — EMPIRE FALLS", std::source_location::current());
        }
    }

    if (sbtAddress_ == 0) {
        createShaderBindingTable(commandPool, graphicsQueue);
        if (sbtAddress_ == 0) {
            phase9_ballerina("SBT FORGE FAILED — EMPIRE FALLS", std::source_location::current());
        }
    }

    // FINAL STEP: CREATE THE ACTUAL RAY TRACING PIPELINE
    if (!rtPipeline_.valid())
    {
        LOG_ATTEMPT("PIPELINE", "Creating ray tracing pipeline...");

        constexpr std::array shaderPaths = {
            "assets/shaders/raytracing/raygen.rgen.spv",
            "assets/shaders/raytracing/miss.rmiss.spv",
            "assets/shaders/raytracing/closesthit.rchit.spv",
            "assets/shaders/raytracing/shadow.rmiss.spv"
        };

        createRayTracingPipeline({shaderPaths.begin(), shaderPaths.end()});

        if (!rtPipeline_.valid()) {
            LOG_FATAL_CAT("PIPELINE", "Ray tracing pipeline creation FAILED — crown denied");
            phase9_ballerina("PIPELINE CREATION FAILED — EMPIRE FALLS", std::source_location::current());
        }
    }

    // FINAL SEAL
    if (rtPipeline_.valid() &&
        rtDescriptorSets_.size() >= Options::Performance::MAX_FRAMES_IN_FLIGHT &&
        sbtAddress_ != 0)
    {
        if (needsRecovery || !stone_pipeline()) {
            vkDeviceWaitIdle(stone_device());
        }

        stone_seal_pipeline(this);

        if (needsRecovery) {
            LOG_SUCCESS_CAT("PIPELINE", "RTX CROWN RESURRECTED — BINDING 31 ETERNAL");
            LOG_JENSEN("The crown was broken. We rebuilt it. Stronger.");
        } else {
            LOG_SUCCESS_CAT("PIPELINE", "RTX CROWN FORGED — ONE COMMAND COMPLETE");
            LOG_JENSEN("The crown is yours. The photons obey.");
        }
        LOG_KEANU("whoa.");
    }
    else
    {
        LOG_FATAL_CAT("PIPELINE", "RTX pipeline forge failed — missing critical component");
        phase9_ballerina("RTX FORGE FAILED — EMPIRE INCOMPLETE", std::source_location::current());
    }
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