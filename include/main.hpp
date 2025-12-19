// =============================================================================
// include/engine/main.hpp
// GORILLAZ RTX ENGINE - PLASTIC BEACH v∞ - PUBLIC ENGINE INTERFACE
// ONE HEADER TO RULE THEM ALL - INCLUDE THIS TO USE THE ENGINE
// ALL RUNTIME STATE IS PUBLIC - FULL EXPOSURE - NO SECRETS
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <filesystem>
#include <memory>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <tiny_obj_loader.h>
#include <stb/stb_image.h>
#include <SDL3_image/SDL_image.h>
#include "options.hpp"

namespace RTX {

// =============================================================================
// PUBLIC CONFIGURATION
// =============================================================================
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 1;

// =============================================================================
// SIMPLE TEXTURE HOLDER
// =============================================================================
struct Texture {
    VkImage        image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView    view = VK_NULL_HANDLE;
    VkFormat       format = VK_FORMAT_UNDEFINED;
    int32_t        width = 0;
    int32_t        height = 0;
    uint32_t       mipLevels = 1;
};

// =============================================================================
// RAII HANDLE
// =============================================================================
template<typename T>
struct Handle {
    using DestroyFn = void(*)(VkDevice, T, const VkAllocationCallbacks*);
    T raw = T{};
    VkDevice device = VK_NULL_HANDLE;
    DestroyFn destroyer = nullptr;

    Handle() noexcept = default;
    Handle(T h, VkDevice d, DestroyFn del = nullptr) noexcept : raw(h), device(d), destroyer(del) {}
    Handle(Handle&& o) noexcept : raw(o.raw), device(o.device), destroyer(o.destroyer) {
        o.raw = T{}; o.device = VK_NULL_HANDLE; o.destroyer = nullptr;
    }
    Handle& operator=(Handle&& o) noexcept {
        if (this != &o) { reset(); raw = o.raw; device = o.device; destroyer = o.destroyer;
                          o.raw = T{}; o.device = VK_NULL_HANDLE; o.destroyer = nullptr; }
        return *this;
    }
    ~Handle() { reset(); }

    void reset() noexcept {
        if (valid() && destroyer)
            destroyer(device, raw, nullptr);
        raw = T{};
        device = VK_NULL_HANDLE;
        destroyer = nullptr;
    }

    explicit operator bool() const noexcept { return valid(); }
    T get() const noexcept { return raw; }

    [[nodiscard]] bool valid() const noexcept {
        return raw != T{} && device != VK_NULL_HANDLE;
    }
};

// =============================================================================
// FATAL ERROR
// =============================================================================
[[noreturn]] inline void fatal(const char* msg)
{
    fprintf(stderr, "[FATAL] %s\n", msg);
    std::exit(1);
}

// =============================================================================
// GLOBAL CONTEXT - SINGLETON ACCESS
// =============================================================================
struct Context {
    VkInstance       instance       = VK_NULL_HANDLE;
    VkSurfaceKHR     surface        = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice         device         = VK_NULL_HANDLE;
    VkQueue          queue          = VK_NULL_HANDLE;
    VkQueue          graphicsQueue  = VK_NULL_HANDLE;
    VkQueue          presentQueue   = VK_NULL_HANDLE;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    uint32_t queueFamily = UINT32_MAX;

    VkDescriptorSetLayout globalDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet       globalDescriptorSet       = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR           swapchain = VK_NULL_HANDLE;
    std::vector<VkImage>     swapchainImages;
    std::vector<VkImageView> swapchainViews;

    // Resolution
    uint32_t width  = 3840;
    uint32_t height = 2160;

    // GPU memory pool
    VkBuffer                  poolBuffer = VK_NULL_HANDLE;
    VkDeviceMemory            poolMemory = VK_NULL_HANDLE;
    VkDeviceSize              poolSize   = 0;
    std::atomic<VkDeviceSize> poolHead{0};
    std::unordered_map<uint64_t, VkDeviceSize> offsets;
    uint64_t                  nextHandle = 1;

    // TLAS
    uint64_t tlasHandle = 0;
};

Context g_context_instance{};

[[nodiscard]] inline Context& g_ctx() noexcept { return g_context_instance; }

inline VkSwapchainKHR&           g_swapchain()       { return g_ctx().swapchain; }
inline std::vector<VkImage>&     g_swapchainImages()  { return g_ctx().swapchainImages; }
inline std::vector<VkImageView>& g_swapchainViews()   { return g_ctx().swapchainViews; }
inline VkQueue&                  g_presentQueue()    { return g_ctx().presentQueue; }

// SDL window - direct global access via smart pointer
using SDLWindowPtr = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
inline SDLWindowPtr& g_window() { static SDLWindowPtr ptr(nullptr, SDL_DestroyWindow); return ptr; }

// =============================================================================
// RAY TRACING EXTENSIONS
// =============================================================================
struct RTXExtensions {
    PFN_vkCmdTraceRaysKHR                       vkCmdTraceRaysKHR                       = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR          vkCreateRayTracingPipelinesKHR          = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR    vkGetRayTracingShaderGroupHandlesKHR    = nullptr;
    PFN_vkCreateAccelerationStructureKHR        vkCreateAccelerationStructureKHR        = nullptr;
    PFN_vkDestroyAccelerationStructureKHR       vkDestroyAccelerationStructureKHR       = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR     vkCmdBuildAccelerationStructuresKHR     = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkGetBufferDeviceAddress                vkGetBufferDeviceAddress                = nullptr;

    void load(VkDevice device);
};

RTXExtensions g_ext{};

// =============================================================================
// SHADER BINDING TABLE REGIONS
// =============================================================================
VkStridedDeviceAddressRegionKHR g_raygenSbt{};
VkStridedDeviceAddressRegionKHR g_missSbt{};
VkStridedDeviceAddressRegionKHR g_hitSbt{};
VkStridedDeviceAddressRegionKHR g_callableSbt{};

// =============================================================================
// CAMERA - FIXED: NO UPSIDE-DOWN VIEW, CORRECT COORDINATE SYSTEM
// =============================================================================
struct Camera {
    glm::vec3 position = glm::vec3(Options::CameraStartPosition.x,
                                   Options::CameraStartPosition.y + Options::CameraEyeHeight,
                                   Options::CameraStartPosition.z);

    // Euler angles (in degrees)
    float yaw   = Options::CameraStartYaw;     // Left/right
    float pitch = Options::CameraStartPitch;   // Up/down

    // Derived direction vectors
    glm::vec3 front;
    glm::vec3 right;
    glm::vec3 up = glm::vec3(0.0f, -1.0f, 0.0f);  // FLIP: World up is now -Y (floor on head → fixed)

    // FPS physics
    float velocityY = 0.0f;
    bool  onGround  = true;

    Camera() {
        updateVectors();
    }

    void updateVectors() noexcept {
        // Flip the pitch sign to match new up direction (-Y)
        // Positive pitch now looks "up" in the flipped world (toward -Y)
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(-pitch));  // negate pitch
        front.y = sin(glm::radians(-pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(-pitch));
        front = glm::normalize(front);

        // Recalculate right vector with flipped world up
        right = glm::normalize(glm::cross(front, up));
    }

    [[nodiscard]] glm::mat4 view() const noexcept {
        // Use flipped up vector in lookAt
        return glm::lookAt(position, position + front, up);
    }

    void look(float dx, float dy, float sensitivity = 1.0f) noexcept {
        if (dx == 0.0f && dy == 0.0f) return;

        yaw += dx * sensitivity;

        float pitchDelta = dy * sensitivity;
        if (Options::InvertMouseLook) {
            pitchDelta = -pitchDelta;
        }
        pitch += pitchDelta;

        updateVectors();
    }

    void updatePhysics(float dt) {
        if (!onGround) {
            // Gravity now pulls toward +Y (since up is -Y, down is +Y)
            velocityY += Options::GravityStrength * dt;
        } else {
            velocityY = 0.0f;
        }

        float newY = position.y + velocityY * dt;

        // Ground is now at higher Y (since world is flipped)
        const float groundY = Options::GroundLevel + Options::CameraEyeHeight;

        if (newY >= groundY) {  // Fall until hitting "ground" from below
            position.y = groundY;
            velocityY = 0.0f;
            onGround = true;
        } else {
            position.y = newY;
            onGround = false;
        }
    }

    void jump() {
        if (onGround) {
            velocityY = -Options::JumpForce;
            onGround = false;
        }
    }

    void moveHorizontal(const glm::vec3& direction, float speed) {
        glm::vec3 flatDir = glm::vec3(direction.x, 0.0f, direction.z);
        if (glm::length(flatDir) > 0.0f) {
            flatDir = glm::normalize(flatDir);
        }
        position += flatDir * speed;
    }
};

// =============================================================================
// VULKAN RENDERER - ALL RUNTIME STATE IS PUBLIC
// =============================================================================
class VulkanRenderer {
public:
    struct PushConstants {
        alignas(16) glm::mat4 invView;
        alignas(16) glm::mat4 invProj;
        alignas(4)  float     totalTime;
        alignas(4)  uint32_t  spp;
        alignas(4)  uint32_t  frameSeed;
        alignas(4)  uint32_t  forceEnvOnly;
        alignas(4)  float     jitterStrength;
        alignas(4)  uint32_t  maxRecursion;
        alignas(4)  uint32_t  useEnvSky;
        alignas(4)  uint32_t  flipEnvV;
        alignas(4)  uint32_t  showHotPink;
        alignas(4)  float     environmentExposure;
        alignas(4)  float     skyIntensity;
        alignas(4)  float     environmentRotationY;
        alignas(16) glm::vec3 billboardBaseColor; // Aligned to 16 bytes
        alignas(4)  float     billboardAlphaCutoff;
        alignas(4)  uint32_t  billboardUseAlphaBlend;
    };

