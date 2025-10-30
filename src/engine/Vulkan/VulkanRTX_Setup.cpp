// AMOURANTH RTX Engine © 2025 Zachary Geurts gzac5314@gmail.com | CC BY-NC 4.0
// Vulkan RTX: SBT, AS, pipelines, descriptors, black fallback.
// Vulkan 1.3+ | KHR_ray_tracing_pipeline | AMD/NVIDIA/Intel | Linux/Windows

#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <vector>
#include <array>
#include <thread>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <dlfcn.h>
#include "engine/logging.hpp"
#include <map>
#include <format>
#include <utility>
#include <ranges>

#define VK_CHECK(result, msg) do { \
    auto r = (result); \
    if (r != VK_SUCCESS) { \
        LOG_ERROR_CAT("VulkanRTX", "{} (VkResult: {})", (msg), static_cast<int>(r)); \
        throw VulkanRTXException(std::format("{}: VkResult={}", (msg), static_cast<int>(r))); \
    } \
} while(0)

// RAII ScopeGuard
template<class F>
struct ScopeGuard { F f; ~ScopeGuard() { f(); } };
template<class F>
ScopeGuard<F> finally(F&& f) { return {std::forward<F>(f)}; }

namespace VulkanRTX {

VulkanRTX::VulkanRTX(VkDevice device, VkPhysicalDevice physicalDevice, const std::vector<std::string>& shaderPaths)
    : device_(device),
      physicalDevice_(physicalDevice),
      shaderPaths_(shaderPaths),
      dsLayout_(),
      dsPool_(),
      ds_(),
      rtPipelineLayout_(),
      rtPipeline_(),
      blasBuffer_(),
      blasMemory_(),
      tlasBuffer_(),
      tlasMemory_(),
      blas_(),
      tlas_(),
      extent_{},
      primitiveCounts_(),
      previousPrimitiveCounts_(),
      previousDimensionCache_(),
      supportsCompaction_(false),
      shaderFeatures_(ShaderFeatures::None),
      numShaderGroups_(0),
      counts_(),
      sbt_(),
      scratchAlignment_(0),
      blackFallbackImage_(),
      blackFallbackMemory_(),
      blackFallbackView_() {

    if (!device_ || !physicalDevice_) {
        LOG_ERROR_CAT("VulkanRTX", "Invalid device or physical device");
        throw VulkanRTXException("Invalid device or physical device");
    }

    for (const auto& path : shaderPaths_) {
        auto absPath = std::filesystem::absolute(path);
        if (!std::filesystem::exists(absPath))
            throw VulkanRTXException(std::format("Shader not found: {}", absPath.string()));
        if (!std::filesystem::is_regular_file(absPath))
            throw VulkanRTXException(std::format("Not a file: {}", absPath.string()));
        if ((std::filesystem::status(absPath).permissions() & std::filesystem::perms::owner_read) == std::filesystem::perms::none)
            throw VulkanRTXException(std::format("No read permission: {}", absPath.string()));
    }

    for (const auto& path : shaderPaths_) {
        auto ext = std::filesystem::path(path).extension().string();
        if (ext == ".rgen") { shaderFeatures_ |= ShaderFeatures::Raygen; counts_.raygen++; }
        else if (ext == ".rmiss") { shaderFeatures_ |= ShaderFeatures::Miss; counts_.miss++; }
        else if (ext == ".rchit") { shaderFeatures_ |= ShaderFeatures::ClosestHit; counts_.chit++; }
        else if (ext == ".rahit") { shaderFeatures_ |= ShaderFeatures::AnyHit; counts_.ahit++; }
        else if (ext == ".rint") { shaderFeatures_ |= ShaderFeatures::Intersection; counts_.intersection++; }
        else if (ext == ".rcall") { shaderFeatures_ |= ShaderFeatures::Callable; counts_.callable++; }
    }

    std::map<std::string, int> priority = {{".rgen",0}, {".rmiss",1}, {".rcall",2}, {".rchit",3}, {".rahit",4}, {".rint",5}};
    std::ranges::sort(shaderPaths_, [&](const auto& a, const auto& b) {
        int pa = priority.contains(std::filesystem::path(a).extension().string()) ? priority[std::filesystem::path(a).extension().string()] : 6;
        int pb = priority.contains(std::filesystem::path(b).extension().string()) ? priority[std::filesystem::path(b).extension().string()] : 6;
        return pa < pb || (pa == pb && a < b);
    });

    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 props2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &asProps};
    vkGetPhysicalDeviceProperties2(physicalDevice_, &props2);
    supportsCompaction_ = asProps.maxGeometryCount > 0;
    scratchAlignment_ = asProps.minAccelerationStructureScratchOffsetAlignment;

