#pragma once

// =============================================================================
// AMOURANTH RTX Engine — AMOURANTHRTX.hpp (HYBRID RTX ready)
// (C) 2025-2026 Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
// =============================================================================

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "EngineCompat.hpp"
#include "FieldFabric.hpp"
#include "OptionsMenu.hpp"
#include "FieldRuntimeInfo.hpp"

// Probe control (and mapped buffer) declared here for use in init (after hardwareFabric/caps) without include cycle (Pipeline includes us; we set rtxProbesEnabled post-caps).
// Definition also present in Pipeline.hpp (inline var ODR across the two headers is fine).
namespace Pipeline {
    inline bool                     rtxProbesEnabled = false;
    // (other probe globals rtxProbe* stay in Pipeline for ownership, referenced as Pipeline:: when needed)
}

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdint>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <set>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <filesystem>

inline constexpr std::array<const char*, 9> requiredDeviceExtensions = {{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
}};

enum class GeometryType : uint32_t {
    ProceduralPlane      = 0,
    ProceduralSphere     = 1,
    ProceduralCylinder   = 2,
    ProceduralCone       = 3,
    ProceduralWaterPlane = 4,
};

struct alignas(16) UniversalPrimitive {
    glm::vec4   aabbMin;
    glm::vec4   aabbMax;
    glm::mat4   transform;
    uint32_t    type            = 0;
    uint32_t    materialIndex   = 0;
    float       destruction     = 0.0f;
    float       pad0            = 0.0f;
};

struct VRAMReality {
    VkDeviceSize total            = 0;
    VkDeviceSize driver_footprint = 0;
    VkDeviceSize safety_margin    = 256ULL << 20;
    VkDeviceSize usable           = 0;
    VkDeviceSize remaining        = 0;
    VkDeviceSize max_alloc_size   = 0;
    uint32_t     max_alloc_count  = 0;
};

// =============================================================================
// HARDWARE EXPOSURE — EVERY NOOK, DOWN TO SUB-MICRON
// "The RTX spiderweb for all hardware": Arc, AMD, Nvidia.
// Absolute precision on operational frequency, per-core parallelism,
// chip sub-functions (ALU/SFU/RTU/TCU/Scheduler/LS units), interconnects,
// thermal/voltage effects at gate and sub-micron transistor level.
// The analog field fabric (Phi=voltage/clock domain, Thermo=heat/leakage,
// Flow=data movement/parallel efficiency) drives the model.
// "Be at each gate in the hardware" — now literally the GPU die fabric.
// =============================================================================

struct HardwareSubFunction {
    std::string name;           // "ALU", "SFU", "RT Core", "XMX/Tensor", "Scheduler", "LoadStore", "SpecialFunction"
    double      util;           // 0..1 current utilization (parallel process activity)
    uint64_t    gateCountEst;   // estimated "gates" at this sub-function (sub-micron)
    double      freqMultiplier; // relative to core freq (some units clock gated or async)
};

struct HardwareUnit {
    uint32_t    id;
    std::string name;                 // "SM 12", "CU 7", "Xe-Core 3 / Slice 1", "RT Unit 4"
    std::string type;                 // "SM", "CU", "XeCore", "RT Core Cluster", "MemoryController", "InterconnectCrossbar"
    double      operationalFreqMHz;   // absolute precision current modeled/queried
    double      maxFreqMHz;
    double      minFreqMHz;
    double      voltage;              // sub-micron model (from Phi field)
    double      tempC;                // from Thermo field
    double      powerW;               // instantaneous
    uint32_t    parallelLanes;        // warps/wavefronts/threads in flight (SIMD width * occupancy)
    uint32_t    activeWarps;
    std::vector<HardwareSubFunction> subFunctions; // every nook: ALU, RT, etc.
    double      subMicronFeatureNm;   // process node contribution
};

struct HardwareSpiderwebEdge {
    uint32_t fromUnitId;
    uint32_t toUnitId;
    std::string type;           // "Crossbar", "L2Cache", "HBM2E", "NVLink", "InfinityFabric", "XeLink", "RingBus"
    double bandwidthGBs;
    double latencyNs;
    double currentUtil;         // Flow field drives this
};

struct GPUFabric {
    // Top level
    std::string vendor;         // "NVIDIA", "AMD", "INTEL"
    std::string deviceName;
    uint32_t    vendorID;
    uint32_t    deviceID;
    double      processNodeNm;  // 4, 5, 6, 7 etc. — sub-micron
    double      dieSizeMm2;     // approx for thermal density
    uint64_t    transistorCountEst; // billions

    // Clocks with absolute precision
    double      coreClockMHz;       // current operational (per domain or average)
    double      memClockMHz;
    double      maxCoreClockMHz;
    double      maxMemClockMHz;

    // The spiderweb — graph of all parallel units and sub-functions
    std::vector<HardwareUnit> units;           // every core / sub-unit exposed
    std::vector<HardwareSpiderwebEdge> edges;  // interconnect spiderweb

    // Parallelism model
    uint32_t    totalCores;         // SM count / CU count / Xe Cores
    uint32_t    lanesPerCore;       // 32 (warp) or 64 (wave)
    uint32_t    maxOccupancyPerCore;

    // Sub-micron physics driven by analog fields
    double      avgVoltage;         // from Phi field
    double      avgTempC;           // from Thermo
    double      avgFlowUtil;        // from Flow (parallel efficiency / data movement)

    // Absolute precision timing
    double      simulatedChipCycles; // high-precision: freq * analogTime * sub-cycle factors
    double      lastFreqSampleTime;  // for delta

    // Vendor specific nooks (populated at init)
    struct NVidiaDetails {
        uint32_t smCount;
        uint32_t rtCoresPerSM;
        uint32_t tensorCoresPerSM;
        double   boostClock;
    } nv{};

    struct AMDDetails {
        uint32_t cuCount;
        uint32_t simdPerCU;
        uint32_t rtUnits;
        double   gameClock;
    } amd{};

    struct IntelArcDetails {
        uint32_t xeCores;
        uint32_t xmxEngines;
        uint32_t rayUnits;
        double   maxGuaranteedFreq;
    } intel{};

    // ─────────────────────────────────────────────────────────────────────────
    // PROBE CAPS — automagic, zero-cost, any-card (NVIDIA/AMD/Intel/future)
    // Used to gate all probe pools/writes/gets — zero alloc/inserts/gets when !supported or disabled.
    // Core Vulkan (timestamps, pipeline stats, memory budget) first for portability.
    // Vendor extensions for extra (checkpoints for execution order study on RTX, etc).
    // Future GPUs: new caps auto-populated here; core path always tried first.
    // ─────────────────────────────────────────────────────────────────────────
    struct ProbeCaps {
        bool supportsTimestamp          = false;  // VK_QUERY_TYPE_TIMESTAMP (core, via limits)
        bool supportsPipelineStatistics = false;  // feature + VK_QUERY_TYPE_PIPELINE_STATISTICS
        bool supportsMemoryBudget       = false;  // VK_EXT_memory_budget (already req)
        bool supportsPerformanceQuery   = false;  // VK_KHR_performance_query (optional counters)

