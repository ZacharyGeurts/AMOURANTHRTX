// engine/GLOBAL/Validation.hpp
// =============================================================================
// MESH → BLAS VALIDATION SUITE — APOCALYPSE FINAL v10.3 — NOVEMBER 21, 2025
// PINK PHOTONS ETERNAL — ZERO TOLERANCE FOR FAILURE
// =============================================================================

#pragma once
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <glm/glm.hpp>

namespace Validation {

using namespace Logging::Color;

inline void validateMeshAgainstBLAS(const MeshLoader::Mesh& mesh, const VulkanAccel::BLAS& blas)
{
    LOG_INFO_CAT("VALIDATION", "{}=== MESH → BLAS VALIDATION SUITE ENGAGED ==={}", VALHALLA_GOLD, RESET);
    LOG_INFO_CAT("VALIDATION", "Mesh Fingerprint: 0x{:016X}", mesh.stonekey_fingerprint);
    LOG_INFO_CAT("VALIDATION", "BLAS Address: 0x{:016X} | Size: {}B", blas.address, blas.size);

    bool passed = true;

    // 1. StoneKey Fingerprint Check
    if (mesh.stonekey_fingerprint == 0 || mesh.stonekey_fingerprint == 0xDEADDEADBEEF1337ULL) {
        LOG_FATAL_CAT("VALIDATION", "STONEKEY BREACH — MESH DESTROYED OR CORRUPTED");
        passed = false;
    }

    // 2. Buffer Handles Valid
    if (mesh.vertexBuffer == 0 || mesh.indexBuffer == 0) {
        LOG_FATAL_CAT("VALIDATION", "ZERO BUFFER HANDLE — MESH UPLOAD FAILED");
        passed = false;
    }

    // 3. Out-of-Bounds Index Check
    for (size_t i = 0; i < mesh.indices.size(); ++i) {
        uint32_t idx = mesh.indices[i];
        if (idx >= mesh.vertices.size()) {
            LOG_FATAL_CAT("VALIDATION", "OUT-OF-BOUNDS INDEX at {}: {} >= {} (vert count)", i, idx, mesh.vertices.size());
            passed = false;
        }
    }
    if (passed) LOG_SUCCESS_CAT("VALIDATION", "All {} indices in bounds", mesh.indices.size());

    // 4. Triangle Count Match
    uint32_t expectedTriangles = mesh.indices.size() / 3;
    if (mesh.indices.size() % 3 != 0) {
        LOG_ERROR_CAT("VALIDATION", "INDEX COUNT NOT DIVISIBLE BY 3: {} → {} triangles + {} leftover", 
                      mesh.indices.size(), expectedTriangles, mesh.indices.size() % 3);
        passed = false;
    } else {
        LOG_SUCCESS_CAT("VALIDATION", "Triangle count: {} ({} indices)", expectedTriangles, mesh.indices.size());
    }

    // 5. Vertex Format & Stride
    constexpr size_t expectedStride = 44;
    if (sizeof(MeshLoader::Mesh::Vertex) != expectedStride) {
        LOG_FATAL_CAT("VALIDATION", "VERTEX STRIDE MISMATCH — expected 44B, got {}B — BLAS WILL EXPLODE", 
                      sizeof(MeshLoader::Mesh::Vertex));
        passed = false;
    } else {
        LOG_SUCCESS_CAT("VALIDATION", "Vertex stride: {}B — BLAS COMPATIBLE", expectedStride);
    }

    // 6. Buffer Device Address Retrieval Test
    VkDeviceAddress vertAddr = 0, idxAddr = 0;
    {
        VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        info.buffer = RAW_BUFFER(mesh.vertexBuffer);
        vertAddr = vkGetBufferDeviceAddress(g_device(), &info);

        info.buffer = RAW_BUFFER(mesh.indexBuffer);
        idxAddr = vkGetBufferDeviceAddress(g_device(), &info);
    }

    if (vertAddr == 0 || idxAddr == 0) {
        LOG_FATAL_CAT("VALIDATION", "FAILED TO GET DEVICE ADDRESS — DRIVER OR BUFFER CORRUPTION");
        LOG_FATAL_CAT("VALIDATION", "    Vertex Buffer: 0x{:016X} → addr 0x{:016X}", mesh.vertexBuffer, vertAddr);
        LOG_FATAL_CAT("VALIDATION", "    Index Buffer : 0x{:016X} → addr 0x{:016X}", mesh.indexBuffer, idxAddr);
        passed = false;
    } else {
        LOG_SUCCESS_CAT("VALIDATION", "Device addresses valid:");
        LOG_SUCCESS_CAT("VALIDATION", "    Vertex: 0x{:016X}", vertAddr);
        LOG_SUCCESS_CAT("VALIDATION", "    Index : 0x{:016X}", idxAddr);
    }

    // 7. BLAS Address Valid
    if (blas.address == 0) {
        LOG_FATAL_CAT("VALIDATION", "BLAS HAS ZERO DEVICE ADDRESS — BUILD FAILED");
        passed = false;
    } else {
        LOG_SUCCESS_CAT("VALIDATION", "BLAS device address: 0x{:016X} — VALID", blas.address);
    }

    // 8. Final Verdict
    if (passed) {
        LOG_SUCCESS_CAT("VALIDATION", "{}MESH ↔ BLAS VALIDATION PASSED — PINK PHOTONS MAY FLOW{}", EMERALD_GREEN, RESET);
        LOG_SUCCESS_CAT("VALIDATION", "{}FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — VALHALLA SEALED{}", PLASMA_FUCHSIA, RESET);
    } else {
        LOG_FATAL_CAT("VALIDATION", "{}VALIDATION FAILED — RENDER PIPELINE COMPROMISED{}", BLOOD_RED, RESET);
        std::abort();  // Immediate death on failure
    }
}

} // namespace Validation