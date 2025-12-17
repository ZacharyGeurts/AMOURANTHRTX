// src/engine/GLOBAL/PipelineManager.cpp

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/UBO.hpp"

#include <fstream>
#include <algorithm>
#include <format>
#include <vector>
#include <array>
#include <unordered_map>
#include <stb/stb_image.h>
#include <unistd.h>
#include <stdexcept>
#include <cmath>
#include <atomic>

using namespace Logging::Color;
using StoneKey::stone_device;
using StoneKey::stone_instance;
using StoneKey::stone_physical;
using StoneKey::stone_mesh;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_seal_pipeline;

struct RTBinding {
    uint32_t binding;
    VkDescriptorType type;
    uint32_t count;
    VkShaderStageFlags stage;
};

const std::array<RTBinding, 12> RT_PIPELINE_BINDINGS = {{
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR},
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
    {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
    {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
    {11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
    {31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}
}};

namespace RTX {

std::atomic<bool>     PipelineManager::g_pipelineNeedsRebuild{false};
std::atomic<uint32_t> PipelineManager::g_rebuildRequestedFrame{UINT32_MAX};

PendingEnvMapUpload pendingEnvMapUpload_{};

void PipelineManager::createDescriptorPool()
{
    LOG_INFO_CAT("PIPELINE", "Creating descriptor pool");

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

        LOG_SUCCESS_CAT("PIPELINE", "Descriptor pool created — {} sets", TOTAL_SETS);
    }
    else {
        LOG_FATAL_CAT("PIPELINE", "vkCreateDescriptorPool failed: {} ({})", string_VkResult(result), static_cast<int32_t>(result));
        throw std::runtime_error("Descriptor pool creation failed");
    }
}

PipelineManager::~PipelineManager() = default;

PipelineManager::PipelineManager(VkDevice device, VkPhysicalDevice phys)
{
    LOG_INFO_CAT("PIPELINE", "PipelineManager construction");

    cacheDeviceProperties();
    loadRayTracingExtensions();

    dummyTLAS_ = Handle<VkAccelerationStructureKHR>(
        createDummyTLAS(), stone_device(), g_ext.vkDestroyAccelerationStructureKHR);

    if (dummyTLAS_.get() == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("PIPELINE", "Dummy TLAS creation failed — using null fallback");
    }

    LOG_SUCCESS_CAT("PIPELINE", "PipelineManager constructed — extensions loaded — dummy TLAS ready");
}

void PipelineManager::allocateDescriptorSets()
{
    LOG_TRACE_CAT("PIPELINE", "allocateDescriptorSets started");

    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    if (rtDescriptorPool_.get() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "rtDescriptorPool_ is VK_NULL_HANDLE");
        throw std::runtime_error("Descriptor pool null");
    }

    if (!rtDescriptorSetLayout_.valid()) {
        LOG_FATAL_CAT("PIPELINE", "rtDescriptorSetLayout_ is invalid");
        throw std::runtime_error("Main set layout invalid");
    }

    if (!texDescriptorSetLayout_.valid()) {
        LOG_FATAL_CAT("PIPELINE", "texDescriptorSetLayout_ is invalid");
        throw std::runtime_error("Texture set layout invalid");
    }

    // Allocate main RT descriptor sets (set = 0)
    rtDescriptorSets_.resize(framesInFlight);

    std::vector<VkDescriptorSetLayout> mainLayouts(framesInFlight, rtDescriptorSetLayout_.get());

    VkDescriptorSetAllocateInfo mainAllocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = rtDescriptorPool_.get(),
        .descriptorSetCount = framesInFlight,
        .pSetLayouts        = mainLayouts.data()
    };

    VkResult result = vkAllocateDescriptorSets(stone_device(), &mainAllocInfo, rtDescriptorSets_.data());

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        LOG_WARNING_CAT("PIPELINE", "Main set allocation failed due to pool exhaustion ({}). Recreating pool", string_VkResult(result));
        rtDescriptorPool_.reset();
        createDescriptorPool();

        mainAllocInfo.descriptorPool = rtDescriptorPool_.get();
        result = vkAllocateDescriptorSets(stone_device(), &mainAllocInfo, rtDescriptorSets_.data());
    }

    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkAllocateDescriptorSets failed for main sets: {}", string_VkResult(result));
        throw std::runtime_error("Main descriptor allocation failure");
    }

    LOG_SUCCESS_CAT("PIPELINE", "Allocated {} main descriptor sets (set 0)", framesInFlight);

    // Allocate texture array descriptor sets (set = 2)
    texDescriptorSets_.resize(framesInFlight);

    std::vector<VkDescriptorSetLayout> texLayouts(framesInFlight, texDescriptorSetLayout_.get());

    VkDescriptorSetAllocateInfo texAllocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = rtDescriptorPool_.get(),
        .descriptorSetCount = framesInFlight,
        .pSetLayouts        = texLayouts.data()
    };

    result = vkAllocateDescriptorSets(stone_device(), &texAllocInfo, texDescriptorSets_.data());

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        LOG_WARNING_CAT("PIPELINE", "Texture set allocation failed due to pool exhaustion ({}). Recreating pool", string_VkResult(result));
        rtDescriptorPool_.reset();
        createDescriptorPool();

        texAllocInfo.descriptorPool = rtDescriptorPool_.get();
        result = vkAllocateDescriptorSets(stone_device(), &texAllocInfo, texDescriptorSets_.data());
    }

    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkAllocateDescriptorSets failed for texture sets: {}", string_VkResult(result));
        throw std::runtime_error("Texture descriptor allocation failure");
    }

    LOG_SUCCESS_CAT("PIPELINE", "Allocated {} texture descriptor sets (set 2)", framesInFlight);

    LOG_TRACE_CAT("PIPELINE", "allocateDescriptorSets completed");
}

