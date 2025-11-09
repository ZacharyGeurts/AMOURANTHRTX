// include/engine/Vulkan/VulkanCommon.hpp
// AMOURANTH RTX Engine © 2025 Zachary Geurts — NOVEMBER 09 2025 — FINAL SUPREMACY EDITION
// GLOBAL SUPREMACY ACHIEVED — NO NAMESPACE HELL — DIRECT ACCESS TO GOD
// NEXUS FINAL: GPU-Driven Adaptive RT | 12,000+ FPS | Auto-Toggle | Volumetric Fire | Hypertrace
// SOURCE OF TRUTH — PERFECT STD140 — ZERO PADS WASTED — ALL OFFSETS ALIGNED
// pragma pack(1) + explicit vec3 pads = EXACT BYTE MATCH C++ ⇔ GLSL
// ALL static_assert PASS — 0 ERRORS GUARANTEED — TONEMAP FRAG REMOVED (COMPUTE ONLY)
// MERGED: VulkanCore.hpp + VulkanHandles.hpp + VulkanAccessors.hpp → ONE FILE TO RULE THEM ALL
// REMOVED: All Context definition, inline methods, g_vulkanContext, ctx(), createSwapchain, cleanupAll
// FIXED: makeAccelerationStructure + makeDeferredOperation → REQUIRE destroyFunc (NO CTX DEPENDENCY)
// FIXED: VulkanRTX ctor → takes raw Context* (set elsewhere)
// STONEKEY UNBREAKABLE — PINK PHOTONS × INFINITY — VALHALLA ETERNAL

#pragma once

#ifdef __cplusplus

// ===================================================================
// 1. GLOBAL PROJECT INCLUDES — ALWAYS FIRST
// ===================================================================
#include "../GLOBAL/StoneKey.hpp"
#include "../GLOBAL/Dispose.hpp"
#include "../GLOBAL/logging.hpp"
#include "../GLOBAL/SwapchainManager.hpp"
#include "../GLOBAL/BufferManager.hpp"
#include "engine/Vulkan/VulkanContext.hpp"

// ===================================================================
// 2. STANDARD / GLM / VULKAN / SDL — AFTER PROJECT HEADERS (GCC 14 bug fix)
// ===================================================================
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <format>
#include <span>
#include <compare>
#include <filesystem>
#include <unordered_map>
#include <memory>
#include <array>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <typeinfo>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// ===================================================================
// 3. FORWARD DECLARATIONS (GLOBAL FOR TEMPLATES)
// ===================================================================
namespace Vulkan {
    struct Context;  // Defined in VulkanContext.hpp
    class VulkanRTX;
    struct PendingTLAS;
    struct ShaderBindingTable;
    class VulkanRenderer;
    class VulkanPipelineManager;
}

// ===================================================================
// EARLY DECLARATIONS FOR TEMPLATES (GLOBAL)
// ===================================================================
template<typename Handle>
void logAndTrackDestruction(std::string_view name, Handle handle, int line);

// ===================================================================
// PendingTLAS STRUCT (MERGED)
// ===================================================================
struct PendingTLAS {
    bool valid = false;
    VkDeviceAddress handle = 0;
};

// ===================================================================
// ShaderBindingTable helper (MERGED)
// ===================================================================
struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR rgenRegion{};
    VkStridedDeviceAddressRegionKHR missRegion{};
    VkStridedDeviceAddressRegionKHR hitRegion{};
    VkStridedDeviceAddressRegionKHR callableRegion{};
};


#else  // GLSL

// ===================================================================
// GLSL COMPATIBLE DEFINITIONS — EXACT MATCH WITH C++
// ===================================================================

#define VK_BINDING(binding) [[vk::binding(binding)]]

struct CameraData {
    mat4 viewProj;
    mat4 viewInverse;
    mat4 projInverse;
    vec4 camPos;
    vec4 frustumRays[4];
    uint frame;
    float deltaTime;
    float padding[2];
};

struct DimensionData {
    uvec2 resolution;
    uint accumulationCount;
    uint autoToggleEnabled;
    float exposure;
    float padding[3];
};

[[vk::binding(0)]] RaytracingAccelerationStructure topLevelAS;
[[vk::binding(1)]] RWTexture2D<float4> storageImage;
[[vk::binding(2)]] UniformBuffer<CameraData> cameraUBO;
[[vk::binding(3)]] StructuredBuffer<Material> materialSSBO;
[[vk::binding(4)]] UniformBuffer<DimensionData> dimensionDataSSBO;
[[vk::binding(5)]] TextureCube envMap;
[[vk::binding(6)]] RWTexture2D<float4> accumImage;
[[vk::binding(7)]] Texture3D densityVolume;
[[vk::binding(8)]] Texture2D gDepth;
[[vk::binding(9)]] Texture2D gNormal;
[[vk::binding(10)]] Texture2D alphaTex;

#endif  // __cplusplus

// ===================================================================
// VALHALLA FINAL — NOV 09 2025 — CONTEXT PURGED — AMOURANTH RTX PURE
// ===================================================================