    VulkanRenderer(int width, int height);
    ~VulkanRenderer();

    VkCommandBuffer recordFrame(const Camera& camera, float deltaTime, uint32_t swapImageIndex) noexcept;

    void setEnvironmentMap(const std::string& filename) noexcept;
    void uploadEnvironmentMap(const float* data, int width, int height) noexcept;

    Texture createTextureImageFromFile(const std::string& filename, VkImageUsageFlags extraUsage = 0) noexcept;
    Texture createTextureImage(const float* pixels, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage = 0);

    // === PUBLIC RUNTIME STATE - FULL EXPOSURE ===
    int      width_  = 0;
    int      height_ = 0;
    uint32_t currentFrame_ = 0;

    uint32_t currentSpp_   = 0;
    float    totalTime_    = 0.0f;
    bool     cameraMoved_  = true;

    // Environment map
    Texture     m_environmentMap{};
    VkSampler   m_environmentSampler = VK_NULL_HANDLE;
    int         m_envMapWidth  = 0;
    int         m_envMapHeight = 0;
    bool        m_hasEnvironmentMap = false;

    // Monster billboard texture
    Texture     m_monsterTexture{};
    VkSampler   m_monsterSampler = VK_NULL_HANDLE;

    VkPipeline       pipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    std::vector<Handle<VkImage>>        rtOutputImages_;
    std::vector<Handle<VkImageView>>    rtOutputViews_;
    std::vector<Handle<VkDeviceMemory>> rtOutputMemories_;

    // Command pool and buffer for persistent reuse
    VkCommandPool       m_cmdPool       = VK_NULL_HANDLE;
    VkCommandBuffer     m_cmdBuffer     = VK_NULL_HANDLE;

    // Descriptor pool (global)
    VkDescriptorPool    m_descriptorPool = VK_NULL_HANDLE;

    // Tonemapping additions
    VkPipeline       tonemapPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout tonemapLayout_ = VK_NULL_HANDLE;
    VkDescriptorSet  tonemapDescSet_ = VK_NULL_HANDLE;

private:
    void createRayTracingPipeline();
    void createShaderBindingTable();
    void createTonemapPipeline();

    VkShaderModule loadShader(const std::string& filename) const noexcept
    {
        const std::vector<std::string> searchPaths = {
            "assets/shaders/raytracing/" + filename,
            "shaders/raytracing/" + filename,
            "../assets/shaders/raytracing/" + filename,
            "../../assets/shaders/raytracing/" + filename,
            "build/bin/Linux/assets/shaders/raytracing/" + filename,
            "assets/shaders/compute/" + filename,
            "shaders/compute/" + filename,
            "../assets/shaders/compute/" + filename,
            "../../assets/shaders/compute/" + filename,
            "build/bin/Linux/assets/shaders/compute/" + filename,
        };

        for (const auto& path : searchPaths) {
            std::ifstream file(path, std::ios::ate | std::ios::binary);
            if (!file.is_open()) continue;

            printf("[2025] SHADER FOUND: %s — Noodle's code compiles\n", path.c_str());

            size_t fileSize = static_cast<size_t>(file.tellg());
            std::vector<uint32_t> code((fileSize + 3) / 4);  // Align to 4 bytes
            file.seekg(0);
            file.read(reinterpret_cast<char*>(code.data()), fileSize);
            file.close();

            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = fileSize;
            createInfo.pCode    = code.data();

            VkShaderModule module;
            if (vkCreateShaderModule(g_ctx().device, &createInfo, nullptr, &module) != VK_SUCCESS) {
                fatal(("FAILED TO CREATE SHADER MODULE: " + path).c_str());
            }

            return module;
        }

        fatal(("SHADER NOT FOUND: " + filename).c_str());
        return VK_NULL_HANDLE;
    }
};

// =============================================================================
// UTILITY — FIND MEMORY TYPE
// =============================================================================
static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(g_ctx().physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
           (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
           return i;
        }
    }

    fatal("NO MEMORY TYPE FOUND — GPU TOO WEAK FOR THE BEACH");
    return 0; // Unreachable
}

// =============================================================================
// EXTENSIONS — LOADED ONCE
// =============================================================================
inline void RTXExtensions::load(VkDevice device)
{
    if (vkCmdTraceRaysKHR != nullptr) return;

#define LOAD_CORE(fn) \
    do { \
        fn = reinterpret_cast<PFN_##fn>(vkGetDeviceProcAddr(device, #fn)); \
        if (!fn) printf("[RTX] [ERROR] Failed to load core function " #fn "\n"); \
    } while (0)

#define LOAD_KHR(fn, name) \
    do { \
        fn = reinterpret_cast<PFN_##fn>(vkGetDeviceProcAddr(device, #fn)); \
        if (!fn) printf("[RTX] [WARN] Failed to load " #fn " (%s)\n", name); \
    } while (0)

    LOAD_CORE(vkGetBufferDeviceAddress);

    LOAD_KHR(vkCmdTraceRaysKHR, "ray tracing pipeline");
    LOAD_KHR(vkCreateRayTracingPipelinesKHR, "ray tracing pipeline");
    LOAD_KHR(vkGetRayTracingShaderGroupHandlesKHR, "ray tracing pipeline");

    LOAD_KHR(vkCreateAccelerationStructureKHR, "acceleration structure");
    LOAD_KHR(vkDestroyAccelerationStructureKHR, "acceleration structure");
    LOAD_KHR(vkCmdBuildAccelerationStructuresKHR, "acceleration structure");
    LOAD_KHR(vkGetAccelerationStructureBuildSizesKHR, "acceleration structure");
    LOAD_KHR(vkGetAccelerationStructureDeviceAddressKHR, "acceleration structure");

#undef LOAD_CORE
#undef LOAD_KHR

    bool rayTracingReady = (vkCmdTraceRaysKHR && vkCreateRayTracingPipelinesKHR && vkGetRayTracingShaderGroupHandlesKHR &&
                            vkCreateAccelerationStructureKHR && vkGetAccelerationStructureDeviceAddressKHR);

    printf("[RTX] Ray tracing %savailable — on melancholy hill\n", rayTracingReady ? "" : "not fully ");
    printf("[RTX] Extension loading complete — Kong Studios online\n");
}

// =============================================================================
// POOL — SEIZE THE GPU
// =============================================================================
static void seize_gpu()
{
    if (g_ctx().poolBuffer) return;

    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(g_context_instance.physicalDevice, &props);

    VkDeviceSize total = 0;
    for (uint32_t i = 0; i < props.memoryHeapCount; ++i)
        if (props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            total += props.memoryHeaps[i].size;

    const VkDeviceSize reservedForDrivers = 4'800'000'000ULL; // 4.5 GiB + margin
    g_ctx().poolSize = (total > reservedForDrivers) ? (total - reservedForDrivers) : 0;

    if (!g_ctx().poolSize) {
        fatal("GPU TOO WEAK — NOT ENOUGH VRAM AFTER RESERVING 4.5 GB FOR DRIVERS — the island sinks");
    }

    printf("[RTX] SEIZED %.2f GB VRAM (reserved 4.5 GB for drivers) — Plastic Beach dominates the rest\n",
           g_ctx().poolSize / (1024.0*1024.0*1024.0));

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size  = g_ctx().poolSize;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(g_context_instance.device, &bci, nullptr, &g_ctx().poolBuffer);
    if (result != VK_SUCCESS) {
        fatal("Failed to create pool buffer — Murdoc's bass drops out");
    }

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(g_context_instance.device, g_ctx().poolBuffer, &req);

    uint32_t type = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
        if ((req.memoryTypeBits & (1u << i)) && (props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            { type = i; break; }

    if (type == 0xFFFFFFFFu) fatal("NO DEVICE LOCAL MEMORY — Russel can't drum");

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = type;

    result = vkAllocateMemory(g_context_instance.device, &mai, nullptr, &g_ctx().poolMemory);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(g_context_instance.device, g_ctx().poolBuffer, nullptr);
        fatal("Failed to allocate pool memory — not enough contiguous VRAM (try reserving more for drivers)");
    }

    result = vkBindBufferMemory(g_context_instance.device, g_ctx().poolBuffer, g_ctx().poolMemory, 0);
    if (result != VK_SUCCESS) {
        vkFreeMemory(g_context_instance.device, g_ctx().poolMemory, nullptr);
        vkDestroyBuffer(g_context_instance.device, g_ctx().poolBuffer, nullptr);
        fatal("Failed to bind pool memory — Noodle's guitar unplugged");
    }
}

// =============================================================================
// BUFFER MANAGER — SMALL
// =============================================================================
[[nodiscard]] inline uint64_t BufferCreate(VkDeviceSize size) noexcept
{
    seize_gpu();
    VkDeviceSize aligned = (size + 255) & ~255ULL;
    VkDeviceSize offset = g_ctx().poolHead.fetch_add(aligned);
    if (offset + aligned > g_ctx().poolSize) {
        g_ctx().poolHead.fetch_sub(aligned);
        return 0;
    }

    uint64_t h = g_ctx().nextHandle++;
    g_ctx().offsets[h] = offset;
    return h;
}

[[nodiscard]] inline VkBuffer BufferGetVkBuffer(uint64_t /*h*/) noexcept { return g_ctx().poolBuffer; }

[[nodiscard]] inline VkDeviceAddress BufferGetDeviceAddress(uint64_t h) noexcept
{
    auto it = g_ctx().offsets.find(h);
    if (it == g_ctx().offsets.end()) return 0;
    VkBufferDeviceAddressInfo i{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = g_ctx().poolBuffer};
    return vkGetBufferDeviceAddress(g_ctx().device, &i) + it->second;
}

void PopulateContext(VkInstance instance, VkSurfaceKHR surface)
{
    // === EARLY POPULATE: instance + surface ===
    g_ctx().instance = instance;
    g_ctx().surface  = surface;

    // === SELECT PHYSICAL DEVICE ===
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) fatal("No GPUs with Vulkan support — no windmill turns");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physicalDevice = dev;
            printf("[SUCCESS] Selected discrete GPU: %s — 2D's eyes light up\n", props.deviceName);
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) {
        physicalDevice = devices[0];
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        printf("[SUCCESS] Selected fallback GPU: %s — Murdoc grumbles but accepts\n", props.deviceName);
    }

    g_ctx().physicalDevice = physicalDevice;

    // === FIND QUEUE FAMILY ===
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, families.data());

    uint32_t queueFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
            queueFamily = i;
            break;
        }
    }
    if (queueFamily == UINT32_MAX) fatal("No suitable queue family — Russel's drums silent");

    g_ctx().queueFamily = queueFamily;

    // === DEVICE EXTENSIONS & FEATURES ===
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
    asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.accelerationStructure = VK_TRUE;
    asFeatures.pNext = &features12;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{};
    rtFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtFeatures.rayTracingPipeline = VK_TRUE;
    rtFeatures.pNext = &asFeatures;

    VkPhysicalDeviceFeatures2 coreFeatures{};
    coreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    coreFeatures.pNext = &rtFeatures;

    // === QUEUE CREATE INFO ===
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamily;
    queueInfo.queueCount = 1;
    float priority = 1.0f;
    queueInfo.pQueuePriorities = &priority;

    // === DEVICE CREATE INFO ===
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &coreFeatures;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS) {
        fatal("Failed to create logical device — Noodle's code crashes");
    }

    // === FINALIZE CONTEXT ===
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queueFamily, 0, &queue);

    g_ctx().device         = device;
    g_ctx().graphicsQueue  = queue;
    g_ctx().queue          = queue;
    g_ctx().presentQueue   = queue;  // Single queue handles all

    // === QUERY RT PROPERTIES ===
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &g_ctx().rtProps;
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

    printf("[SUCCESS] Logical device created — ONE QUEUE — FULL RAY TRACING — Plastic Beach dominates\n");
}

