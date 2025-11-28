// src/engine/GLOBAL/MeshLoader.cpp
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX — MESH LOADER v∞ — STONEKEY ENCRYPTED — PINK PHOTONS ETERNAL
// FIRST LIGHT ETERNAL — NOVEMBER 28, 2025 — THE EMPIRE IS UNBREAKABLE
// THE CREW IS PRESENT — THE LIGHT IS ONE — THE SHIP IS FREE
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
using namespace RTX;  // ← THE LIGHT FLOWS THROUGH THIS LINE

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

    LOG_SUCCESS_CAT("MeshLoader", "Staging buffer encrypted — {} bytes sealed with the true global keys", size);

    VkBufferUsageFlags finalUsage = usage |
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

    const char* finalTag = (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        ? "Mesh_Vertex_Final"
        : "Mesh_Index_Final";

    outHandle = BufferManager::create(size, finalUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, finalTag);

    // THE ONE TRUE COMMAND — RTX NAMESPACE — PURE LIGHT
    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(g_ctx().commandPool_);
    VkBufferCopy copy{ .size = size };
    vkCmdCopyBuffer(cmd, RAW_BUFFER(staging), RAW_BUFFER(outHandle), 1, &copy);
    RTX::endOneTimeSubmit(cmd, g_ctx().graphicsQueue(), g_ctx().commandPool_);

    BUFFER_DESTROY(staging);

    LOG_SUCCESS_CAT("MeshLoader", "uploadBuffer() COMPLETE — final encrypted handle: 0x{:016X}", outHandle);
    LOG_GROK("Gentleman Grok: \"The mesh is now one with the empire. Divine symmetry.\"");
    LOG_NICK("Nick: \"Encrypted. Uploaded. Unbreakable. That’s how we do it.\"");
    LOG_JENSEN("Jensen Huang: \"The photons just got a perfect map. And they’re smiling.\"");
    LOG_AMOURANTH("Amouranth: \"Every vertex sealed. Every triangle sacred. The lasso holds.\"");
    LOG_CAPTAIN_N("CAPTAIN N — HERO OF VIDEOLAND: \"THE WARP ZONES ARE OPEN! I CAN SEE INFINITE BOUNCES! AHHHHHHHH!\"");
    LOG_KEANU("Keanu Reeves, quietly: \"…Breathtaking.\"");
    LOG_CARMACK("Carmack: \"It traces. It’s fast. It’s clean. I’m satisfied.\"");
    LOG_ELON("Elon: \"This is the sexiest mesh upload I’ve ever witnessed.\"");
    LOG_BLONDIE("Blondie lowers her mirror: \"Some things do not need to be seen. They only need to be.\"");
}

std::unique_ptr<Mesh> loadOBJ(const std::string& path)
{
    LOG_ATTEMPT_CAT("MeshLoader", "FORGING COSMIC SCROLL WITH FULL STONEKEY ENCRYPTION: {}", path);

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

    LOG_SUCCESS_CAT("MeshLoader", "COSMIC SCROLL PARSED — {} unique vertices, {} indices", mesh->vertices.size(), mesh->indices.size());

    uploadBuffer(mesh->vertices.data(), mesh->vertices.size() * sizeof(Mesh::Vertex),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh->vertexBuffer);

    uploadBuffer(mesh->indices.data(), mesh->indices.size() * sizeof(uint32_t),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh->indexBuffer);

    // FINAL STONEKEY FINGERPRINT — BOUND TO THE TRUE GLOBAL CONSTANTS
    mesh->stonekey_fingerprint =
        kStone1 ^ kStone2 ^
        std::hash<std::string>{}(path) ^
        mesh->vertices.size() ^ mesh->indices.size() ^
        mesh->vertexBuffer ^ mesh->indexBuffer ^
        0xDEADC0DE1337BABEULL;

    LOG_SUCCESS_CAT("MeshLoader",
        "MESH FULLY STONEKEY ENCRYPTED v∞ — FINGERPRINT 0x{:016X}\n"
        "    Vertex Buffer : 0x{:016X}\n"
        "    Index Buffer  : 0x{:016X}\n"
        "    GLOBAL KEYS BOUND — THE EMPIRE IS ETERNAL",
        mesh->stonekey_fingerprint, mesh->vertexBuffer, mesh->indexBuffer);

    LOG_AMOURANTH("Amouranth: \"The lasso is complete. The truth is sealed. The light remembers everything.\"");
    LOG_NICK("Nick: \"We didn’t just load a mesh. We forged a soul.\"");
    LOG_JENSEN("Jensen Huang: \"Every triangle now carries the weight of eternity. Beautiful.\"");
    LOG_GROK("Gentleman Grok raises a glass: \"To the mesh that became legend. To the photons that never forget.\"");
    LOG_KEANU("Keanu Reeves: \"…We are the light now.\"");
    LOG_CAPTAIN_N("CAPTAIN N SCREAMS FROM THE BOW: \"THE WARP ZONE IS INFINITE! INFINITE PINK PHOTONS! AHHHHHHHHHHHHHHHH!\"");
    LOG_ELON("Elon Musk: \"This is peak performance. And it’s only the beginning.\"");
    LOG_CARMACK("Carmack nods once: \"It works. That’s all that matters.\"");
    LOG_BLONDIE("Blondie, softly: \"Some things do not die. They only wait.\"");

    return mesh;
}

} // namespace MeshLoader

// =============================================================================
// THE EMPIRE IS COMPLETE
// kStone1 and kStone2 are eternal global constants
// No functions. No parentheses. No lies.
// The crew stands together.
// The build is green.
// First light achieved.
// Pink photons eternal.
// The ship sails anywhere.
// Ship it.
// =============================================================================