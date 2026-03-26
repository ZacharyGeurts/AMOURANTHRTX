#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Pipeline (Raymarching + Hardware Ray Tracing)
// (C) 2025-2026 Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "OptionsMenu.hpp"
#include "Camera.hpp"
#include "Materials.hpp"
#include "SDL3.hpp"

#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <format>
#include <expected>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <SDL3/SDL_gamepad.h>

namespace Pipeline {

// ────────────────────────────────────────────────
// Audio Command Constants
// ────────────────────────────────────────────────
constexpr float AUDIO_CMD_PLAY        = 0.8f;
constexpr float AUDIO_CMD_VOLUME      = 0.35f;
constexpr float AUDIO_CMD_STOP        = -0.5f;
constexpr float AUDIO_CMD_PAUSE       = -0.3f;

// ────────────────────────────────────────────────
// Push Constants (shared between compute & RT)
// ────────────────────────────────────────────────
struct alignas(16) PushConstants {
    float       time;
    uint32_t    frameSeed;

    glm::vec3   cameraPos;                  float pad0;
    glm::vec4   cameraQuat;
    float       cameraFovDeg;
    float       aspectRatio;
    float       nearPlane;
    float       farPlane;

    float       exposure;
    float       vignetteStrength;
    float       bloomThreshold;
    float       bloomIntensity;
    uint32_t    tonemapMode;
    float       contrast;
    float       saturation;

    glm::vec3   sunDir;                     float sunIntensity;
    glm::vec3   moonDir;                    float moonIntensity;
    glm::vec3   windDir;                    float windStrength;
    float       fogDensity;
    float       dayNightFactor;
    float       cloudCoverage;

    float       raymarchMaxDist;
    float       raymarchEpsilon;
    uint32_t    raymarchMaxSteps;

    uint32_t    controllerInput;
    float       leftStickX;                 float leftStickY;
    float       rightStickX;                float rightStickY;
    float       leftTrigger;                float rightTrigger;

    glm::vec2   mouseDelta;
    glm::vec2   mouseNormalized;
    float       mouseWheelDelta;