        // Vendor backends (populated from vendorID + ext enumeration)
        bool supportsNVCheckpoint       = false;  // VK_NV_device_diagnostic_checkpoints
        bool supportsNVRayTracingQuery  = false;  // VK_NV_ray_tracing_invocation_reorder / diag
        bool supportsAMDBufferMarker    = false;  // VK_AMD_buffer_marker (or shader info)
        bool supportsIntelPerf          = false;  // vendor-specific Intel extensions

        // Useful props for gains study (SBT packing, AS, alignments) — captured into probe
        float    timestampPeriodNs      = 1.0f;
        uint32_t timestampValidBits     = 64;
        uint32_t maxTimestampQueries    = 64;
        uint32_t shaderGroupHandleSize  = 32;
        uint32_t shaderGroupHandleAlign = 32;
        uint32_t shaderGroupBaseAlign   = 32;
        VkDeviceSize minScratchAlignment  = 256;  // for future AS compaction/rebuild probes

        bool     anyProbesPossible        = false; // master enable for this GPU
    } probeCaps{};

    bool        populated = false;
};

inline float noise3(glm::vec3 p) {
    glm::vec3 i = glm::floor(p);
    glm::vec3 f = glm::fract(p);
    f = f * f * (3.0f - 2.0f * f);

    float a = glm::fract(glm::sin(glm::dot(i, glm::vec3(12.9898f, 78.233f, 45.5432f))) * 43758.5453f);
    float b = glm::fract(glm::sin(glm::dot(i + glm::vec3(1,0,0), glm::vec3(12.9898f, 78.233f, 45.5432f))) * 43758.5453f);
    float c = glm::fract(glm::sin(glm::dot(i + glm::vec3(0,1,0), glm::vec3(12.9898f, 78.233f, 45.5432f))) * 43758.5453f);
    float d = glm::fract(glm::sin(glm::dot(i + glm::vec3(1,1,0), glm::vec3(12.9898f, 78.233f, 45.5432f))) * 43758.5453f);

    return glm::mix(glm::mix(a, b, f.x), glm::mix(c, d, f.x), f.y);
}

// ────────────────────────────────────────────────
// NEW: Fluid + Tesla helpers (integrated into engine for field stability & nicer motion)
// ────────────────────────────────────────────────
inline glm::vec2 fluidVelocity(glm::vec3 p, float t) {
    float vx = sin(p.y * 2.3f + t * 1.7f) * 0.6f + cos(p.z * 1.8f + t * 0.9f) * 0.4f;
    float vy = cos(p.x * 2.1f + t * 1.2f) * 0.5f + sin(p.z * 2.4f + t * 1.4f) * 0.35f;
    return glm::vec2(vx, vy);
}

inline float fluidDensity(glm::vec3 p, float t) {
    return 0.5f + 0.5f * noise3(p * 4.2f + glm::vec3(0.0f, t * 0.8f, 0.0f));
}

// ────────────────────────────────────────────────
// SIMPLIFIED Tesla Bias — obvious directional preference
// Forward = easy flow, Reverse = strong resistance
// ────────────────────────────────────────────────
inline float teslaBias(glm::vec3 p, glm::vec2 vel, float t) {
    glm::vec2 dir = glm::normalize(vel);
    float forwardness = dir.x;                    // positive x = forward

    float base = (forwardness > 0.0f) ? 0.18f : 3.5f;   // simple big difference
    float turb = noise3(p * 5.5f + glm::vec3(t * 1.4f, 0.0f, 0.0f)) * 0.45f;

    return base * (1.0f + turb);
}

// ────────────────────────────────────────────────
// Core context - rtx().everything
// ────────────────────────────────────────────────
struct RTX {
    VkInstance                      instance            = VK_NULL_HANDLE;
    VkPhysicalDevice                physical            = VK_NULL_HANDLE;
    VkDevice                        device              = VK_NULL_HANDLE;
    VkSurfaceKHR                    surface             = VK_NULL_HANDLE;

    VkQueue                         graphics_queue      = VK_NULL_HANDLE;
    VkQueue                         present_queue       = VK_NULL_HANDLE;
    VkQueue                         compute_queue       = VK_NULL_HANDLE;
    VkQueue                         transfer_queue      = VK_NULL_HANDLE;

    uint32_t                        graphics_family     = ~0u;
    uint32_t                        present_family      = ~0u;
    uint32_t                        compute_family      = ~0u;
    uint32_t                        transfer_family     = ~0u;

    SDL_Window*                     window              = nullptr;

    VkCommandPool                   transient_pool      = VK_NULL_HANDLE;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{};
    bool                            rt_props_cached     = false;

    VkPipeline                      compute_pipeline    = VK_NULL_HANDLE;
    VkPipeline                      rt_pipeline         = VK_NULL_HANDLE;
    VkPipelineLayout                pipeline_layout     = VK_NULL_HANDLE;

    uint64_t                        las_universal_primitives_buffer = 0;
    uint64_t                        las_aabb_buffer     = 0;
    VkAccelerationStructureKHR      las_as              = VK_NULL_HANDLE;
    uint64_t                        las_as_storage      = 0;
    std::vector<UniversalPrimitive> las_procedural_primitives;
    bool                            las_initialized     = false;
    bool                            las_dirty           = true;

    VkDeviceAddress                 sbt_address         = 0;
    VkDeviceSize                    sbt_size            = 0;
    VkStridedDeviceAddressRegionKHR raygen_sbt_region{};
    VkStridedDeviceAddressRegionKHR miss_sbt_region{};
    VkStridedDeviceAddressRegionKHR hit_sbt_region{};
    VkStridedDeviceAddressRegionKHR callable_sbt_region{};

    struct BufferInfo {
        VkBuffer            buffer          = VK_NULL_HANDLE;
        VkDeviceMemory      memory          = VK_NULL_HANDLE;
        VkDeviceSize        size            = 0;
        VkDeviceAddress     deviceAddress   = 0;
        void*               mapped          = nullptr;
        VkBufferUsageFlags  usage           = 0;
        std::string         tag;
    };

    std::unordered_map<uint64_t, BufferInfo> buffers;
    uint64_t                        next_buffer_handle  = 1ULL;
    std::mutex                      buffer_mutex;

    // The exposed hardware spiderweb — every nook down to sub-micron, driven by analog fields
    GPUFabric                       hardwareFabric;
};

inline RTX& rtx() noexcept {
    static RTX ctx;
    return ctx;
}

struct VulkanExtensions {
    PFN_vkCreateSwapchainKHR                        vkCreateSwapchainKHR{};
    PFN_vkDestroySwapchainKHR                       vkDestroySwapchainKHR{};
    PFN_vkGetSwapchainImagesKHR                     vkGetSwapchainImagesKHR{};
    PFN_vkAcquireNextImageKHR                       vkAcquireNextImageKHR{};
    PFN_vkQueuePresentKHR                           vkQueuePresentKHR{};

