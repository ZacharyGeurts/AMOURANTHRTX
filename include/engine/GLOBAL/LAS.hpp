// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025-2026 — LAS (Acceleration Structure Manager) Header — v3.0 — DECEMBER 29, 2025
// DYNAMIC EMPIRE EDITION
// MAJOR UPGRADES:
// • Full per-instance transforms with update API → real animated scenes
// • BLAS built once and cached (no rebuild on TLAS refresh)
// • Persistent reusable scratch buffers (zero allocation churn)
// • Animated default pink monster support
// • Material index directly usable as instanceCustomIndex
// • Continuous visibility mandate preserved and enhanced
// PINK PHOTONS ETERNAL, ANIMATED AND UNBROKEN — EMPIRE ETERNAL — PLASTIC BEACH FOREVER
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "engine/GLOBAL/RTXHandler.hpp"   // RTX::Handle<T>

namespace MeshLoader { struct Mesh; } // Forward declare

namespace RTX {

class LAS {
public:
    LAS();
    ~LAS();

    // Called every frame — builds missing BLAS, then builds TLAS if dirty
    void buildOrUpdateTLAS(VkCommandBuffer cmd);

    // Returns current TLAS (valid after first successful build)
    [[nodiscard]] VkAccelerationStructureKHR getCurrentTLAS() const;

    // Called on window resize — purges TLAS (BLAS preserved)
    void notifyResize();

    // Add mesh — takes ownership
    void addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex = 0);

    // Force TLAS rebuild (e.g., after adding meshes)
    void rebuildTLAS();

    // Update transform of a specific mesh instance (index from add order)
    void updateInstanceTransform(size_t meshIndex, const glm::mat4& transform);

    // Optional: animate the sacred pink monster (call every frame with deltaTime)
    void animatePinkMonster(float deltaTime);

private:
    // Internal representation of a mesh instance
    struct InternalMesh {
        uint64_t vertexHandle = 0;
        uint64_t indexHandle = 0;
        uint32_t primitiveCount = 0;
        uint32_t materialIndex = 0;

        VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
        RTX::Handle<VkBuffer> blasBuffer;
        RTX::Handle<VkDeviceMemory> blasMemory;

        glm::mat4 transform = glm::mat4(1.0f);  // Per-instance world transform
    };

    // Persistent scratch buffer state
    struct ScratchBuffers {
        uint64_t handle = 0;
        VkDeviceSize size = 0;
    };

    void buildBLAS(VkCommandBuffer cmd);  // Builds any missing BLAS
    void buildSingleBLAS(VkCommandBuffer cmd, InternalMesh& m);
    void buildTLAS(VkCommandBuffer cmd);
    void clearTLAS();
    void destroyScratchBuffers();
    uint64_t ensureScratch(VkDeviceSize requiredSize, ScratchBuffers& scratch, const std::string& tag);

    std::vector<InternalMesh> meshes_;

    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    RTX::Handle<VkBuffer> tlasBuffer;
    RTX::Handle<VkDeviceMemory> tlasMemory;

    RTX::Handle<VkBuffer> instanceBuffer;
    RTX::Handle<VkDeviceMemory> instanceMemory;

    ScratchBuffers blasScratch{};
    ScratchBuffers tlasScratch{};

    bool tlasDirty = false;  // Set when new meshes or transforms change
};

// Global singleton accessor
[[nodiscard]] inline LAS& las() {
    static LAS instance;
    return instance;
}

} // namespace RTX

// =============================================================================
// LAS HEADER v3.0 — DECEMBER 29, 2025 — DYNAMIC EMPIRE BUILD
// • Added per-instance transforms + updateInstanceTransform()
// • Added animatePinkMonster() for eternal proof-of-life animation
// • BLAS caching + persistent scratch buffers
// • All new members and methods declared
// • Full compatibility with updated LAS.cpp v3.0
// • Eternal photons now move — empire ascends
// PINK PHOTONS ETERNAL — PLASTIC BEACH FOREVER
// =============================================================================