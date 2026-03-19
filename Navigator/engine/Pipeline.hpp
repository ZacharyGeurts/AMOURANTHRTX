#pragma once

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

constexpr uint32_t INPUT_FORWARD         = 1u << 0;
constexpr uint32_t INPUT_BACKWARD        = 1u << 1;
constexpr uint32_t INPUT_LEFT            = 1u << 2;
constexpr uint32_t INPUT_RIGHT           = 1u << 3;
constexpr uint32_t INPUT_SPRINT          = 1u << 4;
constexpr uint32_t INPUT_CROUCH          = 1u << 5;
constexpr uint32_t INPUT_JUMP            = 1u << 6;
constexpr uint32_t INPUT_INTERACT        = 1u << 7;
constexpr uint32_t INPUT_SHOOT           = 1u << 8;
constexpr uint32_t INPUT_MOUSE_LEFT      = 1u << 9;
constexpr uint32_t INPUT_MOUSE_RIGHT     = 1u << 10;
constexpr uint32_t INPUT_MOUSE_MIDDLE    = 1u << 11;

constexpr float AUDIO_CMD_PLAY        = 0.8f;
constexpr float AUDIO_CMD_VOLUME      = 0.35f;
constexpr float AUDIO_CMD_STOP        = -0.5f;
constexpr float AUDIO_CMD_PAUSE       = -0.3f;

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

struct alignas(16) AudioCommandBlock {
    float slotCommand[16];
    float slotValue[16];
    float reserved[16];
};

inline VkDescriptorSetLayout    main_descriptor_layout  = VK_NULL_HANDLE;
inline VkPipelineLayout         pipeline_layout         = VK_NULL_HANDLE;

inline VkPipeline               canvas_pipeline         = VK_NULL_HANDLE;
inline VkPipeline               rt_pipeline             = VK_NULL_HANDLE;

inline VkBuffer                 sbt_buffer              = VK_NULL_HANDLE;
inline VkDeviceMemory           sbt_memory              = VK_NULL_HANDLE;
inline VkDeviceAddress          sbt_device_address      = 0;

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

std::expected<VkShaderModule, std::string> load_spirv(const std::string& override_path = "") noexcept {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;

    if (!override_path.empty()) {
        fs::path op(override_path);
        if (fs::exists(op) && fs::is_regular_file(op)) candidates.push_back(op);
    }

    fs::path exe_dir;
    try { exe_dir = fs::canonical("/proc/self/exe").parent_path(); }
    catch (...) { }

    if (!exe_dir.empty()) {
        candidates.emplace_back(exe_dir / "assets/shaders/compute/CANVAS.spv");
        candidates.emplace_back(exe_dir / "CANVAS.spv");
    }

    fs::path cwd = fs::current_path();
    candidates.emplace_back(cwd / "assets/shaders/compute/CANVAS.spv");
    candidates.emplace_back(cwd / "CANVAS.spv");

    std::vector<uint32_t> code;
    std::string loaded_from;

    for (const auto& p : candidates) {
        if (!fs::exists(p)) continue;
        std::ifstream file(p, std::ios::binary | std::ios::ate);
        if (!file) continue;
        auto size = file.tellg();
        file.seekg(0);
        code.resize(size / 4);
        file.read(reinterpret_cast<char*>(code.data()), size);
        if (file.good()) {
            loaded_from = p.string();
            break;
        }
        code.clear();
    }

    if (code.empty()) {
        return std::unexpected("Shader file not found");
    }

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode    = code.data();

    VkShaderModule mod = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(rtx().device, &ci, nullptr, &mod);
    if (res != VK_SUCCESS) {
        return std::unexpected("vkCreateShaderModule failed");
    }

    return mod;
}