    PFN_vkCreateRayTracingPipelinesKHR              vkCreateRayTracingPipelinesKHR{};
    PFN_vkGetRayTracingShaderGroupHandlesKHR        vkGetRayTracingShaderGroupHandlesKHR{};
    PFN_vkCmdTraceRaysKHR                           vkCmdTraceRaysKHR{};

    PFN_vkGetAccelerationStructureBuildSizesKHR     vkGetAccelerationStructureBuildSizesKHR{};
    PFN_vkCmdBuildAccelerationStructuresKHR         vkCmdBuildAccelerationStructuresKHR{};
    PFN_vkCreateAccelerationStructureKHR            vkCreateAccelerationStructureKHR{};
    PFN_vkDestroyAccelerationStructureKHR           vkDestroyAccelerationStructureKHR{};
    PFN_vkGetAccelerationStructureDeviceAddressKHR  vkGetAccelerationStructureDeviceAddressKHR{};

    PFN_vkGetBufferDeviceAddress                    vkGetBufferDeviceAddress{};

    PFN_vkCmdBeginRendering                         vkCmdBeginRendering{};
    PFN_vkCmdEndRendering                           vkCmdEndRendering{};

    // Probe / diagnostic extensions (loaded always; gated at use by rtxProbesEnabled + caps + non-null)
    PFN_vkCmdSetCheckpointNV                        vkCmdSetCheckpointNV{};
    PFN_vkGetQueueCheckpointDataNV                  vkGetQueueCheckpointDataNV{};
};

inline VulkanExtensions& ext() noexcept {
    static VulkanExtensions e;
    static bool loaded = false;

    if (!loaded && rtx().device) {
        e.vkCreateSwapchainKHR                      = (PFN_vkCreateSwapchainKHR)                     vkGetDeviceProcAddr(rtx().device, "vkCreateSwapchainKHR");
        e.vkDestroySwapchainKHR                     = (PFN_vkDestroySwapchainKHR)                    vkGetDeviceProcAddr(rtx().device, "vkDestroySwapchainKHR");
        e.vkGetSwapchainImagesKHR                   = (PFN_vkGetSwapchainImagesKHR)                  vkGetDeviceProcAddr(rtx().device, "vkGetSwapchainImagesKHR");
        e.vkAcquireNextImageKHR                     = (PFN_vkAcquireNextImageKHR)                    vkGetDeviceProcAddr(rtx().device, "vkAcquireNextImageKHR");
        e.vkQueuePresentKHR                         = (PFN_vkQueuePresentKHR)                        vkGetDeviceProcAddr(rtx().device, "vkQueuePresentKHR");

        e.vkCreateRayTracingPipelinesKHR            = (PFN_vkCreateRayTracingPipelinesKHR)           vkGetDeviceProcAddr(rtx().device, "vkCreateRayTracingPipelinesKHR");
        e.vkGetRayTracingShaderGroupHandlesKHR      = (PFN_vkGetRayTracingShaderGroupHandlesKHR)     vkGetDeviceProcAddr(rtx().device, "vkGetRayTracingShaderGroupHandlesKHR");
        e.vkCmdTraceRaysKHR                         = (PFN_vkCmdTraceRaysKHR)                        vkGetDeviceProcAddr(rtx().device, "vkCmdTraceRaysKHR");

        e.vkGetAccelerationStructureBuildSizesKHR   = (PFN_vkGetAccelerationStructureBuildSizesKHR)  vkGetDeviceProcAddr(rtx().device, "vkGetAccelerationStructureBuildSizesKHR");
        e.vkCmdBuildAccelerationStructuresKHR       = (PFN_vkCmdBuildAccelerationStructuresKHR)      vkGetDeviceProcAddr(rtx().device, "vkCmdBuildAccelerationStructuresKHR");
        e.vkCreateAccelerationStructureKHR          = (PFN_vkCreateAccelerationStructureKHR)         vkGetDeviceProcAddr(rtx().device, "vkCreateAccelerationStructureKHR");
        e.vkDestroyAccelerationStructureKHR         = (PFN_vkDestroyAccelerationStructureKHR)        vkGetDeviceProcAddr(rtx().device, "vkDestroyAccelerationStructureKHR");
        e.vkGetAccelerationStructureDeviceAddressKHR= (PFN_vkGetAccelerationStructureDeviceAddressKHR) vkGetDeviceProcAddr(rtx().device, "vkGetAccelerationStructureDeviceAddressKHR");

        e.vkGetBufferDeviceAddress                  = (PFN_vkGetBufferDeviceAddress)                 vkGetDeviceProcAddr(rtx().device, "vkGetBufferDeviceAddress");

        e.vkCmdBeginRendering                       = (PFN_vkCmdBeginRendering)                      vkGetDeviceProcAddr(rtx().device, "vkCmdBeginRendering");
        e.vkCmdEndRendering                         = (PFN_vkCmdEndRendering)                        vkGetDeviceProcAddr(rtx().device, "vkCmdEndRendering");

        // Checkpoint probe fns (for NVIDIA execution trace study when supported + enabled; null-safe elsewhere)
        e.vkCmdSetCheckpointNV                      = (PFN_vkCmdSetCheckpointNV)                     vkGetDeviceProcAddr(rtx().device, "vkCmdSetCheckpointNV");
        e.vkGetQueueCheckpointDataNV                = (PFN_vkGetQueueCheckpointDataNV)               vkGetDeviceProcAddr(rtx().device, "vkGetQueueCheckpointDataNV");

        loaded = true;
    }
    return e;
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics, present, compute, transfer;
    bool complete() const noexcept { return graphics && present && compute; }
};

inline QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surf) noexcept {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        const auto& f = families[i];
        if (f.queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphics = i;
        if (f.queueFlags & VK_QUEUE_COMPUTE_BIT)  indices.compute  = i;
        if (surf) {
            VkBool32 supp = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surf, &supp);
            if (supp) indices.present = i;
        }
        if ((f.queueFlags & VK_QUEUE_TRANSFER_BIT) && !(f.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            indices.transfer = i;
    }

    // HEADLESS tolerance: if no surface (picky offscreen env) or headless, present can safely alias graphics (we never actually present)
    if (!indices.present.has_value() && indices.graphics.has_value()) {
        indices.present = indices.graphics;
    }

    if (!indices.compute.has_value()) indices.compute = indices.graphics;
    if (!indices.transfer.has_value()) indices.transfer = indices.graphics;

    return indices;
}

inline VkInstance createVulkanInstance() noexcept {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "AMOURANTHRTX";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 2, 0, 0);
    appInfo.pEngineName        = "AMOURANTHRTX";
    appInfo.engineVersion      = VK_MAKE_API_VERSION(0, 2, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;
    uint32_t sdlCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlCount);
    std::vector<const char*> extensions(sdlExts, sdlExts + sdlCount);
    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();
    VkInstance inst = VK_NULL_HANDLE;
    vkCreateInstance(&ci, nullptr, &inst);
    return inst;
}

