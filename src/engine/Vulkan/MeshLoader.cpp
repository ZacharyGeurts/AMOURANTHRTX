// src/engine/Vulkan/MeshLoader.cpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 20, 2025 — APOCALYPSE FINAL v2.0
// MAIN — FIRST LIGHT REBORN — LAS v2.0 VIA VulkanAccel — PINK PHOTONS ETERNAL
// =============================================================================
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 21, 2025 — APOCALYPSE FINAL v10.3
// MESH LOADER — FULLY BULLETPROOF — LOGS EVERYTHING — NO 0x0 — NO FORMAT ERRORS
// PINK PHOTONS ETERNAL — VALHALLA SEALED
// =============================================================================

#include "engine/Vulkan/MeshLoader.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"           // ← brings in beginOneTime() and endSingleTimeCommandsAsync()
#include "engine/GLOBAL/logging.hpp"
#include <tinyobjloader/tiny_obj_loader.h>
#include <unordered_map>
#include <cstring>

using namespace Logging::Color;

namespace MeshLoader {

// =============================================================================
// MESH SAFETY
// =============================================================================
void Mesh::destroy() noexcept {
    LOG_INFO_CAT("MeshLoader", "MESH DESTROY — FINGERPRINT 0x{:016X}", stonekey_fingerprint);
    BUFFER_DESTROY(vertexBuffer);
    BUFFER_DESTROY(indexBuffer);
    stonekey_fingerprint = 0xDEADDEADBEEF1337ULL;
    LOG_SUCCESS_CAT("MeshLoader", "MESH SACRIFICED — RETURNED TO THE VOID");
}

VkBuffer Mesh::getVertexBuffer() const noexcept {
    if (stonekey_fingerprint == 0 || stonekey_fingerprint == 0xDEADDEADBEEF1337ULL) {
        LOG_FATAL_CAT("MeshLoader", "STONEKEY BREACH: Accessing destroyed vertex buffer");
        std::abort();
    }
    VkBuffer buf = RAW_BUFFER(vertexBuffer);
    LOG_DEBUG_CAT("MeshLoader", "getVertexBuffer() → obf 0x{:016X} → raw 0x{:016X}", vertexBuffer, (uint64_t)buf);
    return buf;
}

VkBuffer Mesh::getIndexBuffer() const noexcept {
    if (stonekey_fingerprint == 0 || stonekey_fingerprint == 0xDEADDEADBEEF1337ULL) {
        LOG_FATAL_CAT("MeshLoader", "STONEKEY BREACH: Accessing destroyed index buffer");
        std::abort();
    }
    VkBuffer buf = RAW_BUFFER(indexBuffer);
    LOG_DEBUG_CAT("MeshLoader", "getIndexBuffer() → obf 0x{:016X} → raw 0x{:016X}", indexBuffer, (uint64_t)buf);
    return buf;
}

// =============================================================================
// BULLETPROOF UPLOAD — NO VkBuffer IN LOGS → USE uint64_t INSTEAD
// =============================================================================
static void uploadBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage, uint64_t& outHandle)
{
    LOG_INFO_CAT("MeshLoader", "uploadBuffer() START — size: {} bytes | usage: 0x{:X}", size, (uint32_t)usage);

    if (size == 0) {
        LOG_ERROR_CAT("MeshLoader", "uploadBuffer called with zero size — refusing");
        outHandle = 0;
        return;
    }

    auto& tracker = RTX::UltraLowLevelBufferTracker::get();

    uint64_t staging = 0;
    const char* tag = (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ? "Mesh_Staging_Vertex" : "Mesh_Staging_Index";

    BUFFER_CREATE(staging, size,
              VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              tag);

    void* mapped = tracker.map(staging);
    std::memcpy(mapped, data, size);
    tracker.unmap(staging);
    LOG_SUCCESS_CAT("MeshLoader", "Staging buffer filled — {} bytes copied", size);

    BUFFER_CREATE(outHandle, size,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ? "Mesh_Vertex_Final" : "Mesh_Index_Final");

    LOG_INFO_CAT("MeshLoader", "Copying staging → final: 0x{:016X} → 0x{:016X}", staging, outHandle);

    // NOW USING beginOneTime / endSingleTimeCommandsAsync FROM LAS.hpp
    VkCommandBuffer cmd = beginOneTime(g_ctx().commandPool_);

    VkBufferCopy copy{ .size = size };
    vkCmdCopyBuffer(cmd, RAW_BUFFER(staging), RAW_BUFFER(outHandle), 1, &copy);

    endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, g_ctx().commandPool_);

    BUFFER_DESTROY(staging);

    LOG_SUCCESS_CAT("MeshLoader", "uploadBuffer() COMPLETE — final handle: 0x{:016X} → raw buffer: 0x{:016X}",
                    outHandle, (uint64_t)RAW_BUFFER(outHandle));
}

// =============================================================================
// LOAD OBJ — FINAL VERSION — LOGS EVERYTHING
// =============================================================================
std::unique_ptr<Mesh> loadOBJ(const std::string& path)
{
    LOG_ATTEMPT_CAT("MeshLoader", "LOADING OBJ: {}", path);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), "assets/models/")) {
        if (!err.empty()) throw std::runtime_error("TinyObjLoader: " + err);
        if (!warn.empty()) LOG_WARNING_CAT("MeshLoader", "TinyObj warning: {}", warn);
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

    // VERTEX BUFFER
    LOG_ATTEMPT_CAT("MeshLoader", "UPLOADING VERTEX BUFFER — {} bytes", mesh->vertices.size() * sizeof(Mesh::Vertex));
    uploadBuffer(mesh->vertices.data(),
                 mesh->vertices.size() * sizeof(Mesh::Vertex),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 mesh->vertexBuffer);
    LOG_SUCCESS_CAT("MeshLoader", "VERTEX BUFFER READY — handle 0x{:016X}", mesh->vertexBuffer);

    // INDEX BUFFER
    LOG_ATTEMPT_CAT("MeshLoader", "UPLOADING INDEX BUFFER — {} bytes", mesh->indices.size() * sizeof(uint32_t));
    uploadBuffer(mesh->indices.data(),
                 mesh->indices.size() * sizeof(uint32_t),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 mesh->indexBuffer);
    LOG_SUCCESS_CAT("MeshLoader", "INDEX BUFFER READY — handle 0x{:016X}", mesh->indexBuffer);

    // FINAL FINGERPRINT
    mesh->stonekey_fingerprint =
        kStone1() ^ kStone2() ^
        std::hash<std::string>{}(path) ^
        mesh->vertices.size() ^ mesh->indices.size() ^
        mesh->vertexBuffer ^ mesh->indexBuffer;

    LOG_SUCCESS_CAT("MeshLoader",
        "MESH FULLY STONEKEYED v∞ — FINGERPRINT 0x{:016X}\n"
        "    Vertex Buffer: 0x{:016X}\n"
        "    Index Buffer : 0x{:016X}\n"
        "    PINK PHOTONS BOUND — FIRST LIGHT ETERNAL",
        mesh->stonekey_fingerprint,
        mesh->vertexBuffer,
        mesh->indexBuffer);

    return mesh;
}

} // namespace MeshLoader