    createBlackFallbackTexture();
}

// DESTRUCTOR — **UPDATED WITH SBT CLEANUP**
VulkanRTX::~VulkanRTX() {
    // SBT Cleanup
    if (sbt_.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, sbt_.buffer, nullptr);
        sbt_.buffer = VK_NULL_HANDLE;
    }
    if (sbt_.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, sbt_.memory, nullptr);
        sbt_.memory = VK_NULL_HANDLE;
    }

    if (rtPipeline_.get() != VK_NULL_HANDLE) vkDestroyPipeline(device_, rtPipeline_.get(), nullptr);
    if (rtPipelineLayout_.get() != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, rtPipelineLayout_.get(), nullptr);
    if (dsPool_.get() != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, dsPool_.get(), nullptr);
    if (dsLayout_.get() != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, dsLayout_.get(), nullptr);
    if (blas_.get() != VK_NULL_HANDLE) vkDestroyAccelerationStructureKHR(device_, blas_.get(), nullptr);
    if (tlas_.get() != VK_NULL_HANDLE) vkDestroyAccelerationStructureKHR(device_, tlas_.get(), nullptr);
}

void VulkanRTX::uploadBlackPixelToImage(VkImage image) {
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> stagingBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> stagingMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice_, 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    void* data; VK_CHECK(vkMapMemory(device_, stagingMemory.get(), 0, 4, 0, &data), "Map staging");
    *static_cast<uint32_t*>(data) = 0xFF000000;
    vkUnmapMemory(device_, stagingMemory.get());

    auto cmd = allocateTransientCommandBuffer(VK_NULL_HANDLE);
    VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin), "Begin cmd");

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region = {
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {1, 1, 1}
    };
    vkCmdCopyBufferToImage(cmd, stagingBuffer.get(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VK_CHECK(vkEndCommandBuffer(cmd), "End cmd");
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd};
    VK_CHECK(vkQueueSubmit(VK_NULL_HANDLE, 1, &submit, VK_NULL_HANDLE), "Submit");
    VK_CHECK(vkQueueWaitIdle(VK_NULL_HANDLE), "Wait");
    vkFreeCommandBuffers(device_, VK_NULL_HANDLE, 1, &cmd);
}

void VulkanRTX::createBlackFallbackTexture() {
    VkImageCreateInfo imgInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {1, 1, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VkImage img; VK_CHECK(vkCreateImage(device_, &imgInfo, nullptr, &img), "Create fallback image");
    blackFallbackImage_ = {device_, img, vkDestroyImage};

    VkMemoryRequirements reqs; vkGetImageMemoryRequirements(device_, img, &reqs);
    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = reqs.size,
        .memoryTypeIndex = findMemoryType(physicalDevice_, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VkDeviceMemory mem; VK_CHECK(vkAllocateMemory(device_, &alloc, nullptr, &mem), "Alloc fallback mem");
    VK_CHECK(vkBindImageMemory(device_, img, mem, 0), "Bind fallback");
    blackFallbackMemory_ = {device_, mem, vkFreeMemory};

    uploadBlackPixelToImage(img);

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VkImageView view; VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &view), "Create fallback view");
    blackFallbackView_ = {device_, view, vkDestroyImageView};
}