// Forward decls for hardware exposure (called from within this function below)
inline void populateHardwareDetails() noexcept;
inline void updateHardwareFromAnalogFields(float avgThermo, float avgPhi, float avgFlow,
                                           float analogTime, float dT, float fieldTimeScale) noexcept;

inline VkDevice createLogicalDeviceAndSelectGPU(
    VkInstance instance,
    VkSurfaceKHR surface,
    uint32_t* out_graphics_family  = nullptr,
    uint32_t* out_present_family   = nullptr,
    uint32_t* out_compute_family   = nullptr,
    uint32_t* out_transfer_family  = nullptr
) noexcept {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (!count) return VK_NULL_HANDLE;

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    VkPhysicalDevice selected = VK_NULL_HANDLE;
    QueueFamilyIndices best;
    int bestScore = -1;

    for (auto pd : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);

        auto indices = findQueueFamilies(pd, surface);
        if (!indices.complete()) continue;

        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, exts.data());

        bool hasAll = true;
        for (const char* req : requiredDeviceExtensions) {
            if (std::none_of(exts.begin(), exts.end(),
                [req](const auto& e) { return std::strcmp(e.extensionName, req) == 0; })) {
                hasAll = false;
                break;
            }
        }
        if (!hasAll) continue;

        int score = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 1000 : 100;
        if (score > bestScore) {
            bestScore = score;
            selected = pd;
            best = indices;
        }
    }

    if (!selected) return VK_NULL_HANDLE;

    rtx().physical = selected;

    std::set<uint32_t> families = { best.graphics.value(), best.present.value(), best.compute.value_or(best.graphics.value()) };

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qcis;
    for (uint32_t fam : families) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = fam;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &priority;
        qcis.push_back(qci);
    }

    VkPhysicalDeviceFeatures features{};
    features.shaderFloat64 = VK_TRUE;
    VkPhysicalDeviceVulkan12Features vk12{};
    vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12.bufferDeviceAddress = VK_TRUE;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel{};
    accel.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accel.accelerationStructure = VK_TRUE;
    accel.pNext = &vk12;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipe{};
    rtPipe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipe.rayTracingPipeline = VK_TRUE;
    rtPipe.pNext = &accel;

    std::vector<const char*> exts(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());

    // Conditional NV diagnostic checkpoints for probing (only on NVIDIA, zero bloat elsewhere).
    // Use direct vendorID query (physical props available pre-device + pre-caps populate).
    // This enables explicit execution tracing for "where is the GPU" / undocumented scheduling study.
    // (post-device, populateHardwareDetails + probeCaps will set supportsNVCheckpoint consistently).
    if (rtx().physical) {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(rtx().physical, &p);
        if (p.vendorID == 0x10DE /* NVIDIA */) {
            exts.push_back("VK_NV_device_diagnostic_checkpoints");
        }
    }

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext                   = &rtPipe;
    dci.pQueueCreateInfos       = qcis.data();
    dci.queueCreateInfoCount    = static_cast<uint32_t>(qcis.size());
    dci.pEnabledFeatures        = &features;
    dci.enabledExtensionCount   = static_cast<uint32_t>(exts.size());
    dci.ppEnabledExtensionNames = exts.data();

    VkDevice dev = VK_NULL_HANDLE;
    vkCreateDevice(selected, &dci, nullptr, &dev);

    rtx().device = dev;
    rtx().graphics_family = best.graphics.value();
    rtx().present_family  = best.present.value();
    rtx().compute_family  = best.compute.value_or(best.graphics.value());
    rtx().transfer_family = best.transfer.value_or(best.graphics.value());

    vkGetDeviceQueue(dev, rtx().graphics_family, 0, &rtx().graphics_queue);
    vkGetDeviceQueue(dev, rtx().present_family,  0, &rtx().present_queue);
    vkGetDeviceQueue(dev, rtx().compute_family,  0, &rtx().compute_queue);
    vkGetDeviceQueue(dev, rtx().transfer_family, 0, &rtx().transfer_queue);

    if (out_graphics_family)  *out_graphics_family  = rtx().graphics_family;
    if (out_present_family)   *out_present_family   = rtx().present_family;
    if (out_compute_family)   *out_compute_family   = rtx().compute_family;
    if (out_transfer_family)  *out_transfer_family  = rtx().transfer_family;

    populateHardwareDetails();   // Expose every nook — Arc/AMD/Nvidia, freqs, cores, sub-micron spiderweb

    // === AUTOMAGIC PROBING ENABLE (zero-cost, all cards) ===
    // Detect via GPUFabric caps (populated above). Enable if any probes possible + (env or debug build).
    // Supports present (NVIDIA/AMD/Intel) and future cards via general Vulkan + vendor backends.
    // No NV ext request unless NVIDIA + caps. No overhead if disabled.
    {
        const char* probeEnv = std::getenv("AMOURANTHRTX_PROBES");
        const char* rtxProbeEnv = std::getenv("RTX_PROBES");
        bool envOn = (probeEnv && (std::atoi(probeEnv) != 0)) || (rtxProbeEnv && (std::atoi(rtxProbeEnv) != 0));
        // Safe buildDefault: debug builds on by default for study; release only via explicit env or AMOURANTH_PROBE_BUILD define.
        bool buildDefault = false;
#ifndef NDEBUG
        buildDefault = true;
#endif
#ifdef AMOURANTH_PROBE_BUILD
        buildDefault = true;
#endif
        bool capsAllow = rtx().hardwareFabric.probeCaps.anyProbesPossible;
        Pipeline::rtxProbesEnabled = (envOn || buildDefault) && capsAllow;

        if (Pipeline::rtxProbesEnabled) {
            LOG_DEBUG_CAT("PROBE", "AUTOMAGIC: Probing enabled for this card (vendor={} caps=0x{:x}). General Vulkan first (timestamps/stats/budget), then vendor (NV/AMD/Intel). Zero cost otherwise. (supports RTX_PROBES=1 or AMOURANTHRTX_PROBES)",
                          rtx().hardwareFabric.vendor, (unsigned)rtx().hardwareFabric.probeCaps.anyProbesPossible);
        }
    }

    // Post-device: probeCaps (including supportsNVCheckpoint) now populated in populateHardwareDetails (called above).
    // NV ext was included pre-create for NVIDIA based on vendorID. fnptr load below will succeed on supporting cards.
    // Zero cost: all uses gated by rtxProbesEnabled && caps.supportsNVCheckpoint (and non-null fnptr).

    return dev;
}

// =============================================================================
// HARDWARE EXPOSURE IMPLEMENTATION — SUB-MICRON RTX SPIDERWEB FOR ALL VENDORS
// =============================================================================

