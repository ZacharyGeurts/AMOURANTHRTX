// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — LAS (Acceleration Structure Manager) Header — v1.3 — DECEMBER 21, 2025
// SUPPORTS DYNAMIC MESH LOADING FROM main.cpp (MeshLoader::Mesh)
// addMesh ACCEPTS std::unique_ptr<MeshLoader::Mesh> DIRECTLY
// rebuildTLAS() ADDED FOR EXPLICIT REBUILD AFTER ADDING MESHES
// FULLY INTEGRATED WITH BufferManager AND RTX EXTENSIONS
// PINK PHOTONS ACCELERATED — EMPIRE ETERNAL
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

#include "engine/GLOBAL/RTXHandler.hpp"   // RTX::Handle<T>
#include "engine/GLOBAL/PipelineManager.hpp"

namespace MeshLoader { struct Mesh; } // Forward declare — used in main.cpp

namespace RTX {

class LAS {
public:
    LAS();
    ~LAS();

    // Called every frame — builds BLAS on first call, then builds/updates TLAS
    void buildOrUpdateTLAS(VkCommandBuffer cmd);

    // Returns current TLAS (valid after first build)
    [[nodiscard]] VkAccelerationStructureKHR getCurrentTLAS() const;

    // Called on window resize — purges TLAS (BLAS preserved)
    void notifyResize();

    // === COMPATIBLE WITH main.cpp ===
    // Accepts the unique_ptr returned by MeshLoader
    void addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex = 0);

    // Force full TLAS rebuild after adding new meshes
    void rebuildTLAS();

private:
    void buildBLAS(VkCommandBuffer cmd);
    void buildSingleBLAS(VkCommandBuffer cmd,
                         VkAccelerationStructureKHR& blas,
                         RTX::Handle<VkBuffer>& blasBuffer,
                         RTX::Handle<VkDeviceMemory>& blasMemory,
                         uint64_t vertexHandle,
                         uint64_t indexHandle,
                         uint32_t primitiveCount,
                         const std::string& tag);
    void buildTLAS(VkCommandBuffer cmd);
    void clearTLAS();

    struct InternalMesh {
        uint64_t vertexHandle = 0;
        uint64_t indexHandle = 0;
        uint32_t primitiveCount = 0;
        uint32_t materialIndex = 0;
        VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
        RTX::Handle<VkBuffer> blasBuffer;
        RTX::Handle<VkDeviceMemory> blasMemory;
    };

    std::vector<InternalMesh> meshes_;

    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    RTX::Handle<VkBuffer> tlasBuffer;
    RTX::Handle<VkDeviceMemory> tlasMemory;

    RTX::Handle<VkBuffer> scratchBuffer;
    RTX::Handle<VkDeviceMemory> scratchMemory;

    RTX::Handle<VkBuffer> instanceBuffer;
    RTX::Handle<VkDeviceMemory> instanceMemory;

    bool blasBuilt = false;
    bool tlasDirty = false;  // Set when new meshes are added
};

// Global singleton accessor
[[nodiscard]] LAS& las();

} // namespace RTX

// =============================================================================
// LAS HEADER v1.3 — DECEMBER 21, 2025
// FIXED: No duplicate class definition
// FIXED: addMesh accepts std::unique_ptr<MeshLoader::Mesh> — matches main.cpp
// ADDED: rebuildTLAS() for explicit rebuild
// FULLY COMPATIBLE WITH CURRENT main.cpp AND ENGINE STATE
// EMPIRE ETERNAL — PINK PHOTONS NOW FULLY DYNAMIC AND ACCELERATED
// =============================================================================