VkShaderModule VulkanRTX::createShaderModule(const std::string& filename) {
    auto path = std::filesystem::absolute(filename);
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) throw VulkanRTXException(std::format("Failed to open: {}", path.string()));

    auto size = static_cast<size_t>(file.tellg()); file.seekg(0);
    std::vector<char> buffer(size);
    file.read(buffer.data(), size); file.close();

    if (size % 4 != 0 || *reinterpret_cast<const uint32_t*>(buffer.data()) != 0x07230203)
        throw VulkanRTXException(std::format("Invalid SPIR-V: {}", path.string()));

    VkShaderModuleCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };
    VkShaderModule mod; VK_CHECK(vkCreateShaderModule(device_, &info, nullptr, &mod), "Create shader");
    return mod;
}

void VulkanRTX::loadShadersAsync(std::vector<VkShaderModule>& modules, const std::vector<std::string>& paths) {
    modules.resize(paths.size());
    std::vector<std::jthread> threads;
    for (size_t i = 0; i < paths.size(); ++i)
        threads.emplace_back([this, &modules, i, &paths] { modules[i] = createShaderModule(paths[i]); });
}

void VulkanRTX::buildShaderGroups(std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups) {
    groups.clear(); numShaderGroups_ = 0;
    uint32_t base = 0;
    auto add = [&](auto type, auto count, auto idx) {
        for (uint32_t i = 0; i < count; ++i) {
            groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                              .type = type, .generalShader = idx + i,
                              .closestHitShader = VK_SHADER_UNUSED_KHR,
                              .anyHitShader = VK_SHADER_UNUSED_KHR,
                              .intersectionShader = VK_SHADER_UNUSED_KHR});
            ++numShaderGroups_;
        }
    };

    add(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, counts_.raygen, base); base += counts_.raygen;
    add(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, counts_.miss, base); base += counts_.miss;
    add(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, counts_.callable, base); base += counts_.callable;

    for (uint32_t i = 0; i < counts_.chit; ++i) {
        auto ahit = counts_.ahit ? base + counts_.miss + counts_.callable + (i % counts_.ahit) : VK_SHADER_UNUSED_KHR;
        groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                          .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                          .closestHitShader = base + i, .anyHitShader = ahit});
        ++numShaderGroups_;
    }
    base += counts_.chit;

    if (counts_.intersection) {
        for (uint32_t i = 0; i < counts_.chit; ++i) {
            auto ahit = counts_.ahit ? base + (i % counts_.ahit) : VK_SHADER_UNUSED_KHR;
            auto intIdx = base + counts_.chit + (i % counts_.intersection);
            groups.push_back({.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                              .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
                              .closestHitShader = base - counts_.chit + i, .anyHitShader = ahit,
                              .intersectionShader = intIdx});
            ++numShaderGroups_;
        }
    }
}

void VulkanRTX::createDescriptorSetLayout() {
    const auto stages = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                        VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR |
                        VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;

    std::array bindings = {
        VkDescriptorSetLayoutBinding{static_cast<uint32_t>(DescriptorBindings::TLAS), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, stages},
        VkDescriptorSetLayoutBinding{static_cast<uint32_t>(DescriptorBindings::StorageImage), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, stages},
        VkDescriptorSetLayoutBinding{static_cast<uint32_t>(DescriptorBindings::CameraUBO), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, stages},
        VkDescriptorSetLayoutBinding{static_cast<uint32_t>(DescriptorBindings::MaterialSSBO), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 26, stages},
        VkDescriptorSetLayoutBinding{static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, stages},
        VkDescriptorSetLayoutBinding{static_cast<uint32_t>(DescriptorBindings::DenoiseImage), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, stages},
        VkDescriptorSetLayoutBinding{static_cast<uint32_t>(DescriptorBindings::EnvMap), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, stages},
        VkDescriptorSetLayoutBinding{static_cast<uint32_t>(DescriptorBindings::DensityVolume), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, stages},
        VkDescriptorSetLayoutBinding{static_cast<uint32_t>(DescriptorBindings::GDepth), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, stages},
        VkDescriptorSetLayoutBinding{static_cast<uint32_t>(DescriptorBindings::GNormal), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, stages}
    };

    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };
    VkDescriptorSetLayout layout; VK_CHECK(vkCreateDescriptorSetLayout(device_, &info, nullptr, &layout), "Create DS layout");
    setDescriptorSetLayout(layout);
}