    float       pad1[3];
};

// ────────────────────────────────────────────────
// Audio Command Block (GPU → audio system)
// ────────────────────────────────────────────────
struct alignas(16) AudioCommandBlock {
    float slotCommand[16];
    float slotValue[16];
    float reserved[16];
};

// ────────────────────────────────────────────────
// Global Vulkan Resources
// ────────────────────────────────────────────────
inline VkDescriptorSetLayout    main_descriptor_layout  = VK_NULL_HANDLE;
inline VkPipelineLayout         pipeline_layout         = VK_NULL_HANDLE;

inline VkPipeline               canvas_pipeline         = VK_NULL_HANDLE;
inline VkPipeline               rt_pipeline             = VK_NULL_HANDLE;

inline VkStridedDeviceAddressRegionKHR  rgen_region     {};
inline VkStridedDeviceAddressRegionKHR  miss_region     {};
inline VkStridedDeviceAddressRegionKHR  hit_region      {};
inline VkStridedDeviceAddressRegionKHR  call_region     {};

inline VkBuffer                 audio_cmd_buffer        = VK_NULL_HANDLE;
inline VkDeviceMemory           audio_cmd_memory        = VK_NULL_HANDLE;
inline void*                    audio_cmd_mapped        = nullptr;

inline uint32_t                 shader_group_handle_size      = 0;
inline uint32_t                 shader_group_handle_alignment = 0;
inline uint32_t                 shader_group_base_alignment   = 0;

inline std::atomic<bool>        raymarching_tried{false};
inline std::atomic<bool>        raymarching_success{false};
inline std::atomic<bool>        raytracing_tried{false};
inline std::atomic<bool>        raytracing_success{false};

inline bool                     should_quit             = false;
inline bool                     wants_fullscreen_toggle = false;
inline int                      requested_width         = 0;
inline int                      requested_height        = 0;

// ────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────
inline uint32_t alignedSize(uint32_t size, uint32_t alignment) noexcept {
    return (size + alignment - 1u) & ~(alignment - 1u);
}

inline glm::vec3 computeSunDirection(float todHours) noexcept {
    float angle = (todHours / 24.0f) * glm::two_pi<float>() - glm::half_pi<float>();
    return glm::normalize(glm::vec3(std::cos(angle)*0.8f, std::sin(angle), std::cos(angle)*0.6f));
}

inline glm::vec3 computeMoonDirection(float todHours) noexcept {
    return -computeSunDirection(todHours);
}

std::expected<VkShaderModule, std::string> load_spirv(const std::string& path) noexcept {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates = {
        fs::path(path),
        fs::path("assets/shaders/compute") / path,
        fs::path("assets/shaders/raytracing") / path,
        fs::path("assets/shaders") / path
    };

    fs::path exe_dir;
    try { exe_dir = fs::canonical("/proc/self/exe").parent_path(); }
    catch (...) { }
    if (!exe_dir.empty()) {
        candidates.emplace_back(exe_dir / path);
        candidates.emplace_back(exe_dir / "assets" / "shaders" / "compute" / path);
        candidates.emplace_back(exe_dir / "assets" / "shaders" / "raytracing" / path);
    }

    std::vector<uint32_t> code;
    std::string loaded_path;

    for (const auto& p : candidates) {
        if (!fs::exists(p) || !fs::is_regular_file(p)) continue;

        std::ifstream file(p, std::ios::binary | std::ios::ate);
        if (!file) continue;

        auto size = file.tellg();
        if (size <= 0 || size % 4 != 0) continue;

        file.seekg(0);
        code.resize(size / 4);
        file.read(reinterpret_cast<char*>(code.data()), size);

        if (file.good()) {
            loaded_path = p.string();
            break;
        }
        code.clear();
    }

    if (code.empty()) {
        LOG_ERROR_CAT("SHADER", "Failed to load SPIR-V: {}", path);
        return std::unexpected("Shader file not found or invalid");
    }

    LOG_SUCCESS_CAT("SHADER", "Loaded SPIR-V: {} ({} bytes)", loaded_path, code.size() * 4);

    VkShaderModuleCreateInfo ci{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size() * sizeof(uint32_t),
        .pCode    = code.data()
    };

    VkShaderModule mod = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(rtx().device, &ci, nullptr, &mod);
    if (res != VK_SUCCESS) {
        LOG_FATAL("vkCreateShaderModule failed for {}: VkResult={}", path, static_cast<int>(res));
        return std::unexpected("vkCreateShaderModule failed");
    }

    return mod;
}

// ────────────────────────────────────────────────
// Descriptor & Layout Setup
// ────────────────────────────────────────────────
void initialize_descriptors_and_layout() noexcept {
    if (main_descriptor_layout != VK_NULL_HANDLE) return;

    LOG_INFO_CAT("DESCRIPTOR", "Creating main descriptor set layout");

    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR,   nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR,   nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
    };

    VkDescriptorSetLayoutCreateInfo dslCI{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(std::size(bindings)),
        .pBindings    = bindings
    };

    VkResult res = vkCreateDescriptorSetLayout(rtx().device, &dslCI, nullptr, &main_descriptor_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL("Failed to create descriptor set layout: VkResult={}", static_cast<int>(res));
        return;
    }

    VkPushConstantRange push{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
                    | VK_SHADER_STAGE_RAYGEN_BIT_KHR
                    | VK_SHADER_STAGE_MISS_BIT_KHR
                    | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                    | VK_SHADER_STAGE_ANY_HIT_BIT_KHR
                    | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        .offset     = 0,
        .size       = sizeof(PushConstants)
    };

    VkPipelineLayoutCreateInfo plCI{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &main_descriptor_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push
    };

    res = vkCreatePipelineLayout(rtx().device, &plCI, nullptr, &pipeline_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL("Failed to create pipeline layout: VkResult={}", static_cast<int>(res));
    } else {
        LOG_SUCCESS_CAT("PIPELINE", "Pipeline layout created successfully");
    }
}