// =============================================================================
// OBJ LOADER — 2025
// =============================================================================
static uint64_t LoadOBJ(const std::string& /*path*/) noexcept
{
    if (!Options::EnableBillboard) {
        printf("[RTX] Billboard disabled — no geometry loaded\n");
        return 0;
    }

    printf("[RTX] CREATING BILLBOARD QUAD AND GROUND PLANE — the monster awakens on Plastic Beach with RTX lighting\n");

    struct Vertex {
        float pos[3];
        float normal[3];
        float uv[2];
    };

    // Billboard quad
    const float halfSize = Options::BillboardScale * 0.5f;

    // Camera-facing quad (-Z normal)
    Vertex billboardVerts[4] = {
        {{-halfSize, -halfSize, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        {{-halfSize,  halfSize, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        {{ halfSize,  halfSize, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{ halfSize, -halfSize, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
    };

    uint32_t indices[6] = {0, 1, 2, 0, 2, 3};

    std::vector<Vertex> billboardVertices(billboardVerts, billboardVerts + 4);
    std::vector<uint32_t> billboardIndices_vec(indices, indices + 6);

    // Ground plane
    const float halfGround = 100.0f;

    Vertex groundVerts[4] = {
        {{-halfGround, Options::GroundLevel, -halfGround}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{-halfGround, Options::GroundLevel,  halfGround}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        {{ halfGround, Options::GroundLevel,  halfGround}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{ halfGround, Options::GroundLevel, -halfGround}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    };

    std::vector<Vertex> groundVertices(groundVerts, groundVerts + 4);
    std::vector<uint32_t> groundIndices_vec(indices, indices + 6);  // Same indices

    VkDevice device = g_ctx().device;
    VkQueue graphicsQueue = g_ctx().graphicsQueue;

    // Transient + reset pool for safe reuse
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = g_ctx().queueFamily;

    VkCommandPool uploadPool;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &uploadPool) != VK_SUCCESS)
        fatal("Failed to create upload command pool");

    // === Upload billboard vertices ===
    VkDeviceSize vertexSize = sizeof(Vertex) * billboardVertices.size();

    VkBuffer stagingVertex = VK_NULL_HANDLE;
    VkDeviceMemory stagingVertexMem = VK_NULL_HANDLE;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = vertexSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    if (vkCreateBuffer(device, &bufInfo, nullptr, &stagingVertex) != VK_SUCCESS)
        fatal("Failed to create vertex staging buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingVertex, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingVertexMem) != VK_SUCCESS ||
        vkBindBufferMemory(device, stagingVertex, stagingVertexMem, 0) != VK_SUCCESS)
        fatal("Failed to allocate/bind vertex staging memory");

    void* data;
    vkMapMemory(device, stagingVertexMem, 0, vertexSize, 0, &data);
    memcpy(data, billboardVertices.data(), vertexSize);
    vkUnmapMemory(device, stagingVertexMem);

    uint64_t vertexHandle = BufferCreate(vertexSize);
    if (!vertexHandle) fatal("No memory for billboard vertices");
    VkDeviceAddress vertexAddr = BufferGetDeviceAddress(vertexHandle);

    // === Upload billboard indices ===
    VkDeviceSize indexSize = sizeof(uint32_t) * billboardIndices_vec.size();

    VkBuffer stagingIndex = VK_NULL_HANDLE;
    VkDeviceMemory stagingIndexMem = VK_NULL_HANDLE;

    bufInfo.size = indexSize;
    if (vkCreateBuffer(device, &bufInfo, nullptr, &stagingIndex) != VK_SUCCESS)
        fatal("Failed to create index staging buffer");

    vkGetBufferMemoryRequirements(device, stagingIndex, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingIndexMem) != VK_SUCCESS ||
        vkBindBufferMemory(device, stagingIndex, stagingIndexMem, 0) != VK_SUCCESS)
        fatal("Failed to allocate/bind index staging memory");

    vkMapMemory(device, stagingIndexMem, 0, indexSize, 0, &data);
    memcpy(data, billboardIndices_vec.data(), indexSize);
    vkUnmapMemory(device, stagingIndexMem);

    uint64_t indexHandle = BufferCreate(indexSize);
    if (!indexHandle) fatal("No memory for billboard indices");
    VkDeviceAddress indexAddr = BufferGetDeviceAddress(indexHandle);

    // === Upload ground vertices ===
    VkDeviceSize groundVertexSize = sizeof(Vertex) * groundVertices.size();

    VkBuffer stagingGroundVertex = VK_NULL_HANDLE;
    VkDeviceMemory stagingGroundVertexMem = VK_NULL_HANDLE;

    bufInfo.size = groundVertexSize;
    if (vkCreateBuffer(device, &bufInfo, nullptr, &stagingGroundVertex) != VK_SUCCESS)
        fatal("Failed to create ground vertex staging buffer");

    vkGetBufferMemoryRequirements(device, stagingGroundVertex, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingGroundVertexMem) != VK_SUCCESS ||
        vkBindBufferMemory(device, stagingGroundVertex, stagingGroundVertexMem, 0) != VK_SUCCESS)
        fatal("Failed to allocate/bind ground vertex staging memory");

    vkMapMemory(device, stagingGroundVertexMem, 0, groundVertexSize, 0, &data);
    memcpy(data, groundVertices.data(), groundVertexSize);
    vkUnmapMemory(device, stagingGroundVertexMem);

    uint64_t groundVertexHandle = BufferCreate(groundVertexSize);
    if (!groundVertexHandle) fatal("No memory for ground vertices");
    VkDeviceAddress groundVertexAddr = BufferGetDeviceAddress(groundVertexHandle);

    // === Upload ground indices ===
    VkDeviceSize groundIndexSize = sizeof(uint32_t) * groundIndices_vec.size();

    VkBuffer stagingGroundIndex = VK_NULL_HANDLE;
    VkDeviceMemory stagingGroundIndexMem = VK_NULL_HANDLE;

    bufInfo.size = groundIndexSize;
    if (vkCreateBuffer(device, &bufInfo, nullptr, &stagingGroundIndex) != VK_SUCCESS)
        fatal("Failed to create ground index staging buffer");

    vkGetBufferMemoryRequirements(device, stagingGroundIndex, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingGroundIndexMem) != VK_SUCCESS ||
        vkBindBufferMemory(device, stagingGroundIndex, stagingGroundIndexMem, 0) != VK_SUCCESS)
        fatal("Failed to allocate/bind ground index staging memory");

    vkMapMemory(device, stagingGroundIndexMem, 0, groundIndexSize, 0, &data);
    memcpy(data, groundIndices_vec.data(), groundIndexSize);
    vkUnmapMemory(device, stagingGroundIndexMem);

    uint64_t groundIndexHandle = BufferCreate(groundIndexSize);
    if (!groundIndexHandle) fatal("No memory for ground indices");
    VkDeviceAddress groundIndexAddr = BufferGetDeviceAddress(groundIndexHandle);

    // === Copy to GPU pool ===
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = uploadPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    VkBufferCopy copy{};
    copy.size = vertexSize;
    copy.dstOffset = g_ctx().offsets[vertexHandle];
    vkCmdCopyBuffer(cmd, stagingVertex, g_ctx().poolBuffer, 1, &copy);

    copy.size = indexSize;
    copy.dstOffset = g_ctx().offsets[indexHandle];
    vkCmdCopyBuffer(cmd, stagingIndex, g_ctx().poolBuffer, 1, &copy);

    copy.size = groundVertexSize;
    copy.dstOffset = g_ctx().offsets[groundVertexHandle];
    vkCmdCopyBuffer(cmd, stagingGroundVertex, g_ctx().poolBuffer, 1, &copy);

    copy.size = groundIndexSize;
    copy.dstOffset = g_ctx().offsets[groundIndexHandle];
    vkCmdCopyBuffer(cmd, stagingGroundIndex, g_ctx().poolBuffer, 1, &copy);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, uploadPool, 1, &cmd);

    // Cleanup billboard staging
    vkDestroyBuffer(device, stagingVertex, nullptr);
    vkFreeMemory(device, stagingVertexMem, nullptr);
    vkDestroyBuffer(device, stagingIndex, nullptr);
    vkFreeMemory(device, stagingIndexMem, nullptr);

    // Cleanup ground staging
    vkDestroyBuffer(device, stagingGroundVertex, nullptr);
    vkFreeMemory(device, stagingGroundVertexMem, nullptr);
    vkDestroyBuffer(device, stagingGroundIndex, nullptr);
    vkFreeMemory(device, stagingGroundIndexMem, nullptr);

    // === Build billboard BLAS ===
    uint32_t primCount = 2;

    VkAccelerationStructureGeometryKHR geom{};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geom.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geom.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geom.geometry.triangles.vertexData.deviceAddress = vertexAddr;
    geom.geometry.triangles.vertexStride = sizeof(Vertex);
    geom.geometry.triangles.maxVertex = 3;
    geom.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geom.geometry.triangles.indexData.deviceAddress = indexAddr;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    g_ext.vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                  &buildInfo, &primCount, &sizeInfo);

    uint64_t blasHandle = BufferCreate(sizeInfo.accelerationStructureSize);
    if (!blasHandle) fatal("No memory for billboard BLAS");

    VkAccelerationStructureCreateInfoKHR blasCreate{};
    blasCreate.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    blasCreate.buffer = g_ctx().poolBuffer;
    blasCreate.offset = g_ctx().offsets[blasHandle];
    blasCreate.size = sizeInfo.accelerationStructureSize;
    blasCreate.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkAccelerationStructureKHR blas;
    if (g_ext.vkCreateAccelerationStructureKHR(device, &blasCreate, nullptr, &blas) != VK_SUCCESS)
        fatal("Failed to create billboard BLAS");

    uint64_t scratchHandle = BufferCreate(sizeInfo.buildScratchSize);
    if (!scratchHandle) fatal("No memory for BLAS scratch");

    buildInfo.dstAccelerationStructure = blas;
    buildInfo.scratchData.deviceAddress = BufferGetDeviceAddress(scratchHandle);

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = primCount;

    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    // Fresh command buffer for billboard BLAS build
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);
    vkBeginCommandBuffer(cmd, &begin);
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
    vkEndCommandBuffer(cmd);
    vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, uploadPool, 1, &cmd);

    // === Get billboard BLAS address ===
    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.accelerationStructure = blas;
    VkDeviceAddress blasAddr = g_ext.vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfo);

    // === Build ground BLAS ===
    geom.geometry.triangles.vertexData.deviceAddress = groundVertexAddr;
    geom.geometry.triangles.indexData.deviceAddress = groundIndexAddr;

    g_ext.vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                  &buildInfo, &primCount, &sizeInfo);

    uint64_t groundBlasHandle = BufferCreate(sizeInfo.accelerationStructureSize);
    if (!groundBlasHandle) fatal("No memory for ground BLAS");

    blasCreate.offset = g_ctx().offsets[groundBlasHandle];

    VkAccelerationStructureKHR groundBlas;
    if (g_ext.vkCreateAccelerationStructureKHR(device, &blasCreate, nullptr, &groundBlas) != VK_SUCCESS)
        fatal("Failed to create ground BLAS");

    uint64_t groundScratchHandle = BufferCreate(sizeInfo.buildScratchSize);
    if (!groundScratchHandle) fatal("No memory for ground BLAS scratch");

    buildInfo.dstAccelerationStructure = groundBlas;
    buildInfo.scratchData.deviceAddress = BufferGetDeviceAddress(groundScratchHandle);

    // Fresh command buffer for ground BLAS build
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);
    vkBeginCommandBuffer(cmd, &begin);
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
    vkEndCommandBuffer(cmd);
    vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, uploadPool, 1, &cmd);

    // === Get ground BLAS address ===
    addrInfo.accelerationStructure = groundBlas;
    VkDeviceAddress groundBlasAddr = g_ext.vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfo);

    // === Instances ===
    std::vector<VkAccelerationStructureInstanceKHR> instances(2);

    // Billboard instance
    instances[0].transform.matrix[0][0] = 1.0f;
    instances[0].transform.matrix[0][1] = 0.0f;
    instances[0].transform.matrix[0][2] = 0.0f;
    instances[0].transform.matrix[0][3] = 0.0f;
    instances[0].transform.matrix[1][0] = 0.0f;
    instances[0].transform.matrix[1][1] = 1.0f;
    instances[0].transform.matrix[1][2] = 0.0f;
    instances[0].transform.matrix[1][3] = halfSize;  // Translate up to stand on ground
    instances[0].transform.matrix[2][0] = 0.0f;
    instances[0].transform.matrix[2][1] = 0.0f;
    instances[0].transform.matrix[2][2] = 1.0f;
    instances[0].transform.matrix[2][3] = Options::BillboardZOffset;
    instances[0].instanceCustomIndex = 0;
    instances[0].mask = 0xFF;
    instances[0].instanceShaderBindingTableRecordOffset = 0;
    instances[0].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instances[0].accelerationStructureReference = blasAddr;

    // Ground instance
    instances[1].transform.matrix[0][0] = 1.0f;
    instances[1].transform.matrix[0][1] = 0.0f;
    instances[1].transform.matrix[0][2] = 0.0f;
    instances[1].transform.matrix[0][3] = 0.0f;
    instances[1].transform.matrix[1][0] = 0.0f;
    instances[1].transform.matrix[1][1] = 1.0f;
    instances[1].transform.matrix[1][2] = 0.0f;
    instances[1].transform.matrix[1][3] = 0.0f;
    instances[1].transform.matrix[2][0] = 0.0f;
    instances[1].transform.matrix[2][1] = 0.0f;
    instances[1].transform.matrix[2][2] = 1.0f;
    instances[1].transform.matrix[2][3] = 0.0f;
    instances[1].instanceCustomIndex = 1;  // Different for ground material
    instances[1].mask = 0xFF;
    instances[1].instanceShaderBindingTableRecordOffset = 0;
    instances[1].flags = 0;  // Enable culling for ground
    instances[1].accelerationStructureReference = groundBlasAddr;

    // === Upload instances ===
    VkDeviceSize instSize = sizeof(VkAccelerationStructureInstanceKHR) * instances.size();
    uint64_t instHandle = BufferCreate(instSize);
    if (!instHandle) fatal("No memory for instances");
    VkDeviceAddress instAddr = BufferGetDeviceAddress(instHandle);

    VkBuffer stagingInst = VK_NULL_HANDLE;
    VkDeviceMemory stagingInstMem = VK_NULL_HANDLE;

    bufInfo.size = instSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (vkCreateBuffer(device, &bufInfo, nullptr, &stagingInst) != VK_SUCCESS)
        fatal("Failed to create instance staging buffer");

    vkGetBufferMemoryRequirements(device, stagingInst, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingInstMem) != VK_SUCCESS ||
        vkBindBufferMemory(device, stagingInst, stagingInstMem, 0) != VK_SUCCESS)
        fatal("Failed to allocate/bind instance staging memory");

    vkMapMemory(device, stagingInstMem, 0, instSize, 0, &data);
    memcpy(data, instances.data(), instSize);
    vkUnmapMemory(device, stagingInstMem);

    // Fresh command buffer for instance copy
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);
    vkBeginCommandBuffer(cmd, &begin);
    copy.size = instSize;
    copy.dstOffset = g_ctx().offsets[instHandle];
    vkCmdCopyBuffer(cmd, stagingInst, g_ctx().poolBuffer, 1, &copy);
    vkEndCommandBuffer(cmd);
    vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, uploadPool, 1, &cmd);

    vkDestroyBuffer(device, stagingInst, nullptr);
    vkFreeMemory(device, stagingInstMem, nullptr);

    // === Build TLAS ===
    VkAccelerationStructureGeometryKHR tlasGeom{};
    tlasGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    tlasGeom.geometry.instances.arrayOfPointers = VK_FALSE;
    tlasGeom.geometry.instances.data.deviceAddress = instAddr;

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuild{};
    tlasBuild.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    tlasBuild.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasBuild.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlasBuild.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlasBuild.geometryCount = 1;
    tlasBuild.pGeometries = &tlasGeom;

    uint32_t instCount = static_cast<uint32_t>(instances.size());
    VkAccelerationStructureBuildSizesInfoKHR tlasSize{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    g_ext.vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                  &tlasBuild, &instCount, &tlasSize);

    uint64_t tlasHandle = BufferCreate(tlasSize.accelerationStructureSize);
    if (!tlasHandle) fatal("No memory for billboard TLAS");

    VkAccelerationStructureCreateInfoKHR tlasCreate{};
    tlasCreate.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    tlasCreate.buffer = g_ctx().poolBuffer;
    tlasCreate.offset = g_ctx().offsets[tlasHandle];
    tlasCreate.size = tlasSize.accelerationStructureSize;
    tlasCreate.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkAccelerationStructureKHR tlas;
    if (g_ext.vkCreateAccelerationStructureKHR(device, &tlasCreate, nullptr, &tlas) != VK_SUCCESS)
        fatal("Failed to create billboard TLAS");

    uint64_t tlasScratch = BufferCreate(tlasSize.buildScratchSize);
    if (!tlasScratch) fatal("No memory for TLAS scratch");

    tlasBuild.dstAccelerationStructure = tlas;
    tlasBuild.scratchData.deviceAddress = BufferGetDeviceAddress(tlasScratch);

    VkAccelerationStructureBuildRangeInfoKHR tlasRange{};
    tlasRange.primitiveCount = instCount;

    const VkAccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRange;

    // Final TLAS build command
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);
    vkBeginCommandBuffer(cmd, &begin);
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuild, &pTlasRange);
    vkEndCommandBuffer(cmd);
    vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, uploadPool, 1, &cmd);

    vkDestroyCommandPool(device, uploadPool, nullptr);

    g_ctx().tlasHandle = reinterpret_cast<uint64_t>(tlas);

    printf("[RTX] BILLBOARD AND GROUND FULLY LOADED — the monster and ground now dominate Plastic Beach with RTX lighting effects\n");
    return g_ctx().tlasHandle;
}