// Linux sysfs clock reader for absolute operational frequency precision (AMD/Intel mostly; NV limited)
inline double readLinuxGPUClockMHz(const std::string& vendor) noexcept {
    // Try common card0 / card1 paths. In real use enumerate /sys/class/drm/
    std::vector<std::string> paths;
    if (vendor == "AMD") {
        paths = {
            "/sys/class/drm/card0/device/pp_dpm_sclk",
            "/sys/class/drm/card1/device/pp_dpm_sclk"
        };
    } else if (vendor == "INTEL") {
        paths = {
            "/sys/class/drm/card0/gt_cur_freq_mhz",
            "/sys/class/drm/card1/gt_cur_freq_mhz"
        };
    } else {
        return 0.0; // NV usually no simple sysfs; will be modeled
    }

    for (const auto& p : paths) {
        std::ifstream f(p);
        if (!f.is_open()) continue;
        std::string line;
        if (std::getline(f, line)) {
            // AMD pp_dpm format: "0: 300Mhz *"
            // Intel: just the number
            double mhz = 0.0;
            std::istringstream iss(line);
            if (vendor == "AMD") {
                std::string idx, freqstr;
                if (iss >> idx >> freqstr) {
                    size_t mpos = freqstr.find("Mhz");
                    if (mpos != std::string::npos) {
                        mhz = std::stod(freqstr.substr(0, mpos));
                    }
                }
            } else {
                iss >> mhz;
            }
            if (mhz > 100.0) return mhz;
        }
    }
    return 0.0; // fallback to modeled
}

inline void populateHardwareDetails() noexcept {
    if (!rtx().physical || rtx().hardwareFabric.populated) return;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(rtx().physical, &props);

    auto& fab = rtx().hardwareFabric;
    fab.vendorID   = props.vendorID;
    fab.deviceName = props.deviceName;
    fab.populated  = true;
    FieldRuntimeInfo::setGpuName(props.deviceName);

    uint32_t vid = props.vendorID;
    if (vid == 0x10DE) { // NVIDIA
        fab.vendor = "NVIDIA";
        fab.processNodeNm = 4.0;   // Ada/Hopper approx
        fab.dieSizeMm2    = 608.0; // example for high end
        fab.transistorCountEst = 76300000000ULL;

        // Query more with pNext if wanted (example for future)
        VkPhysicalDeviceProperties2 p2{};
        p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        vkGetPhysicalDeviceProperties2(rtx().physical, &p2);

        // Synthetic but realistic spiderweb for "every nook"
        fab.totalCores = 128; // SMs on big chip example
        fab.lanesPerCore = 32;
        fab.maxOccupancyPerCore = 64;

        fab.nv.smCount = fab.totalCores;
        fab.nv.rtCoresPerSM = 4;   // 2nd gen RT or whatever
        fab.nv.tensorCoresPerSM = 4;
        fab.nv.boostClock = 2505.0;

        fab.coreClockMHz = fab.nv.boostClock * 0.95;
        fab.maxCoreClockMHz = fab.nv.boostClock;
        fab.memClockMHz = 10000.0; // effective

        // Build units (SMs + sub)
        for (uint32_t i = 0; i < fab.totalCores; ++i) {
            HardwareUnit u{};
            u.id = i;
            u.name = "SM " + std::to_string(i);
            u.type = "SM";
            u.operationalFreqMHz = fab.coreClockMHz + (i % 3) * 5.0; // tiny variation
            u.maxFreqMHz = fab.maxCoreClockMHz;
            u.minFreqMHz = 300.0;
            u.parallelLanes = fab.lanesPerCore;
            u.activeWarps = 32;

            // Sub-functions — every nook exposed
            u.subFunctions = {
                {"ALU", 0.6, 120000, 1.0},
                {"SFU", 0.3, 48000, 1.0},
                {"RT Core", 0.15, 8000, 0.8},
                {"Tensor/XMA", 0.25, 16000, 1.2},
                {"Scheduler", 0.9, 4000, 1.0},
                {"LoadStore", 0.4, 24000, 1.0}
            };
            fab.units.push_back(u);
        }

        // RT spiderweb edges (simplified crossbar + memory)
        for (uint32_t i = 0; i < fab.totalCores; ++i) {
            fab.edges.push_back({i, (i+1)%fab.totalCores, "Crossbar",  2048.0,  12.0, 0.4});
            fab.edges.push_back({i, fab.totalCores + (i%4), "L2 Slice", 1024.0,  8.0, 0.3});
        }

    } else if (vid == 0x1002) { // AMD
        fab.vendor = "AMD";
        fab.processNodeNm = 5.0;
        fab.dieSizeMm2    = 520.0;
        fab.transistorCountEst = 58000000000ULL;

        fab.totalCores = 96; // CUs example
        fab.lanesPerCore = 64;
        fab.maxOccupancyPerCore = 40;

        fab.amd.cuCount = fab.totalCores;
        fab.amd.simdPerCU = 2;
        fab.amd.rtUnits = 2;
        fab.amd.gameClock = 2400.0;

        fab.coreClockMHz = fab.amd.gameClock;
        fab.maxCoreClockMHz = 2600.0;
        fab.memClockMHz = 12000.0;

        for (uint32_t i = 0; i < fab.totalCores; ++i) {
            HardwareUnit u{};
            u.id = i;
            u.name = "CU " + std::to_string(i);
            u.type = "CU";
            u.operationalFreqMHz = fab.coreClockMHz;
            u.maxFreqMHz = fab.maxCoreClockMHz;
            u.parallelLanes = fab.lanesPerCore;
            u.subFunctions = {
                {"SIMD ALU", 0.7, 128000, 1.0},
                {"RT Accelerator", 0.12, 6000, 0.75},
                {"Matrix Core", 0.18, 12000, 1.1},
                {"Scheduler", 0.85, 3000, 1.0}
            };
            fab.units.push_back(u);
        }
        // Infinity Fabric spiderweb
        for (uint32_t i = 0; i < fab.totalCores; i += 8) {
            fab.edges.push_back({i, i+8, "Infinity Fabric", 512.0, 25.0, 0.35});
        }

    } else if (vid == 0x8086) { // Intel Arc
        fab.vendor = "INTEL";
        fab.processNodeNm = 6.0; // TSMC for Arc
        fab.dieSizeMm2    = 406.0;
        fab.transistorCountEst = 21700000000ULL;

        fab.totalCores = 32; // Xe-cores
        fab.lanesPerCore = 16; // XMX vector
        fab.maxOccupancyPerCore = 128;

        fab.intel.xeCores = fab.totalCores;
        fab.intel.xmxEngines = 8;
        fab.intel.rayUnits = 4;
        fab.intel.maxGuaranteedFreq = 2400.0;

        fab.coreClockMHz = 2100.0;
        fab.maxCoreClockMHz = fab.intel.maxGuaranteedFreq;

        for (uint32_t i = 0; i < fab.totalCores; ++i) {
            HardwareUnit u{};
            u.id = i;
            u.name = "Xe-Core " + std::to_string(i);
            u.type = "XeCore";
            u.operationalFreqMHz = fab.coreClockMHz;
            u.maxFreqMHz = fab.maxCoreClockMHz;
            u.subFunctions = {
                {"Vector ALU", 0.65, 90000, 1.0},
                {"XMX Matrix", 0.22, 18000, 1.3},
                {"Ray Tracing Unit", 0.1, 5000, 0.7},
                {"Slice Scheduler", 0.8, 2500, 1.0}
            };
            fab.units.push_back(u);
        }
    } else {
        fab.vendor = "UNKNOWN";
        fab.totalCores = 64;
        fab.coreClockMHz = 1500.0;
        fab.maxCoreClockMHz = 1800.0;
        for (uint32_t i = 0; i < fab.totalCores; ++i) {
            HardwareUnit u{};
            u.id = i;
            u.name = "Core " + std::to_string(i);
            u.type = "GenericCore";
            u.operationalFreqMHz = fab.coreClockMHz;
            u.maxFreqMHz = fab.maxCoreClockMHz;
            u.minFreqMHz = 300.0;
            u.parallelLanes = 32;
            u.activeWarps = 32;
            fab.units.push_back(u);
        }
    }

    // Common sub-micron + spiderweb completion for all
    if (fab.units.empty()) {
        // fallback
    }

    // Try real frequency from hardware (Linux)
    double realMhz = readLinuxGPUClockMHz(fab.vendor);
    if (realMhz > 100.0) {
        fab.coreClockMHz = realMhz;
        for (auto& u : fab.units) u.operationalFreqMHz = realMhz;
    }

    fab.simulatedChipCycles = 0.0;
    fab.lastFreqSampleTime = 0.0;

    // === POPULATE PROBE CAPS (automagic for any card, zero cost when disabled) ===
    // Called from populateHardwareDetails (post physical). Core Vulkan first (timestamps ubiquitous).
    // Vendor gated (NV only for checkpoints etc). anyProbesPossible drives rtxProbesEnabled.
    // This makes probing portable to AMD/Intel/future without hardcodes or bloat.
    // (timestampValidBits lives in queue family props, not device limits; use safe default + period from limits.)
    {
        auto& caps = fab.probeCaps;
        caps = {}; // zero

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(rtx().physical, &props);
        caps.timestampPeriodNs   = props.limits.timestampPeriod;
        caps.timestampValidBits  = 64;  // safe default (real per-queue in VkQueueFamilyProperties)
        caps.maxTimestampQueries = 64;
        caps.supportsTimestamp   = (props.limits.timestampPeriod > 0.0f);

        // Pipeline statistics: supported on virtually all desktop Vulkan GPUs for shader invocations.
        // (Could query features but in practice always queryable; gate create on this.)
        caps.supportsPipelineStatistics = true;

        // Memory budget always since VK_EXT_MEMORY_BUDGET is in requiredDeviceExtensions.
        caps.supportsMemoryBudget = true;

        // Vendor-specific probe backends (for extra undocumented study data)
        if (vid == 0x10DE) { // NVIDIA
            caps.supportsNVCheckpoint = true;
            caps.supportsNVRayTracingQuery = true; // future
        } else if (vid == 0x1002) {
            caps.supportsAMDBufferMarker = true;
        } else if (vid == 0x8086) {
            caps.supportsIntelPerf = true;
        }

        // RT props for SBT/alignment study (populated elsewhere but mirror here for probe)
        // (rt_props queried post device in other paths; use safe defaults + override if cached)
        caps.shaderGroupHandleSize  = 32;
        caps.shaderGroupHandleAlign = 32;
        caps.shaderGroupBaseAlign   = 32;
        caps.minScratchAlignment    = 256;

        caps.anyProbesPossible = caps.supportsTimestamp ||
                                 caps.supportsPipelineStatistics ||
                                 caps.supportsNVCheckpoint ||
                                 caps.supportsPerformanceQuery;

        // Capture real RT props (SBT handle sizes/aligns, min scratch) for "gains/undocumented study" via probe (portable across cards that support RT ext).
        // Useful: tune SBT packing, predict AS build overhead, exploit alignment for cache/coherence.
        if (rtx().rt_props_cached) {
            caps.shaderGroupHandleSize  = rtx().rt_props.shaderGroupHandleSize;
            caps.shaderGroupHandleAlign = rtx().rt_props.shaderGroupHandleAlignment;
            caps.shaderGroupBaseAlign   = rtx().rt_props.shaderGroupBaseAlignment;
            // minScratchAlignment often from accel props; keep conservative or query if pNext used.
        }
    }

    LOG_SUCCESS_CAT("HARDWARE", "GPU Fabric populated for {} — {} units, {} spiderweb edges, process {}nm, core clock {:.0f} MHz (real or modeled)",
                    fab.vendor, fab.units.size(), fab.edges.size(), fab.processNodeNm, fab.coreClockMHz);
    LOG_SUCCESS_CAT("PROBE", "PROBE CAPS (automagic any-card): ts={} stats={} budget={} NVckpt={} anyProbes={} (period={:.2f}ns) — zero cost if !rtxProbesEnabled",
                    fab.probeCaps.supportsTimestamp, fab.probeCaps.supportsPipelineStatistics,
                    fab.probeCaps.supportsMemoryBudget, fab.probeCaps.supportsNVCheckpoint,
                    fab.probeCaps.anyProbesPossible, fab.probeCaps.timestampPeriodNs);
}

