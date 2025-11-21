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
// MESH LOADER — FULL STONEKEY v∞ — PINK PHOTONS ETERNAL — 0x0 CRASH DEAD
#include "engine/Vulkan/MeshLoader.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <tinyobjloader/tiny_obj_loader.h>
#include <unordered_map>
#include <cstring>

using namespace Logging::Color;

namespace MeshLoader {

void Mesh::destroy() noexcept {
    BUFFER_DESTROY(vertexBuffer);
    BUFFER_DESTROY(indexBuffer);
    stonekey_fingerprint = 0xDEADDEADBEEF1337ULL;
    LOG_SUCCESS_CAT("MeshLoader", "{}MESH SACRIFICED — BUFFERS RETURNED TO THE VOID{}", PLASMA_FUCHSIA, RESET);
}

VkBuffer Mesh::getVertexBuffer() const noexcept {
    if (stonekey_fingerprint == 0 || stonekey_fingerprint == 0xDEADDEADBEEF1337ULL) {
        LOG_FATAL("STONEKEY BREACH: Accessing destroyed mesh vertex buffer");
        std::abort();
    }
    return RAW_BUFFER(vertexBuffer);
}

VkBuffer Mesh::getIndexBuffer() const noexcept {
    if (stonekey_fingerprint == 0 || stonekey_fingerprint == 0xDEADDEADBEEF1337ULL) {
        LOG_FATAL("STONEKEY BREACH: Accessing destroyed mesh index buffer");
        std::abort();
    }
    return RAW_BUFFER(indexBuffer);
}

static void uploadBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage, uint64_t& outHandle)
{
    LOG_INFO_CAT("MeshLoader", "Uploading buffer — size: {} bytes", size);

    auto& tracker = RTX::UltraLowLevelBufferTracker::get();

    uint64_t staging = 0;
    BUFFER_CREATE(staging, size,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "Mesh_Staging");

    void* mapped = tracker.map(staging);
    std::memcpy(mapped, data, size);
    tracker.unmap(staging);

    BUFFER_CREATE(outHandle, size,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ? "Mesh_Vertex_Final" : "Mesh_Index_Final");

    // Manual one-time submit — your codebase style
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = RTX::g_ctx().commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(g_device(), &allocInfo, &cmd), "Failed to allocate upload cmd buffer");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy copy{.size = size};
    vkCmdCopyBuffer(cmd, RAW_BUFFER(staging), RAW_BUFFER(outHandle), 1, &copy);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    VK_CHECK(vkQueueSubmit(RTX::g_ctx().graphicsQueue_, 1, &submit, VK_NULL_HANDLE), "Mesh upload submit failed");

    vkQueueWaitIdle(RTX::g_ctx().graphicsQueue_);
    vkFreeCommandBuffers(g_device(), RTX::g_ctx().commandPool_, 1, &cmd);

    BUFFER_DESTROY(staging);
    LOG_SUCCESS_CAT("MeshLoader", "Buffer upload complete — obf handle: 0x{:016X}", outHandle);
}

std::unique_ptr<Mesh> loadOBJ(const std::string& path)
{
    LOG_ATTEMPT_CAT("MeshLoader", "Loading OBJ: {}", path);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), "assets/models/")) {
        if (!err.empty()) {
            LOG_FATAL("TinyObjLoader failed: {}", err);
            throw std::runtime_error("TinyObjLoader: " + err);
        }
        if (!warn.empty()) LOG_WARNING_CAT("MeshLoader", "{}", warn);
    }

    auto mesh = std::make_unique<Mesh>();

    // FULLY QUALIFIED — NO SCOPE ISSUES
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

            if (uniqueVertices.find(v) == uniqueVertices.end()) {
                uniqueVertices[v] = static_cast<uint32_t>(mesh->vertices.size());
                mesh->vertices.push_back(v);
            }
            mesh->indices.push_back(uniqueVertices[v]);
        }
    }

    LOG_SUCCESS_CAT("MeshLoader", "Loaded {} → {} vertices, {} indices — GEOMETRY FORGED",
                    path, mesh->vertices.size(), mesh->indices.size());

    uploadBuffer(mesh->vertices.data(),
                 mesh->vertices.size() * sizeof(Mesh::Vertex),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 mesh->vertexBuffer);

    uploadBuffer(mesh->indices.data(),
                 mesh->indices.size() * sizeof(uint32_t),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 mesh->indexBuffer);

    // FINAL STONEKEY RITE
    mesh->stonekey_fingerprint =
        get_kStone1() ^ get_kStone2() ^
        std::hash<std::string>{}(path) ^
        mesh->vertices.size() ^ mesh->indices.size();

    LOG_SUCCESS_CAT("MeshLoader",
        "{}MESH FULLY STONEKEYED v∞ — FINGERPRINT 0x{:016X} — PINK PHOTONS BOUND TO GEOMETRY{}",
        PLASMA_FUCHSIA, mesh->stonekey_fingerprint, RESET);

    return mesh;
}

} // namespace MeshLoader