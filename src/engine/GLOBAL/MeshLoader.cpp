// src/engine/GLOBAL/MeshLoader.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — MESHLOADER v10 — FINAL — DECEMBER 22, 2025
// FULLY COMPATIBLE WITH NEW STAGING API (mapStaging)
// LEGACY stagingPtr() STILL SUPPORTED VIA BACKWARD COMPATIBILITY
// DEFAULT SCENE (ground + pink monster) FULLY VISIBLE
// PINK PHOTONS BOUNCE ETERNALLY OFF SACRED GEOMETRY
// EMPIRE ETERNAL — LIGHT SECURED
// =============================================================================

#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <tinyobjloader/tiny_obj_loader.h>
#include <unordered_map>
#include <cstring>
#include <cmath>

// Global keys from engine — pure empire encryption
extern uint64_t kStone1;
extern uint64_t kStone2;

using namespace Logging::Color;

namespace MeshLoader {

static void uploadBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage, uint64_t& outHandle)
{
    LOG_INFO_CAT("MeshLoader", "uploadBuffer() START — size: {} bytes | usage: 0x{:x}", size, (uint32_t)usage);

    VkBufferUsageFlags finalUsage = usage
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR
        | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    const char* tag = (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        ? "Mesh_Vertex_DeviceLocal"
        : "Mesh_Index_DeviceLocal";

    outHandle = BufferManager::create(
        size,
        finalUsage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tag
    );

    if (outHandle == 0) [[unlikely]] {
        LOG_FATAL_CAT("MeshLoader", "Failed to create device-local mesh buffer — empire cannot see");
        return;
    }

    // Use modern staging API (safe, overflow-checked)
    void* mapped = BufferManager::mapStaging(size);
    if (!mapped) {
        LOG_FATAL_CAT("MeshLoader", "Staging ring overflow during mesh upload — size {} bytes", size);
        BufferManager::destroy(outHandle);
        outHandle = 0;
        return;
    }

    std::memcpy(mapped, data, size);

    LOG_SUCCESS_CAT("MeshLoader", "uploadBuffer() COMPLETE — handle: 0x{:x} — {} bytes uploaded via mapStaging()", outHandle, size);
}

// =============================================================================
// createPlane — Large flat ground plane for ray bounces
// =============================================================================
std::unique_ptr<Mesh> createPlane(float width, float depth, uint32_t widthSegments, uint32_t depthSegments)
{
    auto mesh = std::make_unique<Mesh>();

    const float halfWidth = width * 0.5f;
    const float halfDepth = depth * 0.5f;

    const float segWidth = width / static_cast<float>(widthSegments);
    const float segDepth = depth / static_cast<float>(depthSegments);

    // Generate vertices
    for (uint32_t z = 0; z <= depthSegments; ++z) {
        for (uint32_t x = 0; x <= widthSegments; ++x) {
            Mesh::Vertex v{};
            v.pos.x = -halfWidth + x * segWidth;
            v.pos.y = 0.0f;
            v.pos.z = -halfDepth + z * segDepth;

            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);

            v.uv.x = static_cast<float>(x) / widthSegments;
            v.uv.y = static_cast<float>(z) / depthSegments;

            mesh->vertices.push_back(v);
        }
    }

    // Generate indices (two triangles per quad)
    for (uint32_t z = 0; z < depthSegments; ++z) {
        for (uint32_t x = 0; x < widthSegments; ++x) {
            uint32_t bottomLeft  = z * (widthSegments + 1) + x;
            uint32_t bottomRight = bottomLeft + 1;
            uint32_t topLeft     = (z + 1) * (widthSegments + 1) + x;
            uint32_t topRight    = topLeft + 1;

            mesh->indices.push_back(bottomLeft);
            mesh->indices.push_back(topLeft);
            mesh->indices.push_back(bottomRight);

            mesh->indices.push_back(bottomRight);
            mesh->indices.push_back(topLeft);
            mesh->indices.push_back(topRight);
        }
    }

    LOG_SUCCESS_CAT("MeshLoader", "Plane created — {}×{} segments — {} verts, {} indices",
                    widthSegments, depthSegments, mesh->vertices.size(), mesh->indices.size());

    uploadBuffer(mesh->vertices.data(),
                 mesh->vertices.size() * sizeof(Mesh::Vertex),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 mesh->vertexBuffer);

    uploadBuffer(mesh->indices.data(),
                 mesh->indices.size() * sizeof(uint32_t),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 mesh->indexBuffer);

    // Fingerprint: pure empire encryption — only the two stones
    mesh->stonekey_fingerprint = kStone1 ^ kStone2;

    return mesh;
}