inline void updateHardwareFromAnalogFields(float avgThermo, float avgPhi, float avgFlow,
                                           float analogTime, float dT, float fieldTimeScale) noexcept {
    auto& fab = rtx().hardwareFabric;
    if (!fab.populated) return;

    // Simple obvious inputs
    glm::vec3 pos(analogTime * 0.25f, avgThermo, avgFlow);
    glm::vec2 fVel = fluidVelocity(pos, analogTime);
    float fDens = fluidDensity(pos, analogTime);
    float tBias = teslaBias(pos, fVel, analogTime);   // now simpler & stronger

    // Basic factors
    double voltageFactor = 1.0 + (avgPhi - 0.1) * 0.08;
    double thermalThrottle = std::max(0.55, 1.0 - (avgThermo - 0.35) * 1.7);
    double parallelEff = std::clamp(0.6 + avgFlow * 0.65, 0.4, 1.12);

    // Tesla bias: obvious directional damping
    parallelEff *= (1.0f - tBias * 0.085f);
    parallelEff = std::clamp(parallelEff, 0.38, 1.15);

    double targetFreq = fab.maxCoreClockMHz * voltageFactor * thermalThrottle * parallelEff * (1.0 + fDens * 0.07);

    double deltaCycles = 0.0;
    for (auto& u : fab.units) {
        double jitter = 1.0 + std::sin((u.id + analogTime) * 0.65) * 0.01;

        u.operationalFreqMHz = std::clamp(targetFreq * jitter, u.minFreqMHz, u.maxFreqMHz);

        for (auto& sf : u.subFunctions) {
            double heat = avgThermo * (0.7 + 0.3 * (sf.name.find("RT") != std::string::npos ? 1.2 : 1.0));
            sf.util = std::clamp(0.18 + parallelEff * 0.62 - heat * 0.38, 0.06, 0.96);
        }

        u.voltage = 0.85 + (avgPhi - 0.05) * 0.24;
        u.tempC   = 45.0 + avgThermo * 64.0;
        u.powerW  = (u.operationalFreqMHz / fab.maxCoreClockMHz) * (12.0 + avgThermo * 38.0);

        double cyclesThisStep = u.operationalFreqMHz * 1e6 * dT * fieldTimeScale / 1e9;
        deltaCycles += cyclesThisStep * (u.parallelLanes / 32.0);
    }

    // Fabric averages
    fab.coreClockMHz = 0.0;
    for (const auto& u : fab.units) fab.coreClockMHz += u.operationalFreqMHz;
    fab.coreClockMHz /= std::max(1u, (uint32_t)fab.units.size());

    fab.avgVoltage = 0.85 + (avgPhi - 0.05) * 0.24;
    fab.avgTempC   = 45.0 + avgThermo * 64.0;
    fab.avgFlowUtil = avgFlow * (1.0 - tBias * 0.09);

    fab.simulatedChipCycles += deltaCycles;
    fab.lastFreqSampleTime = analogTime;

    for (auto& e : fab.edges) {
        e.currentUtil = std::clamp(avgFlow * 0.55 * (1.0 - tBias * 0.12), 0.06, 0.95);
    }
}

