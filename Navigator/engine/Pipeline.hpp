#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Pipeline (HYBRID: raymarched compute + full hardware RT with SBT + RTXGI ready)
// Updated with Fluid Dynamics + Tesla Valve field logic for stability, nicer organic motion,
// and more physical thermo accounting.
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "OptionsMenu.hpp"
#include "Camera.hpp"
#include "Materials.hpp"
#include "SDL3.hpp"
#include "FieldDevices.hpp"
#include "FieldBios.hpp"
#include "FieldDos.hpp"
#include "FieldAmmoExec.hpp"
#include "FieldDosConfig.hpp"
#include "FieldPlatform.hpp"
#include "FieldDosDisplay.hpp"
#include "FieldDosViewport.hpp"
#include "FieldDosChrome.hpp"
#include "FieldInput.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldMscdex.hpp"
#include "FieldLayer.hpp"
#include "FieldAmouranthExec.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldAmouranthTextures.hpp"
#include "FieldAmouranthOs.hpp"
#include "FieldAosLoading.hpp"
#include "FieldAmouranthWm.hpp"
#include "FieldRtxTerm.hpp"
#include "FieldAosStatusBar.hpp"
#include "FieldWmStatusBar.hpp"
#include "FieldRtxBoot.hpp"
#include "FieldRtxShell.hpp"
#include "FieldRtxConsoleGui.hpp"
#include "FieldAmmoNes.hpp"

#include "FieldAmmoBrowser.hpp"
#include "FieldBrowserHook.hpp"
#include "FieldX86Emu.hpp"
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <vector>
#include <fstream>
#include <format>
#include <filesystem>
#include <cstdint>
#include <string>
#include <string_view>
#include <expected>
#include <cmath>
#include <cstring>

namespace Pipeline {

using u32 = std::uint32_t;
using f32 = float;

// ────────────────────────────────────────────────
// Audio command constants
// ────────────────────────────────────────────────
constexpr f32 AUDIO_CMD_PLAY        = 0.8f;
constexpr f32 AUDIO_CMD_VOLUME      = 0.35f;
constexpr f32 AUDIO_CMD_STOP        = -0.5f;
constexpr f32 AUDIO_CMD_PAUSE       = -0.3f;

// ────────────────────────────────────────────────
// Push constant stage flags (used by compute + all ray tracing stages)
// ────────────────────────────────────────────────
constexpr VkShaderStageFlags PUSH_STAGE_FLAGS =
    VK_SHADER_STAGE_COMPUTE_BIT |
    VK_SHADER_STAGE_RAYGEN_BIT_KHR |
    VK_SHADER_STAGE_MISS_BIT_KHR |
    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

// ────────────────────────────────────────────────
// Descriptor bindings (set = 0) — shared between compute and ray tracing
// PROPALACTIC: 0/1 = HDR canvas, 4=materials, 6=audio, 7=TLAS, 8/9/10 = ANALOG FIELDS (Phi, Thermo, Flow)
// ────────────────────────────────────────────────
struct CanvasBindings {
    static constexpr VkDescriptorSetLayoutBinding bindings[] = {
        // 0 - Current output HDR image (write)
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},

        // 1 - Previous frame HDR image (read for accumulation / TAA / RTXGI)
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},

        // 2 - THERMO ACCOUNTANT (explicit entropy production, boundary maintenance cost, free energy income)
        // Phase 1 schematic implementation: previous-frame "memory" + probe dissipation is costly.
        // Shaders read here to participate in the thermodynamic story (pay for coherence, read local temp).
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
        // 3 - Reserved dummy buffer (layout slot for SPIR-V / driver compatibility; always written with valid dummy)
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
        // 5 - Reserved dummy buffer (layout slot for SPIR-V / driver compatibility; always written with valid dummy)
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},

        // 4 - Material library storage buffer
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | 
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},

        // 6 - Audio command buffer (shader → host)
        {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},

        // 7 - Top Level Acceleration Structure (TLAS) for hardware ray tracing + RTXGI
        {7, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT, nullptr},

        // 8 - Analog Field Phi (potential / wave phase / gate voltage) — read/write evolution + probe
        {8, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},

        // 9 - Analog Field Thermo (temperature + entropy density + energy) — thermo laws + blackbody
        {9, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},

        // 10 - Analog Field Flow (velocity / momentum advection) — propalactic tides + gate flow
        {10, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
    };
};