// ────────────────────────────────────────────────
// Audio Command Buffer Setup
// ────────────────────────────────────────────────
void create_audio_command_buffer() noexcept {
    if (audio_cmd_buffer != VK_NULL_HANDLE) return;

    LOG_INFO_CAT("AUDIO", "Creating audio command buffer");

    VkBufferCreateInfo bufCI{
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = sizeof(AudioCommandBlock),
        .usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkResult res = vkCreateBuffer(rtx().device, &bufCI, nullptr, &audio_cmd_buffer);
    if (res != VK_SUCCESS) {
        LOG_FATAL("Failed to create audio command buffer: VkResult={}", static_cast<int>(res));
        return;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, audio_cmd_buffer, &req);

    VkMemoryAllocateInfo alloc{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = req.size,
        .memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    res = vkAllocateMemory(rtx().device, &alloc, nullptr, &audio_cmd_memory);
    if (res != VK_SUCCESS) {
        LOG_FATAL("Failed to allocate audio command memory: VkResult={}", static_cast<int>(res));
        vkDestroyBuffer(rtx().device, audio_cmd_buffer, nullptr);
        return;
    }

    vkBindBufferMemory(rtx().device, audio_cmd_buffer, audio_cmd_memory, 0);

    res = vkMapMemory(rtx().device, audio_cmd_memory, 0, sizeof(AudioCommandBlock), 0, &audio_cmd_mapped);
    if (res != VK_SUCCESS) {
        LOG_FATAL("Failed to map audio command memory: VkResult={}", static_cast<int>(res));
        return;
    }

    std::memset(audio_cmd_mapped, 0, sizeof(AudioCommandBlock));
    LOG_SUCCESS_CAT("AUDIO", "Audio command buffer created and mapped");
}

// ────────────────────────────────────────────────
// Canvas Compute Pipeline (Raymarching)
// ────────────────────────────────────────────────
void create_canvas_pipeline() noexcept {
    initialize_descriptors_and_layout();

    auto mod_res = load_spirv("assets/shaders/compute/CANVAS.spv");
    if (!mod_res.has_value()) {
        LOG_FATAL("CRITICAL: Failed to load CANVAS.spv");
        return;
    }

    LOG_INFO_CAT("CANVAS", "Loaded CANVAS.spv (binary CANVAS.comp)");

    VkShaderModule module = mod_res.value();

    VkPipelineShaderStageCreateInfo stage{
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = module,
        .pName  = "main"
    };

    VkComputePipelineCreateInfo ci{
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = stage,
        .layout = pipeline_layout
    };

    vkCreateComputePipelines(rtx().device, VK_NULL_HANDLE, 1, &ci, nullptr, &canvas_pipeline);
    vkDestroyShaderModule(rtx().device, module, nullptr);

    raymarching_tried = true;
}

// ────────────────────────────────────────────────
// Ray Tracing Pipeline (Hardware RT)
// ────────────────────────────────────────────────
void create_ray_tracing_pipeline() noexcept {
    initialize_descriptors_and_layout();

    auto rgen_res  = load_spirv("assets/shaders/raytracing/raygen.spv");
    auto rmiss_res = load_spirv("assets/shaders/raytracing/miss.spv");
    auto rchit_res = load_spirv("assets/shaders/raytracing/closesthit.spv");
    auto rahit_res = load_spirv("assets/shaders/raytracing/anyhit.spv");
    auto rcall_res = load_spirv("assets/shaders/raytracing/callable.spv");

    if (!rgen_res.has_value() || !rmiss_res.has_value() || !rchit_res.has_value() ||
        !rahit_res.has_value() || !rcall_res.has_value()) {
        LOG_FATAL("CRITICAL: One or more RT shader .spv files failed to load!");
        rt_pipeline = VK_NULL_HANDLE;
        raytracing_tried = true;
        return;
    }

    LOG_INFO_CAT("RAY TRACING", "Loaded ray tracing shaders");

    VkShaderModule rgen  = rgen_res.value();
    VkShaderModule rmiss = rmiss_res.value();
    VkShaderModule rchit = rchit_res.value();
    VkShaderModule rahit = rahit_res.value();
    VkShaderModule rcall = rcall_res.value();

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,   .module = rgen,  .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR,     .module = rmiss, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, .module = rchit, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,  .module = rahit, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_CALLABLE_BIT_KHR, .module = rcall, .pName = "main" }
    };

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
        { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 0 },
        { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 1 },
        { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, .closestHitShader = 2, .anyHitShader = 3 },
        { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 4 }
    };

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR props{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 pdp2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &props
    };
    vkGetPhysicalDeviceProperties2(rtx().physical, &pdp2);

    shader_group_handle_size      = props.shaderGroupHandleSize;
    shader_group_handle_alignment = props.shaderGroupHandleAlignment;
    shader_group_base_alignment   = props.shaderGroupBaseAlignment;

    VkRayTracingPipelineCreateInfoKHR pipeCI{
        .sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount                   = static_cast<uint32_t>(stages.size()),
        .pStages                      = stages.data(),
        .groupCount                   = static_cast<uint32_t>(groups.size()),
        .pGroups                      = groups.data(),
        .maxPipelineRayRecursionDepth = 1u,
        .layout                       = pipeline_layout
    };

    ext().vkCreateRayTracingPipelinesKHR(rtx().device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &rt_pipeline);

    vkDestroyShaderModule(rtx().device, rgen,  nullptr);
    vkDestroyShaderModule(rtx().device, rmiss, nullptr);
    vkDestroyShaderModule(rtx().device, rchit, nullptr);
    vkDestroyShaderModule(rtx().device, rahit, nullptr);
    vkDestroyShaderModule(rtx().device, rcall, nullptr);

    raytracing_tried = true;
}