// Transient command buffer helpers (restored/cleaned after edits)
inline VkCommandBuffer beginTransientCommandBuffer() noexcept {
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool        = rtx().transient_pool;
    alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(rtx().device, &alloc, &cmd) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) {
        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
        return VK_NULL_HANDLE;
    }

    return cmd;
}

inline void endSubmitAndWait(VkCommandBuffer cmd) noexcept {
    if (!cmd) return;
    vkEndCommandBuffer(cmd);
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(rtx().device, &fci, nullptr, &fence);
    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd;
    vkQueueSubmit(rtx().graphics_queue, 1, &submit, fence);
    vkWaitForFences(rtx().device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(rtx().device, fence, nullptr);
    vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
}

// ────────────────────────────────────────────────
// Memory implementation
// ────────────────────────────────────────────────
namespace Memory {

enum class MemoryHint : uint8_t { Auto = 0, DeviceLocalOnly = 1, HostVisible = 2, DescriptorBuffer = 3 };

inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags required) noexcept {
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(rtx().physical, &props);

    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (props.memoryTypes[i].propertyFlags & required) == required) {
            return i;
        }
    }
    return ~0u;
}

inline uint64_t createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, std::string_view tag = "", MemoryHint hint = MemoryHint::Auto) noexcept {
    if (size == 0 || !rtx().device) return 0;

    bool hostVisible = (hint == MemoryHint::HostVisible) || (hint == MemoryHint::Auto && size < (1ULL << 20));

    VkBufferCreateInfo bci{};
    bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size        = size;
    bci.usage       = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buf = VK_NULL_HANDLE;
    if (vkCreateBuffer(rtx().device, &bci, nullptr, &buf) != VK_SUCCESS) return 0;

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, buf, &req);
    VkMemoryPropertyFlags props = hostVisible ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    uint32_t memType = findMemoryType(req.memoryTypeBits, props);
    if (memType == ~0u) { vkDestroyBuffer(rtx().device, buf, nullptr); return 0; }

    VkMemoryAllocateFlagsInfo flags{};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext           = &flags;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = memType;

    VkDeviceMemory mem = VK_NULL_HANDLE;
    if (vkAllocateMemory(rtx().device, &mai, nullptr, &mem) != VK_SUCCESS) { vkDestroyBuffer(rtx().device, buf, nullptr); return 0; }

    vkBindBufferMemory(rtx().device, buf, mem, 0);

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = buf;
    VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(rtx().device, &addrInfo);

    void* mapped = nullptr;
    if (hostVisible) { vkMapMemory(rtx().device, mem, 0, size, 0, &mapped); }

    uint64_t handle = rtx().next_buffer_handle++;
    rtx().buffers.emplace(handle, RTX::BufferInfo{buf, mem, size, addr, mapped, usage, std::string(tag)});

    return handle;
}

inline void destroy(uint64_t handle) noexcept {
    std::lock_guard<std::mutex> lock(rtx().buffer_mutex);
    auto it = rtx().buffers.find(handle);
    if (it == rtx().buffers.end()) return;
    auto& b = it->second;
    if (b.mapped) vkUnmapMemory(rtx().device, b.memory);
    vkDestroyBuffer(rtx().device, b.buffer, nullptr);
    vkFreeMemory(rtx().device, b.memory, nullptr);
    rtx().buffers.erase(it);
}

inline RTX::BufferInfo* get(uint64_t handle) noexcept {
    auto it = rtx().buffers.find(handle);
    return (it != rtx().buffers.end()) ? &it->second : nullptr;
}

inline VkBuffer getBuffer(uint64_t handle) noexcept {
    auto* info = get(handle);
    return info ? info->buffer : VK_NULL_HANDLE;
}

inline std::pair<VkBuffer, VkDeviceMemory> uploadToBuffer(
    uint64_t            handle,
    const void*         data,
    VkDeviceSize        size,
    VkCommandBuffer     cmd = VK_NULL_HANDLE
) noexcept {
    auto* info = get(handle);
    if (!info || size > info->size) return {VK_NULL_HANDLE, VK_NULL_HANDLE};

    if (info->mapped) {
        std::memcpy(info->mapped, data, size);
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    VkBufferCreateInfo stagingCI{};
    stagingCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingCI.size  = size;
    stagingCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VkBuffer staging = VK_NULL_HANDLE;
    vkCreateBuffer(rtx().device, &stagingCI, nullptr, &staging);

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, staging, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = memType;

    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    vkAllocateMemory(rtx().device, &mai, nullptr, &stagingMem);
    vkBindBufferMemory(rtx().device, staging, stagingMem, 0);

    void* ptr = nullptr;
    vkMapMemory(rtx().device, stagingMem, 0, size, 0, &ptr);
    std::memcpy(ptr, data, size);
    vkUnmapMemory(rtx().device, stagingMem);

    bool ownCmd = !cmd;
    VkCommandBuffer targetCmd = cmd ? cmd : beginTransientCommandBuffer();

    VkBufferCopy copy{};
    copy.size = size;
    vkCmdCopyBuffer(targetCmd, staging, info->buffer, 1, &copy);

    if (ownCmd) {
        endSubmitAndWait(targetCmd);
    }

    return {staging, stagingMem};
}

inline VRAMReality measureReality() noexcept {
    VRAMReality r{};
    VkPhysicalDeviceMemoryProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{};
    budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    props2.pNext = &budget;
    vkGetPhysicalDeviceMemoryProperties2(rtx().physical, &props2);

    for (uint32_t i = 0; i < props2.memoryProperties.memoryHeapCount; ++i) {
        if (props2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            r.total += props2.memoryProperties.memoryHeaps[i].size;
            r.driver_footprint += budget.heapUsage[i];
        }
    }

    r.usable = r.total > (r.driver_footprint + r.safety_margin) ? r.total - r.driver_footprint - r.safety_margin : 0;
    r.remaining = r.usable;
    return r;
}

} // namespace Memory