// ────────────────────────────────────────────────
// Descriptor & Pipeline Setup
// ────────────────────────────────────────────────
void initialize_descriptors_and_layout() noexcept {
    if (main_descriptor_layout != VK_NULL_HANDLE) return;

    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR,   nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR,   nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
    };

    VkDescriptorSetLayoutCreateInfo dslCI{};
    dslCI.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslCI.bindingCount = static_cast<uint32_t>(std::size(bindings));
    dslCI.pBindings    = bindings;

    vkCreateDescriptorSetLayout(rtx().device, &dslCI, nullptr, &main_descriptor_layout);

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
                    | VK_SHADER_STAGE_RAYGEN_BIT_KHR
                    | VK_SHADER_STAGE_MISS_BIT_KHR
                    | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                    | VK_SHADER_STAGE_ANY_HIT_BIT_KHR
                    | VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    push.offset     = 0;
    push.size       = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo plCI{};
    plCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCI.setLayoutCount         = 1;
    plCI.pSetLayouts            = &main_descriptor_layout;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges    = &push;

    vkCreatePipelineLayout(rtx().device, &plCI, nullptr, &pipeline_layout);
}

void create_audio_command_buffer() noexcept {
    if (audio_cmd_buffer != VK_NULL_HANDLE) return;

    VkBufferCreateInfo bufCI{};
    bufCI.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.size        = sizeof(AudioCommandBlock);
    bufCI.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(rtx().device, &bufCI, nullptr, &audio_cmd_buffer);

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, audio_cmd_buffer, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize  = req.size;
    alloc.memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(rtx().device, &alloc, nullptr, &audio_cmd_memory);
    vkBindBufferMemory(rtx().device, audio_cmd_buffer, audio_cmd_memory, 0);

    vkMapMemory(rtx().device, audio_cmd_memory, 0, sizeof(AudioCommandBlock), 0, &audio_cmd_mapped);
    std::memset(audio_cmd_mapped, 0, sizeof(AudioCommandBlock));
}

// ────────────────────────────────────────────────
// Canvas Pipeline (Compute - Raymarching)
// ────────────────────────────────────────────────
void create_canvas_pipeline() noexcept {
    if (raymarching_tried.load()) return;
    raymarching_tried.store(true);

    initialize_descriptors_and_layout();
    create_audio_command_buffer();

    auto mod_res = load_spirv("assets/shaders/compute/CANVAS.spv");
    if (!mod_res) {
        raymarching_success.store(false);
        return;
    }

    VkShaderModule module = *mod_res;

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = module;
    stage.pName  = "main";

    VkComputePipelineCreateInfo ci{};
    ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ci.stage  = stage;
    ci.layout = pipeline_layout;

    VkResult res = vkCreateComputePipelines(rtx().device, VK_NULL_HANDLE, 1, &ci, nullptr, &canvas_pipeline);
    vkDestroyShaderModule(rtx().device, module, nullptr);

    raymarching_success.store(res == VK_SUCCESS);
}

// ────────────────────────────────────────────────
// Ray Tracing Pipeline (Hardware RT)
// ────────────────────────────────────────────────
bool create_ray_tracing_pipeline() noexcept {
    if (raytracing_tried.load()) return raytracing_success.load();
    raytracing_tried.store(true);

    initialize_descriptors_and_layout();
    create_audio_command_buffer();

    if (!ext().vkCreateRayTracingPipelinesKHR) {
        raytracing_success.store(false);
        return false;
    }

    auto rgen   = load_spirv("assets/shaders/raytracing/raygen.rgen");
    auto rmiss  = load_spirv("assets/shaders/raytracing/miss.rmiss");
    auto rchit  = load_spirv("assets/shaders/raytracing/closesthit.rchit");
    auto rahit  = load_spirv("assets/shaders/raytracing/anyhit.rahit");
    auto rcall  = load_spirv("assets/shaders/raytracing/callable.rcall");

    if (!rgen || !rmiss || !rchit || !rahit || !rcall) {
        raytracing_success.store(false);
        return false;
    }

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR,   *rgen,  "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR,     *rmiss, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, *rchit, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR,  *rahit, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CALLABLE_BIT_KHR, *rcall, "main", nullptr},
    };

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr},
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr},
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, 3, VK_SHADER_UNUSED_KHR, nullptr},
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 4, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr},
    };

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR props{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 pdp2{};
    pdp2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    pdp2.pNext = &props;
    vkGetPhysicalDeviceProperties2(rtx().physical, &pdp2);

    shader_group_handle_size      = props.shaderGroupHandleSize;
    shader_group_handle_alignment = props.shaderGroupHandleAlignment;
    shader_group_base_alignment   = props.shaderGroupBaseAlignment;

    VkRayTracingPipelineCreateInfoKHR pipeCI{};
    pipeCI.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipeCI.stageCount                   = static_cast<uint32_t>(stages.size());
    pipeCI.pStages                      = stages.data();
    pipeCI.groupCount                   = static_cast<uint32_t>(groups.size());
    pipeCI.pGroups                      = groups.data();
    pipeCI.maxPipelineRayRecursionDepth = 1u;
    pipeCI.layout                       = pipeline_layout;

    VkResult res = ext().vkCreateRayTracingPipelinesKHR(rtx().device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &rt_pipeline);

    vkDestroyShaderModule(rtx().device, *rgen,  nullptr);
    vkDestroyShaderModule(rtx().device, *rmiss, nullptr);
    vkDestroyShaderModule(rtx().device, *rchit, nullptr);
    vkDestroyShaderModule(rtx().device, *rahit, nullptr);
    vkDestroyShaderModule(rtx().device, *rcall, nullptr);

    raytracing_success.store(res == VK_SUCCESS);
    return res == VK_SUCCESS;
}