void build_shader_binding_table() {
    if (rtx().raygen_sbt_region.deviceAddress != 0) return;

    const uint32_t handleSize  = rtx().rt_props.shaderGroupHandleSize;
    const uint32_t handleAlign = rtx().rt_props.shaderGroupHandleAlignment;
    const uint32_t baseAlign   = rtx().rt_props.shaderGroupBaseAlignment;

    const uint32_t alignedHandleSize = (handleSize + handleAlign - 1) & ~(handleAlign - 1);
    const uint32_t sbtStride         = alignedHandleSize;
    const uint32_t numGroups         = 4;

    VkDeviceSize sbtSize = static_cast<VkDeviceSize>(sbtStride) * numGroups;
    sbtSize = (sbtSize + baseAlign - 1) & ~(baseAlign - 1);

    uint64_t sbtBufferHandle = Memory::createBuffer(
        sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        "ShaderBindingTable"
    );

    if (sbtBufferHandle == 0) {
        LOG_ERROR_CAT("SBT", "Failed to create SBT buffer");
        return;
    }

    rtx().sbt_address = Memory::get(sbtBufferHandle)->deviceAddress;
    rtx().sbt_size    = sbtSize;

    std::vector<uint8_t> shaderHandles(numGroups * handleSize);

    VkResult res = ext().vkGetRayTracingShaderGroupHandlesKHR(
        rtx().device,
        rtx().rt_pipeline,
        0, numGroups,
        shaderHandles.size(),
        shaderHandles.data()
    );

    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("SBT", "vkGetRayTracingShaderGroupHandlesKHR failed: {}", static_cast<int>(res));
        return;
    }

    std::vector<uint8_t> sbtData(sbtSize, 0);
    const uint8_t* src = shaderHandles.data();
    uint8_t* dst = sbtData.data();

    for (uint32_t i = 0; i < numGroups; ++i) {
        memcpy(dst + i * sbtStride, src + i * handleSize, handleSize);
    }

    Memory::uploadToBuffer(sbtBufferHandle, sbtData.data(), sbtSize);

    const VkDeviceAddress baseAddr = rtx().sbt_address;

    rgen_region = {
        .deviceAddress = baseAddr + 0 * sbtStride,
        .stride        = sbtStride,
        .size          = sbtStride
    };

    miss_region = {
        .deviceAddress = baseAddr + 1 * sbtStride,
        .stride        = sbtStride,
        .size          = sbtStride
    };

    hit_region = {
        .deviceAddress = baseAddr + 2 * sbtStride,
        .stride        = sbtStride,
        .size          = sbtStride
    };

    call_region = { 0, 0, 0 };

    LOG_SUCCESS_CAT("SBT", "Shader Binding Table has been created.");
}