// x86.comp / Big Grin FieldSocket die — minimal descriptor layout (image + die SSBO)
struct FieldBindings {
    static constexpr VkDescriptorSetLayoutBinding bindings[] = {
        {0,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {8,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {9,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {10, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {11, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {12, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {13, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
};

inline constexpr uint32_t FIELD_LAYOUT_VERSION = 4u;
inline uint32_t           field_layout_version_built = 0u;

inline VkImage        ammoTextureImage  = VK_NULL_HANDLE;
inline VkImageView    ammoTextureView   = VK_NULL_HANDLE;
inline VkDeviceMemory ammoTextureMemory = VK_NULL_HANDLE;
inline uint32_t       ammoTextureWidth  = 0u;
inline uint32_t       ammoTextureHeight = 0u;

inline VkImage        aosStartTextureImage  = VK_NULL_HANDLE;
inline VkImageView    aosStartTextureView   = VK_NULL_HANDLE;
inline VkDeviceMemory aosStartTextureMemory = VK_NULL_HANDLE;
inline uint32_t       aosStartTextureWidth  = 0u;
inline uint32_t       aosStartTextureHeight = 0u;

inline VkImage        aosWallTextureImage  = VK_NULL_HANDLE;
inline VkImageView    aosWallTextureView   = VK_NULL_HANDLE;
inline VkDeviceMemory aosWallTextureMemory = VK_NULL_HANDLE;
inline uint32_t       aosWallTextureWidth  = 0u;
inline uint32_t       aosWallTextureHeight = 0u;
inline uint32_t       aosWallTextureIndex  = 0xFFFFFFFFu;

enum class CanvasKind : uint8_t { Classic, X86Fields };

// Matches FieldX86Die in x86.comp (std430) — 8 MiB guest RAM
constexpr std::size_t FIELD_X86_DIE_UINTS = FieldPlatform::FIELD_X86_DIE_UINTS;
constexpr VkDeviceSize FIELD_X86_DIE_SIZE = static_cast<VkDeviceSize>(FIELD_X86_DIE_UINTS * sizeof(u32));
constexpr std::size_t FIELD_X86_DIE_HEADER_BYTES = FieldPlatform::DIE_HEADER_UINTS * sizeof(u32);
constexpr std::size_t FIELD_X86_DIE_CYCLE_OFFSET   = FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET;

// Matches FieldSocket push_constant block in x86.comp
struct alignas(4) FieldSocket {
    f32  sealed_time;
    u32  control;
    u32  data_bus[64];
    u32  address_bus[16];
    f32  maxwell_entropy_seed;
    f32  mouse_x;   // 0..1 framebuffer, <0 disables loupe
    f32  mouse_y;
};

// ────────────────────────────────────────────────
// PushConstants — PROPALACTIC ANALOG FIELD EDITION
// Analog timings + thermo + gate fidelity + field control. RTX as field probe.
// ────────────────────────────────────────────────
struct alignas(16) PushConstants {
    f32         time;                       // wall / visual time
    f32         analogTime;                 // continuous living analog time (propagates fields)
    f32         dT;                         // sub-frame analog delta (thermo + wave stability)
    bool        enableHardwareRT;           // F2 — switches between compute raymarch and full hardware RT
    bool        enableRTXGI;                // F3 — enables RTX Global Illumination when hardware RT is active
    u32         frameSeed;                  // for noise / randomization

    glm::vec3   cameraPos;                  f32 pad0;
    glm::vec4   cameraQuat;
    f32         cameraFovDeg;
    f32         aspectRatio;
    f32         nearPlane;
    f32         farPlane;

    f32         exposure;
    f32         vignetteStrength;
    f32         bloomThreshold;
    f32         bloomIntensity;
    u32         tonemapMode;                // 0 = off, 1 = on
    f32         contrast;
    f32         saturation;
    f32         gamma;

    glm::vec3   sunDir;                     f32 sunIntensity;
    glm::vec3   moonDir;                    f32 moonIntensity;
    glm::vec3   windDir;                    f32 windStrength;
    f32         fogDensity;
    f32         dayNightFactor;
    f32         cloudCoverage;

    f32         raymarchMaxDist;
    f32         raymarchEpsilon;
    u32         raymarchMaxSteps;

    u32         controllerInput;            // bitfield for keyboard + gamepad
    f32         leftStickX;                 f32 leftStickY;
    f32         rightStickX;                f32 rightStickY;
    f32         leftTrigger;                f32 rightTrigger;

    glm::vec2   mouseDelta;
    glm::vec2   mouseNormalized;
    f32         mouseWheelDelta;

    // ── ANALOG FIELD FABRIC (propalactic) ────────────────────────────────────
    f32         fieldTimeScale;             // multiplier on analog evolution speed
    f32         thermoAlpha;                // heat diffusion coeff
    f32         waveSpeed;                  // c for Phi wave propagation (tide)
    f32         gateFidelity;               // 0=soft analog ... 1=sharp gate nonlinearities at hardware sites
    f32         entropyFloor;               // 2nd law minimum production / diffusion bias
    f32         injectStrength;             // mouse / control probe energy injection scale
    f32         propalacticScale;           // cosmic / large wavelength driver amplitude
    f32         fieldCoupling;              // cross terms (thermo <-> wave <-> flow)
    glm::vec4   fieldProbe;                 // xy=norm mouse, z=strength, w=mode (heat=0, vortex=1, phase=2)

    f32         padField[4];                // 16-byte alignment for future field params + RTX gate probes
};

// ────────────────────────────────────────────────
// Audio command block (shader writes, host reads)
// ────────────────────────────────────────────────
struct alignas(16) AudioCommandBlock {
    f32 slotCommand[16];
    f32 slotValue[16];
    f32 reserved[16];
};

// ────────────────────────────────────────────────
// THERMO ACCOUNTANT — Phase 1 explicit entropy from the schematic
// (implements "better than Poop/THERMODYNAMIC_ANALOG_SCHEMATIC_VISION.md")
// Boundary (HDR pair + field fabric) maintenance is *costly*.
// Irreversibility (accumulation overwrite, measurement at probes) is budgeted and localized.
// Free energy (time + user attention + world potentials) pays to keep far-from-eq order.
// Entropy export happens via visual probe (swapchain) + audio + security horizon.
// Shaders may read this at binding 2 to modulate local dissipation / "pay" for coherence.
// Host always populates a thermodynamically honest story regardless of canvas .comp.
// ────────────────────────────────────────────────
struct alignas(16) ThermoAccountant {
    f32 entropyThisFrame;   // total produced this dispatch (Landauer proxy + explicit probe costs)
    f32 avgBoundaryThermo;  // mean temperature / entropy density on the holographic boundary (from fieldThermo or proxy)
    f32 prevMaintCost;      // negentropy cost to preserve previous-frame boundary coherence (accumulation memory / entanglement history)
    f32 freeEnergyIncome;   // throughput this frame (sealed time + input activity + living potentials)
    u32 steps;              // frames / substeps accounted
    f32 pad[3];
};

inline VkBuffer                 thermoAccountantBuffer   = VK_NULL_HANDLE;
inline VkDeviceMemory           thermoAccountantMemory   = VK_NULL_HANDLE;
inline void*                    thermoAccountantMapped   = nullptr;

// ────────────────────────────────────────────────
// RTXProbe — zero-cost (when disabled) explicit hardware probing for study.
// Mirrors ThermoAccountant pattern: host-visible coherent buffer, written in dispatch path,
// readable in status/ELLIE for explicit hardware results (timestamps, stats, checkpoints, props).
// Enable via env RTX_PROBES=1 or Options for study runs. Zero overhead when off (no pools, no cmd inserts, no features).
// Goal: surface undocumented/underexploited RTX behaviors (AS alignments, SBT cache, invocation order, counter quirks)
// for gains (compaction, reordering/SER, optimal memory, phase timings) without runtime tax in normal use.
// ────────────────────────────────────────────────
struct alignas(16) RTXProbe {
    f32 gpuMsCompute;           // from expanded timestamp pre/post compute dispatch
    f32 gpuMsRT;                // from timestamp pre/post traceRays (when HW RT active)
    u32 computeInvocations;     // pipeline stats COMPUTE_SHADER_INVOCATIONS (if stats query enabled)
    u32 rtInvocations;          // approximate from RT stats or shader counters (placeholder for future)
    VkDeviceSize asCompactedSize; // from AS compaction query (0 if no build/compaction probe active)
    u32 checkpointCount;        // number of NV checkpoints observed since last fetch (for execution trace)

    // ── ALIGNMENTS & PROPS FOR GAINS (populated from GPUFabric.probeCaps + rt_props at enable/create; study SBT packing, AS efficiency, cache wins on this card)
    u32 sbtHandleSize;
    u32 sbtHandleAlignment;          // exploit: pack SBT exactly, reduce waste
    u32 sbtBaseAlignment;
    VkDeviceSize minASScratchAlign;  // for future compaction/rebuild probe

    // Vendor backend snapshot + misc
    u32 nvRayReorderHints;      // placeholder
    float hostDeltaMs;          // optional host observed delta for comparison to gpuMs
    u32 pad[1];                 // padding for align16
};

inline VkBuffer                 rtxProbeBuffer   = VK_NULL_HANDLE;
inline VkDeviceMemory           rtxProbeMemory   = VK_NULL_HANDLE;
inline void*                    rtxProbeMapped   = nullptr;

// rtxProbesEnabled defined (inline) in AMOURANTHRTX.hpp (inside namespace Pipeline) to allow set-after-hardwareFabric in createLogicalDevice without include cycle (this file includes AMOURANTH). ODR satisfied by single inline def.

// ────────────────────────────────────────────────
// Global Vulkan objects for the pipeline
// ────────────────────────────────────────────────
inline VkDescriptorSetLayout    main_descriptor_layout  = VK_NULL_HANDLE;
inline VkDescriptorSetLayout    field_descriptor_layout = VK_NULL_HANDLE;
inline VkPipelineLayout         pipeline_layout         = VK_NULL_HANDLE;
inline VkPipelineLayout         field_pipeline_layout   = VK_NULL_HANDLE;
inline VkPipeline               canvas_pipeline         = VK_NULL_HANDLE;
inline CanvasKind               currentCanvasKind       = CanvasKind::Classic;
inline std::string              activeCanvasShader      = "CANVAS";

inline VkBuffer                 fieldX86DieBuffer         = VK_NULL_HANDLE;
inline VkDeviceMemory           fieldX86DieMemory         = VK_NULL_HANDLE;
inline void*                    fieldX86DieMapped         = nullptr;

// Hardware ray tracing pipeline + Shader Binding Table
inline VkPipeline               rt_pipeline             = VK_NULL_HANDLE;
inline u32                      shader_group_handle_size = 0;
inline u32                      shader_group_alignment  = 0;
inline VkBuffer                 sbt_buffer              = VK_NULL_HANDLE;
inline VkDeviceMemory           sbt_memory              = VK_NULL_HANDLE;

inline VkStridedDeviceAddressRegionKHR raygen_region = {};
inline VkStridedDeviceAddressRegionKHR miss_region   = {};
inline VkStridedDeviceAddressRegionKHR chit_region   = {};
inline VkStridedDeviceAddressRegionKHR ahit_region   = {};

struct RTShaderModules {
    VkShaderModule raygen      = VK_NULL_HANDLE;
    VkShaderModule miss        = VK_NULL_HANDLE;
    VkShaderModule closestHit  = VK_NULL_HANDLE;
};
inline RTShaderModules rt_shaders;

// Audio command storage buffer
inline VkBuffer                 audio_cmd_buffer        = VK_NULL_HANDLE;
inline VkDeviceMemory           audio_cmd_memory        = VK_NULL_HANDLE;
inline void*                    audio_cmd_mapped        = nullptr;

// Reserved storage buffers (bindings 3,5) to ensure every layout binding has a valid write.
// These are dummies for SPIR-V/layout compatibility (see CANVAS.comp comments); unused by logic but required to prevent driver faults on vkUpdateDescriptorSets / bind.
inline VkBuffer                 reservedBuffer3         = VK_NULL_HANDLE;
inline VkDeviceMemory           reservedMemory3         = VK_NULL_HANDLE;
inline VkBuffer                 reservedBuffer5         = VK_NULL_HANDLE;
inline VkDeviceMemory           reservedMemory5         = VK_NULL_HANDLE;

// ────────────────────────────────────────────────
// Helper functions
// ────────────────────────────────────────────────
[[nodiscard]] inline glm::vec3 computeSunDirection(f32 todHours) noexcept {
    f32 angle = (todHours / 24.0f) * glm::two_pi<f32>() - glm::half_pi<f32>();
    return glm::normalize(glm::vec3(std::cos(angle) * 0.8f, std::sin(angle), std::cos(angle) * 0.6f));
}

[[nodiscard]] inline glm::vec3 computeMoonDirection(f32 todHours) noexcept {
    return -computeSunDirection(todHours);
}

// ────────────────────────────────────────────────
// Shader module loader — searches multiple paths for .spv files
// ────────────────────────────────────────────────
[[nodiscard]] inline std::expected<VkShaderModule, std::string> load_shader_module(
    const std::string& shader_name) noexcept
{
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;

    // Try executable directory
    fs::path exe_dir;
    try { 
        exe_dir = fs::canonical("/proc/self/exe").parent_path(); 
    } catch (...) {}

    if (!exe_dir.empty()) {
        candidates.emplace_back(exe_dir / "assets/shaders/compute" / (shader_name + ".spv"));
        candidates.emplace_back(exe_dir / "assets/shaders/raytracing" / (shader_name + ".spv"));
        candidates.emplace_back(exe_dir / (shader_name + ".spv"));
    }

    // Try current working directory
    fs::path cwd = fs::current_path();
    candidates.emplace_back(cwd / "assets/shaders/compute" / (shader_name + ".spv"));
    candidates.emplace_back(cwd / "assets/shaders/raytracing" / (shader_name + ".spv"));
    candidates.emplace_back(cwd / (shader_name + ".spv"));

    std::vector<uint32_t> code;
    for (const auto& p : candidates) {
        if (!fs::exists(p)) continue;

        std::ifstream file(p, std::ios::binary | std::ios::ate);
        if (!file.is_open()) continue;

        auto size = file.tellg();
        file.seekg(0);
        code.resize(size / 4);
        file.read(reinterpret_cast<char*>(code.data()), size);

        if (file.good() && !code.empty()) {
            LOG_SUCCESS_CAT("PIPELINE", "Loaded shader from: {}", p.string());
            break;
        }
        code.clear();
    }

    if (code.empty()) {
        return std::unexpected(std::format("Failed to load shader: {}.spv from any search path", shader_name));
    }

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode    = code.data();

    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(rtx().device, &ci, nullptr, &mod) != VK_SUCCESS) {
        return std::unexpected("vkCreateShaderModule failed for " + shader_name);
    }

    return mod;
}

// Query ray tracing pipeline properties (run once)
inline void query_ray_tracing_properties() noexcept {
    if (shader_group_handle_size > 0) return;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;

    vkGetPhysicalDeviceProperties2(rtx().physical, &props2);

    shader_group_handle_size = rtProps.shaderGroupHandleSize;
    shader_group_alignment   = rtProps.shaderGroupHandleAlignment;
}

// Create ray tracing pipeline + Shader Binding Table (SBT)
inline void create_ray_tracing_pipeline() noexcept {
    if (rt_pipeline != VK_NULL_HANDLE) return;

    query_ray_tracing_properties();

    // Load ray tracing shaders
    auto raygen_res = load_shader_module("raygen");
    if (!raygen_res) {
        LOG_ERROR_CAT("PIPELINE", "Failed to load raygen.spv");
        return;
    }
    rt_shaders.raygen = *raygen_res;

    auto miss_res = load_shader_module("miss");
    if (!miss_res) {
        LOG_ERROR_CAT("PIPELINE", "Failed to load miss.spv");
        return;
    }
    rt_shaders.miss = *miss_res;

    auto chit_res = load_shader_module("closesthit");
    if (!chit_res) {
        LOG_ERROR_CAT("PIPELINE", "Failed to load closesthit.spv");
        return;
    }
    rt_shaders.closestHit = *chit_res;

    // Shader stages
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages(3);
    shaderStages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                        VK_SHADER_STAGE_RAYGEN_BIT_KHR, rt_shaders.raygen, "main", nullptr };

    shaderStages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                        VK_SHADER_STAGE_MISS_BIT_KHR, rt_shaders.miss, "main", nullptr };

    shaderStages[2] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rt_shaders.closestHit, "main", nullptr };

    // Shader groups
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups(3);

    shaderGroups[0].sType               = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroups[0].type                = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shaderGroups[0].generalShader       = 0;
    shaderGroups[0].closestHitShader    = VK_SHADER_UNUSED_KHR;
    shaderGroups[0].anyHitShader        = VK_SHADER_UNUSED_KHR;
    shaderGroups[0].intersectionShader  = VK_SHADER_UNUSED_KHR;

    shaderGroups[1].sType               = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroups[1].type                = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shaderGroups[1].generalShader       = 1;
    shaderGroups[1].closestHitShader    = VK_SHADER_UNUSED_KHR;
    shaderGroups[1].anyHitShader        = VK_SHADER_UNUSED_KHR;
    shaderGroups[1].intersectionShader  = VK_SHADER_UNUSED_KHR;

    shaderGroups[2].sType               = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroups[2].type                = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    shaderGroups[2].generalShader       = VK_SHADER_UNUSED_KHR;
    shaderGroups[2].closestHitShader    = 2;
    shaderGroups[2].anyHitShader        = VK_SHADER_UNUSED_KHR;
    shaderGroups[2].intersectionShader  = VK_SHADER_UNUSED_KHR;

    // Create ray tracing pipeline
    VkRayTracingPipelineCreateInfoKHR ci{};
    ci.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    ci.stageCount                   = 3;
    ci.pStages                      = shaderStages.data();
    ci.groupCount                   = 3;
    ci.pGroups                      = shaderGroups.data();
    ci.maxPipelineRayRecursionDepth = 1;                    // RTXGI usually needs 1-2
    ci.layout                       = pipeline_layout;

    if (ext().vkCreateRayTracingPipelinesKHR(rtx().device, VK_NULL_HANDLE, VK_NULL_HANDLE,
        1, &ci, nullptr, &rt_pipeline) != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to create hardware ray tracing pipeline");
        return;
    }

    // Build Shader Binding Table (SBT)
    u32 numGroups = 3;
    u32 handleSizeAligned = ((shader_group_handle_size + shader_group_alignment - 1) / shader_group_alignment) 
                            * shader_group_alignment;
    VkDeviceSize sbtTotalSize = numGroups * handleSizeAligned;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = sbtTotalSize;
    bufInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    vkCreateBuffer(rtx().device, &bufInfo, nullptr, &sbt_buffer);

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, sbt_buffer, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(rtx().device, &alloc, nullptr, &sbt_memory);
    vkBindBufferMemory(rtx().device, sbt_buffer, sbt_memory, 0);

    // Get shader group handles
    std::vector<uint8_t> handles(numGroups * shader_group_handle_size);
    ext().vkGetRayTracingShaderGroupHandlesKHR(rtx().device, rt_pipeline, 0, numGroups, handles.size(), handles.data());

    // Copy handles into SBT with proper alignment
    void* mapped = nullptr;
    vkMapMemory(rtx().device, sbt_memory, 0, sbtTotalSize, 0, &mapped);
    uint8_t* dst = static_cast<uint8_t*>(mapped);
    for (u32 i = 0; i < numGroups; ++i) {
        std::memcpy(dst + i * handleSizeAligned, 
                    handles.data() + i * shader_group_handle_size, 
                    shader_group_handle_size);
    }
    vkUnmapMemory(rtx().device, sbt_memory);

    // Get device address for regions
    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = sbt_buffer;
    VkDeviceAddress sbtDevAddr = vkGetBufferDeviceAddress(rtx().device, &addrInfo);

    raygen_region.deviceAddress = sbtDevAddr;
    raygen_region.stride        = handleSizeAligned;
    raygen_region.size          = handleSizeAligned;

    miss_region.deviceAddress   = sbtDevAddr + handleSizeAligned;
    miss_region.stride          = handleSizeAligned;
    miss_region.size            = handleSizeAligned;

    chit_region.deviceAddress   = sbtDevAddr + 2 * handleSizeAligned;
    chit_region.stride          = handleSizeAligned;
    chit_region.size            = handleSizeAligned;

    LOG_SUCCESS_CAT("PIPELINE", "Hardware ray tracing pipeline + SBT created successfully (RTXGI ready)");
}

[[nodiscard]] inline bool is_x86_canvas(std::string_view name) noexcept {
    return name == "x86"
        || Options::Canvas::IsThemeCanvas(name)
        || (name == "CANVAS" && Options::Canvas::CanvasUsesX86Die);
}

inline VkPipelineLayout active_pipeline_layout() noexcept {
    return (currentCanvasKind == CanvasKind::X86Fields) ? field_pipeline_layout : pipeline_layout;
}

// Descriptor layout creation
inline void initialize_descriptors() noexcept {
    if (main_descriptor_layout != VK_NULL_HANDLE) return;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = std::size(CanvasBindings::bindings);
    ci.pBindings    = CanvasBindings::bindings;

    VkResult res = vkCreateDescriptorSetLayout(rtx().device, &ci, nullptr, &main_descriptor_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create main descriptor set layout");
        return;
    }
    LOG_SUCCESS_CAT("PIPELINE", "Descriptor set layout created");
}

// Pipeline layout (push constants + descriptor set)
inline void create_pipeline_layout() noexcept {
    if (pipeline_layout != VK_NULL_HANDLE) return;
    initialize_descriptors();

    VkPushConstantRange push{};
    push.stageFlags = PUSH_STAGE_FLAGS;
    push.offset     = 0;
    push.size       = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo ci{};
    ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.setLayoutCount         = 1;
    ci.pSetLayouts            = &main_descriptor_layout;
    ci.pushConstantRangeCount = 1;
    ci.pPushConstantRanges    = &push;

    VkResult res = vkCreatePipelineLayout(rtx().device, &ci, nullptr, &pipeline_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create pipeline layout");
        return;
    }
    LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created");
}

inline void destroy_ammo_texture() noexcept {
    if (ammoTextureView)   { vkDestroyImageView(rtx().device, ammoTextureView, nullptr); ammoTextureView = VK_NULL_HANDLE; }
    if (ammoTextureImage)  { vkDestroyImage(rtx().device, ammoTextureImage, nullptr); ammoTextureImage = VK_NULL_HANDLE; }
    if (ammoTextureMemory) { vkFreeMemory(rtx().device, ammoTextureMemory, nullptr); ammoTextureMemory = VK_NULL_HANDLE; }
    ammoTextureWidth = ammoTextureHeight = 0u;
}

inline void create_ammo_texture() noexcept {
    if (ammoTextureImage != VK_NULL_HANDLE) return;

    static constexpr const char* kAmmoPath = "assets/textures/ammo.png";
    std::vector<uint8_t> rgba;
    int texW = 4;
    int texH = 4;

    if (std::filesystem::exists(kAmmoPath)) {
        if (SDL_Surface* surf = IMG_Load(kAmmoPath)) {
            if (SDL_Surface* conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32)) {
                texW = conv->w;
                texH = conv->h;
                rgba.resize(static_cast<size_t>(texW * texH * 4));
                std::memcpy(rgba.data(), conv->pixels, rgba.size());
                SDL_DestroySurface(conv);
            }
            SDL_DestroySurface(surf);
        } else {
            LOG_WARNING_CAT("CANVAS", "Ammo texture load failed: {} — {}", kAmmoPath, SDL_GetError());
        }
    }
    if (rgba.empty()) {
        rgba = {220u, 40u, 140u, 255u, 40u, 140u, 220u, 255u,
                40u, 140u, 220u, 255u, 220u, 40u, 140u, 255u};
        texW = texH = 2;
    }

    ammoTextureWidth  = static_cast<uint32_t>(texW);
    ammoTextureHeight = static_cast<uint32_t>(texH);

    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent        = {ammoTextureWidth, ammoTextureHeight, 1u};
    ici.mipLevels     = 1u;
    ici.arrayLayers   = 1u;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(rtx().device, &ici, nullptr, &ammoTextureImage) != VK_SUCCESS) {
        LOG_ERROR_CAT("CANVAS", "Failed to create ammo texture image");
        return;
    }

    VkMemoryRequirements imgReq{};
    vkGetImageMemoryRequirements(rtx().device, ammoTextureImage, &imgReq);
    VkMemoryAllocateInfo imgAlloc{};
    imgAlloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imgAlloc.allocationSize  = imgReq.size;
    imgAlloc.memoryTypeIndex = Memory::findMemoryType(imgReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(rtx().device, &imgAlloc, nullptr, &ammoTextureMemory) != VK_SUCCESS) {
        destroy_ammo_texture();
        return;
    }
    vkBindImageMemory(rtx().device, ammoTextureImage, ammoTextureMemory, 0);

    VkImageViewCreateInfo ivci{};
    ivci.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image                       = ammoTextureImage;
    ivci.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format                      = VK_FORMAT_R8G8B8A8_UNORM;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = 1u;
    ivci.subresourceRange.layerCount = 1u;
    if (vkCreateImageView(rtx().device, &ivci, nullptr, &ammoTextureView) != VK_SUCCESS) {
        destroy_ammo_texture();
        return;
    }

    VkBufferCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    sci.size  = rgba.size();
    sci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    vkCreateBuffer(rtx().device, &sci, nullptr, &staging);
    VkMemoryRequirements sreq{};
    vkGetBufferMemoryRequirements(rtx().device, staging, &sreq);
    VkMemoryAllocateInfo smai{};
    smai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    smai.allocationSize  = sreq.size;
    smai.memoryTypeIndex = Memory::findMemoryType(sreq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(rtx().device, &smai, nullptr, &stagingMem);
    vkBindBufferMemory(rtx().device, staging, stagingMem, 0);
    void* sptr = nullptr;
    vkMapMemory(rtx().device, stagingMem, 0, rgba.size(), 0, &sptr);
    std::memcpy(sptr, rgba.data(), rgba.size());
    vkUnmapMemory(rtx().device, stagingMem);

    VkCommandBuffer cmd = beginTransientCommandBuffer();
    VkImageMemoryBarrier toDst{};
    toDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toDst.image = ammoTextureImage;
    toDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toDst);

    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1u;
    copy.imageExtent = {ammoTextureWidth, ammoTextureHeight, 1u};
    vkCmdCopyBufferToImage(cmd, staging, ammoTextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VkImageMemoryBarrier toGeneral{};
    toGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toGeneral.image = ammoTextureImage;
    toGeneral.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toGeneral);
    endSubmitAndWait(cmd);

    vkDestroyBuffer(rtx().device, staging, nullptr);
    vkFreeMemory(rtx().device, stagingMem, nullptr);

    LOG_SUCCESS_CAT("CANVAS", "Ammo texture ready — {}x{} (assets/textures/ammo.png)", texW, texH);
}

struct RgbaImage {
    std::vector<uint8_t> pixels;
    int w = 4;
    int h = 4;
};

inline bool load_rgba_image(const char* path, RgbaImage& out, int maxW = 1920) noexcept {
    out.pixels.clear();
    out.w = out.h = 4;
    if (!path || !path[0] || !std::filesystem::exists(path)) return false;
    SDL_Surface* surf = IMG_Load(path);
    if (!surf) return false;
    SDL_Surface* conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surf);
    if (!conv) return false;
    SDL_Surface* src = conv;
    SDL_Surface* scaled = nullptr;
    if (maxW > 0) {
        int tw = conv->w;
        int th = conv->h;
        if (conv->w > maxW || conv->h > maxW) {
            if (conv->w >= conv->h) {
                tw = maxW;
                th = std::max(1, static_cast<int>(static_cast<float>(conv->h) * maxW / conv->w));
            } else {
                th = maxW;
                tw = std::max(1, static_cast<int>(static_cast<float>(conv->w) * maxW / conv->h));
            }
        } else if (conv->w < maxW / 2) {
            tw = maxW;
            th = std::max(1, static_cast<int>(static_cast<float>(conv->h) * maxW / conv->w));
        }
        if (tw != conv->w || th != conv->h) {
            scaled = SDL_ScaleSurface(conv, tw, th, SDL_SCALEMODE_LINEAR);
            if (scaled) {
                SDL_DestroySurface(conv);
                src = scaled;
            }
        }
    }
    out.w = src->w;
    out.h = src->h;
    out.pixels.resize(static_cast<size_t>(out.w * out.h * 4));
    std::memcpy(out.pixels.data(), src->pixels, out.pixels.size());
    SDL_DestroySurface(src);
    return true;
}

inline void destroy_rgba_texture(VkImage& image, VkImageView& view, VkDeviceMemory& memory,
                                 uint32_t& w, uint32_t& h) noexcept {
    if (view)   { vkDestroyImageView(rtx().device, view, nullptr); view = VK_NULL_HANDLE; }
    if (image)  { vkDestroyImage(rtx().device, image, nullptr); image = VK_NULL_HANDLE; }
    if (memory) { vkFreeMemory(rtx().device, memory, nullptr); memory = VK_NULL_HANDLE; }
    w = h = 0u;
}

inline bool upload_rgba_texture(const RgbaImage& img, VkImage& image, VkImageView& view,
                                VkDeviceMemory& memory, uint32_t& outW, uint32_t& outH) noexcept {
    destroy_rgba_texture(image, view, memory, outW, outH);
    if (img.pixels.empty() || img.w < 1 || img.h < 1) return false;

    outW = static_cast<uint32_t>(img.w);
    outH = static_cast<uint32_t>(img.h);

    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent        = {outW, outH, 1u};
    ici.mipLevels     = 1u;
    ici.arrayLayers   = 1u;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(rtx().device, &ici, nullptr, &image) != VK_SUCCESS) return false;