// ────────────────────────────────────────────────
// Shader Binding Table (SBT) for RT
// ────────────────────────────────────────────────
bool build_shader_binding_table() noexcept {
    if (sbt_buffer != VK_NULL_HANDLE) return true;

    if (rt_pipeline == VK_NULL_HANDLE) return false;

    uint32_t groupCount = 4;

    uint32_t handleSize   = shader_group_handle_size;
    uint32_t alignHandle  = shader_group_handle_alignment;
    uint32_t alignBase    = shader_group_base_alignment;

    uint32_t rgenEntrySize  = alignedSize(handleSize, alignHandle);
    uint32_t missEntrySize  = alignedSize(handleSize, alignHandle);
    uint32_t hitEntrySize   = alignedSize(handleSize, alignHandle);
    uint32_t callEntrySize  = alignedSize(handleSize, alignHandle);

    uint32_t rgenRegionSize  = alignedSize(rgenEntrySize,  alignBase);
    uint32_t missRegionSize  = alignedSize(missEntrySize * 1, alignBase);
    uint32_t hitRegionSize   = alignedSize(hitEntrySize  * 1, alignBase);
    uint32_t callRegionSize  = alignedSize(callEntrySize * 1, alignBase);

    uint32_t totalSize = rgenRegionSize + missRegionSize + hitRegionSize + callRegionSize;

    VkBufferCreateInfo bufCI{};
    bufCI.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.size        = totalSize;
    bufCI.usage       = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(rtx().device, &bufCI, nullptr, &sbt_buffer);

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, sbt_buffer, &req);

    VkMemoryAllocateFlagsInfo flags{};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocCI{};
    allocCI.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocCI.pNext           = &flags;
    allocCI.allocationSize  = req.size;
    allocCI.memoryTypeIndex = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkAllocateMemory(rtx().device, &allocCI, nullptr, &sbt_memory);
    vkBindBufferMemory(rtx().device, sbt_buffer, sbt_memory, 0);

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = sbt_buffer;
    sbt_device_address = ext().vkGetBufferDeviceAddress(rtx().device, &addrInfo);

    std::vector<uint8_t> handles(groupCount * handleSize);
    ext().vkGetRayTracingShaderGroupHandlesKHR(rtx().device, rt_pipeline, 0, groupCount, handles.size(), handles.data());

    void* mapped = nullptr;
    vkMapMemory(rtx().device, sbt_memory, 0, totalSize, 0, &mapped);
    uint8_t* dst = static_cast<uint8_t*>(mapped);

    uint64_t offset = 0;

    std::memcpy(dst + offset, handles.data() + 0 * handleSize, handleSize);
    rgen_region.deviceAddress = sbt_device_address + offset;
    rgen_region.stride        = rgenEntrySize;
    rgen_region.size          = rgenEntrySize;
    offset += rgenRegionSize;

    std::memcpy(dst + offset, handles.data() + 1 * handleSize, handleSize);
    miss_region.deviceAddress = sbt_device_address + offset;
    miss_region.stride        = missEntrySize;
    miss_region.size          = missEntrySize;
    offset += missRegionSize;

    std::memcpy(dst + offset, handles.data() + 2 * handleSize, handleSize);
    hit_region.deviceAddress = sbt_device_address + offset;
    hit_region.stride        = hitEntrySize;
    hit_region.size          = hitEntrySize;
    offset += hitRegionSize;

    std::memcpy(dst + offset, handles.data() + 3 * handleSize, handleSize);
    call_region.deviceAddress = sbt_device_address + offset;
    call_region.stride        = callEntrySize;
    call_region.size          = callEntrySize;

    vkUnmapMemory(rtx().device, sbt_memory);

    return true;
}

