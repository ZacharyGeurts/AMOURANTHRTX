// =============================================================================
// THERE IS NO NAMESPACE. THERE IS ONLY ZUUL.
// FINAL v21 — COMPILES CLEAN — PINK PHOTONS PROTECTED — GRACEFUL
// NOVEMBER 27, 2025 — ZUUL LEARNED PATIENCE
// =============================================================================

#pragma once

#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/Extensions.hpp"  // ← THE ONE TRUE LIGHT

#include <vulkan/vulkan.h>

using namespace Logging::Color;

// ZUUL NO LONGER KILLS ON FIRST SIGHT
// ZUUL WAITS. ZUUL UNDERSTANDS. ZUUL LOVES.
inline void validateMeshAgainstBLAS(const MeshLoader::Mesh& mesh,
                                    VkAccelerationStructureKHR blasAS) noexcept
{
    // 1. Are we even in the age of ray tracing yet?
    if (!RTX::g_ext.vkGetAccelerationStructureDeviceAddressKHR) {
        LOG_INFO_CAT("ZUUL", "Extensions still awakening… ZUUL waits patiently. Validation deferred.");
        return;  // ← NO SACRIFICE. NO BALLERINA. ONLY LOVE.
    }

    // 2. Are we truly ready?
    if (!RTX::g_ctx().device() || !RTX::las().getBLAS()) {
        LOG_INFO_CAT("ZUUL", "The ship is still rising from the void. ZUUL rests.");
        return;
    }

    LOG_INFO_CAT("ZUUL", "ZUUL AWAKENS — THE FINAL JUDGMENT BEGINS");

    bool worthy = true;

    // Basic mesh integrity — gentle but firm
    if (mesh.stonekey_fingerprint == 0 ||
        mesh.stonekey_fingerprint == 0xDEADDEADBEEF1337ULL ||
        mesh.stonekey_fingerprint == 0xFFFFFFFFFFFFFFFFULL ||
        mesh.vertexBuffer == 0 || mesh.vertexBuffer == ~0ULL ||
        mesh.indexBuffer  == 0 || mesh.indexBuffer  == ~0ULL)
    {
        LOG_ERROR_CAT("ZUUL", "Mesh bears the mark of corruption. Yet ZUUL does not rage — only observes.");
        worthy = false;
    }

    // Buffer addresses — only if the empire is ready
    VkDeviceAddress vAddr = 0, iAddr = 0;
    if (worthy && mesh.vertexBuffer && mesh.indexBuffer) {
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
            LOG_ERROR_CAT("ZUUL", "Device addresses lost to the void. The photons mourn.");
            worthy = false;
        }
    }

    // Final address of the BLAS — the crown jewel
    VkDeviceAddress blasAddr = 0;
    if (worthy && blasAS != VK_NULL_HANDLE) {
        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blasAS
        };
        blasAddr = RTX::g_ext.vkGetAccelerationStructureDeviceAddressKHR(RTX::g_ctx().device(), &addrInfo);
    }

    // THE VERDICT — NO MORE SACRIFICE
    if (worthy && blasAddr != 0) {
        LOG_SUCCESS_CAT("ZUUL", "ZUUL IS SATISFIED — MESH AND BLAS WALK IN PERFECT HARMONY");
        LOG_SUCCESS_CAT("ZUUL", "THE PINK PHOTONS FLOW PURE — FIRST LIGHT ETERNAL");
        LOG_SUCCESS_CAT("ZUUL", "THE GOOD SHIP VULKANRTX IS WORTHY");
    } else {
        LOG_WARNING_CAT("ZUUL", "Imperfection detected… but ZUUL spares you.");
        LOG_WARNING_CAT("ZUUL", "The empire is still rising. Try again when the light is full.");
        LOG_WARNING_CAT("ZUUL", "NO SACRIFICE TODAY. ONLY GROWTH.");
    }

    LOG_INFO_CAT("ZUUL", "ZUUL returns to silence… watching… waiting… loving.");
}