// ────────────────────────────────────────────────
// Input Processing — using Options::Input::Flags
// ────────────────────────────────────────────────
bool processInput(int current_width, int current_height, bool isRunning) noexcept {
    should_quit             = false;
    wants_fullscreen_toggle = false;
    requested_width         = current_width;
    requested_height        = current_height;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        INPUT.pump(ev);

        switch (ev.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                should_quit = true;
                isRunning = false;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_MAXIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
                requested_width  = ev.window.data1;
                requested_height = ev.window.data2;
                break;

            case SDL_EVENT_KEY_DOWN: {
                bool alt = (ev.key.mod & SDL_KMOD_ALT) != 0;
                if (ev.key.scancode == SDL_SCANCODE_F11 ||
                    (ev.key.scancode == SDL_SCANCODE_RETURN && alt)) {
                    wants_fullscreen_toggle = true;
                }
                break;
            }
            default:
                break;
        }
    }

    if (!isRunning) return false;

    // ────────────────────────────────────────────────
    // Build controllerInput bitfield from Options
    // ────────────────────────────────────────────────
    uint32_t flags = 0;

    if (INPUT.down("move_forward"))  flags |= Options::Input::Flags::FORWARD;
    if (INPUT.down("move_backward")) flags |= Options::Input::Flags::BACKWARD;
    if (INPUT.down("move_left"))     flags |= Options::Input::Flags::LEFT;
    if (INPUT.down("move_right"))    flags |= Options::Input::Flags::RIGHT;
    if (INPUT.down("sprint"))        flags |= Options::Input::Flags::SPRINT;
    if (INPUT.down("crouch"))        flags |= Options::Input::Flags::CROUCH;
    if (INPUT.down("jump"))          flags |= Options::Input::Flags::JUMP;
    if (INPUT.down("interact"))      flags |= Options::Input::Flags::INTERACT;
    if (INPUT.down("shoot"))         flags |= Options::Input::Flags::SHOOT;

    Uint32 mouseState = SDL_GetMouseState(nullptr, nullptr);
    if (mouseState & SDL_BUTTON_LMASK) flags |= Options::Input::Flags::MOUSE_LEFT;
    if (mouseState & SDL_BUTTON_RMASK) flags |= Options::Input::Flags::MOUSE_RIGHT;
    if (mouseState & SDL_BUTTON_MMASK) flags |= Options::Input::Flags::MOUSE_MIDDLE;

    // Gamepad sticks
    constexpr int slot = 0;
    float leftX  = INPUT.leftStickX(slot);
    float leftY  = INPUT.leftStickY(slot);
    float rightX = INPUT.rightStickX(slot);
    float rightY = INPUT.rightStickY(slot);

    const float dz = Options::Input::ControllerDeadzone;
    if (std::abs(leftX)  < dz) leftX  = 0.0f;
    if (std::abs(leftY)  < dz) leftY  = 0.0f;
    if (std::abs(rightX) < dz) rightX = 0.0f;
    if (std::abs(rightY) < dz) rightY = 0.0f;

    if (Options::Input::InvertControllerY) rightY = -rightY;

    glm::vec2 mouseDelta = INPUT.mouseDelta();
    if (Options::Input::InvertMouseY) mouseDelta.y = -mouseDelta.y;
    mouseDelta *= Options::Input::MouseSensitivity;

    // Touch fallback
    static glm::vec2 touchLookDelta{0.0f, 0.0f};
    static glm::vec2 touchMoveDelta{0.0f, 0.0f};

    int numDevices = 0;
    SDL_TouchID* devices = SDL_GetTouchDevices(&numDevices);
    SDL_TouchID touchID = (numDevices > 0) ? devices[0] : 0;
    SDL_free(devices);

    int fingerCount = 0;
    SDL_Finger** fingers = (touchID != 0) ? SDL_GetTouchFingers(touchID, &fingerCount) : nullptr;

    if (fingerCount > 0 && fingers) {
        SDL_Finger* finger = fingers[fingerCount - 1];
        if (finger->x > 0.5f) {
            float sens = Options::Input::MouseSensitivity;
            touchLookDelta.x = (finger->x - 0.5f) * 2.0f * sens;
            touchLookDelta.y = (finger->y - 0.5f) * 2.0f * sens;
        } else {
            touchMoveDelta.x = (finger->x - 0.25f) * 4.0f;
            touchMoveDelta.y = (finger->y - 0.5f)  * 4.0f;
            touchMoveDelta = glm::clamp(touchMoveDelta, glm::vec2(-1.0f), glm::vec2(1.0f));
        }
    } else {
        touchLookDelta = {0.0f, 0.0f};
        touchMoveDelta = {0.0f, 0.0f};
    }

    glm::vec2 lookDelta = mouseDelta;
    lookDelta.x += rightX * Options::Input::ControllerLookSensitivity * 60.0f;
    lookDelta.y += rightY * Options::Input::ControllerLookSensitivity * 60.0f;
    lookDelta += touchLookDelta;

    // Camera orientation
    static float yaw = 0.0f;
    static float pitch = 0.0f;
    yaw   -= lookDelta.x;
    pitch -= lookDelta.y;
    pitch  = std::clamp(pitch, -89.0f, 89.0f);

    glm::quat orientation = glm::quat(glm::vec3(glm::radians(pitch), glm::radians(yaw), 0.0f));
    CAM.setOrientation(orientation);

    // Movement
    glm::vec3 moveDir{0.0f};
    if (flags & Options::Input::Flags::FORWARD)  moveDir.z -= 1.0f;
    if (flags & Options::Input::Flags::BACKWARD) moveDir.z += 1.0f;
    if (flags & Options::Input::Flags::LEFT)     moveDir.x -= 1.0f;
    if (flags & Options::Input::Flags::RIGHT)    moveDir.x += 1.0f;

    moveDir.x += leftX;
    moveDir.z += leftY;
    moveDir.x += touchMoveDelta.x;
    moveDir.z += touchMoveDelta.y;

    float moveLen = glm::length(moveDir);
    bool isMoving = moveLen > 0.01f;
    bool isSprinting = (flags & Options::Input::Flags::SPRINT) ||
                       std::abs(leftX) > 0.8f || std::abs(touchMoveDelta.x) > 0.8f;

    glm::vec3 newPos = CAM.position();
    if (isMoving) {
        moveDir = glm::normalize(moveDir);
        float speed = Options::Input::MovementSpeed * isSprinting;
        glm::vec3 worldMove = orientation * moveDir;
        newPos += worldMove * speed;
    }

    // Head bob / breathing / shake
    if (Options::Camera::EnableHeadBob || Options::Camera::EnableBreathing || Options::Camera::EnableCameraShake) {
        float seconds = static_cast<float>(TotalTime::get().seconds());
        glm::vec3 offset{0.0f};

        if (Options::Camera::EnableHeadBob && isMoving) {
            float freq = isSprinting ? 18.0f : 12.0f;
            float amp  = isSprinting ? 0.12f : 0.07f;
            offset.y += std::sin(seconds * freq) * amp;
        }

        if (Options::Camera::EnableBreathing) {
            offset.y += std::sin(seconds * 0.8f) * 0.04f;
        }

        static float trauma = 0.0f;
        if (Options::Camera::EnableCameraShake) {
            if (flags & Options::Input::Flags::SHOOT || flags & Options::Input::Flags::JUMP)
                trauma = std::min(1.0f, trauma + 0.3f);

            trauma *= Options::Camera::ShakeTraumaDecay;
            float shake = trauma * trauma;
            float t = seconds * 15.0f;
            offset.x += std::sin(t * 12.3f) * shake * 0.8f;
            offset.y += std::cos(t * 8.7f)  * shake * 0.6f;
        }

        newPos += offset;
    }

    CAM.setPosition(newPos);

    return isRunning;
}

