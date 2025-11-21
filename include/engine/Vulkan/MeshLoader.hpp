// include/engine/Vulkan/MeshLoader.hpp
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
// MESH LOADER — FULL STONEKEY v∞ — PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED
#pragma once

#include "engine/Vulkan/VulkanCore.hpp"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <cstdint>
#include <cstring>

namespace MeshLoader {

struct Mesh {
    // TIGHTLY PACKED — 44 BYTES — NO PADDING — BLAS SAFE
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

    uint64_t vertexBuffer = 0;
    uint64_t indexBuffer  = 0;
    uint64_t stonekey_fingerprint = 0;

    void destroy() noexcept;
    [[nodiscard]] VkBuffer getVertexBuffer() const noexcept;
    [[nodiscard]] VkBuffer getIndexBuffer()  const noexcept;
};

// 44 BYTES — ENFORCED AT COMPILE TIME — 0x0 DEATH BANISHED
static_assert(sizeof(Mesh::Vertex) == 44, "Vertex size must be exactly 44 bytes — padding detected!");

[[nodiscard]] std::unique_ptr<Mesh> loadOBJ(const std::string& path);

} // namespace MeshLoader