[[nodiscard]] inline uint64_t LoadScene(const std::string& path) noexcept
{
    return LoadOBJ(path);
}

[[nodiscard]] inline VkAccelerationStructureKHR CurrentTLAS() noexcept
{
    return (VkAccelerationStructureKHR)g_ctx().tlasHandle;
}

// =============================================================================
// VulkanRenderer Implementation
// =============================================================================
VulkanRenderer::VulkanRenderer(int width, int height) : width_(width), height_(height) {
    VkDevice device = g_ctx().device;

    // === GLOBAL DESCRIPTOR LAYOUT (shared by RT + tonemap + any-hit) ===
    VkDescriptorSetLayoutBinding bindings[4]{};

    // Binding 0: TLAS
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    // Binding 1: Storage image (RT accumulation + tonemap)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: Environment map
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    // Binding 3: Monster billboard texture
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 4;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &g_ctx().globalDescriptorSetLayout) != VK_SUCCESS) {
        fatal("Failed to create global descriptor set layout");
    }

    // === DESCRIPTOR POOL ===
    VkDescriptorPoolSize sizes[3]{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    sizes[0].descriptorCount = 1;
    sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    sizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    sizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[2].descriptorCount = 2;  // env + monster

    VkDescriptorPoolCreateInfo descPoolInfo{};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.maxSets = 1;
    descPoolInfo.poolSizeCount = 3;
    descPoolInfo.pPoolSizes = sizes;

    if (vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        fatal("Failed to create descriptor pool");
    }

    // === ALLOCATE GLOBAL DESCRIPTOR SET ===
    VkDescriptorSetAllocateInfo descAllocInfo{};
    descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descAllocInfo.descriptorPool = m_descriptorPool;
    descAllocInfo.descriptorSetCount = 1;
    descAllocInfo.pSetLayouts = &g_ctx().globalDescriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &descAllocInfo, &g_ctx().globalDescriptorSet) != VK_SUCCESS) {
        fatal("Failed to allocate global descriptor set");
    }

    // === MONSTER TEXTURE & SAMPLER ===
    m_monsterTexture = createTextureImageFromFile(Options::MonsterTexturePath);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(m_monsterTexture.mipLevels);

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_monsterSampler) != VK_SUCCESS) {
        fatal("Failed to create monster sampler");
    }

    // Static update for monster texture (binding 3)
    VkDescriptorImageInfo monsterDesc{};
    monsterDesc.sampler = m_monsterSampler;
    monsterDesc.imageView = m_monsterTexture.view;
    monsterDesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet monsterWrite{};
    monsterWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    monsterWrite.dstSet = g_ctx().globalDescriptorSet;
    monsterWrite.dstBinding = 3;
    monsterWrite.descriptorCount = 1;
    monsterWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    monsterWrite.pImageInfo = &monsterDesc;

    vkUpdateDescriptorSets(device, 1, &monsterWrite, 0, nullptr);

    // === RT OUTPUT IMAGES (accumulation buffers) ===
    rtOutputImages_.resize(MAX_FRAMES_IN_FLIGHT);
    rtOutputViews_.resize(MAX_FRAMES_IN_FLIGHT);
    rtOutputMemories_.resize(MAX_FRAMES_IN_FLIGHT);

    VkFormat rtFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(g_ctx().physicalDevice, &memProps);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = rtFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        VkImage image;
        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            fatal("Failed to create RT output image");
        }
        rtOutputImages_[i] = Handle<VkImage>(image, device, vkDestroyImage);

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device, image, &memReqs);

        uint32_t memTypeIndex = UINT32_MAX;
        for (uint32_t j = 0; j < memProps.memoryTypeCount; ++j) {
            if ((memReqs.memoryTypeBits & (1u << j)) && (memProps.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                memTypeIndex = j;
                break;
            }
        }
        if (memTypeIndex == UINT32_MAX) {
            for (uint32_t j = 0; j < memProps.memoryTypeCount; ++j) {
                if (memReqs.memoryTypeBits & (1u << j)) {
                    memTypeIndex = j;
                    break;
                }
            }
        }
        if (memTypeIndex == UINT32_MAX) fatal("No memory type for RT output");

        VkMemoryAllocateInfo memAlloc{};
        memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = memTypeIndex;

        VkDeviceMemory memory;
        if (vkAllocateMemory(device, &memAlloc, nullptr, &memory) != VK_SUCCESS) {
            fatal("Failed to allocate RT output memory");
        }
        rtOutputMemories_[i] = Handle<VkDeviceMemory>(memory, device, vkFreeMemory);

        vkBindImageMemory(device, image, memory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = rtFormat;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkImageView view;
        if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
            fatal("Failed to create RT output view");
        }
        rtOutputViews_[i] = Handle<VkImageView>(view, device, vkDestroyImageView);
    }

    // === COMMAND POOL & BUFFER ===
    VkCommandPoolCreateInfo cmdPoolInfo{};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolInfo.queueFamilyIndex = g_ctx().queueFamily;
    if (vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &m_cmdPool) != VK_SUCCESS) {
        fatal("Failed to create command pool");
    }

    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = m_cmdPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &m_cmdBuffer) != VK_SUCCESS) {
        fatal("Failed to allocate command buffer");
    }

    // === PIPELINES ===
    createRayTracingPipeline();
    createShaderBindingTable();
    createTonemapPipeline();

    printf("[2025] VulkanRenderer initialized — %dx%d accumulation buffers ready — windmill turns\n", width_, height_);
}

