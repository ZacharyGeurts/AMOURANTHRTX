// AMOURANTH RTX Engine, October 2025 - Vulkan buffer management.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, Vulkan_init.hpp, ue_init.hpp, logging.hpp, Dispose.hpp.
// Supported platforms: Linux, Windows.
// Optimized for high-end GPUs with 8 GB VRAM (e.g., NVIDIA RTX 3070, AMD RX 6800).
// Zachary Geurts 2025

#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/logging.hpp"
#include "engine/Dispose.hpp"
#include "ue_init.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string_view>
#include <algorithm> // For std::max

namespace {
// Scaling factors optimized for 8 GB VRAM
constexpr VkDeviceSize VERTEX_BUFFER_SCALE_FACTOR = 4;  // Reduced for memory efficiency
constexpr VkDeviceSize INDEX_BUFFER_SCALE_FACTOR = 4;   // Reduced for memory efficiency
constexpr VkDeviceSize SCRATCH_BUFFER_SCALE_FACTOR = 4; // Reduced for ray-tracing workloads
constexpr VkDeviceSize UNIFORM_BUFFER_EXTRA_SIZE = 2048; // Reduced for smaller UBOs
constexpr uint32_t DEFAULT_UNIFORM_BUFFER_COUNT = 16;   // Reduced for 8 GB VRAM
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;            // Matches VulkanRenderer requirement

std::string formatSize(VkDeviceSize size) {
    if (size == 0) return "0 B";
    if (size < 1024) return std::format("{} B", size);
    size_t kb = size / 1024;
    if (kb < 1024) return std::format("{} KB", kb);
    size_t mb = kb / 1024;
    return std::format("{} MB", mb);
}

bool isMeshShaderSupported(VkPhysicalDevice physicalDevice) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());
    for (const auto& ext : extensions) {
        if (std::strcmp(ext.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
}

void cleanupResources(
    VkDevice device, VkCommandPool commandPool,
    VkBuffer stagingBuffer, VkDeviceMemory stagingBufferMemory,
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE) {
    if (commandBuffer != VK_NULL_HANDLE) {
        std::vector<VkCommandBuffer> vec = {commandBuffer};
        if (commandPool != VK_NULL_HANDLE) {
            Dispose::freeCommandBuffers(device, commandPool, vec);
        }
    }
    if (stagingBuffer != VK_NULL_HANDLE) {
        Dispose::destroySingleBuffer(device, stagingBuffer);
    }
    if (stagingBufferMemory != VK_NULL_HANDLE) {
        Dispose::freeSingleDeviceMemory(device, stagingBufferMemory);
    }
}
}

VulkanBufferManager::VulkanBufferManager(Vulkan::Context& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices)
    : context_(context),
      vertexCount_(0),
      indexCount_(0),
      vertexBufferAddress_(0),
      indexBufferAddress_(0),
      scratchBufferAddress_(0) {
    LOG_DEBUG("Entering VulkanBufferManager constructor with vertices.size()={}, indices.size()={}", vertices.size(), indices.size());
    if (context_.device == VK_NULL_HANDLE || context_.physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR("Invalid Vulkan device or physical device in VulkanBufferManager", std::source_location::current());
        throw std::runtime_error("Invalid Vulkan device or physical device");
    }

    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProperties);
    VkDeviceSize totalDeviceLocalMemory = 0;
    for (uint32_t i = 0; i < memProperties.memoryHeapCount; ++i) {
        if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalDeviceLocalMemory += memProperties.memoryHeaps[i].size;
            LOG_INFO(std::format("Device local heap[{}] size: {} MB", i, memProperties.memoryHeaps[i].size / (1024 * 1024)));
        }
    }
    if (totalDeviceLocalMemory < 8ULL * 1024 * 1024 * 1024) {
        LOG_WARNING(std::format("Total device local memory ({} MB) is less than expected 8192 MB for 8 GB VRAM", totalDeviceLocalMemory / (1024 * 1024)), std::source_location::current());
    }
    LOG_INFO(std::format("Total device local memory: {} MB", totalDeviceLocalMemory / (1024 * 1024)));

    LOG_DEBUG("Step 1: Validating input spans");
    if (vertices.empty() || indices.empty()) {
        LOG_ERROR("Empty vertex or index data provided: vertices.size()={}, indices.size()={}", vertices.size(), indices.size(), std::source_location::current());
        throw std::invalid_argument("Vertex or index data cannot be empty");
    }
    LOG_DEBUG("Step 1 passed: spans are non-empty");

    LOG_DEBUG("Step 2: Setting counts");
    vertexCount_ = static_cast<uint32_t>(vertices.size());
    indexCount_ = static_cast<uint32_t>(indices.size());
    LOG_DEBUG("Set vertexCount_={}, indexCount_={}", vertexCount_, indexCount_);

    LOG_DEBUG("Step 3: Validating geometry");
    if (indexCount_ < 3 || indexCount_ % 3 != 0) {
        LOG_ERROR(std::format("Invalid geometry: indexCount_={} must be >= 3 and divisible by 3", indexCount_), std::source_location::current());
        throw std::invalid_argument("Invalid geometry for triangle-based rendering");
    }
    LOG_DEBUG("Step 3 passed: valid geometry");

    VkDeviceSize vertexBufferSize = sizeof(glm::vec3) * vertices.size() * VERTEX_BUFFER_SCALE_FACTOR;
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size() * INDEX_BUFFER_SCALE_FACTOR;
    VkDeviceSize totalBufferSize = vertexBufferSize + indexBufferSize;
    LOG_DEBUG("Calculated buffer sizes: vertexBufferSize={}, indexBufferSize={}, totalBufferSize={}", formatSize(vertexBufferSize), formatSize(indexBufferSize), formatSize(totalBufferSize));
    if (totalBufferSize > totalDeviceLocalMemory / 2) { // Limit to 50% of VRAM (4 GB)
        LOG_ERROR(std::format("Requested buffer size {} MB exceeds 50% of available VRAM {} MB", totalBufferSize / (1024 * 1024), totalDeviceLocalMemory / (1024 * 1024)), std::source_location::current());
        throw std::runtime_error("Buffer size exceeds VRAM capacity");
    }
    LOG_DEBUG("Step 4: Buffer size validation passed");

    vkDeviceWaitIdle(context_.device);
    LOG_DEBUG("Device waited idle");

    // Query queue family indices for concurrent sharing (graphics, compute, transfer)
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, queueFamilies.data());
    std::vector<uint32_t> queueFamilyIndices;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)) {
            queueFamilyIndices.push_back(i);
        }
    }
    VkSharingMode sharingMode = (queueFamilyIndices.size() > 1) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    uint32_t* pQueueFamilyIndices = (sharingMode == VK_SHARING_MODE_CONCURRENT) ? queueFamilyIndices.data() : nullptr;
    uint32_t queueFamilyIndexCount = (sharingMode == VK_SHARING_MODE_CONCURRENT) ? static_cast<uint32_t>(queueFamilyIndices.size()) : 0;
    LOG_DEBUG("Step 5: Queue family setup - sharingMode={}, queueFamilyCount={}", static_cast<uint32_t>(sharingMode), queueFamilyIndexCount);

    VkBufferCreateInfo stagingBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = vertexBufferSize + indexBufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = sharingMode,
        .queueFamilyIndexCount = queueFamilyIndexCount,
        .pQueueFamilyIndices = pQueueFamilyIndices
    };
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkResult result = vkCreateBuffer(context_.device, &stagingBufferInfo, nullptr, &stagingBuffer);
    if (result != VK_SUCCESS || stagingBuffer == VK_NULL_HANDLE) {
        LOG_ERROR(std::format("Failed to create staging buffer with size={} MB, usage={:x}", (vertexBufferSize + indexBufferSize) / (1024 * 1024), static_cast<uint32_t>(stagingBufferInfo.usage)), std::source_location::current());
        throw std::runtime_error("Failed to create staging buffer");
    }
    LOG_INFO(std::format("Created staging buffer: {:p}, size: {}", static_cast<void*>(stagingBuffer), formatSize(vertexBufferSize + indexBufferSize)));

    VkMemoryRequirements stagingMemReq{};
    vkGetBufferMemoryRequirements(context_.device, stagingBuffer, &stagingMemReq);
    uint32_t stagingMemType = VulkanInitializer::findMemoryType(context_.physicalDevice, stagingMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo stagingAllocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = stagingMemReq.size,
        .memoryTypeIndex = stagingMemType
    };
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    result = vkAllocateMemory(context_.device, &stagingAllocInfo, nullptr, &stagingBufferMemory);
    if (result != VK_SUCCESS || stagingBufferMemory == VK_NULL_HANDLE) {
        LOG_ERROR(std::format("Failed to allocate staging buffer memory for buffer={:p}, size={} MB", static_cast<void*>(stagingBuffer), stagingMemReq.size / (1024 * 1024)), std::source_location::current());
        cleanupResources(context_.device, context_.commandPool, stagingBuffer, VK_NULL_HANDLE);
        throw std::runtime_error("Failed to allocate staging buffer memory");
    }
    result = vkBindBufferMemory(context_.device, stagingBuffer, stagingBufferMemory, 0);
    if (result != VK_SUCCESS) {
        LOG_ERROR(std::format("Failed to bind staging buffer memory for buffer={:p}, memory={:p}", static_cast<void*>(stagingBuffer), static_cast<void*>(stagingBufferMemory)), std::source_location::current());
        cleanupResources(context_.device, context_.commandPool, stagingBuffer, stagingBufferMemory);
        throw std::runtime_error("Failed to bind staging buffer memory");
    }
    LOG_INFO(std::format("Allocated and bound staging buffer memory: {:p} for buffer: {:p}, size: {}", static_cast<void*>(stagingBufferMemory), static_cast<void*>(stagingBuffer), formatSize(stagingMemReq.size)));

    LOG_DEBUG("Step 6: Mapping staging memory and copying data");
    void* data = nullptr;
    result = vkMapMemory(context_.device, stagingBufferMemory, 0, vertexBufferSize / VERTEX_BUFFER_SCALE_FACTOR + indexBufferSize / INDEX_BUFFER_SCALE_FACTOR, 0, &data);
    if (result != VK_SUCCESS) {
        LOG_ERROR(std::format("Failed to map staging buffer memory: VkResult={}", static_cast<int>(result)), std::source_location::current());
        cleanupResources(context_.device, context_.commandPool, stagingBuffer, stagingBufferMemory);
        throw std::runtime_error("Failed to map staging buffer memory");
    }
    LOG_DEBUG("Mapped staging memory at {:p}", data);
    memcpy(data, vertices.data(), sizeof(glm::vec3) * vertices.size());
    LOG_DEBUG("Copied {} vertices to staging buffer", vertices.size());
    memcpy(static_cast<char*>(data) + sizeof(glm::vec3) * vertices.size(), indices.data(), sizeof(uint32_t) * indices.size());
    LOG_DEBUG("Copied {} indices to staging buffer", indices.size());
    vkUnmapMemory(context_.device, stagingBufferMemory);
    LOG_DEBUG("Unmapped staging memory");

    LOG_DEBUG("Step 7: Querying device properties for alignment");
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = nullptr,
        .properties = {}
    };
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
        .pNext = nullptr,
        .maxGeometryCount = 0,
        .maxInstanceCount = 0,
        .maxPrimitiveCount = 0,
        .maxPerStageDescriptorAccelerationStructures = 0,
        .maxPerStageDescriptorUpdateAfterBindAccelerationStructures = 0,
        .maxDescriptorSetAccelerationStructures = 0,
        .maxDescriptorSetUpdateAfterBindAccelerationStructures = 0,
        .minAccelerationStructureScratchOffsetAlignment = 0
    };
    props2.pNext = &accelProps;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    VkDeviceSize alignment = std::max(static_cast<VkDeviceSize>(accelProps.minAccelerationStructureScratchOffsetAlignment), props2.properties.limits.minStorageBufferOffsetAlignment);
    LOG_DEBUG("Alignment for buffers: {}", alignment);

    bool meshShaderSupported = isMeshShaderSupported(context_.physicalDevice);
    LOG_DEBUG("Mesh shader support: {}", meshShaderSupported ? "enabled" : "disabled");
    if (!meshShaderSupported) {
        LOG_WARNING("VK_EXT_mesh_shader not supported; falling back to traditional vertex pipeline", std::source_location::current());
    }

    VkMemoryAllocateFlagsInfo allocFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0
    };
    vertexBufferSize = (vertexBufferSize + alignment - 1) & ~(alignment - 1);
    LOG_DEBUG("Aligned vertexBufferSize: {}", formatSize(vertexBufferSize));
    if (vertexBufferSize > totalDeviceLocalMemory / 4) { // Limit to 25% of VRAM (2 GB)
        LOG_ERROR(std::format("Vertex buffer size {} MB exceeds 25% of VRAM {} MB", vertexBufferSize / (1024 * 1024), totalDeviceLocalMemory / (1024 * 1024)), std::source_location::current());
        cleanupResources(context_.device, context_.commandPool, stagingBuffer, stagingBufferMemory);
        throw std::runtime_error("Vertex buffer size exceeds VRAM capacity");
    }
    VkBufferUsageFlags vertexUsageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (meshShaderSupported) {
        vertexUsageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // Use STORAGE_BUFFER for mesh shaders as a fallback
    }
    LOG_DEBUG("Vertex usage flags: {:x}", static_cast<uint32_t>(vertexUsageFlags));
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice, vertexBufferSize,
        vertexUsageFlags,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        context_.vertexBuffer, context_.vertexBufferMemory, &allocFlagsInfo, context_.resourceManager
    );
    if (context_.vertexBuffer == VK_NULL_HANDLE || context_.vertexBufferMemory == VK_NULL_HANDLE) {
        LOG_ERROR(std::format("Failed to create vertex buffer or memory: buffer={:p}, memory={:p}", static_cast<void*>(context_.vertexBuffer), static_cast<void*>(context_.vertexBufferMemory)), std::source_location::current());
        cleanupResources(context_.device, context_.commandPool, stagingBuffer, stagingBufferMemory);
        throw std::runtime_error("Failed to create vertex buffer");
    }
    vertexBufferAddress_ = VulkanInitializer::getBufferDeviceAddress(context_.device, context_.vertexBuffer);
    if (vertexBufferAddress_ == 0) {
        LOG_ERROR(std::format("Failed to get vertex buffer device address: buffer={:p}", static_cast<void*>(context_.vertexBuffer)), std::source_location::current());
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        cleanupResources(context_.device, context_.commandPool, stagingBuffer, stagingBufferMemory);
        throw std::runtime_error("Failed to get vertex buffer device address");
    }
    LOG_INFO(std::format("Created vertex buffer: {:p} with address: 0x{:x}, size: {}", static_cast<void*>(context_.vertexBuffer), vertexBufferAddress_, formatSize(vertexBufferSize)));

    indexBufferSize = (indexBufferSize + alignment - 1) & ~(alignment - 1);
    LOG_DEBUG("Aligned indexBufferSize: {}", formatSize(indexBufferSize));
    if (indexBufferSize > totalDeviceLocalMemory / 4) {
        LOG_ERROR(std::format("Index buffer size {} MB exceeds 25% of VRAM {} MB", indexBufferSize / (1024 * 1024), totalDeviceLocalMemory / (1024 * 1024)), std::source_location::current());
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        cleanupResources(context_.device, context_.commandPool, stagingBuffer, stagingBufferMemory);
        throw std::runtime_error("Index buffer size exceeds VRAM capacity");
    }
    VkBufferUsageFlags indexUsageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (meshShaderSupported) {
        indexUsageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // Use STORAGE_BUFFER for mesh shaders as a fallback
    }
    LOG_DEBUG("Index usage flags: {:x}", static_cast<uint32_t>(indexUsageFlags));
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice, indexBufferSize,
        indexUsageFlags,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        context_.indexBuffer, context_.indexBufferMemory, &allocFlagsInfo, context_.resourceManager
    );
    if (context_.indexBuffer == VK_NULL_HANDLE || context_.indexBufferMemory == VK_NULL_HANDLE) {
        LOG_ERROR(std::format("Failed to create index buffer or memory: buffer={:p}, memory={:p}", static_cast<void*>(context_.indexBuffer), static_cast<void*>(context_.indexBufferMemory)), std::source_location::current());
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        context_.resourceManager.removeBuffer(context_.indexBuffer);
        context_.resourceManager.removeMemory(context_.indexBufferMemory);
        cleanupResources(context_.device, context_.commandPool, stagingBuffer, stagingBufferMemory);
        throw std::runtime_error("Failed to create index buffer");
    }
    indexBufferAddress_ = VulkanInitializer::getBufferDeviceAddress(context_.device, context_.indexBuffer);
    if (indexBufferAddress_ == 0) {
        LOG_ERROR(std::format("Failed to get index buffer device address: buffer={:p}", static_cast<void*>(context_.indexBuffer)), std::source_location::current());
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        context_.resourceManager.removeBuffer(context_.indexBuffer);
        context_.resourceManager.removeMemory(context_.indexBufferMemory);
        cleanupResources(context_.device, context_.commandPool, stagingBuffer, stagingBufferMemory);
        throw std::runtime_error("Failed to get index buffer device address");
    }
    LOG_INFO(std::format("Created index buffer: {:p} with address: 0x{:x}, size: {}", static_cast<void*>(context_.indexBuffer), indexBufferAddress_, formatSize(indexBufferSize)));

    // Create temporary command pool for transfer (if context_.commandPool is null or to avoid dependency)
    VkCommandPool transferCommandPool = VK_NULL_HANDLE;
    bool usingTempPool = (context_.commandPool == VK_NULL_HANDLE);
    uint32_t transferFamily = UINT32_MAX;
    if (usingTempPool) {
        // Reuse queue family logic from earlier
        uint32_t queueFamilyCountTemp = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCountTemp, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamiliesTemp(queueFamilyCountTemp);
        vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCountTemp, queueFamiliesTemp.data());
        transferFamily = UINT32_MAX;  // Fallback to graphics if no dedicated transfer
        for (uint32_t i = 0; i < queueFamilyCountTemp; ++i) {
            if (queueFamiliesTemp[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
                transferFamily = i;
                break;
            }
        }
        if (transferFamily == UINT32_MAX) {
            // Use graphics as fallback (common on NVIDIA)
            for (uint32_t i = 0; i < queueFamilyCountTemp; ++i) {
                if (queueFamiliesTemp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    transferFamily = i;
                    break;
                }
            }
        }
        if (transferFamily == UINT32_MAX) {
            LOG_ERROR("No suitable queue family for transfer", std::source_location::current());
            context_.resourceManager.removeBuffer(context_.vertexBuffer);
            context_.resourceManager.removeMemory(context_.vertexBufferMemory);
            context_.resourceManager.removeBuffer(context_.indexBuffer);
            context_.resourceManager.removeMemory(context_.indexBufferMemory);
            cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingBufferMemory);  // Safe with null pool
            throw std::runtime_error("No suitable queue family for transfer");
        }

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;  // Optimized for short-lived
        poolInfo.queueFamilyIndex = transferFamily;

        VkResult resultTemp = vkCreateCommandPool(context_.device, &poolInfo, nullptr, &transferCommandPool);
        if (resultTemp != VK_SUCCESS || transferCommandPool == VK_NULL_HANDLE) {
            LOG_ERROR(std::format("Failed to create temporary transfer command pool on family {}: VkResult={}", transferFamily, static_cast<int>(resultTemp)), std::source_location::current());
            context_.resourceManager.removeBuffer(context_.vertexBuffer);
            context_.resourceManager.removeMemory(context_.vertexBufferMemory);
            context_.resourceManager.removeBuffer(context_.indexBuffer);
            context_.resourceManager.removeMemory(context_.indexBufferMemory);
            cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingBufferMemory);
            throw std::runtime_error("Failed to create temporary command pool");
        }
        LOG_INFO(std::format("Created temporary transfer command pool: {:p} on family {}", static_cast<void*>(transferCommandPool), transferFamily));
    }

    // Now allocate (use temp or context pool)
    VkCommandPool activePool = usingTempPool ? transferCommandPool : context_.commandPool;
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = activePool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(context_.device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate command buffer", std::source_location::current());
        if (usingTempPool) {
            vkDestroyCommandPool(context_.device, transferCommandPool, nullptr);
        }
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        context_.resourceManager.removeBuffer(context_.indexBuffer);
        context_.resourceManager.removeMemory(context_.indexBufferMemory);
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingBufferMemory, VK_NULL_HANDLE);
        throw std::runtime_error("Failed to allocate command buffer");
    }
    LOG_INFO(std::format("Allocated command buffer: {:p} from pool: {:p}", static_cast<void*>(commandBuffer), static_cast<void*>(activePool)));

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR("Failed to begin command buffer", std::source_location::current());
        std::vector<VkCommandBuffer> vec{commandBuffer};
        vkFreeCommandBuffers(context_.device, activePool, static_cast<uint32_t>(vec.size()), vec.data());
        if (usingTempPool) {
            vkDestroyCommandPool(context_.device, transferCommandPool, nullptr);
        }
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        context_.resourceManager.removeBuffer(context_.indexBuffer);
        context_.resourceManager.removeMemory(context_.indexBufferMemory);
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingBufferMemory, VK_NULL_HANDLE);
        throw std::runtime_error("Failed to begin command buffer");
    }
    LOG_DEBUG("Began command buffer");

    LOG_DEBUG("Step 8: Recording buffer copies");
    VkBufferCopy vertexCopyRegion{.srcOffset = 0, .dstOffset = 0, .size = sizeof(glm::vec3) * vertices.size()};
    LOG_DEBUG("Vertex copy region: srcOffset={}, dstOffset={}, size={}", vertexCopyRegion.srcOffset, vertexCopyRegion.dstOffset, formatSize(vertexCopyRegion.size));
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, context_.vertexBuffer, 1, &vertexCopyRegion);

    VkBufferCopy indexCopyRegion{.srcOffset = sizeof(glm::vec3) * vertices.size(), .dstOffset = 0, .size = sizeof(uint32_t) * indices.size()};
    LOG_DEBUG("Index copy region: srcOffset={}, dstOffset={}, size={}", indexCopyRegion.srcOffset, indexCopyRegion.dstOffset, formatSize(indexCopyRegion.size));
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, context_.indexBuffer, 1, &indexCopyRegion);

    VkMemoryBarrier memoryBarrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_SHADER_READ_BIT
    };
    if (meshShaderSupported) {
        memoryBarrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT; // Use SHADER_READ as fallback for mesh shaders
    }
    LOG_DEBUG("Memory barrier: srcAccessMask={:x}, dstAccessMask={:x}", static_cast<uint32_t>(memoryBarrier.srcAccessMask), static_cast<uint32_t>(memoryBarrier.dstAccessMask));
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | (meshShaderSupported ? VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT : 0),
        0, 1, &memoryBarrier, 0, nullptr, 0, nullptr
    );
    LOG_DEBUG("Pipeline barrier recorded");

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to end command buffer", std::source_location::current());
        std::vector<VkCommandBuffer> vec{commandBuffer};
        vkFreeCommandBuffers(context_.device, activePool, static_cast<uint32_t>(vec.size()), vec.data());
        if (usingTempPool) {
            vkDestroyCommandPool(context_.device, transferCommandPool, nullptr);
        }
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        context_.resourceManager.removeBuffer(context_.indexBuffer);
        context_.resourceManager.removeMemory(context_.indexBufferMemory);
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingBufferMemory, VK_NULL_HANDLE);
        throw std::runtime_error("Failed to end command buffer");
    }
    LOG_DEBUG("Ended command buffer");

    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(context_.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        LOG_ERROR("Failed to create fence for buffer copy", std::source_location::current());
        std::vector<VkCommandBuffer> vec{commandBuffer};
        vkFreeCommandBuffers(context_.device, activePool, static_cast<uint32_t>(vec.size()), vec.data());
        if (usingTempPool) {
            vkDestroyCommandPool(context_.device, transferCommandPool, nullptr);
        }
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        context_.resourceManager.removeBuffer(context_.indexBuffer);
        context_.resourceManager.removeMemory(context_.indexBufferMemory);
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingBufferMemory, VK_NULL_HANDLE);
        throw std::runtime_error("Failed to create fence");
    }
    LOG_DEBUG("Created fence: {:p}", static_cast<void*>(fence));

    VkQueue submitQueue = context_.graphicsQueue;
    if (usingTempPool) {
        vkGetDeviceQueue(context_.device, transferFamily, 0, &submitQueue);
        LOG_INFO(std::format("Using transfer queue {:p} for buffer copy submit", static_cast<void*>(submitQueue)));
    }

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };
    if (vkQueueSubmit(submitQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        LOG_ERROR("Failed to submit command buffer", std::source_location::current());
        vkDestroyFence(context_.device, fence, nullptr);
        std::vector<VkCommandBuffer> vec{commandBuffer};
        vkFreeCommandBuffers(context_.device, activePool, static_cast<uint32_t>(vec.size()), vec.data());
        if (usingTempPool) {
            vkDestroyCommandPool(context_.device, transferCommandPool, nullptr);
        }
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        context_.resourceManager.removeBuffer(context_.indexBuffer);
        context_.resourceManager.removeMemory(context_.indexBufferMemory);
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingBufferMemory, VK_NULL_HANDLE);
        throw std::runtime_error("Failed to submit command buffer");
    }
    LOG_DEBUG("Submitted to queue");

    if (vkWaitForFences(context_.device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        LOG_ERROR("Failed to wait for fence", std::source_location::current());
        vkDestroyFence(context_.device, fence, nullptr);
        std::vector<VkCommandBuffer> vec{commandBuffer};
        vkFreeCommandBuffers(context_.device, activePool, static_cast<uint32_t>(vec.size()), vec.data());
        if (usingTempPool) {
            vkDestroyCommandPool(context_.device, transferCommandPool, nullptr);
        }
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        context_.resourceManager.removeBuffer(context_.indexBuffer);
        context_.resourceManager.removeMemory(context_.indexBufferMemory);
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingBufferMemory, VK_NULL_HANDLE);
        throw std::runtime_error("Failed to wait for fence");
    }
    LOG_DEBUG("Waited for fence");

    vkDestroyFence(context_.device, fence, nullptr);
    LOG_DEBUG("Destroyed fence");

    // Updated cleanup: free from active pool, destroy temp if used
    std::vector<VkCommandBuffer> cmdVec = {commandBuffer};
    vkFreeCommandBuffers(context_.device, activePool, static_cast<uint32_t>(cmdVec.size()), cmdVec.data());
    LOG_DEBUG("Freed command buffer");
    if (usingTempPool) {
        vkDestroyCommandPool(context_.device, transferCommandPool, nullptr);
        LOG_INFO("Destroyed temporary transfer command pool");
    }
    cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingBufferMemory, VK_NULL_HANDLE);  // No pool needed now
    LOG_INFO("Destroyed staging buffer and memory");

    createUniformBuffers(MAX_FRAMES_IN_FLIGHT);
    LOG_INFO(std::format("Initialized VulkanBufferManager with {} vertices and {} indices, vertex buffer size: {}, index buffer size: {}", 
                         vertexCount_, indexCount_, formatSize(vertexBufferSize), formatSize(indexBufferSize)));

    VkDeviceSize scratchSize = calculateScratchBufferSize() * SCRATCH_BUFFER_SCALE_FACTOR;
    LOG_DEBUG("Calculated scratchSize: {}", formatSize(scratchSize));
    if (scratchSize > totalDeviceLocalMemory / 4) {
        LOG_ERROR(std::format("Scratch buffer size {} MB exceeds 25% of VRAM {} MB", scratchSize / (1024 * 1024), totalDeviceLocalMemory / (1024 * 1024)), std::source_location::current());
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        context_.resourceManager.removeBuffer(context_.indexBuffer);
        context_.resourceManager.removeMemory(context_.indexBufferMemory);
        throw std::runtime_error("Scratch buffer size exceeds VRAM capacity");
    }
    createScratchBuffer(scratchSize);
    LOG_INFO(std::format("Created scratch buffer with size: {}", formatSize(scratchSize)));
    LOG_DEBUG("Exiting VulkanBufferManager constructor");
}

