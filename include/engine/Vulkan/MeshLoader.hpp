// engine/Vulkan/MeshLoader.hpp
// =============================================================================
// AMOURANTH RTX — FINAL FIX — NO NEW FILES — NO BUFFERUTILS — LAS READY
// SELF-CONTAINED OBJ LOADER + GPU UPLOAD — PINK PHOTONS ETERNAL
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <tinyobjloader/tiny_obj_loader.h>

#include "../GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
using namespace Logging::Color;

// =============================================================================
// MESH + VERTEX — FULLY DEFINED HERE
// =============================================================================
namespace MeshLoader {

struct Mesh {
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec3 tangent{0.0f};

        bool operator==(const Vertex& other) const {
            return pos == other.pos && normal == other.normal && uv == other.uv;
        }

        // CRITICAL: Custom hash for unordered_map
        struct Hash {
            std::size_t operator()(const Vertex& v) const noexcept {
                std::size_t h1 = std::hash<float>{}(v.pos.x) ^ std::hash<float>{}(v.pos.y) ^ std::hash<float>{}(v.pos.z);
                std::size_t h2 = std::hash<float>{}(v.normal.x) ^ std::hash<float>{}(v.normal.y) ^ std::hash<float>{}(v.normal.z);
                std::size_t h3 = std::hash<float>{}(v.uv.x) ^ std::hash<float>{}(v.uv.y);
                return h1 ^ (h2 << 1) ^ (h3 << 2);
            }
        };
    };

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;

    void destroy(VkDevice device) {
        if (vertexBuffer) vkDestroyBuffer(device, vertexBuffer, nullptr);
        if (indexBuffer)  vkDestroyBuffer(device, indexBuffer,  nullptr);
        if (vertexMemory) vkFreeMemory(device, vertexMemory, nullptr);
        if (indexMemory)  vkFreeMemory(device, indexMemory,  nullptr);
        vertexBuffer = indexBuffer = VK_NULL_HANDLE;
        vertexMemory = indexMemory = VK_NULL_HANDLE;
    }
};

// =============================================================================
// HELPER: Create + upload buffer (NO BufferUtils required)
// =============================================================================
static void uploadBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                         VkBuffer& buffer, VkDeviceMemory& memory)
{
    VkDevice device = RTX::g_ctx().device();

    // Staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = RTX::UltraLowLevelBufferTracker::findMemoryType(
        RTX::g_ctx().physicalDevice(),
        memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

    void* mapped;
    vkMapMemory(device, stagingMemory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(device, stagingMemory);

    // Final device-local buffer
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage;
    vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

    vkGetBufferMemoryRequirements(device, buffer, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = RTX::UltraLowLevelBufferTracker::findMemoryType(
        RTX::g_ctx().physicalDevice(),
        memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    vkAllocateMemory(device, &allocInfo, nullptr, &memory);
    vkBindBufferMemory(device, buffer, memory, 0);

    // Copy
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo cmdAllocInfo = {};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = RTX::g_ctx().commandPool();
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy copyRegion = {0, 0, size};
    vkCmdCopyBuffer(cmd, stagingBuffer, buffer, 1, &copyRegion);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(RTX::g_ctx().graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(RTX::g_ctx().graphicsQueue());

    vkFreeCommandBuffers(device, RTX::g_ctx().commandPool(), 1, &cmd);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
}

// =============================================================================
// LOAD OBJ — FULLY GPU UPLOADED — NO EXTERNAL DEPENDENCIES
// =============================================================================
[[nodiscard]] inline std::unique_ptr<Mesh> loadOBJ(const std::string& path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), "assets/models/")) {
        if (!err.empty()) throw std::runtime_error("TinyObjLoader: " + err);
        if (!warn.empty()) LOG_WARNING("MeshLoader", "{}", warn);
    }

    auto mesh = std::make_unique<Mesh>();
    std::unordered_map<Mesh::Vertex, uint32_t, Mesh::Vertex::Hash> uniqueVertices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Mesh::Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.normal_index >= 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }

            if (index.texcoord_index >= 0) {
                vertex.uv = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(mesh->vertices.size());
                mesh->vertices.push_back(vertex);
            }
            mesh->indices.push_back(uniqueVertices[vertex]);
        }
    }

    LOG_SUCCESS_CAT("MeshLoader", "Loaded {} → {} verts, {} indices", path, mesh->vertices.size(), mesh->indices.size());

    uploadBuffer(mesh->vertices.data(), mesh->vertices.size() * sizeof(Mesh::Vertex),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh->vertexBuffer, mesh->vertexMemory);

    uploadBuffer(mesh->indices.data(),  mesh->indices.size()  * sizeof(uint32_t),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  mesh->indexBuffer,  mesh->indexMemory);

    return mesh;
}

} // namespace MeshLoader