// src/engine/Vulkan/MeshLoader.cpp
// MESH LOADER — FULL STONEKEY v∞ — PINK PHOTONS ETERNAL
#include "engine/Vulkan/MeshLoader.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <tinyobjloader/tiny_obj_loader.h>
#include <unordered_map>

using namespace Logging::Color;

namespace MeshLoader {

// === NESTED DEFINITIONS OUTSIDE (required for templates) ===
bool Mesh::Vertex::operator==(const Vertex& o) const {
    return pos == o.pos && normal == o.normal && uv == o.uv;
}

std::size_t Mesh::Vertex::Hash::operator()(const Vertex& v) const noexcept {
    std::size_t h1 = std::hash<float>{}(v.pos.x) ^ std::hash<float>{}(v.pos.y) ^ std::hash<float>{}(v.pos.z);
    std::size_t h2 = std::hash<float>{}(v.normal.x) ^ std::hash<float>{}(v.normal.y) ^ std::hash<float>{}(v.normal.z);
    std::size_t h3 = std::hash<float>{}(v.uv.x) ^ std::hash<float>{}(v.uv.y);
    return h1 ^ (h2 << 1) ^ (h3 << 2);
}

void Mesh::destroy() noexcept {
    BUFFER_DESTROY(vertexBuffer);
    BUFFER_DESTROY(indexBuffer);
}

VkBuffer Mesh::getVertexBuffer() const noexcept { return RAW_BUFFER(vertexBuffer); }
VkBuffer Mesh::getIndexBuffer()  const noexcept { return RAW_BUFFER(indexBuffer);  }

// === UPLOAD HELPER ===
static void uploadBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage, uint64_t& handle)
{
    BUFFER_CREATE(handle, size,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ? "Mesh_Vertex" : "Mesh_Index");

    uint64_t staging = 0;
    BUFFER_CREATE(staging, size,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "Mesh_Staging");

    void* mapped = RTX::UltraLowLevelBufferTracker::get().map(staging);
    memcpy(mapped, data, size);
    RTX::UltraLowLevelBufferTracker::get().unmap(staging);

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = RTX::g_ctx().commandPool(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(g_device(), &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy copy{ .size = size };
    vkCmdCopyBuffer(cmd, RAW_BUFFER(staging), RAW_BUFFER(handle), 1, &copy);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    vkQueueSubmit(RTX::g_ctx().graphicsQueue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(RTX::g_ctx().graphicsQueue());

    vkFreeCommandBuffers(g_device(), RTX::g_ctx().commandPool(), 1, &cmd);
    BUFFER_DESTROY(staging);
}

// === MAIN LOADER ===
std::unique_ptr<Mesh> loadOBJ(const std::string& path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), "assets/models/")) {
        if (!err.empty()) throw std::runtime_error("TinyObjLoader: " + err);
        if (!warn.empty()) LOG_WARN_CAT("MeshLoader", "{}", warn);
    }

    auto mesh = std::make_unique<Mesh>();

    // ← FULLY QUALIFIED TYPES — THIS WAS THE ERROR
    using Vertex = Mesh::Vertex;
    std::unordered_map<Vertex, uint32_t, Vertex::Hash> uniqueVertices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex v{};

            v.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.normal_index >= 0) {
                v.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }

            if (index.texcoord_index >= 0) {
                v.uv = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }

            if (uniqueVertices.find(v) == uniqueVertices.end()) {
                uint32_t newIndex = static_cast<uint32_t>(mesh->vertices.size());
                uniqueVertices[v] = newIndex;
                mesh->vertices.push_back(v);
            }
            mesh->indices.push_back(uniqueVertices[v]);
        }
    }

    LOG_SUCCESS_CAT("MeshLoader", "Loaded {} → {} verts, {} indices — STONEKEY v∞ ACTIVE",
                    path, mesh->vertices.size(), mesh->indices.size());

    uploadBuffer(mesh->vertices.data(),
                 mesh->vertices.size() * sizeof(Vertex),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 mesh->vertexBuffer);

    uploadBuffer(mesh->indices.data(),
                 mesh->indices.size() * sizeof(uint32_t),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 mesh->indexBuffer);

    return mesh;
}

} // namespace MeshLoader