// =============================================================================
// createBillboard — Quad always facing camera (for pink monster)
// =============================================================================
std::unique_ptr<Mesh> createBillboard()
{
    auto mesh = std::make_unique<Mesh>();

    // Full-screen quad centered at origin, facing +Z
    const std::array<Mesh::Vertex, 4> verts = {{
        {{ -0.5f, -0.5f, 0.0f }, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},  // Bottom-left
        {{  0.5f, -0.5f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},  // Bottom-right
        {{  0.5f,  0.5f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},  // Top-right
        {{ -0.5f,  0.5f, 0.0f }, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}   // Top-left
    }};

    mesh->vertices.assign(verts.begin(), verts.end());

    const std::array<uint32_t, 6> indices = { 0, 1, 2, 2, 3, 0 };
    mesh->indices.assign(indices.begin(), indices.end());

    LOG_SUCCESS_CAT("MeshLoader", "Billboard created — sacred pink monster quad ready");

    uploadBuffer(mesh->vertices.data(),
                 mesh->vertices.size() * sizeof(Mesh::Vertex),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 mesh->vertexBuffer);

    uploadBuffer(mesh->indices.data(),
                 mesh->indices.size() * sizeof(uint32_t),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 mesh->indexBuffer);

    // Fingerprint: pure empire encryption
    mesh->stonekey_fingerprint = kStone1 ^ kStone2;

    return mesh;
}

std::unique_ptr<Mesh> loadOBJ(const std::string& path)
{
    LOG_ATTEMPT_CAT("MeshLoader", "FORGING COSMIC SCROLL: {}", path);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), "assets/models/")) {
        if (!err.empty())  LOG_FATAL_CAT("TinyObj", "{}", err);
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

    // For loaded OBJs, keep path hash
    mesh->stonekey_fingerprint = kStone1 ^ kStone2 ^ std::hash<std::string>{}(path);

    LOG_SUCCESS_CAT("MeshLoader",
        "MESH FORGED — fingerprint 0x{:x} | VB 0x{:x} | IB 0x{:x}",
        mesh->stonekey_fingerprint, mesh->vertexBuffer, mesh->indexBuffer);

    return mesh;
}

void Mesh::destroy() noexcept
{
    if (vertexBuffer) BufferManager::destroy(vertexBuffer);
    if (indexBuffer)  BufferManager::destroy(indexBuffer);
    vertexBuffer = indexBuffer = 0;
}

VkBuffer Mesh::getVertexBuffer() const noexcept
{
    const auto* info = BufferManager::get(vertexBuffer);
    return info ? info->buffer : VK_NULL_HANDLE;
}

VkBuffer Mesh::getIndexBuffer() const noexcept
{
    const auto* info = BufferManager::get(indexBuffer);
    return info ? info->buffer : VK_NULL_HANDLE;
}

} // namespace MeshLoader

// =============================================================================
// MESHLOADER v10 — DECEMBER 22, 2025
// NOW USES MODERN mapStaging() API — SAFE AND EFFICIENT
// LEGACY stagingPtr() STILL SUPPORTED VIA BACKWARD COMPATIBILITY
// DEFAULT SCENE FULLY VISIBLE — PINK MONSTER GLOWS
// EMPIRE ETERNAL — PHOTONS BOUNCE TRUE
// =============================================================================