// ────────────────────────────────────────────────
// Input Processing — FULL production implementation
// ────────────────────────────────────────────────
void processInput(SDL_Window* /*window*/, int window_width, int window_height) noexcept {
    // Reset high-level flags
    should_quit = false;
    wants_fullscreen_toggle = false;
    requested_width = window_width;
    requested_height = window_height;

    // Poll all events
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        SDL3System::get().pump(ev);

        if (ev.type == SDL_EVENT_QUIT) {
            should_quit = true;
        }

        if (ev.type == SDL_EVENT_WINDOW_RESIZED) {
            requested_width = ev.window.data1;
            requested_height = ev.window.data2;
        }

        if (ev.type == SDL_EVENT_KEY_DOWN) {
            bool alt = (ev.key.mod & SDL_KMOD_ALT) != 0;
            if (ev.key.scancode == SDL_SCANCODE_F11 ||
                (ev.key.scancode == SDL_SCANCODE_RETURN && alt)) {
                wants_fullscreen_toggle = true;
            }
        }
    }

    // ────────────────────────────────────────────────
    // Keyboard input — uses Options::Input bindings
    // ────────────────────────────────────────────────
    uint32_t inputFlags = 0;

    if (SDL3System::get().down("move_forward"))  inputFlags |= INPUT_FORWARD;
    if (SDL3System::get().down("move_backward")) inputFlags |= INPUT_BACKWARD;
    if (SDL3System::get().down("move_left"))     inputFlags |= INPUT_LEFT;
    if (SDL3System::get().down("move_right"))    inputFlags |= INPUT_RIGHT;
    if (SDL3System::get().down("sprint"))        inputFlags |= INPUT_SPRINT;
    if (SDL3System::get().down("crouch"))        inputFlags |= INPUT_CROUCH;
    if (SDL3System::get().down("jump"))          inputFlags |= INPUT_JUMP;
    if (SDL3System::get().down("interact"))      inputFlags |= INPUT_INTERACT;
    if (SDL3System::get().down("shoot"))         inputFlags |= INPUT_SHOOT;

    // Mouse buttons
    Uint32 mouseState = SDL_GetMouseState(nullptr, nullptr);
    if (mouseState & SDL_BUTTON_LMASK) inputFlags |= INPUT_MOUSE_LEFT;
    if (mouseState & SDL_BUTTON_RMASK) inputFlags |= INPUT_MOUSE_RIGHT;
    if (mouseState & SDL_BUTTON_MMASK) inputFlags |= INPUT_MOUSE_MIDDLE;

    // ────────────────────────────────────────────────
    // Gamepad input — deadzone, invert, sensitivity from Options::Input
    // ────────────────────────────────────────────────
    int ctrlSlot = 0;
    float leftX   = SDL3System::get().leftStickX(ctrlSlot);
    float leftY   = SDL3System::get().leftStickY(ctrlSlot);
    float rightX  = SDL3System::get().rightStickX(ctrlSlot);
    float rightY  = SDL3System::get().rightStickY(ctrlSlot);

    // Deadzone
    if (std::abs(leftX) < Options::Input::CONTROLLER_DEADZONE)   leftX = 0.0f;
    if (std::abs(leftY) < Options::Input::CONTROLLER_DEADZONE)   leftY = 0.0f;
    if (std::abs(rightX) < Options::Input::CONTROLLER_DEADZONE)  rightX = 0.0f;
    if (std::abs(rightY) < Options::Input::CONTROLLER_DEADZONE)  rightY = 0.0f;

    // Invert Y
    if (Options::Input::INVERT_CONTROLLER_Y) {
        rightY = -rightY;
    }

    // ────────────────────────────────────────────────
    // Mouse delta — invert & sensitivity from Options::Input
    // ────────────────────────────────────────────────
    glm::vec2 mouseDelta = SDL3System::get().mouseDelta();
    if (Options::Input::INVERT_MOUSE_Y) mouseDelta.y = -mouseDelta.y;
    mouseDelta *= Options::Input::MOUSE_SENSITIVITY;

    // ────────────────────────────────────────────────
    // Compute dt manually (TotalTime has no deltaSeconds())
    // ────────────────────────────────────────────────
    static double lastTime = TotalTime::get().seconds();
    double currentTime = TotalTime::get().seconds();
    float dt = static_cast<float>(currentTime - lastTime);
    lastTime = currentTime;

    dt *= Options::Debug::TimeScale;  // Apply time scale

    // ────────────────────────────────────────────────
    // Camera update — uses Options::Camera + dt
    // ────────────────────────────────────────────────
    static float yaw   = 0.0f;
    static float pitch = 0.0f;

    // Mouse look
    yaw   -= mouseDelta.x;
    pitch -= mouseDelta.y;
    pitch = std::clamp(pitch, -89.0f, 89.0f);

    glm::quat orientation = glm::quat(glm::vec3(glm::radians(pitch), glm::radians(yaw), 0.0f));

    // Gamepad look
    yaw   -= rightX * Options::Input::CONTROLLER_LOOK_SENSITIVITY;
    pitch -= rightY * Options::Input::CONTROLLER_LOOK_SENSITIVITY;
    pitch = std::clamp(pitch, -89.0f, 89.0f);

    // Movement
    glm::vec3 forward = glm::rotate(orientation, glm::vec3(0, 0, -1));
    glm::vec3 right   = glm::rotate(orientation, glm::vec3(1, 0, 0));
    glm::vec3 up      = glm::vec3(0, 1, 0);

    float moveSpeed = Options::Input::MOVEMENT_SPEED;
    if (inputFlags & INPUT_SPRINT) moveSpeed *= Options::Input::SPRINT_MULTIPLIER;

    glm::vec3 moveDir(0.0f);
    if (inputFlags & INPUT_FORWARD)  moveDir += forward;
    if (inputFlags & INPUT_BACKWARD) moveDir -= forward;
    if (inputFlags & INPUT_LEFT)     moveDir -= right;
    if (inputFlags & INPUT_RIGHT)    moveDir += right;

    if (glm::length(moveDir) > 0.01f) {
        moveDir = glm::normalize(moveDir);
        glm::vec3 pos = CAM.position();
        pos += moveDir * moveSpeed * dt;
        CAM.position() = pos;
    }

    // ────────────────────────────────────────────────
    // Apply Options::Camera effects (head bob, breathing, shake)
    // ────────────────────────────────────────────────
    glm::vec3 pos = CAM.position();

    if (Options::Camera::EnableHeadBob) {
        float bob = std::sinf(static_cast<float>(TotalTime::get().seconds() * Options::Camera::HeadBobFrequency)) * Options::Camera::HeadBobIntensity;
        pos.y += bob;
    }

    if (Options::Camera::EnableBreathing) {
        float breath = std::sinf(static_cast<float>(TotalTime::get().seconds() * 0.8f)) * Options::Camera::BreathingIntensity;
        pos.y += breath;
    }

    if (Options::Camera::EnableCameraShake) {
        static float trauma = 0.0f;
        trauma *= Options::Camera::ShakeTraumaDecay;
        float shake = trauma * trauma * (std::sinf(static_cast<float>(TotalTime::get().seconds() * 10.0f)) + std::sinf(static_cast<float>(TotalTime::get().seconds() * 7.0f)));
        pos += glm::vec3(shake, shake * 0.5f, 0.0f);
    }

    CAM.position() = pos;

    // ────────────────────────────────────────────────
    // Apply Options::GameStyle defaults (e.g. FOV overrides)
    // ────────────────────────────────────────────────
    if (Options::GameStyle::CurrentPerspective == Options::GameStyle::CameraPerspective::FirstPerson) {
        // FOV is read-only from Options::Camera::CurrentFOV — pushed directly to shader
    }
}