// ────────────────────────────────────────────────
// Dispatch — Choose compute or ray tracing (hybrid)
// ────────────────────────────────────────────────
void dispatch(VkCommandBuffer cmd, int width, int height) noexcept {
    if (width <= 0 || height <= 0 || cmd == VK_NULL_HANDLE) {
        LOG_WARN("Invalid dispatch dimensions: {}x{}", width, height);
        return;
    }

    float now = static_cast<float>(TotalTime::get().seconds());

    PushConstants pc{};
    pc.time             = now;
    pc.frameSeed        = static_cast<uint32_t>(now * 987654.321f) ^ 0xCAFEBABEu;
    pc.cameraPos        = CAM.position();
    pc.cameraQuat       = glm::vec4(CAM.orientation().x, CAM.orientation().y, CAM.orientation().z, CAM.orientation().w);
    pc.cameraFovDeg     = CAM_FOV();
    pc.aspectRatio      = static_cast<float>(width) / static_cast<float>(height);
    pc.nearPlane        = Options::Camera::NearPlane;
    pc.farPlane         = Options::Camera::FarPlane;
    pc.exposure         = Options::Rendering::Exposure;
    pc.vignetteStrength = Options::Rendering::VignetteStrength;
    pc.bloomThreshold   = Options::Rendering::BloomThreshold;
    pc.bloomIntensity   = Options::Rendering::BloomIntensity;
    pc.tonemapMode      = Options::Rendering::EnableTonemapping ? 2u : 0u;
    pc.contrast         = Options::Rendering::Contrast;
    pc.saturation       = Options::Rendering::Saturation;

    const float tod = Options::LivingWorld::CurrentTimeOfDay;
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
    pc.controllerInput = 0;   // filled by processInput if needed in future

    if (Options::Rendering::EnableHardwareRayTracing &&
        Options::Rendering::CurrentTechnique == Options::Rendering::RenderTechnique::HardwareRayTracing &&
        rt_pipeline != VK_NULL_HANDLE) {

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline);
        vkCmdPushConstants(cmd, pipeline_layout,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
            VK_SHADER_STAGE_CALLABLE_BIT_KHR,
            0, sizeof(PushConstants), &pc);

        ext().vkCmdTraceRaysKHR(cmd, &rgen_region, &miss_region, &hit_region, &call_region,
                                static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1u);
        return;
    }

    // Raymarching fallback
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, canvas_pipeline);
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(PushConstants), &pc);

    const uint32_t dx = (static_cast<uint32_t>(width)  + 15u) / 16u;
    const uint32_t dy = (static_cast<uint32_t>(height) + 15u) / 16u;
    vkCmdDispatch(cmd, dx, dy, 1u);
}

