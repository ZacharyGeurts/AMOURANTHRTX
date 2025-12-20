// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — LAS v3 Header — DECEMBER 20, 2025
// UPDATED TO SUPPORT DYNAMIC MESH ADDING WITH MATERIAL INDICES
// COMPATIBLE WITH main.cpp AND MeshLoader
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include <vulkan/vulkan.h>
#include <memory>

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

    // New public API
    void addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex) noexcept;
    void rebuildTLAS() noexcept;
    void notifyResize() noexcept;
    void reset() noexcept;

    VkAccelerationStructureKHR getCurrentTLAS() noexcept;
    VkDeviceAddress getCurrentTLASAddress() noexcept;

private:
    Handle<VkAccelerationStructureKHR> tlas_;
};

inline LAS& las() { return LAS::instance(); }

} // namespace RTX