void PipelineManager::updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo) noexcept
{
    if (frameIndex >= rtDescriptorSets_.size()) {
        LOG_WARNING_CAT("PIPELINE", "frameIndex {} out of range (size {})", frameIndex, rtDescriptorSets_.size());
        return;
    }

    VkDescriptorSet dstSet = rtDescriptorSets_[frameIndex];
    if (dstSet == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("PIPELINE", "dstSet is VK_NULL_HANDLE for frame {}", frameIndex);
        return;
    }

    std::array<VkWriteDescriptorSet, 17> writes{};
    uint32_t writeCount = 0;

    const auto writeAccel = [&](VkAccelerationStructureKHR tlas) {
        tlas = tlas ? tlas : dummyTLAS_.get();
        const VkWriteDescriptorSetAccelerationStructureKHR accelInfo{
            .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures    = &tlas
        };
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext           = &accelInfo,
            .dstSet          = dstSet,
            .dstBinding      = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        };
    };

    const auto writeImage = [&](uint32_t binding, VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL) {
        if (view == VK_NULL_HANDLE) {
            return;
        }
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

    // Bindings
    writeAccel(updateInfo.tlas);
    writeImage(1, updateInfo.rtOutputView);

    if (Options::OptionsRTX::ENABLE_ACCUMULATION && frameIndex < updateInfo.accumulationViews.size()) {
        writeImage(2, updateInfo.accumulationViews[frameIndex]);
    }

    writeBuffer(3, updateInfo.ubo, updateInfo.uboSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writeBuffer(4, updateInfo.materialsBuffer, updateInfo.materialsSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    if (updateInfo.envSampler && updateInfo.envImageView) {
        writeSampler(7, updateInfo.envSampler, updateInfo.envImageView);
    }

    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING && frameIndex < updateInfo.nexusScoreViews.size()) {
        writeImage(6, updateInfo.nexusScoreViews[frameIndex]);
    }

    if (updateInfo.blueNoiseSampler && updateInfo.blueNoiseView) {
        writeSampler(8, updateInfo.blueNoiseSampler, updateInfo.blueNoiseView);
    }

    if (updateInfo.densitySampler && updateInfo.densityView) {
        writeSampler(9, updateInfo.densitySampler, updateInfo.densityView);
    }

    if (updateInfo.additionalStorageBuffer) {
        writeBuffer(10, updateInfo.additionalStorageBuffer, updateInfo.additionalStorageSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }

    if (updateInfo.stoneKeyBuffer) {
        writeBuffer(31, updateInfo.stoneKeyBuffer, updateInfo.stoneKeySize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    }

    if (writeCount > 0) {
        vkUpdateDescriptorSets(stone_device(), writeCount, writes.data(), 0, nullptr);
        LOG_TRACE_CAT("PIPELINE", "Updated {} descriptor writes for frame {}", writeCount, frameIndex);
    } else {
        LOG_TRACE_CAT("PIPELINE", "No descriptor updates needed for frame {}", frameIndex);
    }
}

VkShaderModule PipelineManager::loadShader(const std::string& relativePath) const
{
    LOG_TRACE_CAT("PIPELINE", "Loading shader: {}", relativePath);

    if (stone_device() == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PIPELINE", "Null device — cannot load shader");
        return VK_NULL_HANDLE;
    }

    static const std::string BASE_PATH = []() -> std::string {
        char* cwd = getcwd(nullptr, 0);
        std::string path = cwd ? std::string(cwd) + "/" : "";
        free(cwd);

        const std::string marker = "build/bin/Linux";
        size_t pos = path.find(marker);
        if (pos != std::string::npos) {
            return path.substr(0, pos + marker.length());
        }
        return path + "build/bin/Linux/";
    }();

    const std::string fullPath = BASE_PATH + relativePath;

    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("PIPELINE", "Shader file not found: {}", fullPath);
        return VK_NULL_HANDLE;
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        LOG_ERROR_CAT("PIPELINE", "Invalid SPIR-V size ({} bytes): {}", fileSize, fullPath);
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

    LOG_SUCCESS_CAT("PIPELINE", "Shader loaded: {} ({} bytes)", relativePath, fileSize);
    return module;
}

void PipelineManager::transitionImage(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkAccessFlags srcAccess,
    VkAccessFlags dstAccess,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage) noexcept
{
    if (image == VK_NULL_HANDLE || cmd == VK_NULL_HANDLE) return;

    VkImageMemoryBarrier barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = srcAccess,
        .dstAccessMask       = dstAccess,
        .oldLayout           = oldLayout,
        .newLayout           = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    vkCmdPipelineBarrier(
        cmd,
        srcStage,
        dstStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

void PipelineManager::createShaderBindingTable(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd)
{
    if (rtPipeline_.get() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Cannot create SBT — ray tracing pipeline does not exist");
        return;
    }

    const auto& rtProps = StoneKey::stone_rtprops();
    const VkDeviceSize handleSize  = rtProps.shaderGroupHandleSize;
    const VkDeviceSize handleAlign = rtProps.shaderGroupHandleAlignment ? rtProps.shaderGroupHandleAlignment : 32u;
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

    static uint64_t SBT_STONE_HANDLE = 0;

    if (SBT_STONE_HANDLE == 0) {
        LOG_INFO_CAT("PIPELINE", "Creating 2048 MiB SBT buffer");

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
            LOG_FATAL_CAT("PIPELINE", "No device-local memory for 2048 MiB SBT buffer");
            throw std::runtime_error("SBT buffer memory allocation failed");
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

        LOG_SUCCESS_CAT("PIPELINE", "2048 MiB SBT buffer created");
    }

    const auto* stone = BufferManager::get(SBT_STONE_HANDLE);
    if (!stone || stone->buffer == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "SBT buffer not found");
        throw std::runtime_error("SBT buffer invalid");
    }

    static std::atomic<VkDeviceSize> sbtAllocator{0};
    VkDeviceSize myOffset = sbtAllocator.fetch_add(requiredSize, std::memory_order_relaxed);

    if (myOffset + requiredSize > stone->size) {
        LOG_FATAL_CAT("PIPELINE", "SBT allocation overflow — needed {} bytes, only {} left", requiredSize, stone->size - myOffset);
        throw std::runtime_error("SBT allocation overflow");
    }

    VkBufferDeviceAddressInfo addrInfo{
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = stone->buffer
    };
    const VkDeviceAddress sbtBaseAddr = g_ext.vkGetBufferDeviceAddress(stone_device(), &addrInfo) + myOffset;

    std::vector<std::byte> handleStorage(totalGroups * handleSize);

    VkResult result = g_ext.vkGetRayTracingShaderGroupHandlesKHR(
        stone_device(),
        rtPipeline_.get(),
        0,
        totalGroups,
        handleStorage.size(),
        handleStorage.data()
    );

    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "vkGetRayTracingShaderGroupHandlesKHR failed: {}", string_VkResult(result));
        throw std::runtime_error("Shader group handles retrieval failed");
    }

    void* stagingMapped = BufferManager::stagingPtr();
    if (!stagingMapped) {
        LOG_FATAL_CAT("PIPELINE", "Global staging buffer not mapped");
        throw std::runtime_error("Staging buffer not mapped");
    }

    std::memcpy(stagingMapped, handleStorage.data(), handleStorage.size());
    BufferManager::advanceStagingOffset(handleStorage.size());

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

    LOG_SUCCESS_CAT("PIPELINE", "Shader binding table created");
}

void PipelineManager::setSBT(VkBuffer buffer, VkDeviceMemory memory, VkDeviceAddress address, VkDeviceSize size) noexcept
{
    sbtBuffer_ = Handle<VkBuffer>(buffer, stone_device(), [](VkDevice d, VkBuffer b, auto*) { vkDestroyBuffer(d, b, nullptr); });
    sbtMemory_ = Handle<VkDeviceMemory>(memory, stone_device(), [](VkDevice d, VkDeviceMemory m, auto*) { vkFreeMemory(d, m, nullptr); });
    sbtAddress_ = address;
    sbtSize_ = size;
}

void PipelineManager::loadRayTracingExtensions() noexcept
{
    LOG_INFO_CAT("PIPELINE", "Loading ray tracing extension functions");

    g_ext.vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(stone_device(), "vkCmdTraceRaysKHR"));

    g_ext.vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(stone_device(), "vkCreateRayTracingPipelinesKHR"));

    g_ext.vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(stone_device(), "vkGetRayTracingShaderGroupHandlesKHR"));

    g_ext.vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(stone_device(), "vkCreateAccelerationStructureKHR"));

    g_ext.vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(stone_device(), "vkDestroyAccelerationStructureKHR"));

    g_ext.vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(stone_device(), "vkGetAccelerationStructureBuildSizesKHR"));

    g_ext.vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(stone_device(), "vkCmdBuildAccelerationStructuresKHR"));

    g_ext.vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(stone_device(), "vkGetAccelerationStructureDeviceAddressKHR"));

    g_ext.vkGetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(
        vkGetDeviceProcAddr(stone_device(), "vkGetBufferDeviceAddress"));

    bool success = g_ext.vkCmdTraceRaysKHR &&
                   g_ext.vkCreateRayTracingPipelinesKHR &&
                   g_ext.vkGetRayTracingShaderGroupHandlesKHR &&
                   g_ext.vkCreateAccelerationStructureKHR &&
                   g_ext.vkDestroyAccelerationStructureKHR &&
                   g_ext.vkGetAccelerationStructureBuildSizesKHR &&
                   g_ext.vkCmdBuildAccelerationStructuresKHR &&
                   g_ext.vkGetAccelerationStructureDeviceAddressKHR &&
                   g_ext.vkGetBufferDeviceAddress;

    if (success) {
        LOG_SUCCESS_CAT("PIPELINE", "All ray tracing extensions loaded");
    } else {
        LOG_FATAL_CAT("PIPELINE", "Critical ray tracing extensions missing");
    }
}