VulkanRenderer::~VulkanRenderer() {
    VkDevice device = g_ctx().device;

    if (tonemapPipeline_) vkDestroyPipeline(device, tonemapPipeline_, nullptr);
    if (tonemapLayout_) vkDestroyPipelineLayout(device, tonemapLayout_, nullptr);

    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline_, nullptr);
    }

    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout_, nullptr);
    }

    if (m_environmentSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_environmentSampler, nullptr);
    }

    if (m_environmentMap.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_environmentMap.view, nullptr);
    }

    if (m_environmentMap.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_environmentMap.image, nullptr);
    }

    if (m_environmentMap.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_environmentMap.memory, nullptr);
    }

    if (m_monsterSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_monsterSampler, nullptr);
    }

    if (m_monsterTexture.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_monsterTexture.view, nullptr);
    }

    if (m_monsterTexture.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_monsterTexture.image, nullptr);
    }

    if (m_monsterTexture.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_monsterTexture.memory, nullptr);
    }

    if (m_cmdPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, m_cmdPool, nullptr);
    }

    if (g_ctx().globalDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, g_ctx().globalDescriptorSetLayout, nullptr);
    }

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
    }
}

void VulkanRenderer::createTonemapPipeline()
{
    VkDevice device = g_ctx().device;

    VkShaderModule tonemapModule = loadShader("tonemap.spv");

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = tonemapModule;
    stage.pName = "main";

    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc.offset = 0;
    pc.size = sizeof(float);  // exposure

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &g_ctx().globalDescriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pc;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &tonemapLayout_) != VK_SUCCESS) {
        fatal("Failed to create tonemap pipeline layout");
    }

    VkComputePipelineCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    createInfo.stage = stage;
    createInfo.layout = tonemapLayout_;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &tonemapPipeline_) != VK_SUCCESS) {
        fatal("Failed to create tonemap pipeline");
    }

    vkDestroyShaderModule(device, tonemapModule, nullptr);
}

