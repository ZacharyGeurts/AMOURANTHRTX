// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025-2026 — VALHALLA v∞ TURBO — DEVELOPER EDITION
// Light Acceleration System (LAS) v4.0 — January 03, 2026
// Clean, modern C++23 header — developer-friendly, safe, extensible
// Fully compatible with existing codebase (RTX::las() singleton access)
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <cstdint>

#include "engine/GLOBAL/MeshLoader.hpp"

namespace RTX {

class LAS {
public:
    LAS();
    ~LAS();

    size_t addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex = 0);
    void setInstanceTransform(size_t instanceIndex, const glm::mat4& transform);
    void requestRebuild();
    void update(VkCommandBuffer cmd);
    VkAccelerationStructureKHR getTLAS() const;
    void onResize();

    // Legacy compatibility
    void notifyResize() { onResize(); }
    void rebuildTLAS() { requestRebuild(); }
    void buildOrUpdateTLAS(VkCommandBuffer cmd) { update(cmd); }
    VkAccelerationStructureKHR getCurrentTLAS() const { return getTLAS(); }

private:
    // Internal mesh representation — moved UP so it's known before use
    struct InternalMesh {
        uint64_t vertexBuffer = 0;
        uint64_t indexBuffer = 0;
        uint32_t primitiveCount = 0;
        uint32_t materialIndex = 0;
        glm::mat4 transform = glm::mat4(1.0f);
        VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
        uint64_t blasStorage = 0;
    };

    void createDefaultDeveloperScene();
    void buildBLAS(VkCommandBuffer cmd, InternalMesh& mesh);  // Now InternalMesh is known
    bool buildTLAS(VkCommandBuffer cmd);
    uint64_t ensureScratch(VkDeviceSize required, const std::string& tag);
    void insertAccelerationStructureBarrier(VkCommandBuffer cmd);
    void clearTLAS();
    void destroyScratchBuffers();

    std::vector<InternalMesh> meshes_;

    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    uint64_t tlasStorage = 0;
    uint64_t instanceStorage = 0;

    uint64_t scratchBuffer = 0;
    VkDeviceSize scratchSize = 0;

    bool tlasDirty = true;
};

// Global singleton — matches existing code (RTX::las())
inline LAS& las()
{
    static LAS instance;
    return instance;
}

} // namespace RTX