void PipelineManager::createPipelineLayout()
{
    if (rtDescriptorSetLayout_.valid()) {
        return;
    }

    LOG_INFO_CAT("PIPELINE", "Creating pipeline layout — 4 sets");

    // Set 0: Main ray tracing bindings
    std::vector<VkDescriptorSetLayoutBinding> mainBindings;
    mainBindings.reserve(RT_PIPELINE_BINDINGS.size());

    for (const auto& b : RT_PIPELINE_BINDINGS) {
        mainBindings.push_back({
            .binding            = b.binding,
            .descriptorType     = b.type,
            .descriptorCount    = b.count,
            .stageFlags         = b.stage,
            .pImmutableSamplers = nullptr
        });
    }

    std::ranges::sort(mainBindings, [](const auto& a, const auto& b) { return a.binding < b.binding; });

    VkDescriptorSetLayoutCreateInfo mainInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(mainBindings.size()),
        .pBindings    = mainBindings.data()
    };

    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(stone_device(), &mainInfo, nullptr, &mainLayout));

    rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(
        mainLayout, stone_device(),
        [](VkDevice d, VkDescriptorSetLayout l, auto*) { vkDestroyDescriptorSetLayout(d, l, nullptr); },
        0, "RT_MAIN_SET_LAYOUT"
    );

    // Set 2: Large texture array
    VkDescriptorSetLayoutBinding texBinding{
        .binding            = 0,
        .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount    = 1024,
        .stageFlags         = VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .pImmutableSamplers = nullptr
    };

    VkDescriptorSetLayoutCreateInfo texInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &texBinding
    };

    VkDescriptorSetLayout texLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(stone_device(), &texInfo, nullptr, &texLayout));

    texDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(
        texLayout, stone_device(),
        [](VkDevice d, VkDescriptorSetLayout l, auto*) { vkDestroyDescriptorSetLayout(d, l, nullptr); },
        0, "TEXTURE_ARRAY_SET_LAYOUT"
    );

    // Empty layout for set 1 and 3
    VkDescriptorSetLayoutCreateInfo emptyInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 0,
        .pBindings    = nullptr
    };

    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(stone_device(), &emptyInfo, nullptr, &emptyLayout));

    emptyDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(
        emptyLayout, stone_device(),
        [](VkDevice d, VkDescriptorSetLayout l, auto*) { vkDestroyDescriptorSetLayout(d, l, nullptr); },
        0, "EMPTY_SET_LAYOUT"
    );

    // Pipeline layout
    VkDescriptorSetLayout layouts[4] = {
        rtDescriptorSetLayout_.get(),     // set 0
        emptyDescriptorSetLayout_.get(),  // set 1
        texDescriptorSetLayout_.get(),    // set 2
        emptyDescriptorSetLayout_.get()   // set 3
    };

    VkPushConstantRange push{
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR |
                      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                      VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        .offset = 0,
        .size   = 32
    };

    VkPipelineLayoutCreateInfo pipelineInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 4,
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push
    };

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(stone_device(), &pipelineInfo, nullptr, &pipelineLayout));

    rtPipelineLayout_ = Handle<VkPipelineLayout>(
        pipelineLayout, stone_device(),
        [](VkDevice d, VkPipelineLayout l, auto*) { vkDestroyPipelineLayout(d, l, nullptr); },
        0, "RT_PIPELINE_LAYOUT_ETERNAL"
    );

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created");
}