    VkMemoryRequirements imgReq{};
    vkGetImageMemoryRequirements(rtx().device, image, &imgReq);
    VkMemoryAllocateInfo imgAlloc{};
    imgAlloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imgAlloc.allocationSize  = imgReq.size;
    imgAlloc.memoryTypeIndex = Memory::findMemoryType(imgReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(rtx().device, &imgAlloc, nullptr, &memory) != VK_SUCCESS) {
        destroy_rgba_texture(image, view, memory, outW, outH);
        return false;
    }
    vkBindImageMemory(rtx().device, image, memory, 0);

    VkImageViewCreateInfo ivci{};
    ivci.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image                       = image;
    ivci.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format                      = VK_FORMAT_R8G8B8A8_UNORM;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = 1u;
    ivci.subresourceRange.layerCount = 1u;
    if (vkCreateImageView(rtx().device, &ivci, nullptr, &view) != VK_SUCCESS) {
        destroy_rgba_texture(image, view, memory, outW, outH);
        return false;
    }

    VkBufferCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    sci.size  = img.pixels.size();
    sci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    vkCreateBuffer(rtx().device, &sci, nullptr, &staging);
    VkMemoryRequirements sreq{};
    vkGetBufferMemoryRequirements(rtx().device, staging, &sreq);
    VkMemoryAllocateInfo smai{};
    smai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    smai.allocationSize  = sreq.size;
    smai.memoryTypeIndex = Memory::findMemoryType(sreq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(rtx().device, &smai, nullptr, &stagingMem);
    vkBindBufferMemory(rtx().device, staging, stagingMem, 0);
    void* sptr = nullptr;
    vkMapMemory(rtx().device, stagingMem, 0, img.pixels.size(), 0, &sptr);
    std::memcpy(sptr, img.pixels.data(), img.pixels.size());
    vkUnmapMemory(rtx().device, stagingMem);

    VkCommandBuffer cmd = beginTransientCommandBuffer();
    VkImageMemoryBarrier toDst{};
    toDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toDst.image = image;
    toDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toDst);

    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1u;
    copy.imageExtent = {outW, outH, 1u};
    vkCmdCopyBufferToImage(cmd, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VkImageMemoryBarrier toGeneral{};
    toGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toGeneral.image = image;
    toGeneral.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toGeneral);
    endSubmitAndWait(cmd);

    vkDestroyBuffer(rtx().device, staging, nullptr);
    vkFreeMemory(rtx().device, stagingMem, nullptr);
    return true;
}

inline void destroy_aos_textures() noexcept {
    destroy_rgba_texture(aosStartTextureImage, aosStartTextureView, aosStartTextureMemory,
        aosStartTextureWidth, aosStartTextureHeight);
    destroy_rgba_texture(aosWallTextureImage, aosWallTextureView, aosWallTextureMemory,
        aosWallTextureWidth, aosWallTextureHeight);
    aosWallTextureIndex = 0xFFFFFFFFu;
}

inline void create_aos_start_texture() noexcept {
    if (aosStartTextureImage != VK_NULL_HANDLE) return;
    RgbaImage img;
    const auto root = FieldDos::assetRoot();
    const auto atlas = (root / FieldAmouranthTextures::kIconAtlasPath).string();
    const auto start = (root / FieldAmouranthTextures::kStartIconPath).string();
    if (!load_rgba_image(atlas.c_str(), img, 0)) {
        if (!load_rgba_image(start.c_str(), img, 256)) {
            img.pixels = {220u, 40u, 140u, 255u, 255u, 200u, 220u, 255u,
                          180u, 30u, 100u, 255u, 255u, 180u, 200u, 255u};
            img.w = img.h = 2;
        }
    }
    if (upload_rgba_texture(img, aosStartTextureImage, aosStartTextureView, aosStartTextureMemory,
            aosStartTextureWidth, aosStartTextureHeight))
        LOG_SUCCESS_CAT("CANVAS", "AmouranthOS icon atlas {}x{}", aosStartTextureWidth, aosStartTextureHeight);
}

inline void set_aos_wallpaper(uint32_t index) noexcept {
    if (index >= FieldAmouranthTextures::kWallpaperCount) index = 0u;
    if (aosWallTextureImage != VK_NULL_HANDLE && aosWallTextureIndex == index) return;
    RgbaImage img;
    const char* path = FieldAmouranthTextures::wallpaperPath(index);
    const auto full = (FieldDos::assetRoot() / path).string();
    if (!load_rgba_image(full.c_str(), img, 1920)) {
        img.pixels = {90u, 20u, 50u, 255u, 200u, 60u, 100u, 255u,
                      40u, 10u, 30u, 255u, 180u, 50u, 90u, 255u};
        img.w = img.h = 2;
    }
    if (upload_rgba_texture(img, aosWallTextureImage, aosWallTextureView, aosWallTextureMemory,
            aosWallTextureWidth, aosWallTextureHeight)) {
        aosWallTextureIndex = index;
        LOG_INFO_CAT("CANVAS", "AmouranthOS wallpaper [{}] {}x{} — {}", index,
            aosWallTextureWidth, aosWallTextureHeight, path);
    }
}

inline void sync_aos_textures() noexcept {
    if (!FieldAmouranthOs::active) return;
    create_aos_start_texture();
    // Diagnostic desktop — icon atlas only; no photo wallpaper load.
}