void VulkanRTX::createDescriptorPoolAndSet() {
    std::array poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 27},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2}
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };
    VkDescriptorPool pool; VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool), "Create pool");
    setDescriptorPool(pool);

    VkDescriptorSetLayout layout = dsLayout_.get();
    VkDescriptorSetAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = dsPool_.get(),
        .descriptorSetCount = 1,
        .pSetLayouts = &layout
    };
    VkDescriptorSet ds; VK_CHECK(vkAllocateDescriptorSets(device_, &alloc, &ds), "Alloc DS");
    setDescriptorSet(ds);
}

void VulkanRTX::createRayTracingPipeline(uint32_t maxRayRecursionDepth) {
    std::vector<VkShaderModule> modules;
    loadShadersAsync(modules, shaderPaths_);
    auto cleanup = finally([&]{ for (auto m : modules) vkDestroyShaderModule(device_, m, nullptr); });

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    for (size_t i = 0; i < shaderPaths_.size(); ++i) {
        auto ext = std::filesystem::path(shaderPaths_[i]).extension().string();
        VkShaderStageFlagBits stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        if (ext == ".rmiss") stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        else if (ext == ".rchit") stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        else if (ext == ".rahit") stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        else if (ext == ".rint") stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        else if (ext == ".rcall") stage = VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        else continue;

        stages.push_back({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = stage,
            .module = modules[i],
            .pName = "main"
        });
    }

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    buildShaderGroups(groups);

    VkPushConstantRange pcRange = {
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                      VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        .size = sizeof(MaterialData::PushConstants)
    };

    VkDescriptorSetLayout layout = dsLayout_.get();
    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pcRange
    };
    VkPipelineLayout pipelineLayout; VK_CHECK(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout), "Create pipeline layout");
    setPipelineLayout(pipelineLayout);

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 props2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &rtProps};
    vkGetPhysicalDeviceProperties2(physicalDevice_, &props2);

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = std::min(maxRayRecursionDepth, rtProps.maxRayRecursionDepth),
        .layout = rtPipelineLayout_.get()
    };
    VkPipeline pipeline; VK_CHECK(vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, nullptr, 1, &pipelineInfo, nullptr, &pipeline), "Create RT pipeline");
    setPipeline(pipeline);
}

void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 props = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &rtProps};
    vkGetPhysicalDeviceProperties2(physicalDevice, &props);

    auto handleSize = rtProps.shaderGroupHandleSize;
    auto handleAlign = rtProps.shaderGroupHandleAlignment;
    auto alignedSize = (handleSize + handleAlign - 1) & ~(handleAlign - 1);
    auto sbtSize = static_cast<VkDeviceSize>(alignedSize) * numShaderGroups_;

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> buffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> memory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffer, memory);

    std::vector<uint8_t> handles(sbtSize);
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device_, rtPipeline_.get(), 0, numShaderGroups_, sbtSize, handles.data()), "Get SBT handles");

    void* data; VK_CHECK(vkMapMemory(device_, memory.get(), 0, sbtSize, 0, &data), "Map SBT");
    for (uint32_t i = 0; i < numShaderGroups_; ++i)
        memcpy(static_cast<uint8_t*>(data) + i * alignedSize, handles.data() + i * handleSize, handleSize);
    vkUnmapMemory(device_, memory.get());

    VkBufferDeviceAddressInfo addrInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer.get()};
    auto addr = vkGetBufferDeviceAddress(device_, &addrInfo);

    auto region = [&](uint32_t count, uint64_t offset) {
        return count ? VkStridedDeviceAddressRegionKHR{addr + offset, static_cast<VkDeviceSize>(count) * alignedSize, alignedSize} : VkStridedDeviceAddressRegionKHR{};
    };

    uint64_t offset = 0;
    sbt_.raygen = region(counts_.raygen, offset); offset += counts_.raygen * alignedSize;
    sbt_.miss = region(counts_.miss, offset); offset += counts_.miss * alignedSize;
    sbt_.hit = region(counts_.chit + (counts_.intersection ? counts_.chit : 0), offset); offset += (counts_.chit + (counts_.intersection ? counts_.chit : 0)) * alignedSize;
    sbt_.callable = region(counts_.callable, offset);

    // FIXED: Extract raw handles with .get() instead of moving the wrappers
    sbt_.buffer = buffer.get();
    sbt_.memory = memory.get();
}

