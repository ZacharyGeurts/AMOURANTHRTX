// include/engine/Vulkan/MeshLoader.hpp
#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace MeshLoader {

struct Mesh {
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec3 tangent{0.0f};
        bool operator==(const Vertex& other) const;
        struct Hash { std::size_t operator()(const Vertex& v) const noexcept; };
    };

    std::vector<Vertex>    vertices;
    std::vector<uint32_t>  indices;

    // ← STONEKEY v∞ OBFUSCATED HANDLES (like all your other code)
    uint64_t vertexBuffer = 0;
    uint64_t indexBuffer  = 0;

    void destroy() noexcept;
    [[nodiscard]] VkBuffer getVertexBuffer() const noexcept;
    [[nodiscard]] VkBuffer getIndexBuffer()  const noexcept;
};

[[nodiscard]] std::unique_ptr<Mesh> loadOBJ(const std::string& path);

} // namespace MeshLoader