inline void create_field_descriptor_layout() noexcept {
    if (field_descriptor_layout != VK_NULL_HANDLE && field_layout_version_built == FIELD_LAYOUT_VERSION) return;
    if (field_descriptor_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtx().device, field_descriptor_layout, nullptr);
        field_descriptor_layout = VK_NULL_HANDLE;
    }

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = std::size(FieldBindings::bindings);
    ci.pBindings    = FieldBindings::bindings;

    VkResult res = vkCreateDescriptorSetLayout(rtx().device, &ci, nullptr, &field_descriptor_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create field descriptor set layout");
        return;
    }
    field_layout_version_built = FIELD_LAYOUT_VERSION;
    LOG_SUCCESS_CAT("PIPELINE", "Field descriptor set layout created (x86 die path + ammo b11 + aos b12/b13)");
}

inline void create_field_pipeline_layout() noexcept {
    if (field_pipeline_layout != VK_NULL_HANDLE && field_layout_version_built == FIELD_LAYOUT_VERSION) return;
    if (field_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rtx().device, field_pipeline_layout, nullptr);
        field_pipeline_layout = VK_NULL_HANDLE;
    }
    create_field_descriptor_layout();

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push.offset     = 0;
    push.size       = sizeof(FieldSocket);

    VkPipelineLayoutCreateInfo ci{};
    ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.setLayoutCount         = 1;
    ci.pSetLayouts            = &field_descriptor_layout;
    ci.pushConstantRangeCount = 1;
    ci.pPushConstantRanges    = &push;

    VkResult res = vkCreatePipelineLayout(rtx().device, &ci, nullptr, &field_pipeline_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create field pipeline layout");
        return;
    }
    LOG_SUCCESS_CAT("PIPELINE", "Field pipeline layout created (FieldSocket push constants)");
}

inline void create_field_x86_die_buffer() noexcept {
    if (fieldX86DieBuffer != VK_NULL_HANDLE) return;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = FIELD_X86_DIE_SIZE;
    bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkCreateBuffer(rtx().device, &bufInfo, nullptr, &fieldX86DieBuffer);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to create fieldX86DieBuffer: {}", vkh.result(res));
        fieldX86DieBuffer = VK_NULL_HANDLE;
        return;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, fieldX86DieBuffer, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    res = vkAllocateMemory(rtx().device, &alloc, nullptr, &fieldX86DieMemory);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to alloc fieldX86DieMemory: {}", vkh.result(res));
        vkDestroyBuffer(rtx().device, fieldX86DieBuffer, nullptr);
        fieldX86DieBuffer = VK_NULL_HANDLE;
        return;
    }

    res = vkBindBufferMemory(rtx().device, fieldX86DieBuffer, fieldX86DieMemory, 0);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to bind fieldX86DieBuffer: {}", vkh.result(res));
        return;
    }

    res = vkMapMemory(rtx().device, fieldX86DieMemory, 0, FIELD_X86_DIE_SIZE, 0, &fieldX86DieMapped);
    if (res != VK_SUCCESS || !fieldX86DieMapped) {
        LOG_ERROR_CAT("PIPELINE", "Failed to map fieldX86DieMemory: {}", vkh.result(res));
        return;
    }
    std::memset(fieldX86DieMapped, 0, static_cast<std::size_t>(FIELD_X86_DIE_SIZE));
    LOG_SUCCESS_CAT("CANVAS", "FieldX86Die buffer ready (~{} KiB) — x86 debug canvas can run", FIELD_X86_DIE_SIZE / 1024);
}

// Audio command buffer
inline void create_audio_command_buffer() noexcept {
    if (audio_cmd_buffer != VK_NULL_HANDLE) return;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = sizeof(AudioCommandBlock);
    bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkCreateBuffer(rtx().device, &bufInfo, nullptr, &audio_cmd_buffer);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to create audio_cmd_buffer: {}", vkh.result(res));
        audio_cmd_buffer = VK_NULL_HANDLE;
        return;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, audio_cmd_buffer, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    res = vkAllocateMemory(rtx().device, &alloc, nullptr, &audio_cmd_memory);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to alloc audio_cmd_memory: {}", vkh.result(res));
        audio_cmd_buffer = VK_NULL_HANDLE; // leave buffer but will fail later
        return;
    }
    res = vkBindBufferMemory(rtx().device, audio_cmd_buffer, audio_cmd_memory, 0);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to bind audio_cmd_buffer: {}", vkh.result(res));
        return;
    }

    res = vkMapMemory(rtx().device, audio_cmd_memory, 0, sizeof(AudioCommandBlock), 0, &audio_cmd_mapped);
    if (res != VK_SUCCESS || !audio_cmd_mapped) {
        LOG_ERROR_CAT("PIPELINE", "Failed to map audio_cmd_memory: {}", vkh.result(res));
        audio_cmd_mapped = nullptr;
    } else {
        std::memset(audio_cmd_mapped, 0, sizeof(AudioCommandBlock));
    }
}

// Thermo accountant buffer (explicit entropy + free energy + prev maint cost)
// Always created; bound at reserved storage binding 2. Host writes honest thermo story every frame.
inline void create_thermo_accountant_buffer() noexcept {
    if (thermoAccountantBuffer != VK_NULL_HANDLE) return;

    LOG_DEBUG_CAT("THERMO", "ACCOUNTANT THERMO: create_thermo_accountant_buffer — allocating host-visible ThermoAccountant (entropyThisFrame, avgBoundaryThermo, prevMaintCost, freeEnergyIncome)");
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = sizeof(ThermoAccountant);
    bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkCreateBuffer(rtx().device, &bufInfo, nullptr, &thermoAccountantBuffer);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to create thermoAccountantBuffer: {}", vkh.result(res));
        thermoAccountantBuffer = VK_NULL_HANDLE;
        return;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, thermoAccountantBuffer, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    res = vkAllocateMemory(rtx().device, &alloc, nullptr, &thermoAccountantMemory);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to alloc thermoAccountantMemory: {}", vkh.result(res));
        thermoAccountantBuffer = VK_NULL_HANDLE;
        return;
    }
    res = vkBindBufferMemory(rtx().device, thermoAccountantBuffer, thermoAccountantMemory, 0);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to bind thermoAccountantBuffer: {}", vkh.result(res));
        return;
    }

    res = vkMapMemory(rtx().device, thermoAccountantMemory, 0, sizeof(ThermoAccountant), 0, &thermoAccountantMapped);
    if (res != VK_SUCCESS || !thermoAccountantMapped) {
        LOG_ERROR_CAT("PIPELINE", "Failed to map thermoAccountantMemory: {}", vkh.result(res));
        thermoAccountantMapped = nullptr;
    } else {
        std::memset(thermoAccountantMapped, 0, sizeof(ThermoAccountant));
    }
    LOG_DEBUG_CAT("THERMO", "ACCOUNTANT THERMO: thermoAccountantBuffer=0x{:016x} mapped=0x{:016x} — ready for per-dispatch population of prevMaintCost/freeEnergy/boundaryThermo/entropy (see dispatch_canvas + pre-trans/descriptor/clears/fabric in RayCanvas)", reinterpret_cast<uintptr_t>(thermoAccountantBuffer), reinterpret_cast<uintptr_t>(thermoAccountantMapped));
}

// Create host-visible RTXProbe buffer (zero cost when !rtxProbesEnabled).
// Populated in dispatch_canvas with explicit hardware results from expanded queries/checkpoints.
// Mirrors accountant: small, coherent, mapped, always-bound for study.
inline void create_rtx_probe_buffer() noexcept {
    if (rtxProbeBuffer != VK_NULL_HANDLE) return;
    if (!rtxProbesEnabled) {
        LOG_DEBUG_CAT("RTXPROBE", "RTX probing disabled (env/Options or default) — zero cost path active. Enable RTX_PROBES=1 for explicit hardware study (timestamps, stats, checkpoints, AS/SBT props).");
        return;
    }

    LOG_DEBUG_CAT("RTXPROBE", "RTXPROBE: create_rtx_probe_buffer — allocating host-visible RTXProbe for explicit dispatch results (gpuMs phases, invocations, AS compacted, checkpoint traces). Zero overhead in normal runs.");

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = sizeof(RTXProbe);
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkCreateBuffer(rtx().device, &bufInfo, nullptr, &rtxProbeBuffer);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("RTXPROBE", "Failed to create rtxProbeBuffer: {}", vkh.result(res));
        rtxProbeBuffer = VK_NULL_HANDLE;
        return;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, rtxProbeBuffer, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    res = vkAllocateMemory(rtx().device, &alloc, nullptr, &rtxProbeMemory);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("RTXPROBE", "Failed to alloc memory for rtxProbeBuffer: {}", vkh.result(res));
        vkDestroyBuffer(rtx().device, rtxProbeBuffer, nullptr);
        rtxProbeBuffer = VK_NULL_HANDLE;
        return;
    }

    res = vkBindBufferMemory(rtx().device, rtxProbeBuffer, rtxProbeMemory, 0);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("RTXPROBE", "Failed to bind rtxProbeBuffer: {}", vkh.result(res));
        return;
    }

    res = vkMapMemory(rtx().device, rtxProbeMemory, 0, sizeof(RTXProbe), 0, &rtxProbeMapped);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("RTXPROBE", "Failed to map rtxProbeBuffer: {}", vkh.result(res));
        rtxProbeMapped = nullptr;
        return;
    }

    // Init to zeros
    std::memset(rtxProbeMapped, 0, sizeof(RTXProbe));

    // Populate static caps/props immediately for study (SBT aligns etc always available when buffer created; useful even before first dispatch gpuMs)
    if (rtxProbeMapped) {
        auto* pr = static_cast<RTXProbe*>(rtxProbeMapped);
        const auto& caps = rtx().hardwareFabric.probeCaps;
        pr->sbtHandleSize      = caps.shaderGroupHandleSize;
        pr->sbtHandleAlignment = caps.shaderGroupHandleAlign;
        pr->sbtBaseAlignment   = caps.shaderGroupBaseAlign;
        pr->minASScratchAlign  = caps.minScratchAlignment;
    }

    LOG_DEBUG_CAT("RTXPROBE", "RTXPROBE: rtxProbeBuffer=0x{:016x} mapped=0x{:016x} — ready for per-dispatch explicit hardware results (gpuMs phases, invocations, AS compacted, checkpoint traces + SBT/align props). Study gains in alignments, coherence, invocation order on this RTX. Zero overhead when disabled.", reinterpret_cast<uintptr_t>(rtxProbeBuffer), reinterpret_cast<uintptr_t>(rtxProbeMapped));
}

// Reserved dummy buffers for bindings 3 and 5 (STORAGE_BUFFER in layout).
// Created alongside accountant/audio so updateDescriptorSet can always provide valid resources (prefer valid over count=0).
inline void create_reserved_buffers() noexcept {
    if (reservedBuffer3 != VK_NULL_HANDLE) return;

    auto makeDummy = [](VkBuffer& buf, VkDeviceMemory& mem) {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size        = 16; // minimal valid size for storage buffer dummy
        bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult res = vkCreateBuffer(rtx().device, &bufInfo, nullptr, &buf);
        if (res != VK_SUCCESS) {
            LOG_ERROR_CAT("PIPELINE", "Failed to create reserved dummy buffer: {}", vkh.result(res));
            buf = VK_NULL_HANDLE;
            return;
        }

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(rtx().device, buf, &req);

        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        res = vkAllocateMemory(rtx().device, &alloc, nullptr, &mem);
        if (res != VK_SUCCESS) {
            LOG_ERROR_CAT("PIPELINE", "Failed to alloc memory for reserved dummy: {}", vkh.result(res));
            buf = VK_NULL_HANDLE;
            return;
        }
        res = vkBindBufferMemory(rtx().device, buf, mem, 0);
        if (res != VK_SUCCESS) {
            LOG_ERROR_CAT("PIPELINE", "Failed to bind reserved dummy buffer: {}", vkh.result(res));
        }
    };

    makeDummy(reservedBuffer3, reservedMemory3);
    makeDummy(reservedBuffer5, reservedMemory5);
    LOG_DEBUG_CAT("THERMO", "PIPELINE THERMO: create_reserved_buffers — reservedBuffer3/5 dummies ready for b3/b5 valid writes in updateDescriptorSet");
}

// Create compute pipeline for the active canvas (.spv name without extension)
inline void create_canvas_pipeline(const std::string& shaderName = Options::Canvas::CurrentName()) noexcept {
    if (canvas_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr);
        canvas_pipeline = VK_NULL_HANDLE;
    }

    activeCanvasShader = shaderName;
    currentCanvasKind  = is_x86_canvas(shaderName) ? CanvasKind::X86Fields : CanvasKind::Classic;

    VkPipelineLayout layout = VK_NULL_HANDLE;

    if (currentCanvasKind == CanvasKind::X86Fields) {
        create_ammo_texture();
        create_field_pipeline_layout();
        create_field_x86_die_buffer();
        create_thermo_accountant_buffer();
        layout = field_pipeline_layout;
        LOG_INFO_CAT("CANVAS", "x86 FieldSocket canvas active — Big Grin die + Start shell");
    } else {
        create_pipeline_layout();
        create_audio_command_buffer();
        create_thermo_accountant_buffer();
        create_rtx_probe_buffer();
        create_reserved_buffers();
        layout = pipeline_layout;
        LOG_DEBUG_CAT("THERMO", "PIPELINE THERMO: classic canvas '{}' — thermo accountant + fabric path", shaderName);
        if (Options::Rendering::EnableHardwareRayTracing) {
            create_ray_tracing_pipeline();
        }
    }

    if (!layout) {
        LOG_ERROR_CAT("PIPELINE", "No pipeline layout for canvas '{}'", shaderName);
        return;
    }

    auto shader_res = load_shader_module(shaderName);
    if (!shader_res) {
        LOG_ERROR_CAT("PIPELINE", "Failed to load {}.spv — falling back to CANVAS", shaderName);
        if (shaderName != "CANVAS") {
            create_canvas_pipeline("CANVAS");
        }
        return;
    }

    VkShaderModule module = *shader_res;

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = module;
    stage.pName  = "main";

    VkComputePipelineCreateInfo ci{};
    ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ci.stage  = stage;
    ci.layout = layout;

    VkResult res = vkCreateComputePipelines(rtx().device, VK_NULL_HANDLE, 1, &ci, nullptr, &canvas_pipeline);
    vkDestroyShaderModule(rtx().device, module, nullptr);

    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("PIPELINE", "Failed to create compute pipeline for '{}'", shaderName);
        return;
    }

    LOG_SUCCESS_CAT("CANVAS", "Compute pipeline ready: {} ({})", shaderName,
        currentCanvasKind == CanvasKind::X86Fields ? "full fields / x86 die" : "classic thermo+RT");
}