void VulkanBufferManager::createUniformBuffers(uint32_t count) {
    LOG_DEBUG(std::format("Creating {} uniform buffers", count));
    if (count == 0) {
        LOG_WARNING("Requested zero uniform buffers; keeping existing buffers", std::source_location::current());
        return;
    }

    // Clear existing uniform buffers in context
    if (!context_.uniformBuffers.empty()) {
        if (context_.device != VK_NULL_HANDLE) {
            for (size_t i = 0; i < context_.uniformBuffers.size(); ++i) {
                if (context_.uniformBuffers[i] != VK_NULL_HANDLE) {
                    context_.resourceManager.removeBuffer(context_.uniformBuffers[i]);
                    LOG_DEBUG(std::format("Removing uniform buffer[{}] {:p} from resource manager", i, static_cast<void*>(context_.uniformBuffers[i])));
                }
                if (context_.uniformBufferMemories[i] != VK_NULL_HANDLE) {
                    context_.resourceManager.removeMemory(context_.uniformBufferMemories[i]);
                    LOG_DEBUG(std::format("Removing uniform buffer memory[{}] {:p} from resource manager", i, static_cast<void*>(context_.uniformBufferMemories[i])));
                }
            }
            Dispose::destroyBuffers(context_.device, context_.uniformBuffers);
            Dispose::freeDeviceMemories(context_.device, context_.uniformBufferMemories);
            LOG_DEBUG("Cleared previous uniform buffers and memories");
        }
        context_.uniformBuffers.clear();
        context_.uniformBufferMemories.clear();
    }

    context_.uniformBuffers.resize(count);
    context_.uniformBufferMemories.resize(count);
    VkDeviceSize bufferSize = sizeof(UE::UniformBufferObject) + sizeof(int) + UNIFORM_BUFFER_EXTRA_SIZE;
    VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VkPhysicalDeviceProperties deviceProps{};
    vkGetPhysicalDeviceProperties(context_.physicalDevice, &deviceProps);
    bufferSize = (bufferSize + deviceProps.limits.minUniformBufferOffsetAlignment - 1) & ~(deviceProps.limits.minUniformBufferOffsetAlignment - 1);
    VkDeviceSize totalUniformSize = bufferSize * count;
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProperties);
    VkDeviceSize totalDeviceLocalMemory = 0;
    for (uint32_t i = 0; i < memProperties.memoryHeapCount; ++i) {
        if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalDeviceLocalMemory += memProperties.memoryHeaps[i].size;
        }
    }
    if (totalUniformSize > totalDeviceLocalMemory / 8) { // Limit to 12.5% of VRAM (1 GB)
        LOG_ERROR(std::format("Total uniform buffer size {} MB exceeds 12.5% of VRAM {} MB", totalUniformSize / (1024 * 1024), totalDeviceLocalMemory / (1024 * 1024)), std::source_location::current());
        throw std::runtime_error("Uniform buffer size exceeds VRAM capacity");
    }

    for (uint32_t i = 0; i < count; ++i) {
        VulkanInitializer::createBuffer(
            context_.device, context_.physicalDevice, bufferSize,
            usageFlags, memoryFlags,
            context_.uniformBuffers[i], context_.uniformBufferMemories[i], nullptr, context_.resourceManager
        );
        if (context_.uniformBuffers[i] == VK_NULL_HANDLE || context_.uniformBufferMemories[i] == VK_NULL_HANDLE) {
            LOG_ERROR(std::format("Failed to create uniform buffer[{}] or memory: buffer={:p}, memory={:p}", i, static_cast<void*>(context_.uniformBuffers[i]), static_cast<void*>(context_.uniformBufferMemories[i])), std::source_location::current());
            for (size_t j = 0; j < i; ++j) {
                context_.resourceManager.removeBuffer(context_.uniformBuffers[j]);
                context_.resourceManager.removeMemory(context_.uniformBufferMemories[j]);
            }
            Dispose::destroyBuffers(context_.device, context_.uniformBuffers);
            Dispose::freeDeviceMemories(context_.device, context_.uniformBufferMemories);
            context_.uniformBuffers.clear();
            context_.uniformBufferMemories.clear();
            throw std::runtime_error("Failed to create uniform buffer");
        }
        VkMemoryRequirements memReq{};
        vkGetBufferMemoryRequirements(context_.device, context_.uniformBuffers[i], &memReq);
        if (memReq.size < bufferSize) {
            LOG_ERROR(std::format("Uniform buffer[{}] allocated size {} is less than requested size {}", i, memReq.size, bufferSize), std::source_location::current());
            for (size_t j = 0; j <= i; ++j) {
                context_.resourceManager.removeBuffer(context_.uniformBuffers[j]);
                context_.resourceManager.removeMemory(context_.uniformBufferMemories[j]);
            }
            Dispose::destroyBuffers(context_.device, context_.uniformBuffers);
            Dispose::freeDeviceMemories(context_.device, context_.uniformBufferMemories);
            context_.uniformBuffers.clear();
            context_.uniformBufferMemories.clear();
            throw std::runtime_error("Uniform buffer size insufficient");
        }
        LOG_INFO(std::format("Created uniform buffer[{}]: {:p}, size: {}", i, static_cast<void*>(context_.uniformBuffers[i]), formatSize(bufferSize)));
    }
    LOG_INFO(std::format("Created {} uniform buffers successfully", count));
}