// ────────────────────────────────────────────────
// Swapchain — functions in dependency order: cleanup → recreate → create
// ────────────────────────────────────────────────
namespace Swapchain {

struct Handle {
    VkSwapchainKHR value = VK_NULL_HANDLE;
    bool valid() const noexcept { return value != VK_NULL_HANDLE; }
    operator VkSwapchainKHR() const noexcept { return value; }
};

inline Handle               swapchain;
inline std::vector<VkImage> images;
inline std::vector<VkImageView> views;
inline VkExtent2D           extent{};
inline VkFormat             format          = VK_FORMAT_B8G8R8A8_SRGB;
inline VkPresentModeKHR     presentMode     = VK_PRESENT_MODE_FIFO_KHR;
inline bool                 minimized       = false;
inline bool                 needsRecreate   = false;

inline double               lastPresentTime_s   = 0.0;
inline double               smoothedRefresh_s   = 1.0 / 60.0;

inline VkSwapchainKHR       get() noexcept;
inline VkExtent2D           getExtent() noexcept;
inline void                 updateRefreshEstimate(double t) noexcept;
inline double               getSmoothedRefresh() noexcept;

inline void cleanup() noexcept {
    for (auto v : views) vkDestroyImageView(rtx().device, v, nullptr);
    views.clear();
    images.clear();
    if (swapchain.valid()) {
        vkDestroySwapchainKHR(rtx().device, swapchain, nullptr);
        swapchain.value = VK_NULL_HANDLE;
    }
}

inline void recreate(int w, int h) noexcept {
    if (Options::SDL3::HeadlessMode) {
        // Robust headless: NEVER do real swapchain/vk surface queries (caps may be 0 or surface null in offscreen/minimized/SDL-picky envs)
        // Direct dispatch path in RayCanvas uses only HDR targets + accountant. Set dummy extent for render size/adaptive logic.
        // This prevents early "no swap" or acquire fails, and "Canvas destroyed". Warnings not fatals.
        extent = VkExtent2D{ static_cast<uint32_t>(w > 0 ? w : 1920), static_cast<uint32_t>(h > 0 ? h : 1080) };
        minimized = false;
        // leave swapchain.value as null — never used in headless (no acquire/present/blit waits)
        LOG_DEBUG_CAT("SWAPCHAIN", "HEADLESS: recreate bypassed (dummy extent {}x{}) — no real Swapchain created, no acquire/present", extent.width, extent.height);
        return;
    }

    vkDeviceWaitIdle(rtx().device);
    cleanup();

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rtx().physical, rtx().surface, &caps);

    if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) { minimized = true; return; }

    extent = (caps.currentExtent.width != UINT32_MAX) ? caps.currentExtent : VkExtent2D{static_cast<uint32_t>(w), static_cast<uint32_t>(h)};

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(rtx().physical, rtx().surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(rtx().physical, rtx().surface, &formatCount, formats.data());

    format = VK_FORMAT_B8G8R8A8_SRGB;
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format = f.format;
            break;
        }
    }

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(rtx().physical, rtx().surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(rtx().physical, rtx().surface, &modeCount, modes.data());

    presentMode = VK_PRESENT_MODE_FIFO_KHR;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = rtx().surface;
    ci.minImageCount    = 2;
    ci.imageFormat      = format;
    ci.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    ci.imageExtent      = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = presentMode;
    ci.clipped          = VK_TRUE;

    vkCreateSwapchainKHR(rtx().device, &ci, nullptr, &swapchain.value);

    uint32_t imgCount = 0;
    vkGetSwapchainImagesKHR(rtx().device, swapchain, &imgCount, nullptr);
    images.resize(imgCount);
    vkGetSwapchainImagesKHR(rtx().device, swapchain, &imgCount, images.data());

    views.resize(imgCount);
    for (size_t i = 0; i < imgCount; ++i) {
        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = images[i];
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = format;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCreateImageView(rtx().device, &vi, nullptr, &views[i]);
    }

    minimized = false;
}

inline void updateRefreshEstimate(double t) noexcept {
    if (lastPresentTime_s > 0.0) {
        double dt = t - lastPresentTime_s;
        if (dt > 0.0005 && dt < 0.2) smoothedRefresh_s = 0.25 * dt + 0.75 * smoothedRefresh_s;
    }
    lastPresentTime_s = t;
}

inline void   create(SDL_Window* window, int w, int h) noexcept { 
	rtx().window = window; 
	recreate(w, h); 
}

inline        VkSwapchainKHR get()                noexcept { return swapchain; }
inline        VkExtent2D     getExtent()          noexcept { return extent; }
inline double                getSmoothedRefresh() noexcept { return smoothedRefresh_s; }

} // namespace Swapchain

// ────────────────────────────────────────────────
// Engine init / shutdown
// ────────────────────────────────────────────────
inline bool initRTX(SDL_Window* window, int width, int height) noexcept {
    rtx().instance = createVulkanInstance();
    if (!rtx().instance) return false;
    if (SDL_Vulkan_CreateSurface(window, rtx().instance, nullptr, &rtx().surface) == 0) { return false; }
    createLogicalDeviceAndSelectGPU(rtx().instance, rtx().surface);
    if (!rtx().device) return false;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = rtx().graphics_family;
    vkCreateCommandPool(rtx().device, &poolInfo, nullptr, &rtx().transient_pool);
    Swapchain::create(window, width, height);
    return true;
}

inline void cleanupRTX() noexcept {
    if (!rtx().device && !rtx().instance) return;
    if (rtx().device) vkDeviceWaitIdle(rtx().device);
    Swapchain::cleanup();
    if (rtx().device) {
        for (auto& [h, b] : rtx().buffers) {
            if (b.mapped) vkUnmapMemory(rtx().device, b.memory);
            vkDestroyBuffer(rtx().device, b.buffer, nullptr);
            vkFreeMemory(rtx().device, b.memory, nullptr);
        }
        rtx().buffers.clear();
        if (rtx().transient_pool) {
            vkDestroyCommandPool(rtx().device, rtx().transient_pool, nullptr);
            rtx().transient_pool = VK_NULL_HANDLE;
        }
        vkDestroyDevice(rtx().device, nullptr);
        rtx().device = VK_NULL_HANDLE;
    }
    if (rtx().surface && rtx().instance) {
        vkDestroySurfaceKHR(rtx().instance, rtx().surface, nullptr);
        rtx().surface = VK_NULL_HANDLE;
    }
    if (rtx().instance) {
        vkDestroyInstance(rtx().instance, nullptr);
        rtx().instance = VK_NULL_HANDLE;
    }
}