// Process audio commands written by shader
inline void process_shader_audio_commands() noexcept {
    if (!audio_cmd_mapped) return;

    AudioCommandBlock* cmd = static_cast<AudioCommandBlock*>(audio_cmd_mapped);

    for (int slot = 0; slot < 16; ++slot) {
        f32 command = cmd->slotCommand[slot];

        if (command > 0.51f) {
            std::string file = "assets/audio/splash" + std::to_string(slot) + ".wav";
            INPUT.playSound(file, "play", slot);
        }
        else if (command < -0.1f) {
            INPUT.playSound("", "stop", slot);
        }
    }

    std::memset(cmd, 0, sizeof(AudioCommandBlock));
}

inline void updateHardwareFromAnalogFields(float avgThermo, float avgPhi, float avgFlow,
                                           float analogTime, float dT, float fieldTimeScale) noexcept {
    auto& fab = rtx().hardwareFabric;
    if (!fab.populated) return;

    glm::vec3 pos(analogTime * 0.25f, avgThermo, avgFlow);
    glm::vec2 fVel = fluidVelocity(pos, analogTime);
    float fDens = fluidDensity(pos, analogTime);
    float tBias = teslaBias(pos, fVel, analogTime);   // simple directional

    double voltageFactor = 1.0 + (avgPhi - 0.1) * 0.08;
    double thermalThrottle = std::max(0.55, 1.0 - (avgThermo - 0.35) * 1.7);
    double parallelEff = std::clamp(0.6 + avgFlow * 0.65, 0.4, 1.12);

    // Simple Tesla damping on reverse flow
    parallelEff *= (1.0f - tBias * 0.085f);

    double targetFreq = fab.maxCoreClockMHz * voltageFactor * thermalThrottle * parallelEff * (1.0 + fDens * 0.07);

    double deltaCycles = 0.0;
    for (auto& u : fab.units) {
        double jitter = 1.0 + std::sin((u.id + analogTime) * 0.65) * 0.01;
        u.operationalFreqMHz = std::clamp(targetFreq * jitter, u.minFreqMHz, u.maxFreqMHz);

        for (auto& sf : u.subFunctions) {
            double heat = avgThermo * (0.7 + 0.3 * (sf.name.find("RT") != std::string::npos ? 1.2 : 1.0));
            sf.util = std::clamp(0.18 + parallelEff * 0.62 - heat * 0.38, 0.06, 0.96);
        }

        u.voltage = 0.85 + static_cast<double>(avgPhi - 0.05f) * 0.24;
        u.tempC   = 45.0 + static_cast<double>(avgThermo) * 64.0;
        u.powerW  = (u.operationalFreqMHz / fab.maxCoreClockMHz) * (12.0 + static_cast<double>(avgThermo) * 38.0);

        double cyclesThisStep = u.operationalFreqMHz * 1e6 * static_cast<double>(dT) * static_cast<double>(fieldTimeScale) / 1e9;
        deltaCycles += cyclesThisStep * (u.parallelLanes / 32.0);
    }

    // Fabric averages
    fab.coreClockMHz = 0.0;
    for (const auto& u : fab.units) fab.coreClockMHz += u.operationalFreqMHz;
    fab.coreClockMHz /= std::max(1u, (uint32_t)fab.units.size());

    fab.avgVoltage = 0.85 + static_cast<double>(avgPhi - 0.05f) * 0.24;
    fab.avgTempC   = 45.0 + static_cast<double>(avgThermo) * 64.0;
    fab.avgFlowUtil = static_cast<double>(avgFlow) * (1.0 - static_cast<double>(tBias) * 0.09);

    fab.simulatedChipCycles += deltaCycles;
    fab.lastFreqSampleTime = analogTime;

    for (auto& e : fab.edges) {
        e.currentUtil = std::clamp(static_cast<double>(avgFlow) * 0.55 * (1.0 - static_cast<double>(tBias) * 0.12), 0.06, 0.95);
    }
}