void VulkanBufferManager::createScratchBuffer(VkDeviceSize size) {
    LOG_DEBUG(std::format("Creating scratch buffer with size={}", formatSize(size)));
    if (size == 0) {
        LOG_ERROR("Scratch buffer size cannot be zero", std::source_location::current());
        throw std::invalid_argument("Scratch buffer size cannot be zero");
    }

    if (context_.scratchBuffer != VK_NULL_HANDLE) {
        context_.resourceManager.removeBuffer(context_.scratchBuffer);
        LOG_DEBUG(std::format("Removing previous scratch buffer {:p} from resource manager", static_cast<void*>(context_.scratchBuffer)));
        Dispose::destroySingleBuffer(context_.device, context_.scratchBuffer);
        context_.scratchBuffer = VK_NULL_HANDLE;
        LOG_DEBUG(std::format("Destroyed previous scratch buffer: {:p}", static_cast<void*>(context_.scratchBuffer)));
    }
    if (context_.scratchBufferMemory != VK_NULL_HANDLE) {
        context_.resourceManager.removeMemory(context_.scratchBufferMemory);
        LOG_DEBUG(std::format("Removing previous scratch buffer memory {:p} from resource manager", static_cast<void*>(context_.scratchBufferMemory)));
        Dispose::freeSingleDeviceMemory(context_.device, context_.scratchBufferMemory);
        context_.scratchBufferMemory = VK_NULL_HANDLE;
        LOG_DEBUG(std::format("Freed previous scratch buffer memory: {:p}", static_cast<void*>(context_.scratchBufferMemory)));
    }

    LOG_DEBUG("Step 1: Querying acceleration structure properties");
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
        .pNext = nullptr,
        .maxGeometryCount = 0,
        .maxInstanceCount = 0,
        .maxPrimitiveCount = 0,
        .maxPerStageDescriptorAccelerationStructures = 0,
        .maxPerStageDescriptorUpdateAfterBindAccelerationStructures = 0,
        .maxDescriptorSetAccelerationStructures = 0,
        .maxDescriptorSetUpdateAfterBindAccelerationStructures = 0,
        .minAccelerationStructureScratchOffsetAlignment = 0
    };
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &accelProps,
        .properties = {}
    };
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    VkDeviceSize alignment = std::max(static_cast<VkDeviceSize>(accelProps.minAccelerationStructureScratchOffsetAlignment), props2.properties.limits.minStorageBufferOffsetAlignment);
    LOG_DEBUG("Scratch buffer alignment: {}", alignment);

    size = (size + alignment - 1) & ~(alignment - 1);
    LOG_DEBUG("Aligned scratch size: {}", formatSize(size));

    // Reuse queue family indices from constructor
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, queueFamilies.data());
    std::vector<uint32_t> queueFamilyIndices;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)) {
            queueFamilyIndices.push_back(i);
        }
    }
    VkSharingMode sharingMode = (queueFamilyIndices.size() > 1) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    uint32_t* pQueueFamilyIndices = (sharingMode == VK_SHARING_MODE_CONCURRENT) ? queueFamilyIndices.data() : nullptr;
    uint32_t queueFamilyIndexCount = (sharingMode == VK_SHARING_MODE_CONCURRENT) ? static_cast<uint32_t>(queueFamilyIndices.size()) : 0;
    LOG_DEBUG("Scratch buffer sharingMode={}, queueFamilyIndexCount={}", static_cast<uint32_t>(sharingMode), queueFamilyIndexCount);

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = sharingMode,
        .queueFamilyIndexCount = queueFamilyIndexCount,
        .pQueueFamilyIndices = pQueueFamilyIndices
    };
    VkBuffer tempBuffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(context_.device, &bufferInfo, nullptr, &tempBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create temporary buffer for memory requirements", std::source_location::current());
        throw std::runtime_error("Failed to create temporary buffer for memory requirements");
    }
    VkMemoryRequirements memRequirements{};
    vkGetBufferMemoryRequirements(context_.device, tempBuffer, &memRequirements);
    vkDestroyBuffer(context_.device, tempBuffer, nullptr);
    LOG_DEBUG("Temp buffer memRequirements: size={}, alignment={}", formatSize(memRequirements.size), memRequirements.alignment);

    size = (size > memRequirements.size) ? size : memRequirements.size;
    size = (size + memRequirements.alignment - 1) & ~(memRequirements.alignment - 1);
    LOG_DEBUG("Final scratch size after memRequirements adjustment: {}", formatSize(size));

    VkMemoryAllocateFlagsInfo allocFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0
    };
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice, size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        context_.scratchBuffer, context_.scratchBufferMemory, &allocFlagsInfo, context_.resourceManager
    );
    if (context_.scratchBuffer == VK_NULL_HANDLE || context_.scratchBufferMemory == VK_NULL_HANDLE) {
        LOG_ERROR(std::format("Failed to create scratch buffer or memory: buffer={:p}, memory={:p}", static_cast<void*>(context_.scratchBuffer), static_cast<void*>(context_.scratchBufferMemory)), std::source_location::current());
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        context_.resourceManager.removeBuffer(context_.indexBuffer);
        context_.resourceManager.removeMemory(context_.indexBufferMemory);
        throw std::runtime_error("Failed to create scratch buffer");
    }

    scratchBufferAddress_ = VulkanInitializer::getBufferDeviceAddress(context_.device, context_.scratchBuffer);
    if (scratchBufferAddress_ == 0) {
        LOG_ERROR(std::format("Failed to get scratch buffer device address: buffer={:p}", static_cast<void*>(context_.scratchBuffer)), std::source_location::current());
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        context_.resourceManager.removeBuffer(context_.indexBuffer);
        context_.resourceManager.removeMemory(context_.indexBufferMemory);
        context_.resourceManager.removeBuffer(context_.scratchBuffer);
        context_.resourceManager.removeMemory(context_.scratchBufferMemory);
        throw std::runtime_error("Failed to get scratch buffer device address");
    }

    vkGetBufferMemoryRequirements(context_.device, context_.scratchBuffer, &memRequirements);
    if (memRequirements.size < size) {
        LOG_ERROR(std::format("Scratch buffer allocated size {} is less than requested size {}", memRequirements.size, size), std::source_location::current());
        context_.resourceManager.removeBuffer(context_.vertexBuffer);
        context_.resourceManager.removeMemory(context_.vertexBufferMemory);
        context_.resourceManager.removeBuffer(context_.indexBuffer);
        context_.resourceManager.removeMemory(context_.indexBufferMemory);
        context_.resourceManager.removeBuffer(context_.scratchBuffer);
        context_.resourceManager.removeMemory(context_.scratchBufferMemory);
        throw std::runtime_error("Scratch buffer size insufficient");
    }

    LOG_INFO(std::format("Created scratch buffer: {:p} with address: 0x{:x}, size: {}", static_cast<void*>(context_.scratchBuffer), scratchBufferAddress_, formatSize(size)));
}

