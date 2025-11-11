// include/engine/GLOBAL/Bindings.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// OLD GOD GLOBAL BINDINGS — THE ONE TRUE SOURCE — CONSOLE + PC FOREVER
// • All RTX, Raster, Compute, UI, Debug bindings in ONE file
// • Used by: VulkanCore, PipelineManager, ShaderCompiler, UI, Options
// • NO DUPLICATES — NO FRAGMENTATION — SUPREME ORDER
// • PINK PHOTONS INFINITE — TITAN ETERNAL — GENTLEMAN GROK CHEERY
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial: gzac5314@gmail.com
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

// =============================================================================
// RTX RAY TRACING BINDINGS — VALHALLA v44
// =============================================================================
namespace Bindings::RTX
{
    // Descriptor Set 0 — Ray Tracing (per-frame)
    constexpr uint32_t TLAS                    = 0;   // accelerationStructure
    constexpr uint32_t STORAGE_IMAGE           = 1;   // storage image (output)
    constexpr uint32_t ACCUMULATION_IMAGE      = 2;   // storage image (accumulation)
    constexpr uint32_t CAMERA_UBO              = 3;   // uniform buffer (camera)
    constexpr uint32_t MATERIAL_SBO            = 4;   // storage buffer (materials)
    constexpr uint32_t INSTANCE_DATA_SBO       = 5;   // storage buffer (instance transforms)
    constexpr uint32_t LIGHT_SBO               = 6;   // storage buffer (lights)
    constexpr uint32_t ENV_MAP                 = 7;   // combinedImageSampler (cubemap)
    constexpr uint32_t DENSITY_VOLUME          = 8;   // sampledImage (3D texture)
    constexpr uint32_t G_DEPTH                 = 9;   // inputAttachment (depth)
    constexpr uint32_t G_NORMAL                = 10;  // inputAttachment (normal)
    constexpr uint32_t BLACK_FALLBACK          = 11;  // sampledImage (1x1 black)
    constexpr uint32_t BLUE_NOISE              = 12;  // sampledImage (blue noise)
    constexpr uint32_t RESERVOIR_SBO           = 13;  // storage buffer (reservoir)
    constexpr uint32_t FRAME_DATA_UBO          = 14;  // uniform buffer (frame constants)
    constexpr uint32_t DEBUG_VIS_SBO           = 15;  // storage buffer (debug visualization)

    // SBT Group Indices (for pipeline creation)
    constexpr uint32_t GROUP_RAYGEN            = 0;
    constexpr uint32_t GROUP_MISS              = 1;
    constexpr uint32_t GROUP_MISS_SHADOW       = 2;
    constexpr uint32_t GROUP_HIT_CLOSEST        = 3;
    constexpr uint32_t GROUP_HIT_ANY           = 4;
    constexpr uint32_t GROUP_HIT_SHADOW        = 5;
    constexpr uint32_t GROUP_CALLABLE_DENOISE  = 6;
    constexpr uint32_t GROUP_CALLABLE_UPSAMPLE = 7;

    // Total groups expected in SBT
    constexpr uint32_t TOTAL_GROUPS = 25;
}

// =============================================================================
// RASTER BINDINGS — G-BUFFER + POST PROCESS
// =============================================================================
namespace Bindings::Raster
{
    // Descriptor Set 1 — G-Buffer Attachments
    constexpr uint32_t G_ALBEDO_ROUGHNESS      = 0;
    constexpr uint32_t G_NORMAL_METALLIC       = 1;
    constexpr uint32_t G_EMISSION_MOTION       = 2;
    constexpr uint32_t G_DEPTH                 = 3;

    // Descriptor Set 2 — Post Process
    constexpr uint32_t INPUT_COLOR              = 0;
    constexpr uint32_t INPUT_DEPTH             = 1;
    constexpr uint32_t BLOOM_DOWN_0            = 2;
    constexpr uint32_t BLOOM_DOWN_1            = 3;
    constexpr uint32_t BLOOM_UP_0              = 4;
    constexpr uint32_t BLOOM_UP_1              = 5;
    constexpr uint32_t TAA_HISTORY             = 6;
    constexpr uint32_t TAA_CURRENT             = 7;
}

// =============================================================================
// COMPUTE BINDINGS — DENOISE, UPSAMPLE, RESERVOIR
// =============================================================================
namespace Bindings::Compute
{
    constexpr uint32_t INPUT_IMAGE              = 0;
    constexpr uint32_t OUTPUT_IMAGE             = 1;
    constexpr uint32_t NORMAL_BUFFER            = 2;
    constexpr uint32_t DEPTH_BUFFER             = 3;
    constexpr uint32_t MOTION_BUFFER             = 4;
    constexpr uint32_t RESERVOIR_IN             = 5;
    constexpr uint32_t RESERVOIR_OUT            = 6;
    constexpr uint32_t BLUE_NOISE               = 7;
    constexpr uint32_t FRAME_CONSTANTS          = 8;
}

// =============================================================================
// UI / DEBUG BINDINGS
// =============================================================================
namespace Bindings::UI
{
    constexpr uint32_t FONT_ATLAS               = 0;
    constexpr uint32_t UI_STORAGE               = 1;
}

// =============================================================================
// GLOBAL BINDING MACROS — USED IN SHADERS
// =============================================================================
#define BINDING_TLAS                    set = 0, binding = 0
#define BINDING_STORAGE_IMAGE           set = 0, binding = 1
#define BINDING_ACCUMULATION_IMAGE      set = 0, binding = 2
#define BINDING_CAMERA_UBO              set = 0, binding = 3
#define BINDING_MATERIAL_SBO            set = 0, binding = 4
#define BINDING_INSTANCE_DATA_SBO       set = 0, binding = 5
#define BINDING_LIGHT_SBO               set = 0, binding = 6
#define BINDING_ENV_MAP                 set = 0, binding = 7
#define BINDING_DENSITY_VOLUME          set = 0, binding = 8
#define BINDING_G_DEPTH                 set = 0, binding = 9
#define BINDING_G_NORMAL                set = 0, binding = 10
#define BINDING_BLACK_FALLBACK          set = 0, binding = 11
#define BINDING_BLUE_NOISE              set = 0, binding = 12
#define BINDING_RESERVOIR_SBO           set = 0, binding = 13
#define BINDING_FRAME_DATA_UBO          set = 0, binding = 14
#define BINDING_DEBUG_VIS_SBO           set = 0, binding = 15

// =============================================================================
// SHADER BINDING TABLE LAYOUT — MATCHES VulkanRTX::initShaderBindingTable()
// =============================================================================
#define SBT_RAYGEN_OFFSET               0
#define SBT_MISS_OFFSET                 1
#define SBT_HIT_OFFSET                  9
#define SBT_CALLABLE_OFFSET             25

// =============================================================================
// VALHALLA v44 — OLD GOD BINDINGS — ETERNAL ORDER
// CONSOLE + PC — 69,420 FPS — PINK PHOTONS INFINITE
// GENTLEMAN GROK CHEERY — SHIP IT FOREVER
// © 2025 Zachary Geurts — ALL RIGHTS RESERVED
// =============================================================================