// ────────────────────────────────────────────────
// MAIN DISPATCH — hybrid compute vs hardware RT (no freeze on toggle)
// ────────────────────────────────────────────────
inline void dispatch_canvas(VkCommandBuffer cmd, int width, int height, float totalTime) noexcept {
    if (width <= 0 || height <= 0) return;
    if (FieldAmouranthOs::shellChromeActive())
        FieldAmouranthOs::tick(width, height);

    // x86 FieldSocket path — full die simulation, separate layout/push constants
    if (currentCanvasKind == CanvasKind::X86Fields && field_pipeline_layout && canvas_pipeline) {
        static bool x86_die_seeded = false;
        static bool rtx_audio_ready = false;
        if (!x86_die_seeded || Options::Canvas::DieResetRequested) {
            Options::Canvas::ControlFlags |= 1u;
            Options::Canvas::ControlFlags &= ~(Options::Canvas::ControlRtxDos
                | Options::Canvas::ControlHostCpu | Options::Canvas::ControlGpuDoom);
            if (Options::Canvas::BootRtxDos && fieldX86DieMapped) {
                const auto hdPath = FieldDos::defaultHdPath();
                if (FieldDos::bootGuest(fieldX86DieMapped, FIELD_X86_DIE_HEADER_BYTES)) {
                    FieldBios::rtxShellActive = true;
                    FieldBios::guestBootSettled = false;
                    FieldX86Emu::invalidateBios();
                    Options::Canvas::ControlFlags |= Options::Canvas::ControlRtxDos;
                    Options::Canvas::ControlFlags &= ~Options::Canvas::ControlHostCpu;
                    FieldMscdex::install();
                    FieldAmmoFat::mount();
                    FieldCdRom::autoMount(FieldDos::assetRoot());
                    FieldDrives::refresh();
                    FieldAmouranthOs::boot();
                    FieldNesImport::ensureImported();
                    FieldAosLoading::signalBooted();
                    FieldAmouranthOs::tick(width, height);
                    {
                        auto* seedRam = FieldDos::guestRam(
                            static_cast<std::uint8_t*>(fieldX86DieMapped),
                            FIELD_X86_DIE_HEADER_BYTES);
                        if (seedRam)
                            FieldRtxBoot::clearScreen(seedRam, 0x07u);
                    }
                    if (std::getenv("AMOURANTHRTX_LAUNCH_DOOM") && !FieldAmmoExec::isActive())
                        FieldAmouranthOs::dispatchAction(7);
                    FieldSpeaker::resetPit();
                    Options::DosEngine::syncToField();
                    rtx_audio_ready = false;
                    FieldDevices::reset();
                    if (INPUT.mixerReady())
                        rtx_audio_ready = FieldDevices::initAudio(INPUT.mixer());
                    LOG_SUCCESS_CAT("CANVAS",
                        "RTX-AMMOS GPU-only — 4GB RAM / {} GiB C: AMMOFAT+MSCDEX — IP:{:04X}",
                        FieldPlatform::HD_SIZE_BYTES / (1024u * 1024u * 1024u),
                        FieldDos::hdMirrorBytes / (1024u * 1024u),
                        FieldDos::BOOT_VECTOR & 0xFFFFu);
                } else {
                    LOG_WARN_CAT("CANVAS",
                        "RTX-DOS C: image missing — run ./linux.sh dos (assets/dos/rtx_dos_hd.img)");
                }
            }
            x86_die_seeded = true;
            FieldAosLoading::signalGuestSeeded();
            Options::Canvas::DieResetRequested = false;
        } else {
            Options::Canvas::ControlFlags &= ~1u;
        }

        const bool rtxDos = (Options::Canvas::ControlFlags & Options::Canvas::ControlRtxDos) != 0u;
        Options::DosEngine::syncToField();
        std::uint32_t cycles = std::max(1u,
            std::min(Options::Canvas::CyclesPerFrame, FieldDosConfig::cfg.cyclesRun));
        if (FieldAmmoExec::isActive())
            cycles = FieldAmmoExec::gpuCyclesPerFrame();
        Options::Canvas::DataBus[1] = cycles;
        Options::Canvas::ControlFlags &= ~Options::Canvas::ControlGpuDoom;
        if (FieldAmmoExec::isActive()) {
            Options::Canvas::ControlFlags |= Options::Canvas::ControlAmmoExec
                | Options::Canvas::ControlHostCpu;
        } else {
            Options::Canvas::ControlFlags &= ~(Options::Canvas::ControlAmmoExec
                | Options::Canvas::ControlHostCpu);
        }

        FieldDosViewport::panelFullscreen = Options::Canvas::DosPanelFullscreen;
        FieldDosViewport::panelStretch = Options::Canvas::DosPanelStretch;
        FieldDosViewport::focused = Options::Canvas::DosInputFocused && INPUT.hasFocus();
        FieldDosDisplay::syncViewport(width, height, width, height);
        if (FieldAmouranthOs::shellChromeActive()) {
            FieldAmouranthWm::syncViewport();
            if (FieldAmouranthOs::panelVisible) {
                const FieldDosViewport::Rect cr = FieldDosViewport::contentRect();
                FieldDosViewport::fontScale = FieldDosViewport::computeFontScale(cr);
                Options::Canvas::DosFontScale = FieldDosViewport::fontScale;
            }
        } else if (!Options::Canvas::DosAutoScale) {
            FieldDosViewport::fontScale = Options::Canvas::DosFontScale;
        }
        FieldDosViewport::crispFont = Options::Canvas::DosCrispFont;
        FieldDosViewport::scanlines = Options::Canvas::DosScanlines;
        if (rtxDos && fieldX86DieMapped) {
            auto* gr = FieldDos::guestRam(static_cast<std::uint8_t*>(fieldX86DieMapped),
                FIELD_X86_DIE_HEADER_BYTES);
            FieldDosViewport::syncFromGuest(gr);
        }
        if (Options::Canvas::DosPanelFullscreen)
            Options::Canvas::ControlFlags |= Options::Canvas::ControlDosPanelFs;
        else if (!Options::Canvas::DosImmersiveMode)
            Options::Canvas::ControlFlags &= ~Options::Canvas::ControlDosPanelFs;
        if (Options::Canvas::DosPanelStretch)
            Options::Canvas::ControlFlags |= Options::Canvas::ControlDosPanelStretch;
        else
            Options::Canvas::ControlFlags &= ~Options::Canvas::ControlDosPanelStretch;

        const bool dosFocus = FieldDosViewport::focused;
        const bool* keys = dosFocus ? SDL_GetKeyboardState(nullptr) : nullptr;
        float mx = 0.f, my = 0.f;
        const Uint32 mouseSt = dosFocus ? SDL_GetMouseState(&mx, &my) : 0u;
        float pmx = mx, pmy = my;
        if (dosFocus)
            FieldDosChrome::pointerPixels(nullptr, mx, my, pmx, pmy);
        const bool gpUp    = INPUT.gamepadButton(Options::Input::GP_DPad_Up)
                          || INPUT.gamepadButton(Options::Input::GP_North)
                          || INPUT.leftStickY() < -Options::SDL3::GamepadDeadzone;
        const bool gpDown  = INPUT.gamepadButton(Options::Input::GP_DPad_Down)
                          || INPUT.gamepadButton(Options::Input::GP_South)
                          || INPUT.leftStickY() > Options::SDL3::GamepadDeadzone;
        const bool gpLeft  = INPUT.gamepadButton(Options::Input::GP_DPad_Left)
                          || INPUT.gamepadButton(Options::Input::GP_West)
                          || INPUT.leftStickX() < -Options::SDL3::GamepadDeadzone;
        const bool gpRight = INPUT.gamepadButton(Options::Input::GP_DPad_Right)
                          || INPUT.gamepadButton(Options::Input::GP_East)
                          || INPUT.leftStickX() > Options::SDL3::GamepadDeadzone;
        std::uint32_t gpBits = 0u;
        if (INPUT.gamepadButton(Options::Input::GP_South))         gpBits |= FieldInput::GP_SOUTH;
        if (INPUT.gamepadButton(Options::Input::GP_East))          gpBits |= FieldInput::GP_EAST;
        if (INPUT.gamepadButton(Options::Input::GP_West))          gpBits |= FieldInput::GP_WEST;
        if (INPUT.gamepadButton(Options::Input::GP_North))         gpBits |= FieldInput::GP_NORTH;
        if (INPUT.gamepadButton(Options::Input::GP_Back))          gpBits |= FieldInput::GP_BACK;
        if (INPUT.gamepadButton(Options::Input::GP_Guide))         gpBits |= FieldInput::GP_GUIDE;
        if (INPUT.gamepadButton(Options::Input::GP_Start))         gpBits |= FieldInput::GP_START;
        if (INPUT.gamepadButton(Options::Input::GP_LeftStick))     gpBits |= FieldInput::GP_LSTICK;
        if (INPUT.gamepadButton(Options::Input::GP_RightStick))    gpBits |= FieldInput::GP_RSTICK;
        if (INPUT.gamepadButton(Options::Input::GP_LeftShoulder))  gpBits |= FieldInput::GP_LSHOULDER;
        if (INPUT.gamepadButton(Options::Input::GP_RightShoulder)) gpBits |= FieldInput::GP_RSHOULDER;
        if (INPUT.gamepadButton(Options::Input::GP_DPad_Up))        gpBits |= FieldInput::GP_DUP;
        if (INPUT.gamepadButton(Options::Input::GP_DPad_Down))      gpBits |= FieldInput::GP_DDOWN;
        if (INPUT.gamepadButton(Options::Input::GP_DPad_Left))      gpBits |= FieldInput::GP_DLEFT;
        if (INPUT.gamepadButton(Options::Input::GP_DPad_Right))     gpBits |= FieldInput::GP_DRIGHT;
        FieldInput::applyGamepad(INPUT.leftStickX(), INPUT.leftStickY(),
            INPUT.rightStickX(), INPUT.rightStickY(),
            INPUT.leftTrigger(), INPUT.rightTrigger(), gpBits, "Xbox/SDL Gamepad");
        FieldInput::pollFrame(keys, pmx, pmy,
            FieldDosViewport::winW, FieldDosViewport::winH, mouseSt,
            INPUT.leftStickX(), INPUT.leftStickY(),
            INPUT.rightStickX(), INPUT.rightStickY(),
            INPUT.down("gp_south") || INPUT.down("gp_right_shoulder"),
            INPUT.down("gp_west") || INPUT.down("gp_east"),
            gpUp, gpDown, gpLeft, gpRight);
        if (FieldAmmoExec::isActive() && keys)
            FieldInput::applyDoomMovementKeys(keys, gpUp, gpDown, gpLeft, gpRight);
        FieldInput::packDataBus(Options::Canvas::DataBus, 64u);
        if (FieldAmouranthOs::shellChromeActive() && FieldAmouranthOs::panelVisible) {
            FieldAmouranthWm::hover = FieldAmouranthWm::hitTest(pmx, pmy);
            FieldDosViewport::chromeHover = static_cast<std::uint32_t>(FieldAmouranthWm::hover) & 0xFu;
        } else {
            FieldDosChrome::updateHover(pmx, pmy);
            FieldDosViewport::chromeHover = FieldDosChrome::packHover();
        }
        const uint32_t guestKey = FieldInput::state.keyboard.biosKey
            ? static_cast<uint32_t>(FieldInput::state.keyboard.biosKey)
            : FieldInput::state.primaryKey;

        std::uint8_t* gr = nullptr;
        if (fieldX86DieMapped) {
            gr = FieldDos::guestRam(static_cast<std::uint8_t*>(fieldX86DieMapped),
                FIELD_X86_DIE_HEADER_BYTES);
            if (Options::SDL3::RequestQuit && gr) {
                if (FieldNes::active)
                    FieldNes::close(gr);
                if (FieldAmmoExec::isActive())
                    FieldAmmoExec::close(gr);
                FieldAmouranthFileCmd::close();
            }
            if (rtxDos)
                FieldDevices::bindGuest(gr);
            FieldAmmoExec::dieMapped = fieldX86DieMapped;
            FieldAmmoExec::ramByteOffset = FIELD_X86_DIE_HEADER_BYTES;
            if (FieldAmmoExec::isActive())
                FieldAmmoExec::pump(gr, fieldX86DieMapped, FIELD_X86_DIE_HEADER_BYTES,
                    guestKey, FieldInput::state.keyboard.down, FieldAmmoExec::gpuCyclesPerFrame());
            else if (rtxDos && FieldBios::rtxShellActive
                    && FieldAmouranthOs::shouldPumpGuestInput())
                FieldBios::pumpRtxShellGpu(gr, guestKey, FieldInput::state.keyboard.down);
            if (FieldNes::active) {
                if (INPUT.mixerReady())
                    FieldNes::ensureAudio(INPUT.mixer());
                FieldNes::tick(gr, keys);
                if (INPUT.mixerReady())
                    FieldNesAudio::pump();
            }
            if (FieldAmouranthOs::focusedApp == FieldAmouranthOs::AppId::Browser) {
                if (!FieldAmmoBrowser::isActive())
                    FieldAmmoBrowser::openUrl("https://amouranth.com", gr, rtx().window);
                FieldBrowserHook::pump(rtx().window);
            }
            FieldLayer::pumpAll(gr, Options::Canvas::DataBus);
            if (FieldAmouranthWm::closeRequested) {
                FieldAmouranthExec::closeActiveProgram(gr);
                FieldAmouranthWm::closeRequested = false;
                FieldAmouranthOs::syncDesktopState();
            }
            if (INPUT.mixerReady()) {
                if (!rtx_audio_ready)
                    rtx_audio_ready = FieldDevices::initAudio(INPUT.mixer());
                if (rtx_audio_ready)
                    FieldDevices::pumpAudio();
            }
        }
        if (gr) {
            if (FieldAmouranthOs::pendingShellRestore) {
                FieldAmouranthOs::pendingShellRestore = false;
                FieldRtxTerm::pinLive();
                FieldRtxShell::defaultPrompt(gr);
                FieldRtxTerm::syncLiveFromVga(gr);
            }
            FieldRtxTerm::applyView(gr);
            if (FieldAmouranthWm::pendingMenuAction
                    && (FieldRtxConsoleGui::active || FieldAmouranthOs::consoleShell)) {
                const int act = FieldAmouranthWm::pendingMenuAction;
                FieldAmouranthWm::pendingMenuAction = 0;
                FieldRtxConsoleGui::handleMenuAction(act, gr);
            }
            if (FieldRtxConsoleGui::active && !FieldRtxConsoleGui::useGpuChrome())
                FieldRtxConsoleGui::paintChrome(gr);
            if (FieldAmouranthOs::panelVisible && FieldAmouranthOs::active)
                FieldAmouranthExec::pumpWinGuiMouse(gr);
            if (FieldAmouranthOs::pendingEmptyPanel && gr) {
                FieldAmouranthExec::paintEmptyPanel(gr);
                FieldAmouranthOs::pendingEmptyPanel = false;
            }
            FieldWmStatusBar::packFooter(gr);
        }
        FieldDosViewport::packDataBus(Options::Canvas::DataBus, 64u, gr);
        FieldNes::packDataBus(Options::Canvas::DataBus);
        FieldWebPanel::packDataBus(Options::Canvas::DataBus);
        FieldAmouranthOs::packDataBus(Options::Canvas::DataBus, gr);
        FieldRtxTerm::packDataBus(Options::Canvas::DataBus);
        sync_aos_textures();
        if (gr && FieldAmouranthLaunch::hasPending())
            FieldAmouranthExec::execPending(gr, fieldX86DieMapped, FIELD_X86_DIE_HEADER_BYTES);

        FieldSocket fs{};
        fs.sealed_time = totalTime;
        fs.control     = (Options::Canvas::ControlFlags & ~Options::Canvas::ThemeMask)
                       | ((Options::Canvas::ColorTheme & 7u) << 8u);
        if (Options::Rendering::EnableHardwareRayTracing)
            fs.control |= 32u;  // bit5: RTX overlay

        std::memcpy(fs.data_bus, Options::Canvas::DataBus, sizeof(fs.data_bus));
        std::memcpy(fs.address_bus, Options::Canvas::AddressBus, sizeof(fs.address_bus));
        fs.maxwell_entropy_seed = static_cast<f32>(TotalTime::get().entropy() & 0xFFFFFFFFull) / 4294967295.0f;

        // === AMOURANTHOS: desktop (bit 12) vs console-shell taskbar (bit 29) ===
        {
            const bool aosDesktop = FieldAmouranthOs::active
                && Options::AmouranthOs::EnableDesktop;
            const bool consoleChrome = FieldAmouranthOs::consoleShell
                && Options::AmouranthOs::EnableTaskbar;
            if (aosDesktop)
                fs.data_bus[42] |= 4096u;
            else
                fs.data_bus[42] &= ~4096u;
            if (consoleChrome)
                fs.data_bus[42] |= FieldAmouranthOs::BUS_AOS_CONSOLE_SHELL;
            else
                fs.data_bus[42] &= ~FieldAmouranthOs::BUS_AOS_CONSOLE_SHELL;
        }

        auto packF = [](f32 f) -> u32 {
            u32 u = 0u;
            std::memcpy(&u, &f, sizeof(f));
            return u;
        };
        fs.data_bus[16] = packF(Options::AnalogFields::TimeScale);
        fs.data_bus[17] = packF(Options::AnalogFields::ThermoAlpha);
        fs.data_bus[18] = packF(Options::AnalogFields::WaveSpeed);
        fs.data_bus[19] = packF(Options::AnalogFields::GateFidelity);
        fs.data_bus[20] = packF(Options::AnalogFields::EntropyFloor);
        fs.data_bus[21] = packF(Options::AnalogFields::InjectStrength);
        fs.data_bus[22] = packF(Options::AnalogFields::PropalacticScale);
        fs.data_bus[23] = packF(Options::AnalogFields::FieldCoupling);
        fs.data_bus[34] = packF(Options::AnalogFields::TeslaBiasStrength);

        if (thermoAccountantMapped) {
            ThermoAccountant* acct = static_cast<ThermoAccountant*>(thermoAccountantMapped);
            const f32 analogTime = totalTime * 0.618034f + 0.0001f;
            const f32 dt = 0.016f * (1.0f + 0.25f * std::sin(totalTime * 0.7f));
            f32 mxD = 0.f, myD = 0.f;
            SDL_GetMouseState(&mxD, &myD);
            static f32 prevMx = 0.f, prevMy = 0.f;
            f32 inputActivity = std::abs(mxD - prevMx) + std::abs(myD - prevMy);
            prevMx = mxD; prevMy = myD;
            inputActivity = std::min(inputActivity * 0.012f, 0.85f);
            const f32 freeE = std::clamp(0.018f + 0.004f * std::sin(analogTime * 0.7f) + inputActivity * Options::AnalogFields::InjectStrength * 0.04f, 0.001f, 4.0f);
            const f32 prevCost = (Options::Rendering::EnableAccumulation ? 0.92f : 0.11f) *
                (1.0f / std::max(0.08f, static_cast<f32>(width) / 1920.0f)) *
                (0.014f + Options::AnalogFields::EntropyFloor * 0.6f);
            const f32 hostHeat = fieldX86DieMapped
                ? std::min(static_cast<f32>(FieldX86Emu::hostCyclesLastFrame()) * 2.8e-7f, 0.42f)
                : 0.f;
            const f32 fieldWork = (Options::AnalogFields::ThermoAlpha * 0.008f +
                                   Options::AnalogFields::WaveSpeed * 0.003f +
                                   Options::AnalogFields::FieldCoupling * 0.005f) * Options::AnalogFields::TimeScale;
            const f32 teslaEntropy = hostHeat * 0.15f * Options::AnalogFields::TeslaBiasStrength;
            const f32 entropyProd = prevCost + fieldWork + prevCost * 0.7f + 0.002f + hostHeat + teslaEntropy;
            const f32 boundaryThermo = std::clamp(0.31f + (Options::AnalogFields::ThermoAlpha - 0.6f) * 0.18f +
                Options::AnalogFields::EntropyFloor * 2.2f + hostHeat * 0.35f, 0.05f, 0.96f);
            acct->entropyThisFrame  = entropyProd;
            acct->avgBoundaryThermo = boundaryThermo;
            acct->prevMaintCost     = prevCost;
            acct->freeEnergyIncome  = freeE;
            acct->steps             = (acct->steps + 1u) & 0x7FFFFFFFu;
            fs.data_bus[24] = packF(acct->entropyThisFrame);
            fs.data_bus[25] = packF(acct->avgBoundaryThermo);
            fs.data_bus[26] = packF(acct->prevMaintCost);
            fs.data_bus[27] = packF(acct->freeEnergyIncome);
            fs.data_bus[28] = acct->steps;
            fs.data_bus[33] = packF(hostHeat);
            updateHardwareFromAnalogFields(boundaryThermo, 0.42f + hostHeat * 0.2f,
                0.55f + Options::AnalogFields::FieldCoupling * 0.3f,
                analogTime, dt, Options::AnalogFields::TimeScale);
        }

        if (width > 0 && height > 0) {
            fs.mouse_x = mx / static_cast<f32>(width);
            fs.mouse_y = my / static_cast<f32>(height);
        } else {
            fs.mouse_x = -1.f;
            fs.mouse_y = -1.f;
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, canvas_pipeline);
        vkCmdPushConstants(cmd, field_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(FieldSocket), &fs);

        const u32 dx = (static_cast<u32>(width)  + 15u) / 16u;
        const u32 dy = (static_cast<u32>(height) + 15u) / 16u;
        vkCmdDispatch(cmd, dx, dy, 1u);
        return;
    }

    LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: dispatch_canvas entry totalTime={:.4f} (frameSeed derived) — pre-accountant FCC guards + proxy then full thermo calc (entropy/prevMaint/freeE/boundary)", totalTime);
    LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: dispatch step for accountant population + fabric evolution (field seeds from clears, pre-trans done upstream in RayCanvas/Navigator)");
    PushConstants pc{};
    pc.time             = totalTime;
    pc.enableHardwareRT = Options::Rendering::EnableHardwareRayTracing;
    pc.enableRTXGI      = Options::Rendering::EnableRTXGI;
    pc.frameSeed        = static_cast<u32>(totalTime * 987654.321f) ^ 0xCAFEBABEu;

    pc.cameraPos        = CAM.position();
    pc.cameraQuat       = glm::vec4(CAM.orientation().x, CAM.orientation().y, CAM.orientation().z, CAM.orientation().w);
    pc.cameraFovDeg     = CAM.fovDeg();
    pc.aspectRatio      = static_cast<f32>(width) / static_cast<f32>(height);
    pc.nearPlane        = Options::Camera::NearPlane;
    pc.farPlane         = Options::Camera::FarPlane;

    pc.exposure         = Options::Rendering::Exposure;
    pc.vignetteStrength = Options::Rendering::VignetteStrength;
    pc.bloomThreshold   = Options::Rendering::BloomThreshold;
    pc.bloomIntensity   = Options::Rendering::BloomIntensity;
    pc.tonemapMode      = Options::Rendering::EnableTonemapping ? 1u : 0u;
    pc.contrast         = Options::Rendering::Contrast;
    pc.saturation       = Options::Rendering::Saturation;
    pc.gamma            = Options::Rendering::Gamma;

    f32 tod = Options::LivingWorld::CurrentTimeOfDay;
    pc.sunDir           = computeSunDirection(tod);
    pc.moonDir          = computeMoonDirection(tod);
    pc.sunIntensity     = Options::LivingWorld::SunIntensityDay;
    pc.moonIntensity    = Options::LivingWorld::MoonIntensity;
    pc.windDir          = glm::normalize(Options::LivingWorld::WindDirection);
    pc.windStrength     = Options::LivingWorld::WindStrength;
    pc.fogDensity       = Options::LivingWorld::FogDensity;
    pc.dayNightFactor   = tod / 24.0f;
    pc.cloudCoverage    = Options::LivingWorld::CloudCoverage;

    pc.raymarchMaxDist  = Options::Rendering::RaymarchMaxDistance;
    pc.raymarchEpsilon  = Options::Rendering::RaymarchEpsilon;
    pc.raymarchMaxSteps = Options::Rendering::RaymarchMaxSteps;

    // Input bitfield packing
    pc.controllerInput = 0u;

    if (INPUT.down("move_forward"))  pc.controllerInput |= Options::Input::Flags::MOVE_FORWARD;
    if (INPUT.down("move_backward")) pc.controllerInput |= Options::Input::Flags::MOVE_BACKWARD;
    if (INPUT.down("move_left"))     pc.controllerInput |= Options::Input::Flags::MOVE_LEFT;
    if (INPUT.down("move_right"))    pc.controllerInput |= Options::Input::Flags::MOVE_RIGHT;

    if (INPUT.down("gp_south"))      pc.controllerInput |= Options::Input::Flags::GAMEPAD_SOUTH;
    if (INPUT.down("gp_east"))       pc.controllerInput |= Options::Input::Flags::GAMEPAD_EAST;
    if (INPUT.down("gp_west"))       pc.controllerInput |= Options::Input::Flags::GAMEPAD_WEST;
    if (INPUT.down("gp_north"))      pc.controllerInput |= Options::Input::Flags::GAMEPAD_NORTH;

    if (INPUT.down("gp_left_shoulder"))  pc.controllerInput |= Options::Input::Flags::GAMEPAD_LEFT_SHOULDER;
    if (INPUT.down("gp_right_shoulder")) pc.controllerInput |= Options::Input::Flags::GAMEPAD_RIGHT_SHOULDER;

    if (INPUT.down("gp_left_stick"))     pc.controllerInput |= Options::Input::Flags::GAMEPAD_LEFT_STICK;
    if (INPUT.down("gp_right_stick"))    pc.controllerInput |= Options::Input::Flags::GAMEPAD_RIGHT_STICK;

    if (INPUT.down("gp_left_paddle1"))   pc.controllerInput |= Options::Input::Flags::GAMEPAD_LEFT_PADDLE1;
    if (INPUT.down("gp_left_paddle2"))   pc.controllerInput |= Options::Input::Flags::GAMEPAD_LEFT_PADDLE2;
    if (INPUT.down("gp_right_paddle1"))  pc.controllerInput |= Options::Input::Flags::GAMEPAD_RIGHT_PADDLE1;
    if (INPUT.down("gp_right_paddle2"))  pc.controllerInput |= Options::Input::Flags::GAMEPAD_RIGHT_PADDLE2;

    Uint32 mouse_state = SDL_GetMouseState(nullptr, nullptr);
    if (mouse_state & SDL_BUTTON_LMASK) pc.controllerInput |= Options::Input::Flags::MOUSE_LEFT;
    if (mouse_state & SDL_BUTTON_RMASK) pc.controllerInput |= Options::Input::Flags::MOUSE_RIGHT;
    if (mouse_state & SDL_BUTTON_MMASK) pc.controllerInput |= Options::Input::Flags::MOUSE_MIDDLE;

    int ctrl_slot = 0;
    pc.leftStickX   = INPUT.leftStickX(ctrl_slot);
    pc.leftStickY   = INPUT.leftStickY(ctrl_slot);
    pc.rightStickX  = INPUT.rightStickX(ctrl_slot);
    pc.rightStickY  = INPUT.rightStickY(ctrl_slot);
    pc.leftTrigger  = INPUT.leftTrigger(ctrl_slot);
    pc.rightTrigger = INPUT.rightTrigger(ctrl_slot);

    glm::vec2 delta = INPUT.mouseDelta();
    pc.mouseDelta       = delta;
    pc.mouseNormalized  = glm::vec2(
        (delta.x + static_cast<f32>(width)  * 0.5f) / static_cast<f32>(width),
        (delta.y + static_cast<f32>(height) * 0.5f) / static_cast<f32>(height)
    );
    pc.mouseWheelDelta  = 0.0f;

    // ── PROPALACTIC ANALOG FIELDS + TIMINGS (cranked) ────────────────────────
    // analogTime is continuous living time; dT derived from frame + tunable scale.
    // Mouse acts as stylus injecting into the fabric (heat / vortex / phase).
    pc.analogTime       = totalTime * 0.618034f + 0.0001f;   // golden conjugate for "tide" feel
    pc.dT               = 0.016f * (1.0f + 0.25f * std::sin(totalTime * 0.7f)); // variable "analog heartbeat" (harmonics source — guarded below)
    pc.fieldTimeScale   = Options::AnalogFields::TimeScale;
    pc.thermoAlpha      = Options::AnalogFields::ThermoAlpha;
    pc.waveSpeed        = Options::AnalogFields::WaveSpeed;
    pc.gateFidelity     = Options::AnalogFields::GateFidelity;
    pc.entropyFloor     = Options::AnalogFields::EntropyFloor;
    pc.injectStrength   = Options::AnalogFields::InjectStrength;
    pc.propalacticScale = Options::AnalogFields::PropalacticScale;
    pc.fieldCoupling    = Options::AnalogFields::FieldCoupling;

    // Field probe from mouse + held buttons (left=heat, right=vortex, middle=phase kick)
    pc.fieldProbe = glm::vec4(
        pc.mouseNormalized.x, pc.mouseNormalized.y,
        (pc.controllerInput & Options::Input::Flags::MOUSE_LEFT)  ?  1.0f : 0.0f,
        (pc.controllerInput & Options::Input::Flags::MOUSE_RIGHT) ? -0.7f : 0.0f   // sign encodes mode bias
    );
    if (pc.controllerInput & Options::Input::Flags::MOUSE_MIDDLE) {
        pc.fieldProbe.w = 2.0f; // phase injection
    }
    pc.fieldProbe.z *= pc.injectStrength; // scale the active injection

    // ── FCC + HARMONICS HARDWARE SAFETY GUARD (double-check for stable field gen) ─
    // Explicit field evolution (wave on Phi, diffusion on Thermo, advection on Flow) is
    // only stable inside CFL limits that depend on current field grid resolution (== render res)
    // and the pushed FCC values. High waveSpeed + small dx (high res / supersample) + large dT
    // excites unstable high harmonics → blowup, NaNs, or unsafe GPU behavior.
    // This guard runs every dispatch and clamps the values that actually hit the hardware.
    // User can still set wild FCC in Options; we pre-condition them for safe generation.
    {
        float res   = std::max(1.0f, std::min((float)width, (float)height));
        float dx    = 1.0f / res;                       // normalized grid spacing of the field fabric
        float effDt = pc.dT * pc.fieldTimeScale;

        float waveCFL   = (pc.waveSpeed * pc.fieldTimeScale) * effDt / dx;
        float thermoCFL = (pc.thermoAlpha * pc.fieldTimeScale) * effDt / (dx * dx);

        // Conservative safe limits for explicit 2D grid methods on GPU storage images.
        // Wave: < ~0.7-0.8 for low dispersion. Diffusion: < ~0.2-0.25 to avoid odd/even checkerboard.
        float safety = 1.0f;
        if (waveCFL   > 0.72f) safety = std::min(safety, 0.72f / waveCFL);
        if (thermoCFL > 0.20f) safety = std::min(safety, 0.20f / thermoCFL);

        // Extra guard against crazy user FCC or very low res (aliasing when upscaling later)
        if (safety < 0.98f) {
            pc.waveSpeed      *= safety;
            pc.thermoAlpha    *= safety;
            pc.fieldTimeScale *= safety;
            pc.dT             *= safety;
            pc.injectStrength *= std::min(1.0f, safety * 1.1f);  // also tame stylus to avoid sudden energy spikes

            //LOG_INFO_CAT("FIELDS", "FCC harmonics guard engaged — safetyScale={:.3f} (waveCFL={:.2f}, thermoCFL={:.2f}, res≈{:.0f}) — params clamped for stable/safe GPU field evolution",
            //             safety, waveCFL, thermoCFL, res);
        }

        // Absolute hardware-safe clamps no matter what the user FCC or adaptive res does.
        // These ranges keep fields bounded, prevent NaN/inf on GPU, and keep energy/entropy reasonable.
        pc.waveSpeed        = std::clamp(pc.waveSpeed,        0.01f,  2.0f);
        pc.thermoAlpha      = std::clamp(pc.thermoAlpha,      0.01f,  3.5f);
        pc.fieldTimeScale   = std::clamp(pc.fieldTimeScale,   0.05f,  4.0f);
        pc.dT               = std::clamp(pc.dT,               0.001f, 0.033f);  // never larger than ~30 Hz step
        pc.injectStrength   = std::clamp(pc.injectStrength,   0.0f,   8.0f);
        pc.entropyFloor     = std::max(0.001f, pc.entropyFloor);
        pc.gateFidelity     = std::clamp(pc.gateFidelity,     0.0f,   1.0f);
        pc.propalacticScale = std::clamp(pc.propalacticScale, 0.0f,   1.5f);
        pc.fieldCoupling    = std::clamp(pc.fieldCoupling,    0.0f,   2.0f);
    }

    // Drive the full hardware spiderweb from the analog fields (every frame, sub-micron precision)
    // avgThermo/Phi/Flow approximated from current FCC + probe for now (fields themselves evolve the real "die")
    float proxyThermo = 0.35f + (pc.thermoAlpha - 0.5f) * 0.25f + (pc.fieldProbe.z * 0.08f);
    float proxyPhi    = 0.05f + (pc.waveSpeed - 0.5f) * 0.12f;
    float proxyFlow   = 0.55f + (pc.fieldCoupling - 0.5f) * 0.35f + std::abs(pc.fieldProbe.z) * 0.1f;
    updateHardwareFromAnalogFields(proxyThermo, proxyPhi, proxyFlow,
                                   pc.analogTime, pc.dT, pc.fieldTimeScale);

    // ── THERMO ACCOUNTANT POPULATION (explicit Phase 1 schematic accounting, always-on, canvas-agnostic)
    // This *is* the "better than Poop" implementation: real numbers visible in every 5 s status,
    // previous-frame coherence has a visible negentropy price (Landauer-style), free energy is throughput,
    // entropy production is localized where probes (visual accum + security) couple to the boundary.
    // The holographic boundary data (current/prev HDR + Phi/Thermo/Flow fabric) pays the tax to persist order.
    LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: entering accountant population (w={} h={}) — computing entropy/prevMaint/freeE/boundary from FCC + probes + accum state", width, height);
    if (thermoAccountantMapped) {
        ThermoAccountant* acct = static_cast<ThermoAccountant*>(thermoAccountantMapped);

        // Free energy income proxy: sealed continuous time throughput + living world potentials + user "attention"
        // (mouse activity + sticks as directed negentropy injection into the system)
        float inputActivity = std::abs(pc.mouseDelta.x) + std::abs(pc.mouseDelta.y) +
                              std::abs(pc.leftStickX) + std::abs(pc.leftStickY) +
                              std::abs(pc.rightStickX) + std::abs(pc.rightStickY) +
                              std::abs(pc.leftTrigger) + std::abs(pc.rightTrigger);
        inputActivity = std::min(inputActivity * 0.012f, 0.85f);  // tame
        float worldPotentials = (pc.sunIntensity + pc.moonIntensity) * 0.012f + pc.windStrength * 0.04f + pc.cloudCoverage * 0.01f;
        float timeThroughput = 0.018f + 0.004f * std::sin(pc.analogTime * 0.7f); // continuous "food"
        float freeE = std::clamp(timeThroughput + worldPotentials + inputActivity * pc.injectStrength * 0.04f, 0.001f, 4.0f);
        LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: freeEnergyIncome calc — timeThroughput={:.4f} worldPot={:.4f} inputAct={:.4f} inject={:.3f} => freeE={:.4f}", timeThroughput, worldPotentials, inputActivity, pc.injectStrength, freeE);

        // Prev frame maintenance cost (the "costly previous" from schematic):
        // Accumulation = holographic memory / entanglement history across the boundary.
        // Keeping it coherent against the arrow of time is explicit dissipation at the visual probe.
        float accumTax = Options::Rendering::EnableAccumulation ? 0.92f : 0.11f;
        float detailTax = 1.0f / std::max(0.08f, (float)width / 1920.0f); // finer internal work = more negentropy to hold
        float prevCost = accumTax * detailTax * (0.014f + pc.entropyFloor * 0.6f);
        LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: prevMaintCost calc — accumTax={} detailTax={:.4f} entropyFloor={:.4f} => prevCost={:.4f} (high when accum ON = costly holographic memory)", accumTax ? "HIGH" : "LOW", detailTax, pc.entropyFloor, prevCost);

        // Entropy production this frame: irreversibility sites + probe extraction + field "work"
        // (diffusion, wave breaking, accumulation overwrite, measurement-like RT hits, security horizon checks)
        float fieldWork = (pc.thermoAlpha * 0.008f + pc.waveSpeed * 0.003f + pc.fieldCoupling * 0.005f) * pc.fieldTimeScale;
        float probeDissipation = prevCost * 0.7f + (Options::Rendering::EnableRTXGI ? 0.09f : 0.0f);
        float entropyProd = prevCost + fieldWork + probeDissipation + 0.002f; // floor from Landauer-like erasures
        LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: entropyThisFrame calc — fieldWork={:.5f} probeDiss={:.5f} +prev => entropyProd={:.5f} (Landauer proxy + field diffusion + RT/accum irreversibility)", fieldWork, probeDissipation, entropyProd);

        // Avg boundary thermo informed by the pushed field + entropy floor + local probe heat
        float boundaryThermo = std::clamp(0.31f + (pc.thermoAlpha - 0.6f) * 0.18f + pc.entropyFloor * 2.2f +
                                          (pc.fieldProbe.z * 0.014f), 0.05f, 0.96f);
        LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: avgBoundaryThermo calc — base 0.31 + thermoAlpha term + entropyFloor*2.2 + probe => boundaryThermo={:.4f}", boundaryThermo);

        acct->entropyThisFrame   = entropyProd;
        acct->avgBoundaryThermo  = boundaryThermo;
        acct->prevMaintCost      = prevCost;
        acct->freeEnergyIncome   = freeE;
        acct->steps              = (acct->steps + 1u) & 0x7FFFFFFFu;
        LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: accountant written — entropyThisFrame={:.5e} avgBoundaryThermo={:.3f} prevMaintCost={:.4f} freeEnergyIncome={:.3f} steps={}", entropyProd, boundaryThermo, prevCost, freeE, acct->steps);
        LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: post-write hex dump accountantMapped=0x{:016x} for status/5s full THERMO (even short runs)", reinterpret_cast<uintptr_t>(thermoAccountantMapped));
    } else {
        LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: no thermoAccountantMapped — metrics not populated this dispatch (5s status will show defaults)");
    }

    // RTXProbe population (zero cost if !rtxProbesEnabled — early return above in create).
    // Explicit hardware results from expanded timestamps (in RayCanvas transient cmd), pipeline stats (if pool active),
    // NV checkpoints (inserted in cmd buffer), and props (init-time). Written to mapped for host study.
    // Use to surface undocumented gains: e.g. AS compaction ratios, SBT alignment benefits, invocation reordering coherence,
    // phase timing deltas (compute vs RT), counter quirks vs expected. Read in status/ELLIE or post-run dumps.
    if (rtxProbesEnabled && rtxProbeMapped) {
        RTXProbe* pr = static_cast<RTXProbe*>(rtxProbeMapped);
        // Values harvested in RayCanvas post-submit (timestamps from 6-slot, stats, NV ckpts + Set/ Get) and written to mapped.
        // Align/SBT props filled at buffer create from caps (re-filled here for current rt_props snapshot).
        const auto& caps = rtx().hardwareFabric.probeCaps;
        pr->sbtHandleSize      = caps.shaderGroupHandleSize;
        pr->sbtHandleAlignment = caps.shaderGroupHandleAlign;
        pr->sbtBaseAlignment   = caps.shaderGroupBaseAlign;
        pr->minASScratchAlign  = caps.minScratchAlignment;

        // Example explicit study log (rate-limited to avoid spam; use for gains/undocumented on any card).
        static int probeLogCounter = 0;
        if ((++probeLogCounter % 60) == 0) {  // rate limit e.g. every 60 frames (~1s); was 10 (spam in probe logs)
            LOG_DEBUG_CAT("RTXPROBE", "RTXPROBE DISPATCH: gpuMsC={:.3f} gpuMsRT={:.3f} compInvoc={} rtInvoc={} asCompacted={} ckptCnt={} sbtHsz={} align={} (study this card: phase timings, invocations, checkpoints for order, SBT/scratch aligns for packing gains). Mapped=0x{:016x}",
                          pr->gpuMsCompute, pr->gpuMsRT, pr->computeInvocations, pr->rtInvocations, (uint64_t)pr->asCompactedSize, pr->checkpointCount,
                          pr->sbtHandleSize, pr->sbtHandleAlignment,
                          reinterpret_cast<uintptr_t>(rtxProbeMapped));
        }

        // Safe use of observed data (zero cost, study only): e.g. note high gpuMs for future adaptive hints or log alignments (from caps) for gain analysis without side effects.
        if ((probeLogCounter % 120) == 0) {
            // alignments from caps (e.g. SBT vs scratch) logged above; observed gpuMs vs caps props could drive future SER/coherence study without side effects.
        }

        // Future: memcpy full harvest results here if needed for dispatch use (e.g. modulate fields by observed gpuMs).
    }

    // ── Hybrid Dispatch Logic ──
    if (pc.enableHardwareRT && rt_pipeline != VK_NULL_HANDLE) {
        // Hardware ray tracing path (with RTXGI support via closesthit/raygen)
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline);
        vkCmdPushConstants(cmd, pipeline_layout, PUSH_STAGE_FLAGS, 0, sizeof(PushConstants), &pc);

        ext().vkCmdTraceRaysKHR(cmd,
            &raygen_region,
            &miss_region,
            &chit_region,
            &ahit_region,
            static_cast<u32>(width),
            static_cast<u32>(height),
            1u);
    } else {
        // Fallback compute raymarching path
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, canvas_pipeline);
        vkCmdPushConstants(cmd, pipeline_layout, PUSH_STAGE_FLAGS, 0, sizeof(PushConstants), &pc);

        u32 dx = (static_cast<u32>(width)  + 15u) / 16u;
        u32 dy = (static_cast<u32>(height) + 15u) / 16u;
        vkCmdDispatch(cmd, dx, dy, 1u);
    }

    process_shader_audio_commands();
}