VkCommandBuffer VulkanRenderer::recordFrame(const Camera& camera, float deltaTime, uint32_t swapImageIndex) noexcept
{
    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    uint32_t frameIndex = currentFrame_;

    totalTime_ += deltaTime;

    if (cameraMoved_) {
        currentSpp_ = 0;
        cameraMoved_ = false;
    }

    // === UPDATE DESCRIPTOR SET (global set — shared for RT + tonemap) ===
    VkAccelerationStructureKHR tlas = CurrentTLAS();

    VkWriteDescriptorSetAccelerationStructureKHR accelWrite{};
    accelWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    accelWrite.accelerationStructureCount = 1;
    accelWrite.pAccelerationStructures = tlas ? &tlas : nullptr;

    VkDescriptorImageInfo storageInfo{};
    storageInfo.imageView = rtOutputViews_[frameIndex].get();
    storageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo envInfo{};
    envInfo.sampler = m_environmentSampler;
    envInfo.imageView = m_environmentMap.view;
    envInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo monsterInfo{};
    monsterInfo.sampler = m_monsterSampler;
    monsterInfo.imageView = m_monsterTexture.view;
    monsterInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[4]{};
    uint32_t writeCount = 0;

    writes[writeCount] = {};
    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[writeCount].pNext = &accelWrite;
    writes[writeCount].dstSet = g_ctx().globalDescriptorSet;
    writes[writeCount].dstBinding = 0;
    writes[writeCount].descriptorCount = 1;
    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    ++writeCount;

    writes[writeCount] = {};
    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[writeCount].dstSet = g_ctx().globalDescriptorSet;
    writes[writeCount].dstBinding = 1;
    writes[writeCount].descriptorCount = 1;
    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[writeCount].pImageInfo = &storageInfo;
    ++writeCount;

    if (m_hasEnvironmentMap) {
        writes[writeCount] = {};
        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].dstSet = g_ctx().globalDescriptorSet;
        writes[writeCount].dstBinding = 2;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[writeCount].pImageInfo = &envInfo;
        ++writeCount;
    }

    writes[writeCount] = {};
    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[writeCount].dstSet = g_ctx().globalDescriptorSet;
    writes[writeCount].dstBinding = 3;
    writes[writeCount].descriptorCount = 1;
    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[writeCount].pImageInfo = &monsterInfo;
    ++writeCount;

    vkUpdateDescriptorSets(g_ctx().device, writeCount, writes, 0, nullptr);

    // === PUSH CONSTANTS ===
    PushConstants push{};

    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);

    push.invView   = glm::inverse(camera.view());
    push.invProj   = glm::inverse(proj);
    push.totalTime = totalTime_;
    push.spp       = currentSpp_;
    push.frameSeed = currentFrame_ ^ 0xDEADBEEF;
    push.forceEnvOnly = Options::ForceEnvironmentOnly ? 1u : 0u;
    push.jitterStrength = Options::EnableJitterAA ? Options::JitterStrength : 0.0f;
    push.maxRecursion = Options::MaxRayDepth;
    push.useEnvSky = Options::UseEnvironmentAsSky ? 1u : 0u;
    push.flipEnvV = Options::FlipEnvironmentV ? 1u : 0u;
    push.showHotPink = Options::ShowHotPinkOnHit ? 1u : 0u;
    push.environmentExposure = Options::EnvironmentExposure;
    push.skyIntensity = Options::SkyIntensity;
    push.environmentRotationY = Options::EnvironmentRotationY;
    push.billboardBaseColor = Options::BillboardBaseColor;
    push.billboardAlphaCutoff = Options::BillboardAlphaCutoff;
    push.billboardUseAlphaBlend = Options::BillboardUseAlphaBlend ? 1u : 0u;

    vkResetCommandBuffer(m_cmdBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(m_cmdBuffer, &beginInfo) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    VkImage rtImage   = rtOutputImages_[frameIndex].get();
    VkImage swapImage = g_swapchainImages()[swapImageIndex];

    // === RAY TRACING PASS ===
    vkCmdBindPipeline(m_cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_);
    vkCmdBindDescriptorSets(m_cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout_, 0, 1, &g_ctx().globalDescriptorSet, 0, nullptr);
    vkCmdPushConstants(m_cmdBuffer, pipelineLayout_,
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                       0, sizeof(PushConstants), &push);

    g_ext.vkCmdTraceRaysKHR(m_cmdBuffer,
                            &g_raygenSbt,
                            &g_missSbt,
                            &g_hitSbt,
                            &g_callableSbt,
                            static_cast<uint32_t>(width_),
                            static_cast<uint32_t>(height_),
                            1);

    ++currentSpp_;

    // === TONEMAP PASS (reuse global descriptor set — binding 1 is storage image) ===
    float tonemapExposure = Options::EnvironmentExposure > 0.001f ? 1.0f / Options::EnvironmentExposure : 1.0f;

    vkCmdBindPipeline(m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapPipeline_);
    vkCmdBindDescriptorSets(m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapLayout_, 0, 1, &g_ctx().globalDescriptorSet, 0, nullptr);
    vkCmdPushConstants(m_cmdBuffer, tonemapLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &tonemapExposure);
    vkCmdDispatch(m_cmdBuffer, (width_ + 15) / 16, (height_ + 15) / 16, 1);

    // Barrier after tonemap
    VkMemoryBarrier tonemapBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT};
    vkCmdPipelineBarrier(m_cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &tonemapBarrier, 0, nullptr, 0, nullptr);

    // === BLIT TO SWAPCHAIN ===
    VkImageMemoryBarrier barriers[2]{};

    barriers[0] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                  rtImage, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    barriers[1] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                  swapImage, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    vkCmdPipelineBarrier(m_cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

    VkImageBlit blit{};
    blit.srcSubresource = blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[1] = {width_, height_, 1};
    blit.dstOffsets[1] = {static_cast<int32_t>(g_ctx().width), static_cast<int32_t>(g_ctx().height), 1};

    vkCmdBlitImage(m_cmdBuffer, rtImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    VkImageMemoryBarrier presentBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapImage, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    vkCmdPipelineBarrier(m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

    if (vkEndCommandBuffer(m_cmdBuffer) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return m_cmdBuffer;
}

void VulkanRenderer::setEnvironmentMap(const std::string& filename) noexcept {
    if (filename.empty()) {
        printf("[ENV] No filename provided — environment map unchanged\n");
        return;
    }

    VkDevice device = g_ctx().device;

    // Cleanup previous envmap if exists
    if (m_hasEnvironmentMap) {
        if (m_environmentMap.view)       vkDestroyImageView(device, m_environmentMap.view, nullptr);
        if (m_environmentMap.image)      vkDestroyImage(device, m_environmentMap.image, nullptr);
        if (m_environmentMap.memory)     vkFreeMemory(device, m_environmentMap.memory, nullptr);
        if (m_environmentSampler)        vkDestroySampler(device, m_environmentSampler, nullptr);

        m_environmentMap = {};
        m_environmentSampler = VK_NULL_HANDLE;
    }

    // Try SDL3_image first (supports WebP with alpha)
    SDL_Surface* surface = IMG_Load(filename.c_str());
    if (surface) {
        // Convert to a consistent, platform-independent format: RGBA8888
        // This ensures byte order is always R(0), G(1), B(2), A(3) regardless of endianness
        SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
        SDL_DestroySurface(surface);
        if (!converted) {
            printf("[ENV] Failed to convert surface to RGBA8888: %s\n", SDL_GetError());
            return;
        }
        surface = converted;

        int width = surface->w;
        int height = surface->h;

        printf("[ENV] Loading SDR/WebP envmap via SDL3_image: %s — %dx%d\n",
               filename.c_str(), width, height);

        // Convert to float32 RGBA with gamma correction and exposure boost
        std::vector<float> floatData(width * height * 4);
        uint8_t* pixels = static_cast<uint8_t*>(surface->pixels);
        int pitch = surface->pitch;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                uint8_t* pixel = pixels + y * pitch + x * 4;
                float r = pixel[0] / 255.0f;  // R
                float g = pixel[1] / 255.0f;  // G
                float b = pixel[2] / 255.0f;  // B
                float a = pixel[3] / 255.0f;  // A

                // sRGB gamma to linear + exposure boost (common for envmaps)
                r = powf(r, 2.2f) * Options::EnvironmentExposure;
                g = powf(g, 2.2f) * Options::EnvironmentExposure;
                b = powf(b, 2.2f) * Options::EnvironmentExposure;

                int idx = (y * width + x) * 4;
                floatData[idx + 0] = r;
                floatData[idx + 1] = g;
                floatData[idx + 2] = b;
                floatData[idx + 3] = a;  // Preserve alpha for potential transparency effects
            }
        }

        SDL_DestroySurface(surface);

        uploadEnvironmentMap(floatData.data(), width, height);
        m_hasEnvironmentMap = true;
        printf("[ENV] SDR/WebP envmap loaded (full alpha support): %s — the sky breathes in modern glory\n", filename.c_str());
        return;
    }

    // Fallback to stb_image for true HDR (.hdr, .exr, etc.)
    int width, height, channels;
    float* data = stbi_loadf(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) {
        printf("[ENV] Failed to load image: %s (neither SDL3_image nor stb succeeded)\n", filename.c_str());
        return;
    }

    printf("[ENV] Loading HDR envmap via stb: %s — %dx%d — Plastic Beach sky rising\n",
           filename.c_str(), width, height);

    // Apply exposure to HDR data (already linear)
    for (int i = 0; i < width * height; ++i) {
        int idx = i * 4;
        data[idx + 0] *= Options::EnvironmentExposure;
        data[idx + 1] *= Options::EnvironmentExposure;
        data[idx + 2] *= Options::EnvironmentExposure;
        // alpha unchanged
    }

    uploadEnvironmentMap(data, width, height);

    stbi_image_free(data);
    m_hasEnvironmentMap = true;
    printf("[ENV] HDR envmap loaded and bound: %s — the sky now breathes over Plastic Beach\n", filename.c_str());
}

void VulkanRenderer::uploadEnvironmentMap(const float* data, int width, int height) noexcept
{
    VkDevice device = g_ctx().device;
    VkQueue queue = g_ctx().queue;

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 16;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    auto cleanup = [&]() {
        if (stagingBuffer) vkDestroyBuffer(device, stagingBuffer, nullptr);
        if (stagingMemory) vkFreeMemory(device, stagingMemory, nullptr);
        if (commandBuffer && commandPool) vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
    };

    // Staging buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) { cleanup(); return; }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS ||
        vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) { cleanup(); return; }

    void* mapped;
    if (vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mapped) != VK_SUCCESS) { cleanup(); return; }
    memcpy(mapped, data, imageSize);
    vkUnmapMemory(device, stagingMemory);

    // Image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_environmentMap.image) != VK_SUCCESS) { cleanup(); return; }

    vkGetImageMemoryRequirements(device, m_environmentMap.image, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_environmentMap.memory) != VK_SUCCESS ||
        vkBindImageMemory(device, m_environmentMap.image, m_environmentMap.memory, 0) != VK_SUCCESS) { cleanup(); return; }

    // Upload via command buffer
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = g_ctx().queueFamily;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) { cleanup(); return; }

    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = commandPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &cmdAlloc, &commandBuffer) != VK_SUCCESS) { cleanup(); return; }

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.image = m_environmentMap.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy copyRegion{};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_environmentMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    // View
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = m_environmentMap.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(device, &viewInfo, nullptr, &m_environmentMap.view) != VK_SUCCESS) { cleanup(); return; }

    // Sampler
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_environmentSampler) != VK_SUCCESS) { cleanup(); return; }

    // Update descriptor
    VkDescriptorImageInfo descImage{};
    descImage.sampler = m_environmentSampler;
    descImage.imageView = m_environmentMap.view;
    descImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = g_ctx().globalDescriptorSet;
    write.dstBinding = 2;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &descImage;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    cleanup();

    m_envMapWidth = width;
    m_envMapHeight = height;
}

