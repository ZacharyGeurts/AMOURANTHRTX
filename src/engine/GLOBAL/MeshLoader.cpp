// src/engine/GLOBAL/MeshLoader.cpp
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <tinyobjloader/tiny_obj_loader.h>
#include <unordered_map>
#include <cstring>

using namespace Logging::Color;

using StoneKey::stone_graphics_queue;

namespace MeshLoader {

static void uploadBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage, uint64_t& outHandle)
{
    LOG_INFO_CAT("MeshLoader", "uploadBuffer() START — size: {} bytes | usage: 0x{:X}", size, (uint32_t)usage);

    // 1. Staging buffer (host-visible)
    uint64_t staging = BufferManager::create(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ? "Mesh_Staging_Vertex" : "Mesh_Staging_Index"
    );

    // Map → copy → unmap
    void* mapped = BufferManager::map(staging);
    std::memcpy(mapped, data, size);
    BufferManager::unmap(staging);

    LOG_SUCCESS_CAT("MeshLoader", "Data copied to staging — {} bytes ready for transfer", size);

    // 2. Final device-local buffer
    VkBufferUsageFlags finalUsage = usage
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR
        | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

    const char* finalTag = (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        ? "Mesh_Vertex_Final"
        : "Mesh_Index_Final";

    outHandle = BufferManager::create(size, finalUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, finalTag);

    // 3. One-time copy using StoneKey helpers
    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(RTX::g_ctx().commandPool_);
    VkBufferCopy copy{};
    copy.size = size;

    vkCmdCopyBuffer(cmd,
        BufferManager::getVkBuffer(staging),      // source (staging)
        BufferManager::getVkBuffer(outHandle),   // destination (final)
        1, &copy);

    RTX::endOneTimeSubmit(cmd, stone_graphics_queue(), RTX::g_ctx().commandPool_);

    // 4. Destroy staging buffer immediately
    BufferManager::destroy(staging);

    LOG_SUCCESS_CAT("MeshLoader", "uploadBuffer() COMPLETE — final handle: 0x{:X}", outHandle);
}

std::unique_ptr<Mesh> loadOBJ(const std::string& path)
{
    LOG_ATTEMPT_CAT("MeshLoader", "FORGING COSMIC SCROLL: {}", path);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), "assets/models/")) {
        if (!err.empty())  LOG_FATAL_CAT("TinyObj", "{}", err.c_str());
        if (!warn.empty()) LOG_WARNING_CAT("MeshLoader", "{}", warn);
        return nullptr;
    }

    auto mesh = std::make_unique<Mesh>();
    std::unordered_map<Mesh::Vertex, uint32_t, Mesh::Vertex::Hash> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Mesh::Vertex v{};

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

            auto it = uniqueVertices.find(v);
            if (it == uniqueVertices.end()) {
                uint32_t newIdx = static_cast<uint32_t>(mesh->vertices.size());
                uniqueVertices[v] = newIdx;
                mesh->vertices.push_back(v);
                mesh->indices.push_back(newIdx);
            } else {
                mesh->indices.push_back(it->second);
            }
        }
    }

    LOG_SUCCESS_CAT("MeshLoader", "COSMIC SCROLL PARSED — {} unique vertices, {} indices",
                    mesh->vertices.size(), mesh->indices.size());

    uploadBuffer(mesh->vertices.data(),
                 mesh->vertices.size() * sizeof(Mesh::Vertex),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 mesh->vertexBuffer);

    uploadBuffer(mesh->indices.data(),
                 mesh->indices.size() * sizeof(uint32_t),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 mesh->indexBuffer);

    // Fingerprint remains unchanged
    mesh->stonekey_fingerprint =
        kStone1 ^ kStone2 ^
        std::hash<std::string>{}(path) ^
        mesh->vertices.size() ^ mesh->indices.size() ^
        mesh->vertexBuffer ^ mesh->indexBuffer ^
        0xDEADC0DE1337BABEULL;

    LOG_SUCCESS_CAT("MeshLoader",
        "MESH FORGED — fingerprint 0x{:X} | VB 0x{:X} | IB 0x{:X}",
        mesh->stonekey_fingerprint, mesh->vertexBuffer, mesh->indexBuffer);

    return mesh;
}

} // namespace MeshLoader