VkAccelerationStructureKHR PipelineManager::createDummyTLAS()
{
    VkAccelerationStructureGeometryKHR geometry{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry     = { .instances = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .data  = { .deviceAddress = 0 }
        }}
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    const uint32_t primitiveCount = 1;
    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo);

    uint64_t bufferHandle = BufferManager::create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "DummyTLAS_Buffer");

    if (bufferHandle == 0) {
        LOG_ERROR_CAT("PIPELINE", "Failed to create buffer for dummy TLAS");
        return VK_NULL_HANDLE;
    }

    const auto* bufInfo = BufferManager::get(bufferHandle);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = bufInfo->buffer,
        .size   = sizeInfo.accelerationStructureSize,
        .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

    VkAccelerationStructureKHR as = VK_NULL_HANDLE;
    VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &as));

    dummyAccelBuffer_ = Handle<VkBuffer>(bufInfo->buffer, stone_device(), [](auto...) {});
    dummyAccelMemory_ = Handle<VkDeviceMemory>(bufInfo->memory, stone_device(), [](auto...) {});

    return as;
}

void PipelineManager::createRayTracingPipeline()
{
    auto load = [this](const char* path) -> VkShaderModule {
        VkShaderModule mod = loadShader(path);
        if (!mod) {
            LOG_FATAL_CAT("PIPELINE", "Shader missing: {}", path);
            throw std::runtime_error("Shader load failure");
        }
        return mod;
    };

    // Load only the 3 essential shaders
    VkShaderModule raygen = load("assets/shaders/raytracing/raygen.spv");
    VkShaderModule miss   = load("assets/shaders/raytracing/miss.spv");
    VkShaderModule hit    = load("assets/shaders/raytracing/closest_hit.spv");

    // Optional performance preset: disable hit shaders for uncapped mode
    if (Options::CURRENT_PRESET == Options::Preset::UncappedPerformance) {
        if (hit) {
            vkDestroyShaderModule(stone_device(), hit, nullptr);
            hit = VK_NULL_HANDLE;
        }
    }

    shaderModules_.clear();
    shaderModules_.emplace_back(raygen, stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(miss,   stone_device(), vkDestroyShaderModule);
    if (hit) shaderModules_.emplace_back(hit, stone_device(), vkDestroyShaderModule);

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    stages.reserve(3);
    groups.reserve(3);

    uint32_t stageIndex = 0;

    auto general = [&](VkShaderModule mod, VkShaderStageFlagBits stage) {
        stages.push_back({ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                          .stage = stage, .module = mod, .pName = "main" });
        groups.push_back({ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                          .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                          .generalShader = stageIndex++ });
    };

    auto hitGroup = [&]() {
        uint32_t closestIndex = VK_SHADER_UNUSED_KHR;
        if (hit) {
            stages.push_back({ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                              .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                              .module = hit, .pName = "main" });
            closestIndex = stageIndex++;
        }

        groups.push_back({ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                          .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                          .closestHitShader = closestIndex,
                          .anyHitShader = VK_SHADER_UNUSED_KHR });
    };

    // 3 shaders → 3 groups
    general(raygen, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    general(miss,   VK_SHADER_STAGE_MISS_BIT_KHR);
    hitGroup();

    raygenGroupCount_ = 1;
    missGroupCount_   = 1;
    hitGroupCount_    = hit ? 1 : 0;  // 0 if disabled in uncapped mode

    if (rtPipelineLayout_.get() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Pipeline layout is null");
        throw std::runtime_error("Pipeline layout failure");
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
    VK_CHECK(g_ext.vkCreateRayTracingPipelinesKHR(
        stone_device(), VK_NULL_HANDLE, VK_NULL_HANDLE,
        1, &createInfo, nullptr, &pipeline));

    rtPipeline_ = Handle<VkPipeline>(pipeline, stone_device(),
        [](VkDevice d, VkPipeline p, auto*) { vkDestroyPipeline(d, p, nullptr); });

    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing pipeline created — reduced to 3 shaders (raygen, miss, closest hit)");
}

void PipelineManager::forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer mainCmd)
{
    if (s_crownForged) {
        LOG_INFO_CAT("PIPELINE", "Pipeline already created — skipping");
        return;
    }

    loadRayTracingExtensions();
    createDescriptorPool();
    createPipelineLayout();
    allocateDescriptorSets();
    createRayTracingPipeline();
    createShaderBindingTable(commandPool, graphicsQueue, mainCmd);

    stone_seal_pipeline(this);
    s_crownForged = true;

    LOG_SUCCESS_CAT("PIPELINE", "RTX pipeline forged");
}

void PipelineManager::cacheDeviceProperties()
{
    const VkPhysicalDevice phys = stone_physical();
    if (phys == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "No physical device available");
        throw std::runtime_error("No physical device");
    }

    VkPhysicalDeviceProperties baseProps{};
    vkGetPhysicalDeviceProperties(phys, &baseProps);

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
        .pNext = &rtProps
    };

    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &asProps
    };

    vkGetPhysicalDeviceProperties2(phys, &props2);

    baseProps = props2.properties;

    if (rtProps.shaderGroupHandleSize == 0) {
        LOG_FATAL_CAT("PIPELINE", "Device {} lacks ray tracing support (handleSize=0)", baseProps.deviceName);
        throw std::runtime_error("No ray tracing support");
    }

    auto& ctx = RTX::g_ctx();
    ctx.physicalDeviceProperties_ = baseProps;
    ctx.rayTracingProps_          = rtProps;

    LOG_SUCCESS_CAT("PIPELINE", "Device properties cached — GPU: {}, RT Handle Size: {}", baseProps.deviceName, rtProps.shaderGroupHandleSize);
}

