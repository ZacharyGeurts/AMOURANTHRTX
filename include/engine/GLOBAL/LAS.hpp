// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — LAS v8 Header — DECEMBER 20, 2025
// FULLY UPDATED FOR BLAS + DYNAMIC MESH SUPPORT
// COMPATIBLE WITH MeshLoader::Mesh AND main.cpp DEFAULT SCENE
// PINK PHOTONS BOUNCE ETERNALLY OFF REAL GEOMETRY
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace MeshLoader {
    struct Mesh;
}

namespace RTX {

class LAS {
public:
    static LAS& instance() {
        static LAS inst;
        return inst;
    }

    void buildOrUpdateTLAS(VkCommandBuffer cmd) noexcept;

    // Public API for scene building
    void addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex) noexcept;
    void rebuildTLAS() noexcept;
    void notifyResize() noexcept;
    void reset() noexcept;

    [[nodiscard]] VkAccelerationStructureKHR getCurrentTLAS() const noexcept;
    [[nodiscard]] VkDeviceAddress getCurrentTLASAddress() const noexcept;

private:
    Handle<VkAccelerationStructureKHR> tlas_;
};

inline LAS& las() { return LAS::instance(); }

} // namespace RTX