VkDeviceSize VulkanBufferManager::calculateScratchBufferSize() {
    LOG_DEBUG("Entering calculateScratchBufferSize: vertexCount_={}, indexCount_={}", vertexCount_, indexCount_);
    if (indexCount_ < 3 || indexCount_ % 3 != 0) {
        LOG_ERROR(std::format("Invalid geometry for acceleration structure: indexCount_={} must be >= 3 and divisible by 3", indexCount_), std::source_location::current());
        throw std::invalid_argument("Invalid geometry for acceleration structure");
    }
    if (vertexCount_ == 0) {
        LOG_ERROR("vertexCount_ == 0, cannot build acceleration structure", std::source_location::current());
        throw std::invalid_argument("No vertices for acceleration structure");
    }

    auto vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetAccelerationStructureBuildSizesKHR"));
    if (!vkGetAccelerationStructureBuildSizesKHR) {
        LOG_ERROR("Failed to load vkGetAccelerationStructureBuildSizesKHR", std::source_location::current());
        throw std::runtime_error("Failed to load vkGetAccelerationStructureBuildSizesKHR");
    }

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .pNext = nullptr,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = vertexBufferAddress_ },
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = vertexCount_ - 1,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = indexBufferAddress_ },
        .transformData = { .deviceAddress = 0 }
    };
    LOG_DEBUG("Triangles setup: maxVertex={}, vertexAddress=0x{:x}, indexAddress=0x{:x}", triangles.maxVertex, triangles.vertexData.deviceAddress, triangles.indexData.deviceAddress);
    if (triangles.maxVertex == UINT32_MAX) {
        LOG_ERROR("Invalid maxVertex={} < 0", triangles.maxVertex);
        throw std::runtime_error("Invalid maxVertex for geometry");
    }
    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = triangles },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries = &geometry,
        .ppGeometries = nullptr,
        .scratchData = { .deviceAddress = 0 }
    };
    uint32_t primitiveCount = indexCount_ / 3;
    LOG_DEBUG("primitiveCount = {}", primitiveCount);
    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    vkGetAccelerationStructureBuildSizesKHR(
        context_.device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildGeometryInfo,
        &primitiveCount,
        &buildSizesInfo
    );
    LOG_INFO(std::format("Calculated scratch buffer size: {}", formatSize(buildSizesInfo.buildScratchSize)));
    LOG_DEBUG("Exiting calculateScratchBufferSize, returning {}", formatSize(buildSizesInfo.buildScratchSize));
    return buildSizesInfo.buildScratchSize;
}

