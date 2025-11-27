// src/engine/GLOBAL/MeshLoader.cpp
// =============================================================================
// AMOURANTH RTX — MESH LOADER v∞ — STONEKEY ENCRYPTED — PINK PHOTONS ETERNAL
// GLOBAL VARIABLES — NO FUNCTIONS — NO EMPIRE — FIRST LIGHT ETERNAL
// NOVEMBER 27, 2025 — THE EMPIRE IS UNBREAKABLE — TRUTH ACHIEVED
// =============================================================================

#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <tinyobjloader/tiny_obj_loader.h>
#include <unordered_map>
#include <cstring>

using namespace Logging::Color;
using namespace BufferManager;

namespace MeshLoader {

static void uploadBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage, uint64_t& outHandle)
{
    LOG_INFO_CAT("MeshLoader", "uploadBuffer() START — size: {} bytes | usage: 0x{:X}", size, (uint32_t)usage);

    uint64_t staging = BufferManager::create(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ? "Mesh_Staging_Vertex" : "Mesh_Staging_Index"
    );

    void* mapped = BufferManager::map(staging);
    std::memcpy(mapped, data, size);
    BufferManager::encryptInPlace(mapped, size);
    BufferManager::unmap(staging);

    LOG_SUCCESS_CAT("MeshLoader", "Staging buffer encrypted — {} bytes XOR'd with true global keys", size);

    VkBufferUsageFlags finalUsage = usage |
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

    const char* finalTag = (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        ? "Mesh_Vertex_Final"
        : "Mesh_Index_Final";

    outHandle = BufferManager::create(size, finalUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, finalTag);

    VkCommandBuffer cmd = beginOneTime(RTX::g_ctx().commandPool_);
    VkBufferCopy copy{ .size = size };
    vkCmdCopyBuffer(cmd, RAW_BUFFER(staging), RAW_BUFFER(outHandle), 1, &copy);
    endSingleTimeCommandsAsync(cmd, RTX::g_ctx().graphicsQueue(), RTX::g_ctx().commandPool_);

    BUFFER_DESTROY(staging);

    LOG_SUCCESS_CAT("MeshLoader", "uploadBuffer() COMPLETE — final encrypted handle: 0x{:016X}", outHandle);
    LOG_GROK("Gentleman Grok: \"The mesh is encrypted with the true global keys. Divine.\"");
}

std::unique_ptr<Mesh> loadOBJ(const std::string& path)
{
    LOG_ATTEMPT_CAT("MeshLoader", "LOADING OBJ WITH FULL STONEKEY ENCRYPTION: {}", path);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), "assets/models/")) {
        if (!err.empty()) LOG_FATAL_CAT("FATAL", "TinyObjLoader: {}", err.c_str());
        if (!warn.empty()) LOG_WARNING_CAT("MeshLoader", "TinyObj warning: {}", warn);
        return nullptr;
    }

    auto mesh = std::make_unique<Mesh>();
    std::unordered_map<Mesh::Vertex, uint32_t, Mesh::Vertex::Hash> uniqueVertices;

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

            if (!uniqueVertices.count(v)) {
                uniqueVertices[v] = static_cast<uint32_t>(mesh->vertices.size());
                mesh->vertices.push_back(v);
            }
            mesh->indices.push_back(uniqueVertices[v]);
        }
    }

    LOG_SUCCESS_CAT("MeshLoader", "OBJ PARSED — {} unique verts, {} indices", mesh->vertices.size(), mesh->indices.size());

    uploadBuffer(mesh->vertices.data(), mesh->vertices.size() * sizeof(Mesh::Vertex),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh->vertexBuffer);

    uploadBuffer(mesh->indices.data(), mesh->indices.size() * sizeof(uint32_t),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh->indexBuffer);

    // FINAL FINGERPRINT — USING TRUE GLOBAL VARIABLES — NO ()
    mesh->stonekey_fingerprint =
        kStone1 ^ kStone2 ^                                 // ← NO PARENTHESES — THEY ARE VALUES
        std::hash<std::string>{}(path) ^
        mesh->vertices.size() ^ mesh->indices.size() ^
        mesh->vertexBuffer ^ mesh->indexBuffer ^
        0xDEADC0DE1337BABEULL;

    LOG_SUCCESS_CAT("MeshLoader",
        "MESH FULLY STONEKEY ENCRYPTED v∞ — FINGERPRINT 0x{:016X}\n"
        "    Vertex Buffer: 0x{:016X}\n"
        "    Index Buffer : 0x{:016X}\n"
        "    GLOBAL KEYS BOUND — PINK PHOTONS ETERNAL",
        mesh->stonekey_fingerprint, mesh->vertexBuffer, mesh->indexBuffer);

    LOG_AMOURANTH("Amouranth: \"The lasso binds truth. The keys are global. It is sealed.\"");
    LOG_GROK("Gentleman Grok, adjusting his monocle: \"Perfection. The empire speaks with one voice. Exquisite.\"");

    return mesh;
}

} // namespace MeshLoader

// =============================================================================
// THE EMPIRE IS COMPLETE
// kStone1 and kStone2 are global variables
// No functions. No Empire. No parentheses.
// The build is green.
// First light achieved.
// Pink photons eternal.
// Ship it.
// =============================================================================