Texture VulkanRenderer::createTextureImage(const float* pixels, uint32_t width, uint32_t height,
                                           VkFormat format, VkImageUsageFlags usage /*= 0*/)
{
    VkDevice device = g_ctx().device;

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 16; // 4 floats = 16 bytes

    // === Staging buffer (host visible) ===
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = imageSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        fatal("Failed to create staging buffer for texture");
    }

    VkMemoryRequirements memReqs{};
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to allocate staging memory for texture");
    }

    vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

    void* mapped;
    if (vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mapped) != VK_SUCCESS) {
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to map staging memory for texture");
    }
    memcpy(mapped, pixels, imageSize);
    vkUnmapMemory(device, stagingMemory);

    // === Device-local image ===
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = format;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | usage;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    Texture tex{};
    tex.width     = width;
    tex.height    = height;
    tex.format    = format;
    tex.mipLevels = 1;

    if (vkCreateImage(device, &imageInfo, nullptr, &tex.image) != VK_SUCCESS) {
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to create texture image");
    }

    vkGetImageMemoryRequirements(device, tex.image, &memReqs);
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &tex.memory) != VK_SUCCESS) {
        vkDestroyImage(device, tex.image, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to allocate texture memory");
    }

    vkBindImageMemory(device, tex.image, tex.memory, 0);

    // === One-time copy command ===
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = g_ctx().queueFamily;

    VkCommandPool cmdPool;
    vkCreateCommandPool(device, &poolInfo, nullptr, &cmdPool);

    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool        = cmdPool;
    cmdAlloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    // Transition to transfer dst
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask       = 0;
    barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.image               = tex.image;
    barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent                 = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, stagingBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to shader read
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    vkQueueSubmit(g_ctx().graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_ctx().graphicsQueue);

    // Cleanup
    vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
    vkDestroyCommandPool(device, cmdPool, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    // Create view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = tex.image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &tex.view) != VK_SUCCESS) {
        vkFreeMemory(device, tex.memory, nullptr);
        vkDestroyImage(device, tex.image, nullptr);
        fatal("Failed to create texture image view");
    }

    return tex;
}

Texture VulkanRenderer::createTextureImageFromFile(const std::string& filename, VkImageUsageFlags extraUsage) noexcept
{
    // Use SDL3_image to load WebP (or PNG/JPG/etc.) with full alpha support
    SDL_Surface* surface = IMG_Load(filename.c_str());
    if (!surface) {
        printf("[TEX] Failed to load texture '%s': %s\n", filename.c_str(), SDL_GetError());
        return {};
    }

    // Convert to a consistent, easy-to-read format: RGBA8888 (R G B A order in memory)
    SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
    SDL_DestroySurface(surface);
    if (!converted) {
        printf("[TEX] Failed to convert texture '%s' to RGBA8888: %s\n", filename.c_str(), SDL_GetError());
        return {};
    }
    surface = converted;

    uint32_t width  = static_cast<uint32_t>(surface->w);
    uint32_t height = static_cast<uint32_t>(surface->h);

    printf("[TEX] Loaded texture '%s' — %ux%u (with alpha support)\n", filename.c_str(), width, height);

    // Convert to linear float32 RGBA (preserve alpha, apply sRGB → linear for RGB)
    std::vector<float> floatPixels(width * height * 4);
    uint8_t* pixels = static_cast<uint8_t*>(surface->pixels);
    int pitch = surface->pitch;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t* src = pixels + y * pitch + x * 4;
            float r = src[0] / 255.0f;  // R
            float g = src[1] / 255.0f;  // G
            float b = src[2] / 255.0f;  // B
            float a = src[3] / 255.0f;  // A

            // sRGB to linear for color channels (alpha stays as-is)
            r = powf(r, 2.2f);
            g = powf(g, 2.2f);
            b = powf(b, 2.2f);

            size_t idx = (y * width + x) * 4;
            floatPixels[idx + 0] = r;
            floatPixels[idx + 1] = g;
            floatPixels[idx + 2] = b;
            floatPixels[idx + 3] = a;  // Full transparency preserved
        }
    }

    SDL_DestroySurface(surface);

    // Use the existing float-pixel uploader with SAMPLER usage
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | extraUsage;
    return createTextureImage(floatPixels.data(), width, height, VK_FORMAT_R32G32B32A32_SFLOAT, usage);
}

void VulkanRenderer::createRayTracingPipeline()
{
    // === PUSH CONSTANT RANGE ===
    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                              VK_SHADER_STAGE_MISS_BIT_KHR |
                              VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                              VK_SHADER_STAGE_ANY_HIT_BIT_KHR;  // For any-hit alpha test
    pushConstant.offset = 0;
    pushConstant.size   = sizeof(PushConstants);

    VkDescriptorSetLayout rawLayout = g_ctx().globalDescriptorSetLayout;

    // === PIPELINE LAYOUT USING GLOBAL LAYOUT ===
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &rawLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstant;

    if (vkCreatePipelineLayout(g_ctx().device, &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        fatal("FAILED TO CREATE PIPELINE LAYOUT — GLOBAL LAYOUT REJECTED — the beach denies the light");
    }

    // Load shaders using the member function
    VkShaderModule raygenModule   = loadShader("raygen.spv");
    VkShaderModule missModule     = loadShader("miss.spv");
    VkShaderModule closestHitModule = loadShader("closesthit.spv");
    VkShaderModule anyHitModule     = loadShader("anyhit.spv");

    // === SHADER STAGES ===
    VkPipelineShaderStageCreateInfo stages[4] = {};

    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage  = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[0].module = raygenModule;
    stages[0].pName  = "main";

    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage  = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[1].module = missModule;
    stages[1].pName  = "main";

    stages[2] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[2].stage  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stages[2].module = closestHitModule;
    stages[2].pName  = "main";

    stages[3] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[3].stage  = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    stages[3].module = anyHitModule;
    stages[3].pName  = "main";

    // === SHADER GROUPS ===
    VkRayTracingShaderGroupCreateInfoKHR groups[4] = {};

    // Group 0: Raygen
    groups[0].sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[0].type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].generalShader      = 0;
    groups[0].closestHitShader   = VK_SHADER_UNUSED_KHR;
    groups[0].anyHitShader       = VK_SHADER_UNUSED_KHR;
    groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

    // Group 1: Miss
    groups[1].sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[1].type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[1].generalShader      = 1;
    groups[1].closestHitShader   = VK_SHADER_UNUSED_KHR;
    groups[1].anyHitShader       = VK_SHADER_UNUSED_KHR;
    groups[1].intersectionShader = VK_SHADER_UNUSED_KHR;

    // Group 2: Hit group with any-hit (for alpha cutout)
    groups[2].sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[2].type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[2].generalShader      = VK_SHADER_UNUSED_KHR;
    groups[2].closestHitShader   = 2;
    groups[2].anyHitShader       = 3;
    groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;

    // Group 3: Hit group without any-hit (optional fallback, not used here)
    groups[3].sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[3].type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[3].generalShader      = VK_SHADER_UNUSED_KHR;
    groups[3].closestHitShader   = 2;
    groups[3].anyHitShader       = VK_SHADER_UNUSED_KHR;
    groups[3].intersectionShader = VK_SHADER_UNUSED_KHR;

    // === CREATE RAY TRACING PIPELINE ===
    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount                   = 4;  // Now including any-hit
    pipelineInfo.pStages                      = stages;
    pipelineInfo.groupCount                   = 4;
    pipelineInfo.pGroups                      = groups;
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout                       = pipelineLayout_;

    VkResult result = g_ext.vkCreateRayTracingPipelinesKHR(
        g_ctx().device,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        1,
        &pipelineInfo,
        nullptr,
        &pipeline_
    );

    if (result != VK_SUCCESS) {
        printf("[FATAL] vkCreateRayTracingPipelinesKHR failed with VkResult: %d\n", result);
        fatal("FAILED TO CREATE RAY TRACING PIPELINE — driver rejects eternal radiance");
    }

    // Cleanup shader modules
    vkDestroyShaderModule(g_ctx().device, raygenModule, nullptr);
    vkDestroyShaderModule(g_ctx().device, missModule, nullptr);
    vkDestroyShaderModule(g_ctx().device, closestHitModule, nullptr);
    vkDestroyShaderModule(g_ctx().device, anyHitModule, nullptr);

    printf("[2025] RAY TRACING PIPELINE FORGED USING GLOBAL DESCRIPTOR LAYOUT (with any-hit support)\n");
    printf("[2025] FIRST LIGHT ACHIEVED — VALHALLA AWAKENS FULLY — PLASTIC BEACH v∞ SEES ALL\n");
}