void VulkanBufferManager::prepareDescriptorBufferInfo(std::vector<VkDescriptorBufferInfo>& bufferInfos, uint32_t count) const {
    LOG_DEBUG(std::format("Preparing {} descriptor buffer infos", count));
    if (count > context_.uniformBuffers.size()) {
        LOG_ERROR(std::format("Requested {} descriptor buffer infos, but only {} uniform buffers available", count, context_.uniformBuffers.size()), std::source_location::current());
        throw std::runtime_error("Insufficient uniform buffers for descriptor update");
    }

    bufferInfos.resize(count);
    VkDeviceSize bufferSize = sizeof(UE::UniformBufferObject) + sizeof(int) + UNIFORM_BUFFER_EXTRA_SIZE;
    VkPhysicalDeviceProperties deviceProps{};
    vkGetPhysicalDeviceProperties(context_.physicalDevice, &deviceProps);
    bufferSize = (bufferSize + deviceProps.limits.minUniformBufferOffsetAlignment - 1) & ~(deviceProps.limits.minUniformBufferOffsetAlignment - 1);
    LOG_DEBUG("Uniform buffer size for descriptors: {}", formatSize(bufferSize));

    for (uint32_t i = 0; i < count; ++i) {
        if (context_.uniformBuffers[i] == VK_NULL_HANDLE) {
            LOG_ERROR(std::format("Uniform buffer[{}] is null", i), std::source_location::current());
            throw std::runtime_error("Invalid uniform buffer handle");
        }
        VkMemoryRequirements memReq{};
        vkGetBufferMemoryRequirements(context_.device, context_.uniformBuffers[i], &memReq);
        if (bufferSize > memReq.size || bufferSize == VK_WHOLE_SIZE) {
            LOG_ERROR(std::format("Uniform buffer[{}] range {} exceeds allocated size {}", i, bufferSize, memReq.size), std::source_location::current());
            throw std::runtime_error("Uniform buffer range exceeds allocated size");
        }
        bufferInfos[i] = {
            .buffer = context_.uniformBuffers[i],
            .offset = 0,
            .range = bufferSize
        };
        LOG_INFO(std::format("Prepared descriptor buffer info[{}]: buffer={:p}, offset={}, range={}", 
                             i, static_cast<void*>(bufferInfos[i].buffer), bufferInfos[i].offset, formatSize(bufferInfos[i].range)));
    }
    LOG_DEBUG("Prepared descriptor buffer infos successfully");
}