void VulkanRTX::createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries) {
    std::vector<VkAccelerationStructureGeometryKHR> asGeometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges;
    for (const auto& [vertexBuffer, indexBuffer, vertexCount, indexCount, instanceCustomIndex] : geometries) {
        VkAccelerationStructureGeometryKHR geometry = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {
                .triangles = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                    .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                    .vertexData = { .deviceAddress = getBufferDeviceAddress(vertexBuffer) },
                    .vertexStride = sizeof(glm::vec3) * 3 + sizeof(glm::vec2),
                    .maxVertex = vertexCount - 1,
                    .indexType = VK_INDEX_TYPE_UINT32,
                    .indexData = { .deviceAddress = getBufferDeviceAddress(indexBuffer) }
                }
            },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };
        asGeometries.push_back(geometry);
        buildRanges.push_back({
            .primitiveCount = indexCount / 3,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        });
    }
    setPrimitiveCounts(buildRanges);

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = static_cast<uint32_t>(asGeometries.size()),
        .pGeometries = asGeometries.data()
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    std::vector<uint32_t> primCounts(buildRanges.size());
    for (size_t i = 0; i < buildRanges.size(); ++i) primCounts[i] = buildRanges[i].primitiveCount;
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, primCounts.data(), &sizeInfo);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> blasBufferTemp(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> blasMemoryTemp(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBufferTemp, blasMemoryTemp);

    VkAccelerationStructureCreateInfoKHR asCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blasBufferTemp.get(),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    VkAccelerationStructureKHR tempAS; VK_CHECK(vkCreateAccelerationStructureKHR(device_, &asCreateInfo, nullptr, &tempAS), "Create BLAS");
    setBLAS(tempAS);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> scratchBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> scratchMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    auto scratchSize = (sizeInfo.buildScratchSize + scratchAlignment_ - 1) & ~(scratchAlignment_ - 1);
    createBuffer(physicalDevice, scratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    buildInfo.dstAccelerationStructure = blas_.get();
    buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(scratchBuffer.get());

    auto cmdBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Begin BLAS cmd");
    const VkAccelerationStructureBuildRangeInfoKHR* rangePtr = buildRanges.data();
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &rangePtr);
    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "End BLAS cmd");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);

    blasBuffer_ = std::move(blasBufferTemp);
    blasMemory_ = std::move(blasMemoryTemp);
}

