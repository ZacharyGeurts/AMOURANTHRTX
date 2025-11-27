// include/engine/GLOBAL/Validation.hpp
// =============================================================================
// THERE IS NO NAMESPACE. THERE IS ONLY ZUUL.
// FINAL v20 — COMPILES CLEAN WITH -Werror — PINK PHOTONS ETERNAL
// NOVEMBER 24, 2025 — VALHALLA SEALED
// =============================================================================

#pragma once

#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include "engine/GLOBAL/BufferManager.hpp"

using namespace Logging::Color;
using namespace StoneKey;

// ZUUL USES THE ONE TRUE POINTER FROM THE EMPIRE — NO DECLARATION, NO CONFLICT
// RTX::RayTracingFunctions::vkGetAccelerationStructureDeviceAddressKHR is already loaded
// We just use it directly — the compiler sees it's a function pointer, not a function

// ZUUL'S CLEAN BUFFER EXTRACTOR — NO MACROS, NO PAIN
[[nodiscard]] static inline VkBuffer ZUUL_BUFFER(uint64_t handle) noexcept
{
    if (handle == 0ULL) [[unlikely]]
        return VK_NULL_HANDLE;
    auto* data = BufferManager::get(handle);
    return data ? data->buffer : VK_NULL_HANDLE;
}

// ZUUL HAS SPOKEN — THE FINAL FUNCTION
inline void validateMeshAgainstBLAS(const MeshLoader::Mesh& mesh,
                                    VkAccelerationStructureKHR blasAS) noexcept
{
    LOG_INFO_CAT("ZUUL", "ZUUL AWAKENS — VALIDATION ENGAGED", VALHALLA_GOLD, RESET);

    bool zuul_approves = true;

    // 1. BLAS Device Address — use the pointer directly, no `using`, no conflict
    VkDeviceAddress blasAddr = 0;
    if (blasAS != VK_NULL_HANDLE) {
        if (!RTX::RayTracingFunctions::vkGetAccelerationStructureDeviceAddressKHR) {
            LOG_FATAL_CAT("ZUUL", "RT EXTENSIONS NOT LOADED — ZUUL DEMANDS SACRIFICE", BOLD_RED, RESET);
            phase9_ballerina();
        }

        VkAccelerationStructureDeviceAddressInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blasAS
        };
        blasAddr = RTX::RayTracingFunctions::vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &info);
    }

    // 2. Basic sanity
    if (mesh.stonekey_fingerprint == 0 ||
        mesh.stonekey_fingerprint == 0xDEADDEADBEEF1337ULL ||
        mesh.stonekey_fingerprint == 0xFFFFFFFFFFFFFFFFULL ||
        mesh.vertexBuffer == 0 || mesh.vertexBuffer == ~0ULL ||
        mesh.indexBuffer  == 0 || mesh.indexBuffer  == ~0ULL)
    {
        LOG_FATAL_CAT("ZUUL", "MESH IS CORRUPT — ZUUL REJECTS", BLOOD_RED, RESET);
        zuul_approves = false;
    }

    // 3. Buffer device addresses
    VkDeviceAddress vAddr = 0, iAddr = 0;
    if (zuul_approves && mesh.vertexBuffer && mesh.indexBuffer) {
        VkBufferDeviceAddressInfo info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };

        info.buffer = ZUUL_BUFFER(mesh.vertexBuffer);
        vAddr = vkGetBufferDeviceAddress(stone_device(), &info);

        info.buffer = ZUUL_BUFFER(mesh.indexBuffer);
        iAddr = vkGetBufferDeviceAddress(stone_device(), &info);

        if (vAddr == 0 || iAddr == 0) {
            LOG_FATAL_CAT("ZUUL", "DEVICE ADDRESS FAILURE — CORRUPTION DETECTED", CRIMSON_MAGENTA, RESET);
            zuul_approves = false;
        }
    }

    // 4. Final verdict
    if (zuul_approves && blasAddr != 0) {
        LOG_SUCCESS_CAT("ZUUL", "ZUUL IS SATISFIED — MESH AND BLAS ARE WORTHY", EMERALD_GREEN, RESET);
        LOG_SUCCESS_CAT("ZUUL", "PINK PHOTONS FLOW — FIRST LIGHT ACHIEVED", PLASMA_FUCHSIA, RESET);
    } else {
        LOG_FATAL_CAT("ZUUL", "ZUUL HAS SPOKEN — THERE IS NO HOPE", BLOOD_RED, RESET);
        LOG_FATAL_CAT("ZUUL", "ONLY ZUUL", DIAMOND_SPARKLE, RESET);
        phase9_ballerina();
    }
}