void VulkanBufferManager::updateDescriptorSet(VkDescriptorSet descriptorSet, uint32_t binding, uint32_t descriptorCount, VkDescriptorType descriptorType) const {
    LOG_DEBUG(std::format("Updating descriptor set {:p} with {} descriptors at binding {}, type={}", static_cast<void*>(descriptorSet), descriptorCount, binding, static_cast<uint32_t>(descriptorType)));
    if (descriptorSet == VK_NULL_HANDLE) {
        LOG_ERROR("Invalid descriptor set provided for update", std::source_location::current());
        throw std::invalid_argument("Descriptor set cannot be null");
    }

    if (descriptorCount > context_.uniformBuffers.size() && descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        LOG_ERROR(std::format("Descriptor update requires {} uniform buffers, but only {} available", descriptorCount, context_.uniformBuffers.size()), std::source_location::current());
        throw std::runtime_error("Insufficient uniform buffers for descriptor update");
    }

    std::vector<VkDescriptorBufferInfo> bufferInfos;
    try {
        prepareDescriptorBufferInfo(bufferInfos, descriptorCount);
    } catch (const std::exception& e) {
        LOG_ERROR(std::format("Failed to prepare descriptor buffer info: {}", e.what()), std::source_location::current());
        throw;
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites;
    descriptorWrites.reserve(descriptorCount);
    for (uint32_t i = 0; i < descriptorCount; ++i) {
        if (bufferInfos[i].buffer == VK_NULL_HANDLE) {
            LOG_ERROR(std::format("Descriptor buffer info[{}] has null buffer", i), std::source_location::current());
            throw std::runtime_error("Invalid buffer in descriptor update");
        }
        descriptorWrites.push_back({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = binding + i,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = descriptorType,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferInfos[i],
            .pTexelBufferView = nullptr
        });
        LOG_DEBUG("Descriptor write[{}]: binding={}, type={}", i, descriptorWrites[i].dstBinding, static_cast<uint32_t>(descriptorWrites[i].descriptorType));
    }

    vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    LOG_INFO(std::format("Updated descriptor set {:p} with {} buffers starting at binding {}, type={}", 
                         static_cast<void*>(descriptorSet), descriptorCount, binding, static_cast<uint32_t>(descriptorType)));
}

VkDeviceMemory VulkanBufferManager::getVertexBufferMemory() const { return context_.vertexBufferMemory; }
VkDeviceMemory VulkanBufferManager::getIndexBufferMemory() const { return context_.indexBufferMemory; }
VkDeviceMemory VulkanBufferManager::getUniformBufferMemory(uint32_t index) const {
    validateUniformBufferIndex(index);
    return context_.uniformBufferMemories[index];
}
VkDeviceAddress VulkanBufferManager::getVertexBufferAddress() const { return vertexBufferAddress_; }
VkDeviceAddress VulkanBufferManager::getIndexBufferAddress() const { return indexBufferAddress_; }
VkDeviceAddress VulkanBufferManager::getScratchBufferAddress() const { return scratchBufferAddress_; }
VkBuffer VulkanBufferManager::getVertexBuffer() const { return context_.vertexBuffer; }
VkBuffer VulkanBufferManager::getIndexBuffer() const { return context_.indexBuffer; }
uint32_t VulkanBufferManager::getVertexCount() const { 
    LOG_DEBUG("getVertexCount() returning {}", vertexCount_);
    return vertexCount_; 
}
uint32_t VulkanBufferManager::getIndexCount() const { 
    LOG_DEBUG("getIndexCount() returning {}", indexCount_);
    return indexCount_; 
}
VkBuffer VulkanBufferManager::getUniformBuffer(uint32_t index) const {
    validateUniformBufferIndex(index);
    return context_.uniformBuffers[index];
}
uint32_t VulkanBufferManager::getUniformBufferCount() const { return static_cast<uint32_t>(context_.uniformBuffers.size()); }

void VulkanBufferManager::validateUniformBufferIndex(uint32_t index) const {
    if (index >= context_.uniformBuffers.size()) {
        LOG_ERROR(std::format("Invalid uniform buffer index: {} (size: {})", index, context_.uniformBuffers.size()), std::source_location::current());
        throw std::out_of_range("Uniform buffer index out of range");
    }
}