void VulkanRTX::createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                 const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances) {
    std::vector<VkAccelerationStructureInstanceKHR> asInstances;
    for (const auto& [as, transform] : instances) {
        VkTransformMatrixKHR vkTransform;
        glm::mat4 transposed = glm::transpose(transform);
        memcpy(&vkTransform.matrix, glm::value_ptr(transposed), sizeof(VkTransformMatrixKHR));
        asInstances.push_back({
            .transform = vkTransform,
            .instanceCustomIndex = 0,
            .mask = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = getAccelerationStructureDeviceAddress(as)
        });
    }

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> instanceBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> instanceMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeof(VkAccelerationStructureInstanceKHR) * asInstances.size(),
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instanceBuffer, instanceMemory);

    void* mappedData; VK_CHECK(vkMapMemory(device_, instanceMemory.get(), 0, sizeof(VkAccelerationStructureInstanceKHR) * asInstances.size(), 0, &mappedData), "Map instance");
    memcpy(mappedData, asInstances.data(), sizeof(VkAccelerationStructureInstanceKHR) * asInstances.size());
    vkUnmapMemory(device_, instanceMemory.get());

    VkAccelerationStructureGeometryKHR geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {
            .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .data = { .deviceAddress = getBufferDeviceAddress(instanceBuffer.get()) }
            }
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildRangeInfoKHR buildRange = {
        .primitiveCount = static_cast<uint32_t>(asInstances.size())
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &buildRange.primitiveCount, &sizeInfo);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> tlasBufferTemp(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> tlasMemoryTemp(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBufferTemp, tlasMemoryTemp);

    VkAccelerationStructureCreateInfoKHR asCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlasBufferTemp.get(),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    VkAccelerationStructureKHR tempAS; VK_CHECK(vkCreateAccelerationStructureKHR(device_, &asCreateInfo, nullptr, &tempAS), "Create TLAS");
    setTLAS(tempAS);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> scratchBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> scratchMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    auto scratchSize = (sizeInfo.buildScratchSize + scratchAlignment_ - 1) & ~(scratchAlignment_ - 1);
    createBuffer(physicalDevice, scratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    buildInfo.dstAccelerationStructure = tlas_.get();
    buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(scratchBuffer.get());

    auto cmdBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Begin TLAS cmd");
    const VkAccelerationStructureBuildRangeInfoKHR* rangePtr = &buildRange;
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &rangePtr);
    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "End TLAS cmd");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);

    tlasBuffer_ = std::move(tlasBufferTemp);
    tlasMemory_ = std::move(tlasMemoryTemp);
}

void VulkanRTX::createStorageImage(VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
                                   VulkanResource<VkImage, PFN_vkDestroyImage>& image,
                                   VulkanResource<VkImageView, PFN_vkDestroyImageView>& imageView,
                                   VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) {
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {extent.width, extent.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VkImage tempImage; VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &tempImage), "Create storage image");
    image = {device_, tempImage, vkDestroyImage};

    VkMemoryRequirements memReqs; vkGetImageMemoryRequirements(device_, tempImage, &memReqs);
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VkDeviceMemory tempMemory; VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &tempMemory), "Alloc image mem");
    memory = {device_, tempMemory, vkFreeMemory};
    VK_CHECK(vkBindImageMemory(device_, tempImage, tempMemory, 0), "Bind image");

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = tempImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VkImageView tempView; VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &tempView), "Create image view");
    imageView = {device_, tempView, vkDestroyImageView};
}