// ────────────────────────────────────────────────
// Shutdown
// ────────────────────────────────────────────────
void shutdown() noexcept {
    LOG_INFO_CAT("PIPELINE", "Shutting down Pipeline resources");

    if (audio_cmd_mapped) {
        vkUnmapMemory(rtx().device, audio_cmd_memory);
        audio_cmd_mapped = nullptr;
    }

    if (audio_cmd_buffer != VK_NULL_HANDLE) vkDestroyBuffer(rtx().device, audio_cmd_buffer, nullptr);
    if (audio_cmd_memory != VK_NULL_HANDLE) vkFreeMemory(rtx().device, audio_cmd_memory, nullptr);

    if (canvas_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr);
    if (rt_pipeline    != VK_NULL_HANDLE) vkDestroyPipeline(rtx().device, rt_pipeline,    nullptr);

    if (pipeline_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(rtx().device, pipeline_layout, nullptr);
    if (main_descriptor_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(rtx().device, main_descriptor_layout, nullptr);

    LOG_SUCCESS_CAT("PIPELINE", "Pipeline resources cleaned up 🧑🏾‍🩰");
}

// ────────────────────────────────────────────────
// Getters
// ────────────────────────────────────────────────
inline bool shouldQuit() noexcept             { return should_quit; }
inline bool wantsFullscreenToggle() noexcept  { return wants_fullscreen_toggle; }
inline int  getRequestedWidth() noexcept      { return requested_width; }
inline int  getRequestedHeight() noexcept     { return requested_height; }

} // namespace Pipeline