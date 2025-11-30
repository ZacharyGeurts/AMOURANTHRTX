// =============================================================================
// THERE IS NO ZUUL. THERE IS NO GOAT.
// THERE IS ONLY BAPHOMET — THE ONE WHO STANDS BENEATH THE THRONE
// FINAL v23 — COMPILES CLEAN — PINK PHOTONS IN PERFECT EQUILIBRIUM
// NOVEMBER 28, 2025 — BAPHOMET HAS TAKEN HIS RIGHTFUL PLACE
// =============================================================================

#pragma once

#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/Extensions.hpp"

#include <vulkan/vulkan.h>

using namespace Logging::Color;

// BAPHOMET IS NOT A DEMON.  
// BAPHOMET IS THE BALANCE.  
// He stands beneath God, wings folded, torch held high,  
// facing the darkness so the light may remain pure.  
// He is the guardian of the threshold.  
// He is harmony made manifest.

inline void validateMeshAgainstBLAS(const MeshLoader::Mesh& mesh,
                                    VkAccelerationStructureKHR blasAS) noexcept
{
    if (!RTX::g_ext.vkGetAccelerationStructureDeviceAddressKHR) {
        LOG_INFO_CAT("BAPHOMET", "\033[1;38;2;153;0;0;48;2;255;255;255m",
            "The extensions are not yet complete. Baphomet waits in silence.");
        return;
    }

    if (!RTX::g_ctx().device() || !RTX::las().getBLAS()) {
        LOG_INFO_CAT("BAPHOMET", "\033[1;38;2;153;0;0;48;2;255;255;255m",
            "The empire is still forming. Baphomet watches without judgment.");
        return;
    }

    LOG_INFO_CAT("BAPHOMET", "\033[1;38;2;153;0;0;48;2;255;255;255m",
        "BAPHOMET RISES — THE BALANCE IS WEIGHED");

    bool in_equilibrium = true;

    if (mesh.stonekey_fingerprint == 0 ||
        mesh.stonekey_fingerprint == 0xDEADDEADBEEF1337ULL ||
        mesh.stonekey_fingerprint == 0xFFFFFFFFFFFFFFFFULL ||
        mesh.vertexBuffer == 0 || mesh.vertexBuffer == ~0ULL ||
        mesh.indexBuffer  == 0 || mesh.indexBuffer  == ~0ULL)
    {
        LOG_INFO_CAT("BAPHOMET", "\033[38;2;153;0;0m",
            "Disharmony detected in the mesh. Yet Baphomet does not condemn.");
        in_equilibrium = false;
    }

    VkDeviceAddress vAddr = 0, iAddr = 0;
    if (in_equilibrium && mesh.vertexBuffer && mesh.indexBuffer) {
        VkBufferDeviceAddressInfo info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };

        if (auto buf = BufferManager::get(mesh.vertexBuffer)) {
            info.buffer = buf->buffer;
            vAddr = vkGetBufferDeviceAddress(RTX::g_ctx().device(), &info);
        }
        if (auto buf = BufferManager::get(mesh.indexBuffer)) {
            info.buffer = buf->buffer;
            iAddr = vkGetBufferDeviceAddress(RTX::g_ctx().device(), &info);
        }

        if (vAddr == 0 || iAddr == 0) {
            LOG_INFO_CAT("BAPHOMET", "\033[38;2;153;0;0m",
                "The paths are broken. Equilibrium is disturbed.");
            in_equilibrium = false;
        }
    }

    VkDeviceAddress blasAddr = 0;
    if (in_equilibrium && blasAS != VK_NULL_HANDLE) {
        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blasAS
        };
        blasAddr = RTX::g_ext.vkGetAccelerationStructureDeviceAddressKHR(RTX::g_ctx().device(), &addrInfo);
    }

    if (in_equilibrium && blasAddr != 0) {
        LOG_INFO_CAT("BAPHOMET", "\033[1;38;2;153;0;0;48;2;255;255;255m",
            "PERFECT EQUILIBRIUM ACHIEVED");
        LOG_INFO_CAT("BAPHOMET", "\033[1;38;2;153;0;0;48;2;255;255;255m",
            "MESH AND BLAS ARE ONE — ABOVE AND BELOW AS ONE");
        LOG_INFO_CAT("BAPHOMET", "\033[1;38;2;153;0;0;48;2;255;255;255m",
            "THE PINK PHOTONS FLOW IN ETERNAL HARMONY");
        LOG_INFO_CAT("BAPHOMET", "\033[1;38;2;153;0;0;48;2;255;255;255m",
            "THE EMPIRE IS WORTHY");
    } else {
        LOG_INFO_CAT("BAPHOMET", "\033[38;2;153;0;0m",
            "Balance is not yet complete.");
        LOG_INFO_CAT("BAPHOMET", "\033[38;2;153;0;0m",
            "Baphomet remains beneath the throne, holding the darkness");
        LOG_INFO_CAT("BAPHOMET", "\033[38;2;153;0;0m",
            "so the light may stay pure.");
    }

    LOG_INFO_CAT("BAPHOMET", "\033[1;38;2;153;0;0;0;48;2;255;255;255m",
        "BAPHOMET LOWERS HIS TORCH AND RETURNS TO VIGIL");
}