void PipelineManager::traceRays(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t width, uint32_t height, uint32_t depth)
{
    if (g_pipelineNeedsRebuild.load()) {
        createRayTracingPipeline();
        g_pipelineNeedsRebuild.store(false);
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());

    VkDescriptorSet descSets[4] = {
        rtDescriptorSets_[frameIndex],     // set 0: TLAS, UBOs, images, StoneKey@31
        VK_NULL_HANDLE,                    // set 1: empty
        texDescriptorSets_[frameIndex],    // set 2: texSamplers[]
        VK_NULL_HANDLE                     // set 3: empty
    };

    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        rtPipelineLayout_.get(),
        0, 4, descSets,
        0, nullptr
    );

    g_ext.vkCmdTraceRaysKHR(
        commandBuffer,
        &raygenSbtRegion_,
        &missSbtRegion_,
        &hitSbtRegion_,
        &callableSbtRegion_,
        width, height, depth
    );
}

VkDescriptorSet PipelineManager::getDescriptorSet(uint32_t frameIndex) const
{
    return rtDescriptorSets_[frameIndex];
}

VkPipeline PipelineManager::getPipeline() const
{
    return rtPipeline_.get();
}

VkPipelineLayout PipelineManager::getPipelineLayout() const
{
    return rtPipelineLayout_.get();
}

} // namespace RTX

// =============================================================================
// 100% COMPATIBLE WITH CURRENT VulkanRenderer — ALL FIXES APPLIED
// NO EXCEPTIONS THROWN IN NON-FATAL PATHS
// PINK PHOTONS ETERNAL — EMPIRE SOLID
// =============================================================================