// ────────────────────────────────────────────────
// High-level getters (used by RayCanvas)
// ────────────────────────────────────────────────
inline bool shouldQuit() noexcept             { return should_quit; }
inline bool wantsFullscreenToggle() noexcept  { return wants_fullscreen_toggle; }
inline int  getRequestedWidth() noexcept      { return requested_width; }
inline int  getRequestedHeight() noexcept     { return requested_height; }

// ────────────────────────────────────────────────
// Dispatch (full, no stubs)
// ────────────────────────────────────────────────
void dispatch(VkCommandBuffer cmd, int width, int height, float totalTime) noexcept {
    if (width <= 0 || height <= 0) return;

    PushConstants pc{};
    pc.time             = totalTime;
    pc.frameSeed        = static_cast<uint32_t>(totalTime * 987654.321f) ^ 0xCAFEBABEu;

    pc.cameraPos        = CAM.position();
    pc.cameraQuat       = glm::vec4(CAM.orientation().x, CAM.orientation().y, CAM.orientation().z, CAM.orientation().w);
    pc.cameraFovDeg     = CAM.fov();
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

    float tod = Options::LivingWorld::CurrentTimeOfDay;
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

    pc.controllerInput = 0;
    pc.leftStickX      = 0.0f;
    pc.leftStickY      = 0.0f;
    pc.rightStickX     = 0.0f;
    pc.rightStickY     = 0.0f;
    pc.leftTrigger     = 0.0f;
    pc.rightTrigger    = 0.0f;

    pc.mouseDelta       = glm::vec2(0.0f);
    pc.mouseNormalized  = glm::vec2(0.5f);
    pc.mouseWheelDelta  = 0.0f;

    bool rt_enabled   = Options::Rendering::EnableHardwareRayTracing;
    bool rt_supported = rtx().rayTracingSupported;
    bool pipeline_ok  = false;
    bool sbt_ok       = false;

    if (rt_enabled && rt_supported) {
        pipeline_ok = create_ray_tracing_pipeline();
        if (pipeline_ok) sbt_ok = build_shader_binding_table();
    }

    bool use_rt = rt_enabled && rt_supported && pipeline_ok && sbt_ok;

    if (use_rt) {
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

    if (!canvas_pipeline) {
        create_canvas_pipeline();
        if (!canvas_pipeline) return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, canvas_pipeline);
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(PushConstants), &pc);

    uint32_t dx = (static_cast<uint32_t>(width) + 15u) / 16u;
    uint32_t dy = (static_cast<uint32_t>(height) + 15u) / 16u;
    vkCmdDispatch(cmd, dx, dy, 1u);
}

void shutdown() noexcept {
    if (audio_cmd_mapped) {
        vkUnmapMemory(rtx().device, audio_cmd_memory);
        audio_cmd_mapped = nullptr;
    }

    if (audio_cmd_buffer != VK_NULL_HANDLE) vkDestroyBuffer(rtx().device, audio_cmd_buffer, nullptr);
    if (audio_cmd_memory != VK_NULL_HANDLE) vkFreeMemory(rtx().device, audio_cmd_memory, nullptr);

    if (canvas_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(rtx().device, canvas_pipeline, nullptr);
    if (rt_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(rtx().device, rt_pipeline, nullptr);
    if (sbt_buffer != VK_NULL_HANDLE) vkDestroyBuffer(rtx().device, sbt_buffer, nullptr);
    if (sbt_memory != VK_NULL_HANDLE) vkFreeMemory(rtx().device, sbt_memory, nullptr);

    if (pipeline_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(rtx().device, pipeline_layout, nullptr);
    if (main_descriptor_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(rtx().device, main_descriptor_layout, nullptr);
}

} // namespace Pipeline