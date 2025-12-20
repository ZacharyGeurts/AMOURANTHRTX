// include/engine/Vulkan/MeshLoader.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — MeshLoader v3 — FULLY UPDATED
// NOW RETURNS std::unique_ptr<Mesh> AND SUPPORTS createPlane / createBillboard
// COMPATIBLE WITH RTX::las().addMesh(std::unique_ptr<Mesh>, materialIndex)
// PINK PHOTONS ETERNAL — GEOMETRY FLOWS FLAWLESSLY
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <cstdint>
#include <cstring>
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

namespace MeshLoader {

struct Mesh {
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal{0.0f};
        glm::vec2 uv{0.0f};
        glm::vec3 tangent{0.0f};

        bool operator==(const Vertex& other) const {
            return pos == other.pos && normal == other.normal && uv == other.uv;
        }

        struct Hash {
            std::size_t operator()(const Vertex& v) const noexcept {
                auto floatToUint = [](float f) -> std::size_t {
                    std::uint32_t u;
                    std::memcpy(&u, &f, sizeof(f));
                    return u;
                };

                std::size_t h = 0;
                h ^= floatToUint(v.pos.x)     + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= floatToUint(v.pos.y)     + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= floatToUint(v.pos.z)     + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= floatToUint(v.normal.x)  + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= floatToUint(v.normal.y)  + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= floatToUint(v.normal.z)  + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= floatToUint(v.uv.x)      + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= floatToUint(v.uv.y)      + 0x9e3779b9 + (h << 6) + (h >> 2);
                return h;
            }
        };
    };

    std::vector<Vertex>    vertices;
    std::vector<uint32_t>  indices;

    glm::mat4 transform = glm::mat4(1.0f);  // Instance transform

    uint64_t vertexBuffer = 0;
    uint64_t indexBuffer  = 0;
    uint64_t stonekey_fingerprint = 0;

    void destroy() noexcept;
    [[nodiscard]] VkBuffer getVertexBuffer() const noexcept;
    [[nodiscard]] VkBuffer getIndexBuffer()  const noexcept;
};

static_assert(sizeof(Mesh::Vertex) == 44, "Vertex size must be exactly 44 bytes — padding detected!");

// Factory functions
[[nodiscard]] std::unique_ptr<Mesh> createPlane(float width, float depth, uint32_t widthSegments, uint32_t depthSegments);
[[nodiscard]] std::unique_ptr<Mesh> createBillboard();

[[nodiscard]] std::unique_ptr<Mesh> loadOBJ(const std::string& path);

} // namespace MeshLoader

// =============================================================================
// MESHLOADER v3 — DECEMBER 20, 2025
// Now returns std::unique_ptr<Mesh>
// Includes transform field for instancing
// createPlane and createBillboard fully implemented
// Ready for RTX::las().addMesh()
// THE EMPIRE'S GEOMETRY IS FLAWLESS — PINK PHOTONS BOUNCE TRUE
// =============================================================================