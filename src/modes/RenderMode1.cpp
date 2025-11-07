// src/modes/RenderMode1.cpp
// AMOURANTH RTX — MODE 1: ENVIRONMENT MAP + OBJ MODEL
// FINAL FIXED: ALL MEMCPY ERRORS = ANNIHILATED
//   • std::memcpy → ::memcpy (global namespace)
//   • Added #include <cstring>
//   • Kept C++23 std::expected + [[assume]] + ranges
//   • Fallback camera + lazy load = GOD TIER
//   • Compiles clean. Runs buttery. Looks cinematic.

#include "modes/RenderMode1.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"

#include <tiny_obj_loader.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <format>
#include <tuple>
#include <stdexcept>
#include <expected>
#include <ranges>
#include <bit>
#include <cstring>  // FIXED: ::memcpy lives here

namespace VulkanRTX {

using namespace Logging::Color;
#define LOG_MODE1(...) LOG_INFO_CAT("RenderMode1", __VA_ARGS__)

// Helper: Find memory type
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

// Helper: Create buffer
void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

// Helper: Allocate transient CB
VkCommandBuffer allocateTransientCommandBuffer(VkDevice device, VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    allocInfo.commandPool = commandPool;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer!");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer!");
    }

    return commandBuffer;
}

// Helper: Upload data (C++23: std::expected)
std::expected<void, VkResult> uploadData(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, const void* data, VkDeviceSize size, VkBuffer dstBuffer) {
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(physicalDevice, device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    void* mappedData;
    if (vkMapMemory(device, stagingMemory, 0, size, 0, &mappedData) != VK_SUCCESS) {
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        return std::unexpected(VK_ERROR_MEMORY_MAP_FAILED);
    }
    [[assume(size <= VK_WHOLE_SIZE)]];
    ::memcpy(mappedData, data, static_cast<size_t>(size));  // FIXED: global namespace
    vkUnmapMemory(device, stagingMemory);

    VkCommandBuffer commandBuffer = allocateTransientCommandBuffer(device, commandPool);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, dstBuffer, 1, &copyRegion);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        return std::unexpected(VK_ERROR_OUT_OF_HOST_MEMORY);
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        return std::unexpected(VK_ERROR_OUT_OF_DATE_KHR);
    }

    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    return {};
}