void VulkanRenderer::createShaderBindingTable()
{
    // Early exit if already created
    if (g_raygenSbt.deviceAddress != 0) {
        return;
    }

    const uint32_t handleSize      = g_ctx().rtProps.shaderGroupHandleSize;
    const uint32_t handleAlign     = g_ctx().rtProps.shaderGroupHandleAlignment;
    const uint32_t baseAlign       = g_ctx().rtProps.shaderGroupBaseAlignment;

    // Align individual shader group handles
    const uint32_t alignedHandleSize = (handleSize + handleAlign - 1) & ~(handleAlign - 1);

    // Stride = size of one aligned handle (required by Vulkan spec)
    const uint32_t sbtStride = alignedHandleSize;

    // We now have 4 groups: raygen, miss, hit (closest only), hit (closest + any-hit)
    VkDeviceSize sbtSize = static_cast<VkDeviceSize>(sbtStride) * 4;

    // Align entire SBT to base alignment
    sbtSize = (sbtSize + baseAlign - 1) & ~(baseAlign - 1);

    // Allocate in GPU memory pool
    uint64_t sbtHandle = BufferCreate(sbtSize);
    if (sbtHandle == 0) {
        fatal("SBT allocation failed — shaders scatter");
    }

    VkDeviceAddress sbtBaseAddress = BufferGetDeviceAddress(sbtHandle);

    // Retrieve shader group handles (4 groups now)
    std::vector<uint8_t> shaderHandles(4 * handleSize);
    VkResult result = g_ext.vkGetRayTracingShaderGroupHandlesKHR(
        g_ctx().device,
        pipeline_,
        0,                  // firstGroup
        4,                  // groupCount
        shaderHandles.size(),
        shaderHandles.data()
    );

    if (result != VK_SUCCESS) {
        fatal("Failed to get ray tracing shader group handles");
    }

    // Staging buffer on host
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = sbtSize;
    bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(g_ctx().device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        fatal("Failed to create SBT staging buffer");
    }

    VkMemoryRequirements memReqs{};
    vkGetBufferMemoryRequirements(g_ctx().device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if (vkAllocateMemory(g_ctx().device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        vkDestroyBuffer(g_ctx().device, stagingBuffer, nullptr);
        fatal("Failed to allocate SBT staging memory");
    }

    vkBindBufferMemory(g_ctx().device, stagingBuffer, stagingMemory, 0);

    // Fill staging buffer
    void* mapped = nullptr;
    vkMapMemory(g_ctx().device, stagingMemory, 0, sbtSize, 0, &mapped);
    memset(mapped, 0, sbtSize);

    uint8_t* dst = static_cast<uint8_t*>(mapped);
    const uint8_t* src = shaderHandles.data();

    memcpy(dst + 0 * sbtStride, src + 0 * handleSize, handleSize); // raygen (group 0)
    memcpy(dst + 1 * sbtStride, src + 1 * handleSize, handleSize); // miss (group 1)
    memcpy(dst + 2 * sbtStride, src + 2 * handleSize, handleSize); // hit without any-hit (group 2) — unused
    memcpy(dst + 3 * sbtStride, src + 3 * handleSize, handleSize); // hit with any-hit (group 3)

    vkUnmapMemory(g_ctx().device, stagingMemory);

    // Copy to GPU pool
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer cmd       = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = g_ctx().queueFamily;

    vkCreateCommandPool(g_ctx().device, &poolInfo, nullptr, &commandPool);

    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool        = commandPool;
    cmdAlloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;

    vkAllocateCommandBuffers(g_ctx().device, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size      = sbtSize;
    copyRegion.dstOffset = g_ctx().offsets[sbtHandle];

    vkCmdCopyBuffer(cmd, stagingBuffer, g_ctx().poolBuffer, 1, &copyRegion);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    vkQueueSubmit(g_ctx().graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_ctx().graphicsQueue);

    // Cleanup
    vkFreeCommandBuffers(g_ctx().device, commandPool, 1, &cmd);
    vkDestroyCommandPool(g_ctx().device, commandPool, nullptr);
    vkDestroyBuffer(g_ctx().device, stagingBuffer, nullptr);
    vkFreeMemory(g_ctx().device, stagingMemory, nullptr);

    // === SBT REGIONS ===
    // Use group 3 for hit (closest + any-hit) — alpha cutout enabled
    g_raygenSbt   = { sbtBaseAddress + 0 * sbtStride, sbtStride, sbtSize };
    g_missSbt     = { sbtBaseAddress + 1 * sbtStride, sbtStride, sbtSize };
    g_hitSbt      = { sbtBaseAddress + 3 * sbtStride, sbtStride, sbtSize };  // Group 3 = any-hit + closest
    g_callableSbt = { 0, 0, 0 };

    printf("[2025] SBT FORGED CORRECTLY — Plastic Beach armed (with any-hit alpha cutout)\n");
}

// =============================================================================
// CORE ENGINE FUNCTIONS
// =============================================================================
inline void createSwapchain() noexcept
{
    auto& ctx = g_ctx();
    VkDevice device = ctx.device;
    VkPhysicalDevice phys = ctx.physicalDevice;
    VkSurfaceKHR surface = ctx.surface;

    if (device == VK_NULL_HANDLE || surface == VK_NULL_HANDLE) {
        return;
    }

    vkDeviceWaitIdle(device);

    // Cleanup old swapchain
    for (auto view : g_swapchainViews()) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
        }
    }
    g_swapchainViews().clear();

    if (g_swapchain() != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, g_swapchain(), nullptr);
        g_swapchain() = VK_NULL_HANDLE;
    }
    g_swapchainImages().clear();

    // Query surface capabilities
    VkSurfaceCapabilitiesKHR caps{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps) != VK_SUCCESS) {
        fatal("Failed to query surface capabilities");
    }

    // Get window pixel size (high-DPI aware)
    int drawableW, drawableH;
    SDL_GetWindowSizeInPixels(g_window().get(), &drawableW, &drawableH);
    drawableW = std::max(drawableW, 1);
    drawableH = std::max(drawableH, 1);

    VkExtent2D extent{};
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width  = static_cast<uint32_t>(drawableW);
        extent.height = static_cast<uint32_t>(drawableH);
        extent.width  = std::clamp(extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    ctx.width  = extent.width;
    ctx.height = extent.height;

    // Preferred sRGB format
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, formats.data());

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            break;
        }
    }

    // === PRESENT MODE SELECTION USING OPTIONS ===
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;  // Safe default (vsync)

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, modes.data());

    if (Options::ForceVSync) {
        // Hard vsync - capped, no tearing
        presentMode = VK_PRESENT_MODE_FIFO_KHR;
    } else {
        // Uncapped rendering
        if (Options::PreferMailboxForNoTearing) {
            // Prefer MAILBOX: uncapped + no tearing (best when supported)
            for (auto mode : modes) {
                if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    presentMode = mode;
                    break;
                }
            }
        }

        // If MAILBOX not used/available, fall back to IMMEDIATE (max FPS, possible tearing)
        if (presentMode != VK_PRESENT_MODE_MAILBOX_KHR) {
            bool hasImmediate = false;
            for (auto mode : modes) {
                if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                    hasImmediate = true;
                    break;
                }
            }
            if (hasImmediate) {
                presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }

    // Image count: 3 for MAILBOX (optimal triple buffer), user base otherwise
    uint32_t imageCount = (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) ? 3 : Options::SwapchainImageCount;
    imageCount = std::max(imageCount, caps.minImageCount);
    if (caps.maxImageCount > 0) {
        imageCount = std::min(imageCount, caps.maxImageCount);
    }

    // Create swapchain
    VkSwapchainCreateInfoKHR info{};
    info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface          = surface;
    info.minImageCount    = imageCount;
    info.imageFormat      = chosenFormat.format;
    info.imageColorSpace  = chosenFormat.colorSpace;
    info.imageExtent      = extent;
    info.imageArrayLayers = 1;
    info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform     = caps.currentTransform;
    info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode      = presentMode;
    info.clipped          = VK_TRUE;
    info.oldSwapchain     = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &info, nullptr, &g_swapchain()) != VK_SUCCESS) {
        fatal("Swapchain creation failed");
    }

    // Retrieve images and create views
    uint32_t imgCount = 0;
    vkGetSwapchainImagesKHR(device, g_swapchain(), &imgCount, nullptr);
    g_swapchainImages().resize(imgCount);
    vkGetSwapchainImagesKHR(device, g_swapchain(), &imgCount, g_swapchainImages().data());

    g_swapchainViews().resize(imgCount);
    for (uint32_t i = 0; i < imgCount; ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = g_swapchainImages()[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = chosenFormat.format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        if (vkCreateImageView(device, &viewInfo, nullptr, &g_swapchainViews()[i]) != VK_SUCCESS) {
            fatal("Failed to create swapchain image view");
        }
    }

    const char* modeName;
    if (Options::ForceVSync) {
        modeName = "FIFO (VSYNC - CAPPED, NO TEARING)";
    } else if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        modeName = "MAILBOX (UNCAPPED + NO TEARING)";
    } else {
        modeName = "IMMEDIATE (UNCAPPED - POSSIBLE TEARING)";
    }

    printf("[SWAPCHAIN] Created - %ux%u - %u images - %s\n",
           extent.width, extent.height, imgCount, modeName);
}

} // namespace RTX

// =============================================================================
// PLASTIC BEACH v∞ - DECEMBER 15, 2025 - FULL EXPOSURE - THE ENGINE IS YOURS
// =============================================================================