void VulkanRTX::updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer,
                                VkImageView storageImageView, VkImageView denoiseImageView, VkImageView envMapView, VkSampler envMapSampler,
                                VkImageView densityVolumeView, VkImageView gDepthView, VkImageView gNormalView) {
    constexpr uint32_t MAT_COUNT = 26;
    VkDescriptorBufferInfo camInfo = {.buffer = cameraBuffer, .range = VK_WHOLE_SIZE};
    std::vector<VkDescriptorBufferInfo> matInfos(MAT_COUNT);
    for (uint32_t i = 0; i < MAT_COUNT; ++i)
        matInfos[i] = {.buffer = materialBuffer, .offset = i * sizeof(MaterialData), .range = sizeof(MaterialData)};
    VkDescriptorBufferInfo dimInfo = {.buffer = dimensionBuffer, .range = sizeof(DimensionState)};

    VkDescriptorImageInfo storageInfo = {.imageView = storageImageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo denoiseInfo = {.imageView = denoiseImageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo envInfo = {.sampler = envMapSampler, .imageView = envMapView ? envMapView : blackFallbackView_.get(), .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo densityInfo = {.sampler = envMapSampler, .imageView = densityVolumeView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo gDepthInfo = {.imageView = gDepthView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo gNormalInfo = {.imageView = gNormalView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

    VkAccelerationStructureKHR tlasHandle = tlas_.get();
    VkWriteDescriptorSetAccelerationStructureKHR asInfo = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlasHandle
    };

    std::array writes = {
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &asInfo, .dstSet = ds_.get(), .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS), .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR},
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_.get(), .dstBinding = static_cast<uint32_t>(DescriptorBindings::StorageImage), .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &storageInfo},
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_.get(), .dstBinding = static_cast<uint32_t>(DescriptorBindings::CameraUBO), .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &camInfo},
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_.get(), .dstBinding = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO), .dstArrayElement = 0, .descriptorCount = MAT_COUNT, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = matInfos.data()},
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_.get(), .dstBinding = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO), .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dimInfo},
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_.get(), .dstBinding = static_cast<uint32_t>(DescriptorBindings::DenoiseImage), .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &denoiseInfo},
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_.get(), .dstBinding = static_cast<uint32_t>(DescriptorBindings::EnvMap), .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &envInfo},
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_.get(), .dstBinding = static_cast<uint32_t>(DescriptorBindings::DensityVolume), .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &densityInfo},
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_.get(), .dstBinding = static_cast<uint32_t>(DescriptorBindings::GDepth), .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &gDepthInfo},
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_.get(), .dstBinding = static_cast<uint32_t>(DescriptorBindings::GNormal), .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &gNormalInfo}
    };

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanRTX::initializeRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                              const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                              uint32_t maxRayRecursionDepth, const std::vector<DimensionState>& dimensionCache) {
    createDescriptorSetLayout();
    createDescriptorPoolAndSet();
    createRayTracingPipeline(maxRayRecursionDepth);
    createShaderBindingTable(physicalDevice);
    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);
    createTopLevelAS(physicalDevice, commandPool, graphicsQueue, {{blas_.get(), glm::mat4(1.0f)}});
    setPreviousDimensionCache(dimensionCache);
}

void VulkanRTX::updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                          const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                          const std::vector<DimensionState>& dimensionCache) {
    if (dimensionCache != previousDimensionCache_) {
        createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);
        createTopLevelAS(physicalDevice, commandPool, graphicsQueue, {{blas_.get(), glm::mat4(1.0f)}});
        setPreviousDimensionCache(dimensionCache);
    }
}

void VulkanRTX::recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage,
                                        VkImageView, const MaterialData::PushConstants& pc) {
    VkCommandBufferBeginInfo beginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Begin RT cmd");

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = outputImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());
    VkDescriptorSet ds = ds_.get();
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_.get(), 0, 1, &ds, 0, nullptr);
    vkCmdPushConstants(cmdBuffer, rtPipelineLayout_.get(), VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 0, sizeof(pc), &pc);
    vkCmdTraceRaysKHR(cmdBuffer, &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable, extent.width, extent.height, 1);

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "End RT cmd");
}

void VulkanRTX::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
    VkAccelerationStructureKHR tlasHandle = tlas;
    VkWriteDescriptorSetAccelerationStructureKHR asInfo = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlasHandle
    };
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asInfo,
        .dstSet = ds_.get(),
        .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