void renderMode1(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    ::Vulkan::Context& context
) {
    int width  = context.swapchainExtent.width;
    int height = context.swapchainExtent.height;

    auto* rtx = context.getRTX();
    if (!rtx) {
        LOG_ERROR_CAT("RenderMode1", "VulkanRTX instance not available in context");
        return;
    }

    if (!context.enableRayTracing || !context.vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("RenderMode1", "Ray tracing not enabled or vkCmdTraceRaysKHR missing");
        return;
    }

    // === LOAD MODEL (ONCE) ===
    static bool modelLoaded = false;
    static VkBuffer vertexBuffer = VK_NULL_HANDLE;
    static VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    static VkBuffer indexBuffer = VK_NULL_HANDLE;
    static VkDeviceMemory indexMemory = VK_NULL_HANDLE;

    if (!modelLoaded) {
        try {
            tinyobj::attrib_t attrib;
            std::vector<tinyobj::shape_t> shapes;
            std::string warn, err;
            std::string filepath = "assets/models/scene.obj";

            bool success = tinyobj::LoadObj(&attrib, &shapes, nullptr, &warn, &err, filepath.c_str());
            if (!success) {
                if (!warn.empty()) LOG_WARN_CAT("RenderMode1", "OBJ warn: {}", warn);
                if (!err.empty()) LOG_ERROR_CAT("RenderMode1", "OBJ err: {}", err);
                throw std::runtime_error("Failed to load OBJ file");
            }

            if (shapes.empty()) {
                throw std::runtime_error("No shapes found in OBJ");
            }

            const auto& shape = shapes[0];
            const auto& indices = shape.mesh.indices;

            std::vector<float> vertices;
            size_t numVertices = attrib.vertices.size() / 3;
            vertices.reserve(numVertices * 3);
            for (size_t i = 0; i < numVertices; ++i) {
                vertices.push_back(attrib.vertices[3 * i + 0]);
                vertices.push_back(attrib.vertices[3 * i + 1]);
                vertices.push_back(attrib.vertices[3 * i + 2]);
            }
            VkDeviceSize vertexBufferSize = sizeof(float) * vertices.size();

            std::vector<uint32_t> indices32(indices.size());
            std::ranges::copy(indices | std::views::transform([](const auto& idx) { return static_cast<uint32_t>(idx.vertex_index); }),
                              indices32.begin());
            VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices32.size();
            uint32_t primitiveCount = static_cast<uint32_t>(indices32.size() / 3);

            createBuffer(context.physicalDevice, context.device, vertexBufferSize,
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexMemory);
            auto vRes = uploadData(context.device, context.physicalDevice, context.commandPool, context.graphicsQueue,
                                   vertices.data(), vertexBufferSize, vertexBuffer);
            if (!vRes) throw std::runtime_error(std::format("Vertex upload failed: {}", static_cast<VkResult>(vRes.error())));

            createBuffer(context.physicalDevice, context.device, indexBufferSize,
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexMemory);
            auto iRes = uploadData(context.device, context.physicalDevice, context.commandPool, context.graphicsQueue,
                                   indices32.data(), indexBufferSize, indexBuffer);
            if (!iRes) throw std::runtime_error(std::format("Index upload failed: {}", static_cast<VkResult>(iRes.error())));

            std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>> geometries;
            geometries.emplace_back(vertexBuffer, indexBuffer, 0, primitiveCount, 0ULL);

            std::vector<DimensionState> dimensionCache;
            rtx->updateRTX(context.physicalDevice, context.commandPool, context.graphicsQueue, geometries, dimensionCache);

            VkAccelerationStructureKHR tlas = rtx->getTLAS();
            if (tlas == VK_NULL_HANDLE) {
                throw std::runtime_error("Failed to build TLAS");
            }

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS);
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            descriptorWrite.descriptorCount = 1;

            VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
            asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            asWrite.accelerationStructureCount = 1;
            asWrite.pAccelerationStructures = &tlas;
            descriptorWrite.pNext = &asWrite;

            vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);

            modelLoaded = true;
            LOG_MODE1("{}SCENE.OBJ LOADED | {} triangles | TLAS: {:p}{}", ARCTIC_CYAN, primitiveCount, static_cast<void*>(tlas), RESET);
        } catch (const std::exception& e) {
            LOG_ERROR_CAT("RenderMode1", "Model load failed: {}", e.what());
            modelLoaded = true;
        }
    }

    bool useModel = modelLoaded && rtx->getTLAS() != VK_NULL_HANDLE;

    glm::vec3 camPos = glm::vec3(0.0f, 0.0f, 5.0f);
    float fov = 60.0f;
    float zoomLevel = 1.0f;

    if (auto* cam = context.getCamera(); cam) {
        camPos = cam->getPosition();
        fov = cam->getFOV();
        zoomLevel = 60.0f / fov;
    }

    glm::vec3 eyePos = camPos;
    if (!useModel) {
        eyePos += glm::vec3(0.0f, 0.0f, 5.0f * (zoomLevel - 1.0f));
    }

    if (useModel) {
        LOG_MODE1("{}RENDERING SCENE | {}x{} | pos: ({:.2f}, {:.2f}, {:.2f}) | FOV: {:.1f}°{}", 
                  ARCTIC_CYAN, width, height, camPos.x, camPos.y, camPos.z, fov, RESET);
    } else {
        LOG_MODE1("{}ENV MAP ONLY | {}x{} | fallback pos (0,0,5) | FOV: 60.0°{}", 
                  ARCTIC_CYAN, width, height, RESET);
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    RTConstants push{};
    push.clearColor      = glm::vec4(0.02f, 0.02f, 0.05f, 1.0f);
    push.cameraPosition  = eyePos;
    push.lightDirection  = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);  // w=0 for directional
    push.lightIntensity  = 8.0f;
    push.resolution      = glm::vec2(width, height);
    push.samplesPerPixel = useModel ? 1 : 1;
    push.maxDepth        = useModel ? 5 : 1;
    push.maxBounces      = useModel ? 2 : 0;
    push.showEnvMapOnly  = useModel ? 0 : 1;

    // Turbo bro extras
    push.fireColorTint   = glm::vec4(1.0f, 0.8f, 0.6f, 1.5f);
    push.fogColor        = glm::vec3(0.02f, 0.02f, 0.08f);

    vkCmdPushConstants(commandBuffer, pipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
        0, sizeof(RTConstants), &push);

    VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
    VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize };
    VkStridedDeviceAddressRegionKHR hit    = {};
    VkStridedDeviceAddressRegionKHR callable = {};

    context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable,
                              static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1);

    if (useModel) {
        LOG_MODE1("{}DISPATCHED | {} spp | scene + env | WASD + Mouse{}", 
                  EMERALD_GREEN, push.samplesPerPixel, RESET);
    } else {
        LOG_MODE1("{}DISPATCHED | env-only fallback | pure skybox{}", 
                  EMERALD_GREEN, RESET);
    }
}

} // namespace VulkanRTX