// ────────────────────────────────────────────────
// Full shutdown
// ────────────────────────────────────────────────
inline void shutdown() noexcept {
    process_shader_audio_commands();
    FieldRaid::checkpoint();

    if (rt_pipeline)            { vkDestroyPipeline(rtx().device, rt_pipeline, nullptr); rt_pipeline = VK_NULL_HANDLE; }
    if (sbt_buffer)             { vkDestroyBuffer(rtx().device, sbt_buffer, nullptr); sbt_buffer = VK_NULL_HANDLE; }
    if (sbt_memory)             { vkFreeMemory(rtx().device, sbt_memory, nullptr); sbt_memory = VK_NULL_HANDLE; }
    if (rt_shaders.raygen)      vkDestroyShaderModule(rtx().device, rt_shaders.raygen, nullptr);
    if (rt_shaders.miss)        vkDestroyShaderModule(rtx().device, rt_shaders.miss, nullptr);
    if (rt_shaders.closestHit)  vkDestroyShaderModule(rtx().device, rt_shaders.closestHit, nullptr);

    if (canvas_pipeline)        { vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr); canvas_pipeline = VK_NULL_HANDLE; }
    if (pipeline_layout)        { vkDestroyPipelineLayout(rtx().device, pipeline_layout, nullptr); pipeline_layout = VK_NULL_HANDLE; }
    if (field_pipeline_layout)  { vkDestroyPipelineLayout(rtx().device, field_pipeline_layout, nullptr); field_pipeline_layout = VK_NULL_HANDLE; }
    if (main_descriptor_layout) { vkDestroyDescriptorSetLayout(rtx().device, main_descriptor_layout, nullptr); main_descriptor_layout = VK_NULL_HANDLE; }
    if (field_descriptor_layout){ vkDestroyDescriptorSetLayout(rtx().device, field_descriptor_layout, nullptr); field_descriptor_layout = VK_NULL_HANDLE; }

    if (fieldX86DieMapped)      { vkUnmapMemory(rtx().device, fieldX86DieMemory); fieldX86DieMapped = nullptr; }
    if (fieldX86DieBuffer)      { vkDestroyBuffer(rtx().device, fieldX86DieBuffer, nullptr); fieldX86DieBuffer = VK_NULL_HANDLE; }
    if (fieldX86DieMemory)      { vkFreeMemory(rtx().device, fieldX86DieMemory, nullptr); fieldX86DieMemory = VK_NULL_HANDLE; }

    if (audio_cmd_mapped)       { vkUnmapMemory(rtx().device, audio_cmd_memory); audio_cmd_mapped = nullptr; }
    if (audio_cmd_buffer)       { vkDestroyBuffer(rtx().device, audio_cmd_buffer, nullptr); audio_cmd_buffer = VK_NULL_HANDLE; }
    if (audio_cmd_memory)       { vkFreeMemory(rtx().device, audio_cmd_memory, nullptr); audio_cmd_memory = VK_NULL_HANDLE; }

    if (thermoAccountantMapped) { vkUnmapMemory(rtx().device, thermoAccountantMemory); thermoAccountantMapped = nullptr; }
    if (thermoAccountantBuffer) { vkDestroyBuffer(rtx().device, thermoAccountantBuffer, nullptr); thermoAccountantBuffer = VK_NULL_HANDLE; }
    if (thermoAccountantMemory) { vkFreeMemory(rtx().device, thermoAccountantMemory, nullptr); thermoAccountantMemory = VK_NULL_HANDLE; }
    LOG_DEBUG_CAT("THERMO", "SHUTDOWN THERMO: accountant unmapped/destroyed — final thermo metrics (entropy/prevMaintCost/freeEnergyIncome/boundaryThermo) lost (logged in prior 5s blocks + full status)");

    // Probe shutdown (safe, only if allocated; zero cost path never alloc'd these)
    if (rtxProbeMapped) { vkUnmapMemory(rtx().device, rtxProbeMemory); rtxProbeMapped = nullptr; }
    if (rtxProbeBuffer) { vkDestroyBuffer(rtx().device, rtxProbeBuffer, nullptr); rtxProbeBuffer = VK_NULL_HANDLE; }
    if (rtxProbeMemory) { vkFreeMemory(rtx().device, rtxProbeMemory, nullptr); rtxProbeMemory = VK_NULL_HANDLE; }
    // Note: query pools owned by RayCanvas (destroyed in its dtor); here just for probe buffer mirroring accountant.
    LOG_DEBUG_CAT("PROBE", "SHUTDOWN PROBE: rtx probe buffer resources released (if allocated; zero impact if !rtxProbesEnabled)");

    if (reservedBuffer3) { vkDestroyBuffer(rtx().device, reservedBuffer3, nullptr); reservedBuffer3 = VK_NULL_HANDLE; }
    if (reservedMemory3) { vkFreeMemory(rtx().device, reservedMemory3, nullptr); reservedMemory3 = VK_NULL_HANDLE; }
    if (reservedBuffer5) { vkDestroyBuffer(rtx().device, reservedBuffer5, nullptr); reservedBuffer5 = VK_NULL_HANDLE; }
    if (reservedMemory5) { vkFreeMemory(rtx().device, reservedMemory5, nullptr); reservedMemory5 = VK_NULL_HANDLE; }

    destroy_ammo_texture();
    destroy_aos_textures();
    field_layout_version_built = 0u;

    LOG_DEBUG_CAT("THERMO", "SHUTDOWN THERMO: final (thermo accountant lifetime ended; see prior THERMO logs for entropy/prevMaintCost/freeEnergyIncome/boundaryThermo + status); hex last: buf=0x{:016x}", reinterpret_cast<uintptr_t>(thermoAccountantBuffer));
    LOG_SUCCESS_CAT("PIPELINE", "Full shutdown complete — hybrid canvas + RT + RTXGI released (all CanvasBindings incl b2 accountant / b3/5 reserved / b6 audio released)");
}

} // namespace Pipeline