void VulkanRTX::compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue) {
    if (!supportsCompaction_) return;

    VkQueryPoolCreateInfo queryInfo = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, .queryCount = 2};
    VkQueryPool pool; VK_CHECK(vkCreateQueryPool(device_, &queryInfo, nullptr, &pool), "Create query pool");
    auto poolGuard = finally([&]{ vkDestroyQueryPool(device_, pool, nullptr); });

    auto cmd = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin), "Begin compact cmd");
    vkCmdResetQueryPool(cmd, pool, 0, 2);
    std::array structures = {blas_.get(), tlas_.get()};
    vkCmdWriteAccelerationStructuresPropertiesKHR(cmd, 2, structures.data(), VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, pool, 0);
    VK_CHECK(vkEndCommandBuffer(cmd), "End compact cmd");
    submitAndWaitTransient(cmd, queue, commandPool);

    std::array<VkDeviceSize, 2> sizes;
    VK_CHECK(vkGetQueryPoolResults(device_, pool, 0, 2, sizeof(sizes), sizes.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "Get compact sizes");

    for (int i = 0; i < 2; ++i) if (sizes[i]) {
        VulkanResource<VkBuffer, PFN_vkDestroyBuffer> newBuf(device_, VK_NULL_HANDLE, vkDestroyBuffer);
        VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> newMem(device_, VK_NULL_HANDLE, vkFreeMemory);
        createBuffer(physicalDevice, sizes[i], VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, newBuf, newMem);

        VkAccelerationStructureCreateInfoKHR createInfo = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, .buffer = newBuf.get(), .size = sizes[i], .type = i ? VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR};
        VkAccelerationStructureKHR newAS; VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &newAS), "Create compacted AS");
        auto asGuard = finally([&]{ vkDestroyAccelerationStructureKHR(device_, newAS, nullptr); });

        cmd = allocateTransientCommandBuffer(commandPool);
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin), "Begin copy cmd");
        VkCopyAccelerationStructureInfoKHR copyInfo = {.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR, .src = structures[i], .dst = newAS, .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR};
        vkCmdCopyAccelerationStructureKHR(cmd, &copyInfo);
        VK_CHECK(vkEndCommandBuffer(cmd), "End copy cmd");
        submitAndWaitTransient(cmd, queue, commandPool);

        if (i == 0) { blas_ = {device_, newAS, vkDestroyAccelerationStructureKHR}; blasBuffer_ = std::move(newBuf); blasMemory_ = std::move(newMem); }
        else { tlas_ = {device_, newAS, vkDestroyAccelerationStructureKHR}; tlasBuffer_ = std::move(newBuf); tlasMemory_ = std::move(newMem); updateDescriptorSetForTLAS(tlas_.get()); }
    }
}

VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = commandPool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    VkCommandBuffer cmd; VK_CHECK(vkAllocateCommandBuffers(device_, &info, &cmd), "Alloc transient cmd");
    return cmd;
}

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer cmdBuffer, VkQueue queue, VkCommandPool commandPool) {
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmdBuffer};
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "Submit transient");
    VK_CHECK(vkQueueWaitIdle(queue), "Wait transient");
    vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
}

void VulkanRTX::createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties, VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
                             VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) {
    VkBufferCreateInfo info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage};
    VkBuffer buf; VK_CHECK(vkCreateBuffer(device_, &info, nullptr, &buf), "Create buffer");

    VkMemoryRequirements reqs; vkGetBufferMemoryRequirements(device_, buf, &reqs);
    VkMemoryAllocateFlagsInfo flags = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT};
    VkMemoryAllocateInfo alloc = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &flags, .allocationSize = reqs.size, .memoryTypeIndex = findMemoryType(physicalDevice, reqs.memoryTypeBits, properties)};
    VkDeviceMemory mem; VK_CHECK(vkAllocateMemory(device_, &alloc, nullptr, &mem), "Alloc buffer mem");
    VK_CHECK(vkBindBufferMemory(device_, buf, mem, 0), "Bind buffer");

    buffer = {device_, buf, vkDestroyBuffer};
    memory = {device_, mem, vkFreeMemory};
}

uint32_t VulkanRTX::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps; vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    throw VulkanRTXException("No suitable memory type");
}

VkDeviceAddress VulkanRTX::getBufferDeviceAddress(VkBuffer buffer) {
    VkBufferDeviceAddressInfo info = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer};
    auto addr = vkGetBufferDeviceAddress(device_, &info);
    if (!addr) throw VulkanRTXException("Invalid buffer address");
    return addr;
}

VkDeviceAddress VulkanRTX::getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as) {
    VkAccelerationStructureDeviceAddressInfoKHR info = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, .accelerationStructure = as};
    auto addr = vkGetAccelerationStructureDeviceAddressKHR(device_, &info);
    if (!addr) throw VulkanRTXException("Invalid AS address");
    return addr;
}

} // namespace VulkanRTX