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
#include <memory>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stb/stb_image.h>
#include <SDL3_image/SDL_image.h>
#include "options.hpp"
#include <map>
#include <tuple>
#include <sstream>
#include <algorithm>

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
        if (raw != T{} && device != VK_NULL_HANDLE && destroyer) destroyer(device, raw, nullptr);
        raw = T{}; device = VK_NULL_HANDLE; destroyer = nullptr;
    }

    explicit operator bool() const noexcept { return raw != T{} && device != VK_NULL_HANDLE; }
    T get() const noexcept { return raw; }
};

// =============================================================================
// FATAL ERROR
// =============================================================================
[[noreturn]] inline void fatal(const char* msg) {
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
};

inline Context& g_ctx() { static Context ctx{}; return ctx; }

inline VkSwapchainKHR&           g_swapchain()       { return g_ctx().swapchain; }
inline std::vector<VkImage>&     g_swapchainImages()  { return g_ctx().swapchainImages; }
inline std::vector<VkImageView>& g_swapchainViews()   { return g_ctx().swapchainViews; }
inline VkQueue&                  g_presentQueue()    { return g_ctx().presentQueue; }

// SDL window - direct global access via smart pointer
inline auto& g_window() { static std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> ptr(nullptr, SDL_DestroyWindow); return ptr; }

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

inline RTXExtensions& g_ext() { static RTXExtensions ext{}; return ext; }

// =============================================================================
// SHADER BINDING TABLE REGIONS
// =============================================================================
inline VkStridedDeviceAddressRegionKHR& g_raygenSbt()   { static VkStridedDeviceAddressRegionKHR region{}; return region; }
inline VkStridedDeviceAddressRegionKHR& g_missSbt()     { static VkStridedDeviceAddressRegionKHR region{}; return region; }
inline VkStridedDeviceAddressRegionKHR& g_hitSbt()      { static VkStridedDeviceAddressRegionKHR region{}; return region; }
inline VkStridedDeviceAddressRegionKHR& g_callableSbt() { static VkStridedDeviceAddressRegionKHR region{}; return region; }

// =============================================================================
// CAMERA - STANDARD +Y UP, RIGHT-HANDED COORDINATE SYSTEM
// =============================================================================
struct Camera {
    glm::vec3 position = glm::vec3(Options::CameraStartPosition.x,
                                   Options::CameraEyeHeight,
                                   Options::CameraStartPosition.z);

    float yaw   = Options::CameraStartYaw;
    float pitch = Options::CameraStartPitch;

    glm::vec3 front;
    glm::vec3 right;
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    float velocityY = 0.0f;
    bool  onGround  = true;

    float bobblePhase = 0.0f;

    Camera() { updateVectors(); }

    void updateVectors() noexcept {
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(front);
        right = glm::normalize(glm::cross(front, up));
    }

    [[nodiscard]] glm::mat4 view() const noexcept {
        return glm::lookAt(position, position + front, up);
    }

    void look(float dx, float dy, float sensitivity = 1.0f) noexcept {
        if (dx == 0.0f && dy == 0.0f) return;
        yaw += dx * sensitivity;
        float pitchDelta = dy * sensitivity;
        if (Options::InvertMouseLook) pitchDelta = -pitchDelta;
        pitch += pitchDelta;
        pitch = glm::clamp(pitch, -89.9f, 89.9f);
        updateVectors();
    }

    void updatePhysics(float dt) {
        if (!onGround) velocityY -= Options::GravityStrength * dt;
        else velocityY = 0.0f;
        float newY = position.y + velocityY * dt;
        const float groundY = Options::GroundLevel;
        if (newY <= groundY + Options::CameraEyeHeight) {
            position.y = groundY + Options::CameraEyeHeight;
            velocityY = 0.0f;
            onGround = true;
        } else {
            position.y = newY;
            onGround = false;
        }
    }

    void jump() {
        if (onGround) {
            velocityY = Options::JumpForce;
            onGround = false;
        }
    }

    void moveHorizontal(const glm::vec3& direction, float speed) {
        glm::vec3 flatDir = glm::vec3(direction.x, 0.0f, direction.z);
        if (glm::length(flatDir) > 0.0f) flatDir = glm::normalize(flatDir);
        position += flatDir * speed;
    }

    void applyBobble(float totalTime, bool isMoving) {
        if (Options::EnableCameraBobble && isMoving) {
            bobblePhase = totalTime * Options::CameraBobbleFrequency * 2.0f * glm::pi<float>();
            float bobOffset = sin(bobblePhase) * Options::CameraBobbleAmplitude;
            position.y += bobOffset;
        }
    }
};

// =============================================================================
// OBJ LOADER
// =============================================================================
struct Vertex {
    float pos[3];
    float normal[3];
    float uv[2];
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

Mesh loadOBJ(const std::string& path) {
    Mesh mesh;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::map<std::tuple<int, int, int>, uint32_t> vertexMap;

    std::ifstream file(path);
    if (!file) fatal("Failed to load OBJ file");

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string type;
        iss >> type;
        if (type == "v") {
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        } else if (type == "vn") {
            glm::vec3 norm;
            iss >> norm.x >> norm.y >> norm.z;
            normals.push_back(norm);
        } else if (type == "vt") {
            glm::vec2 uv;
            iss >> uv.x >> uv.y;
            uvs.push_back(uv);
        } else if (type == "f") {
            std::vector<int> p_list, t_list, n_list;
            std::string token;
            while (iss >> token) {
                std::replace(token.begin(), token.end(), '/', ' ');
                std::istringstream tiss(token);
                int p = 0, t = 0, n = 0;
                tiss >> p >> t >> n;
                p_list.push_back(p);
                t_list.push_back(t);
                n_list.push_back(n);
            }
            if (p_list.size() < 3) continue;
            for (size_t i = 1; i < p_list.size() - 1; ++i) {
                for (int j : {0, (int)i, (int)i + 1}) {
                    int pp = p_list[j], tt = t_list[j], nn = n_list[j];
                    std::tuple<int, int, int> key(pp, tt, nn);
                    auto it = vertexMap.find(key);
                    uint32_t idx;
                    if (it == vertexMap.end()) {
                        idx = static_cast<uint32_t>(mesh.vertices.size());
                        Vertex v{};
                        v.pos[0] = positions[pp - 1].x;
                        v.pos[1] = positions[pp - 1].y;
                        v.pos[2] = positions[pp - 1].z;
                        if (nn > 0) {
                            v.normal[0] = normals[nn - 1].x;
                            v.normal[1] = normals[nn - 1].y;
                            v.normal[2] = normals[nn - 1].z;
                        }
                        if (tt > 0) {
                            v.uv[0] = uvs[tt - 1].x;
                            v.uv[1] = uvs[tt - 1].y;  // Note: not flipping V to preserve orientation
                        }
                        mesh.vertices.push_back(v);
                        vertexMap[key] = idx;
                    } else {
                        idx = it->second;
                    }
                    mesh.indices.push_back(idx);
                }
            }
        }
    }
    return mesh;
}

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
        alignas(16) glm::vec3 billboardBaseColor;
        alignas(4)  float     billboardAlphaCutoff;
        alignas(4)  uint32_t  billboardUseAlphaBlend;
        alignas(4)  uint32_t  showNormals;
        alignas(4)  uint32_t  showUVs;
        alignas(4)  uint32_t  showWireframe;
        alignas(4)  uint32_t  enableReflections;
        alignas(4)  uint32_t  enableShadows;
        alignas(4)  uint32_t  enableVolumetrics;
        alignas(4)  float     fogPulseSpeed;
        alignas(4)  float     fogPulseAmount;
        alignas(4)  float     lightBobSpeed;
        alignas(4)  float     lightBobAmplitude;
        alignas(4)  float     lightOrbitSpeed;
        alignas(4)  float     lightOrbitAmplitude;
        alignas(4)  float     lightColorPulseSpeed;
        alignas(4)  float     lightColorPulseAmount;
    };

    VulkanRenderer(int width, int height);
    ~VulkanRenderer();

    VkCommandBuffer recordFrame(const Camera& camera, float deltaTime, uint32_t swapImageIndex) noexcept;

    void setEnvironmentMap(const std::string& filename) noexcept;

    Texture createTextureImageFromFile(const std::string& filename, VkImageUsageFlags extraUsage = 0) noexcept;

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

private:
    void initScene();
    VkAccelerationStructureKHR buildSingleBLAS(VkCommandBuffer cmd, VkDeviceAddress vertexAddr, VkDeviceAddress indexAddr, uint32_t primCount, uint64_t& blasBufferHandle, uint64_t& scratchHandle);
    void updateAndBuildTLAS(VkCommandBuffer cmd, const Camera& camera, float totalTime);

    bool m_dynamicModel = false;
    bool m_hasModel = false;

    VkDeviceSize m_tlasSize = 0;

    uint64_t m_modelVertexHandle = 0;
    uint64_t m_modelIndexHandle = 0;
    uint64_t m_groundVertexHandle = 0;
    uint64_t m_groundIndexHandle = 0;
    uint64_t m_modelBlasBufferHandle = 0;
    uint64_t m_groundBlasBufferHandle = 0;
    uint64_t m_modelBlasScratchHandle = 0;
    uint64_t m_groundBlasScratchHandle = 0;
    VkAccelerationStructureKHR m_modelBlas = VK_NULL_HANDLE;
    VkAccelerationStructureKHR m_groundBlas = VK_NULL_HANDLE;

    uint64_t m_instHandle = 0;
    VkBuffer m_instStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_instStagingMemory = VK_NULL_HANDLE;

    uint64_t m_tlasBufferHandle = 0;
    uint64_t m_tlasScratchHandle = 0;
    VkAccelerationStructureKHR m_tlas = VK_NULL_HANDLE;

    void createRayTracingPipeline();
    void createShaderBindingTable();
    void createTonemapPipeline();

    VkShaderModule loadShader(const std::string& filename) const noexcept;

    void uploadEnvironmentMap(const float* data, int width, int height) noexcept;

    Texture createTextureImage(const float* pixels, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage = 0);
};

// =============================================================================
// UTILITY — FIND MEMORY TYPE
// =============================================================================
static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(g_ctx().physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    fatal("NO MEMORY TYPE FOUND");
    return 0;
}

// =============================================================================
// EXTENSIONS — LOADED ONCE
// =============================================================================
void RTXExtensions::load(VkDevice device) {
    if (vkCmdTraceRaysKHR) return;

    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkGetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddress"));
}

// =============================================================================
// POOL — SEIZE THE GPU
// =============================================================================
static void seize_gpu() {
    if (g_ctx().poolBuffer) return;

    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(g_ctx().physicalDevice, &props);

    VkDeviceSize total = 0;
    for (uint32_t i = 0; i < props.memoryHeapCount; ++i)
        if (props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            total += props.memoryHeaps[i].size;

    const VkDeviceSize reservedForDrivers = 4'800'000'000ULL;
    g_ctx().poolSize = (total > reservedForDrivers) ? (total - reservedForDrivers) : 0;

    if (!g_ctx().poolSize) fatal("GPU TOO WEAK — NOT ENOUGH VRAM");

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size  = g_ctx().poolSize;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(g_ctx().device, &bci, nullptr, &g_ctx().poolBuffer) != VK_SUCCESS) {
        fatal("Failed to create pool buffer");
    }

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(g_ctx().device, g_ctx().poolBuffer, &req);

    uint32_t type = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = type;

    if (vkAllocateMemory(g_ctx().device, &mai, nullptr, &g_ctx().poolMemory) != VK_SUCCESS ||
        vkBindBufferMemory(g_ctx().device, g_ctx().poolBuffer, g_ctx().poolMemory, 0) != VK_SUCCESS) {
        vkDestroyBuffer(g_ctx().device, g_ctx().poolBuffer, nullptr);
        fatal("Failed to allocate/bind pool memory");
    }
}

// =============================================================================
// BUFFER MANAGER — SMALL
// =============================================================================
[[nodiscard]] inline uint64_t BufferCreate(VkDeviceSize size) noexcept {
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

[[nodiscard]] inline VkDeviceAddress BufferGetDeviceAddress(uint64_t h) noexcept {
    auto it = g_ctx().offsets.find(h);
    if (it == g_ctx().offsets.end()) return 0;
    VkBufferDeviceAddressInfo i{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = g_ctx().poolBuffer};
    return g_ext().vkGetBufferDeviceAddress(g_ctx().device, &i) + it->second;
}

void PopulateContext(VkInstance instance, VkSurfaceKHR surface) {
    g_ctx().instance = instance;
    g_ctx().surface  = surface;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) fatal("No GPUs with Vulkan support");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physicalDevice = dev;
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) physicalDevice = devices[0];

    g_ctx().physicalDevice = physicalDevice;

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
    if (queueFamily == UINT32_MAX) fatal("No suitable queue family");

    g_ctx().queueFamily = queueFamily;

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

    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamily;
    queueInfo.queueCount = 1;
    float priority = 1.0f;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &coreFeatures;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS) {
        fatal("Failed to create logical device");
    }

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queueFamily, 0, &queue);

    g_ctx().device         = device;
    g_ctx().graphicsQueue  = queue;
    g_ctx().queue          = queue;
    g_ctx().presentQueue   = queue;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &g_ctx().rtProps;
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);
}

// =============================================================================
// VulkanRenderer Implementation
// =============================================================================
VulkanRenderer::VulkanRenderer(int width, int height) : width_(width), height_(height) {
    VkDevice device = g_ctx().device;

    // === GLOBAL DESCRIPTOR LAYOUT ===
    VkDescriptorSetLayoutBinding bindings[4]{};

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

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
    sizes[2].descriptorCount = 2;

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

    // === RT OUTPUT IMAGES ===
    rtOutputImages_.resize(MAX_FRAMES_IN_FLIGHT);
    rtOutputViews_.resize(MAX_FRAMES_IN_FLIGHT);
    rtOutputMemories_.resize(MAX_FRAMES_IN_FLIGHT);

    VkFormat rtFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

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
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        VkImage image;
        vkCreateImage(device, &imageInfo, nullptr, &image);
        rtOutputImages_[i] = Handle<VkImage>(image, device, vkDestroyImage);

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device, image, &memReqs);

        uint32_t memTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo memAlloc{};
        memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = memTypeIndex;

        VkDeviceMemory memory;
        vkAllocateMemory(device, &memAlloc, nullptr, &memory);
        rtOutputMemories_[i] = Handle<VkDeviceMemory>(memory, device, vkFreeMemory);

        vkBindImageMemory(device, image, memory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = rtFormat;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkImageView view;
        vkCreateImageView(device, &viewInfo, nullptr, &view);
        rtOutputViews_[i] = Handle<VkImageView>(view, device, vkDestroyImageView);
    }

    // === COMMAND POOL & BUFFER ===
    VkCommandPoolCreateInfo cmdPoolInfo{};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolInfo.queueFamilyIndex = g_ctx().queueFamily;
    vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &m_cmdPool);

    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = m_cmdPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, &m_cmdBuffer);

    // === PIPELINES ===
    createRayTracingPipeline();
    createShaderBindingTable();
    createTonemapPipeline();

    // === INIT SCENE ===
    initScene();
}

VulkanRenderer::~VulkanRenderer() {
    VkDevice device = g_ctx().device;

    if (m_tlas) g_ext().vkDestroyAccelerationStructureKHR(device, m_tlas, nullptr);
    if (m_modelBlas) g_ext().vkDestroyAccelerationStructureKHR(device, m_modelBlas, nullptr);
    if (m_groundBlas) g_ext().vkDestroyAccelerationStructureKHR(device, m_groundBlas, nullptr);

    if (m_instStagingBuffer) vkDestroyBuffer(device, m_instStagingBuffer, nullptr);
    if (m_instStagingMemory) vkFreeMemory(device, m_instStagingMemory, nullptr);

    if (tonemapPipeline_) vkDestroyPipeline(device, tonemapPipeline_, nullptr);
    if (tonemapLayout_) vkDestroyPipelineLayout(device, tonemapLayout_, nullptr);

    if (pipeline_) vkDestroyPipeline(device, pipeline_, nullptr);
    if (pipelineLayout_) vkDestroyPipelineLayout(device, pipelineLayout_, nullptr);

    if (m_environmentSampler) vkDestroySampler(device, m_environmentSampler, nullptr);
    if (m_environmentMap.view) vkDestroyImageView(device, m_environmentMap.view, nullptr);
    if (m_environmentMap.image) vkDestroyImage(device, m_environmentMap.image, nullptr);
    if (m_environmentMap.memory) vkFreeMemory(device, m_environmentMap.memory, nullptr);

    if (m_monsterSampler) vkDestroySampler(device, m_monsterSampler, nullptr);
    if (m_monsterTexture.view) vkDestroyImageView(device, m_monsterTexture.view, nullptr);
    if (m_monsterTexture.image) vkDestroyImage(device, m_monsterTexture.image, nullptr);
    if (m_monsterTexture.memory) vkFreeMemory(device, m_monsterTexture.memory, nullptr);

    if (m_cmdPool) vkDestroyCommandPool(device, m_cmdPool, nullptr);

    if (g_ctx().globalDescriptorSetLayout) vkDestroyDescriptorSetLayout(device, g_ctx().globalDescriptorSetLayout, nullptr);

    if (m_descriptorPool) vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
}

void VulkanRenderer::initScene() {
    m_hasModel = true;
    m_dynamicModel = Options::BillboardFaceCamera || Options::BillboardAutoRotateY != 0.0f;

    VkDevice device = g_ctx().device;
    VkQueue graphicsQueue = g_ctx().graphicsQueue;

    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, g_ctx().queueFamily};

    VkCommandPool initPool;
    vkCreateCommandPool(device, &poolInfo, nullptr, &initPool);

    VkCommandBufferAllocateInfo cmdAlloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, initPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};

    VkCommandBuffer initCmd;
    vkAllocateCommandBuffers(device, &cmdAlloc, &initCmd);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(initCmd, &beginInfo);

    Vertex groundVerts[4] = {
        {{-100.0f, Options::GroundLevel, -100.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{-100.0f, Options::GroundLevel,  100.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        {{ 100.0f, Options::GroundLevel,  100.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 100.0f, Options::GroundLevel, -100.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    };
    uint32_t groundIndices[6] = {0, 1, 2, 0, 2, 3};

    // Upload ground
    VkDeviceSize groundVertexSize = sizeof(groundVerts);
    m_groundVertexHandle = BufferCreate(groundVertexSize);
    VkDeviceSize groundIndexSize = sizeof(groundIndices);
    m_groundIndexHandle = BufferCreate(groundIndexSize);

    VkBuffer stagingGroundVertex, stagingGroundIndex;
    VkDeviceMemory stagingGroundVertexMem, stagingGroundIndexMem;

    VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, groundVertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
    vkCreateBuffer(device, &bufInfo, nullptr, &stagingGroundVertex);
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingGroundVertex, &memReqs);
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memReqs.size, findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    vkAllocateMemory(device, &allocInfo, nullptr, &stagingGroundVertexMem);
    vkBindBufferMemory(device, stagingGroundVertex, stagingGroundVertexMem, 0);
    void* data;
    vkMapMemory(device, stagingGroundVertexMem, 0, groundVertexSize, 0, &data);
    memcpy(data, groundVerts, groundVertexSize);
    vkUnmapMemory(device, stagingGroundVertexMem);

    bufInfo.size = groundIndexSize;
    vkCreateBuffer(device, &bufInfo, nullptr, &stagingGroundIndex);
    vkGetBufferMemoryRequirements(device, stagingGroundIndex, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    vkAllocateMemory(device, &allocInfo, nullptr, &stagingGroundIndexMem);
    vkBindBufferMemory(device, stagingGroundIndex, stagingGroundIndexMem, 0);
    vkMapMemory(device, stagingGroundIndexMem, 0, groundIndexSize, 0, &data);
    memcpy(data, groundIndices, groundIndexSize);
    vkUnmapMemory(device, stagingGroundIndexMem);

    VkBufferCopy copy{0, g_ctx().offsets[m_groundVertexHandle], groundVertexSize};
    vkCmdCopyBuffer(initCmd, stagingGroundVertex, g_ctx().poolBuffer, 1, &copy);
    copy.dstOffset = g_ctx().offsets[m_groundIndexHandle];
    copy.size = groundIndexSize;
    vkCmdCopyBuffer(initCmd, stagingGroundIndex, g_ctx().poolBuffer, 1, &copy);

    // Model (mug.obj)
    VkBuffer stagingVertex = VK_NULL_HANDLE, stagingIndex = VK_NULL_HANDLE;
    VkDeviceMemory stagingVertexMem = VK_NULL_HANDLE, stagingIndexMem = VK_NULL_HANDLE;

    if (m_hasModel) {
        Mesh modelMesh = loadOBJ("assets/models/mug.obj");

        VkDeviceSize vertexSize = modelMesh.vertices.size() * sizeof(Vertex);
        m_modelVertexHandle = BufferCreate(vertexSize);
        VkDeviceSize indexSize = modelMesh.indices.size() * sizeof(uint32_t);
        m_modelIndexHandle = BufferCreate(indexSize);

        bufInfo.size = vertexSize;
        vkCreateBuffer(device, &bufInfo, nullptr, &stagingVertex);
        vkGetBufferMemoryRequirements(device, stagingVertex, &memReqs);
        allocInfo.allocationSize = memReqs.size;
        vkAllocateMemory(device, &allocInfo, nullptr, &stagingVertexMem);
        vkBindBufferMemory(device, stagingVertex, stagingVertexMem, 0);
        vkMapMemory(device, stagingVertexMem, 0, vertexSize, 0, &data);
        memcpy(data, modelMesh.vertices.data(), vertexSize);
        vkUnmapMemory(device, stagingVertexMem);

        bufInfo.size = indexSize;
        vkCreateBuffer(device, &bufInfo, nullptr, &stagingIndex);
        vkGetBufferMemoryRequirements(device, stagingIndex, &memReqs);
        allocInfo.allocationSize = memReqs.size;
        vkAllocateMemory(device, &allocInfo, nullptr, &stagingIndexMem);
        vkBindBufferMemory(device, stagingIndex, stagingIndexMem, 0);
        vkMapMemory(device, stagingIndexMem, 0, indexSize, 0, &data);
        memcpy(data, modelMesh.indices.data(), indexSize);
        vkUnmapMemory(device, stagingIndexMem);

        copy.dstOffset = g_ctx().offsets[m_modelVertexHandle];
        copy.size = vertexSize;
        vkCmdCopyBuffer(initCmd, stagingVertex, g_ctx().poolBuffer, 1, &copy);
        copy.dstOffset = g_ctx().offsets[m_modelIndexHandle];
        copy.size = indexSize;
        vkCmdCopyBuffer(initCmd, stagingIndex, g_ctx().poolBuffer, 1, &copy);
    }

    // Build BLAS
    uint32_t primCount = 2;
    VkAccelerationStructureGeometryKHR geom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geom.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geom.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geom.geometry.triangles.vertexStride = sizeof(Vertex);
    geom.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};

    // Ground BLAS
    geom.geometry.triangles.vertexData.deviceAddress = BufferGetDeviceAddress(m_groundVertexHandle);
    geom.geometry.triangles.indexData.deviceAddress = BufferGetDeviceAddress(m_groundIndexHandle);
    geom.geometry.triangles.maxVertex = 3;
    g_ext().vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);
    m_groundBlasScratchHandle = BufferCreate(sizeInfo.buildScratchSize);
    m_groundBlasBufferHandle = BufferCreate(sizeInfo.accelerationStructureSize);
    m_groundBlas = buildSingleBLAS(initCmd, geom.geometry.triangles.vertexData.deviceAddress, geom.geometry.triangles.indexData.deviceAddress, primCount, m_groundBlasBufferHandle, m_groundBlasScratchHandle);

    // Model BLAS
    if (m_hasModel) {
        Mesh modelMesh = loadOBJ("assets/models/mug.obj");  // Load again for primCount, but could cache
        primCount = modelMesh.indices.size() / 3;
        geom.geometry.triangles.vertexData.deviceAddress = BufferGetDeviceAddress(m_modelVertexHandle);
        geom.geometry.triangles.indexData.deviceAddress = BufferGetDeviceAddress(m_modelIndexHandle);
        geom.geometry.triangles.maxVertex = modelMesh.vertices.size() - 1;
        g_ext().vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);
        m_modelBlasScratchHandle = BufferCreate(sizeInfo.buildScratchSize);
        m_modelBlasBufferHandle = BufferCreate(sizeInfo.accelerationStructureSize);
        m_modelBlas = buildSingleBLAS(initCmd, geom.geometry.triangles.vertexData.deviceAddress, geom.geometry.triangles.indexData.deviceAddress, primCount, m_modelBlasBufferHandle, m_modelBlasScratchHandle);
    }

    vkEndCommandBuffer(initCmd);

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &initCmd, 0, nullptr};
    vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, initPool, 1, &initCmd);
    vkDestroyCommandPool(device, initPool, nullptr);

    // Cleanup staging
    vkDestroyBuffer(device, stagingGroundVertex, nullptr);
    vkFreeMemory(device, stagingGroundVertexMem, nullptr);
    vkDestroyBuffer(device, stagingGroundIndex, nullptr);
    vkFreeMemory(device, stagingGroundIndexMem, nullptr);

    if (m_hasModel) {
        vkDestroyBuffer(device, stagingVertex, nullptr);
        vkFreeMemory(device, stagingVertexMem, nullptr);
        vkDestroyBuffer(device, stagingIndex, nullptr);
        vkFreeMemory(device, stagingIndexMem, nullptr);
    }

    // Compute TLAS sizes
    uint32_t instCount = m_hasModel ? 2 : 1;

    VkAccelerationStructureGeometryKHR tlasGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    tlasGeom.geometry.instances.arrayOfPointers = VK_FALSE;

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuild{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    tlasBuild.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasBuild.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlasBuild.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlasBuild.geometryCount = 1;
    tlasBuild.pGeometries = &tlasGeom;

    VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    g_ext().vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasBuild, &instCount, &tlasSizeInfo);

    m_tlasSize = tlasSizeInfo.accelerationStructureSize;
    m_tlasBufferHandle = BufferCreate(m_tlasSize);
    m_tlasScratchHandle = BufferCreate(tlasSizeInfo.buildScratchSize);
    m_instHandle = BufferCreate(sizeof(VkAccelerationStructureInstanceKHR) * instCount);

    // Create staging for instances if dynamic
    if (m_dynamicModel) {
        VkDeviceSize instSize = sizeof(VkAccelerationStructureInstanceKHR) * instCount;
        bufInfo.size = instSize;
        vkCreateBuffer(device, &bufInfo, nullptr, &m_instStagingBuffer);
        vkGetBufferMemoryRequirements(device, m_instStagingBuffer, &memReqs);
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device, &allocInfo, nullptr, &m_instStagingMemory);
        vkBindBufferMemory(device, m_instStagingBuffer, m_instStagingMemory, 0);
    }

    // Build fixed TLAS if not dynamic
    if (!m_dynamicModel) {
        VkCommandPool fixedPool;
        vkCreateCommandPool(device, &poolInfo, nullptr, &fixedPool);
        vkAllocateCommandBuffers(device, &cmdAlloc, &initCmd);
        vkBeginCommandBuffer(initCmd, &beginInfo);
        updateAndBuildTLAS(initCmd, Camera(), 0.0f);
        vkEndCommandBuffer(initCmd);
        vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
        vkFreeCommandBuffers(device, fixedPool, 1, &initCmd);
        vkDestroyCommandPool(device, fixedPool, nullptr);
    }
}

VkAccelerationStructureKHR VulkanRenderer::buildSingleBLAS(VkCommandBuffer cmd, VkDeviceAddress vertexAddr, VkDeviceAddress indexAddr, uint32_t primCount, uint64_t& blasBufferHandle, uint64_t& scratchHandle) {
    VkDevice device = g_ctx().device;

    VkAccelerationStructureGeometryKHR geom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geom.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geom.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geom.geometry.triangles.vertexData.deviceAddress = vertexAddr;
    geom.geometry.triangles.vertexStride = 32;
    geom.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geom.geometry.triangles.indexData.deviceAddress = indexAddr;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    g_ext().vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);

    blasBufferHandle = BufferCreate(sizeInfo.accelerationStructureSize);
    scratchHandle = BufferCreate(sizeInfo.buildScratchSize);

    VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    createInfo.buffer = g_ctx().poolBuffer;
    createInfo.offset = g_ctx().offsets[blasBufferHandle];
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkAccelerationStructureKHR blas;
    g_ext().vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &blas);

    buildInfo.dstAccelerationStructure = blas;
    buildInfo.scratchData.deviceAddress = BufferGetDeviceAddress(scratchHandle);

    VkAccelerationStructureBuildRangeInfoKHR range{primCount};
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    g_ext().vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    return blas;
}

void VulkanRenderer::updateAndBuildTLAS(VkCommandBuffer cmd, [[maybe_unused]] const Camera& camera, float totalTime) {
    VkDevice device = g_ctx().device;

    uint32_t instCount = m_hasModel ? 2 : 1;
    std::vector<VkAccelerationStructureInstanceKHR> instances(instCount);

    int idx = 0;
    if (m_hasModel) {
        auto& inst = instances[idx++];
        // Base position: centered above ground at BillboardZOffset on Z axis
        glm::mat4 trans = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, Options::GroundLevel, Options::BillboardZOffset));

        // Auto Y-rotation (windmill style)
        float angleY = totalTime * Options::BillboardAutoRotateY;
        trans = glm::rotate(trans, glm::radians(angleY), glm::vec3(0.0f, 1.0f, 0.0f));

        // NEW: Optional tipping of the mug (e.g. lying on its side)
        if (Options::MugTipEnabled) {
            // Rotate around X axis (roll) to tip it sideways
            trans = glm::rotate(trans, glm::radians(Options::MugTipAngleDegrees), glm::vec3(1.0f, 0.0f, 0.0f));

            // Compensate Y position so the side of the mug rests on the ground
            // Adjust this value based on your mug model's scale/size
            trans = glm::translate(trans, glm::vec3(0.0f, Options::MugTipGroundOffsetY, 0.0f));
        }

        // Optional face-camera billboard (currently disabled in original code)
        if (Options::BillboardFaceCamera) {
            // You could add yaw-based rotation here if desired
        }

        // Copy matrix to Vulkan row-major format
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                inst.transform.matrix[row][col] = trans[col][row];

        inst.instanceCustomIndex = 0;
        inst.mask = 0xFF;
        inst.instanceShaderBindingTableRecordOffset = 0;
        inst.flags = 0;  // No special flags (backface culling off for 3D model)

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            nullptr,
            m_modelBlas
        };
        inst.accelerationStructureReference = g_ext().vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfo);
    }

    // Ground instance - always identity
    auto& groundInst = instances[idx];
    groundInst.transform.matrix[0][0] = 1.0f;
    groundInst.transform.matrix[1][1] = 1.0f;
    groundInst.transform.matrix[2][2] = 1.0f;
    groundInst.instanceCustomIndex = 1;
    groundInst.mask = 0xFF;
    groundInst.instanceShaderBindingTableRecordOffset = 0;

    VkAccelerationStructureDeviceAddressInfoKHR groundAddrInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        nullptr,
        m_groundBlas
    };
    groundInst.accelerationStructureReference = g_ext().vkGetAccelerationStructureDeviceAddressKHR(device, &groundAddrInfo);

    // Upload instances if model is dynamic
    if (m_dynamicModel) {
        VkDeviceSize instSize = sizeof(VkAccelerationStructureInstanceKHR) * instCount;
        void* mapped;
        vkMapMemory(device, m_instStagingMemory, 0, instSize, 0, &mapped);
        memcpy(mapped, instances.data(), instSize);
        vkUnmapMemory(device, m_instStagingMemory);

        VkBufferCopy copy{0, g_ctx().offsets[m_instHandle], instSize};
        vkCmdCopyBuffer(cmd, m_instStagingBuffer, g_ctx().poolBuffer, 1, &copy);

        VkMemoryBarrier memBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                   VK_ACCESS_TRANSFER_WRITE_BIT,
                                   VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR};
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             0, 1, &memBarrier, 0, nullptr, 0, nullptr);
    }

    // Destroy old TLAS
    if (m_tlas) {
        g_ext().vkDestroyAccelerationStructureKHR(device, m_tlas, nullptr);
        m_tlas = VK_NULL_HANDLE;
    }

    // Create new TLAS
    VkAccelerationStructureCreateInfoKHR tlasCreate{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    tlasCreate.buffer = g_ctx().poolBuffer;
    tlasCreate.offset = g_ctx().offsets[m_tlasBufferHandle];
    tlasCreate.size = m_tlasSize;
    tlasCreate.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    g_ext().vkCreateAccelerationStructureKHR(device, &tlasCreate, nullptr, &m_tlas);

    // Build TLAS
    VkAccelerationStructureGeometryKHR tlasGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    tlasGeom.geometry.instances.arrayOfPointers = VK_FALSE;
    tlasGeom.geometry.instances.data.deviceAddress = BufferGetDeviceAddress(m_instHandle);

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuild{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    tlasBuild.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasBuild.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlasBuild.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlasBuild.dstAccelerationStructure = m_tlas;
    tlasBuild.geometryCount = 1;
    tlasBuild.pGeometries = &tlasGeom;
    tlasBuild.scratchData.deviceAddress = BufferGetDeviceAddress(m_tlasScratchHandle);

    VkAccelerationStructureBuildRangeInfoKHR tlasRange{instCount};
    const VkAccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRange;

    g_ext().vkCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuild, &pTlasRange);

    VkMemoryBarrier memBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                               VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                               VK_ACCESS_SHADER_READ_BIT};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 1, &memBarrier, 0, nullptr, 0, nullptr);
}

void VulkanRenderer::createTonemapPipeline() {
    VkDevice device = g_ctx().device;

    VkShaderModule tonemapModule = loadShader("tonemap.spv");

    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, tonemapModule, "main"};

    VkPushConstantRange pc{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float)};

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &g_ctx().globalDescriptorSetLayout, 1, &pc};
    vkCreatePipelineLayout(device, &layoutInfo, nullptr, &tonemapLayout_);

    VkComputePipelineCreateInfo createInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0, stage, tonemapLayout_};
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &tonemapPipeline_);

    vkDestroyShaderModule(device, tonemapModule, nullptr);
}

VkCommandBuffer VulkanRenderer::recordFrame(const Camera& camera, float deltaTime, uint32_t swapImageIndex) noexcept {
    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    uint32_t frameIndex = currentFrame_;

    totalTime_ += deltaTime;

    if (cameraMoved_) {
        currentSpp_ = 0;
        cameraMoved_ = false;
    }

    vkResetCommandBuffer(m_cmdBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(m_cmdBuffer, &beginInfo) != VK_SUCCESS) return VK_NULL_HANDLE;

    if (m_dynamicModel) updateAndBuildTLAS(m_cmdBuffer, camera, totalTime_);

    // Update descriptors
    VkWriteDescriptorSetAccelerationStructureKHR accelWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, nullptr, 1, &m_tlas};

    VkDescriptorImageInfo storageInfo{{}, rtOutputViews_[frameIndex].get(), VK_IMAGE_LAYOUT_GENERAL};

    VkDescriptorImageInfo envInfo{m_environmentSampler, m_environmentMap.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkDescriptorImageInfo monsterInfo{m_monsterSampler, m_monsterTexture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkWriteDescriptorSet writes[4]{};
    uint32_t writeCount = 0;

    writes[writeCount] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, &accelWrite, g_ctx().globalDescriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR};
    ++writeCount;

    writes[writeCount] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, g_ctx().globalDescriptorSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storageInfo};
    ++writeCount;

    if (m_hasEnvironmentMap) {
        writes[writeCount] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, g_ctx().globalDescriptorSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &envInfo};
        ++writeCount;
    }

    writes[writeCount] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, g_ctx().globalDescriptorSet, 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &monsterInfo};
    ++writeCount;

    vkUpdateDescriptorSets(g_ctx().device, writeCount, writes, 0, nullptr);

    // Push constants
    PushConstants push{};
    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
    proj[1][1] *= -1.0f; // Flip Y for Vulkan clip space
    push.invView = glm::inverse(camera.view());
    push.invProj = glm::inverse(proj);
    push.totalTime = totalTime_;
    push.spp = currentSpp_;
    push.frameSeed = currentFrame_ ^ 0xDEADBEEF;
    push.forceEnvOnly = Options::ForceEnvironmentOnly ? 1u : 0u;
    push.jitterStrength = Options::EnableJitterAA ? Options::JitterStrength : 0.0f;
    push.useEnvSky = Options::UseEnvironmentAsSky ? 1u : 0u;
    push.flipEnvV = Options::FlipEnvironmentV ? 1u : 0u;
    push.showHotPink = Options::ShowHotPinkOnHit ? 1u : 0u;
    push.environmentExposure = Options::EnvironmentExposure;
    push.skyIntensity = Options::SkyIntensity;
    push.environmentRotationY = Options::EnvironmentRotationY + totalTime_ * Options::SkyRotationSpeed;
    push.billboardBaseColor = Options::BillboardBaseColor;
    push.billboardAlphaCutoff = Options::BillboardAlphaCutoff;
    push.billboardUseAlphaBlend = Options::BillboardUseAlphaBlend ? 1u : 0u;
    push.showNormals = Options::ShowNormals ? 1u : 0u;
    push.showUVs = Options::ShowUVs ? 1u : 0u;
    push.showWireframe = Options::ShowWireframe ? 1u : 0u;
    push.enableReflections = Options::EnableReflections ? 1u : 0u;
    push.enableShadows = Options::EnableShadows ? 1u : 0u;
    push.enableVolumetrics = Options::EnableVolumetrics ? 1u : 0u;
    push.maxRecursion = push.enableReflections ? Options::MaxRayDepth : 1u;
    push.fogPulseSpeed = Options::FogPulseSpeed;
    push.fogPulseAmount = Options::FogPulseAmount;
    push.lightBobSpeed = Options::LightBobSpeed;
    push.lightBobAmplitude = Options::LightBobAmplitude;
    push.lightOrbitSpeed = Options::LightOrbitSpeed;
    push.lightOrbitAmplitude = Options::LightOrbitAmplitude;
    push.lightColorPulseSpeed = Options::LightColorPulseSpeed;
    push.lightColorPulseAmount = Options::LightColorPulseAmount * 2.0f;  // more colorful

    // Ray tracing pass
    uint32_t samplesThisFrame = Options::SamplesPerPixel;
    if (Options::AccumulationFrames > 0 && currentSpp_ + samplesThisFrame > Options::AccumulationFrames) {
        samplesThisFrame = Options::AccumulationFrames - currentSpp_;
    }

    if (samplesThisFrame > 0) {
        vkCmdBindPipeline(m_cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_);
        vkCmdBindDescriptorSets(m_cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout_, 0, 1, &g_ctx().globalDescriptorSet, 0, nullptr);

        for (uint32_t s = 0; s < samplesThisFrame; ++s) {
            push.spp = currentSpp_ + s;
            push.frameSeed = (currentFrame_ ^ 0xDEADBEEF) + s;
            vkCmdPushConstants(m_cmdBuffer, pipelineLayout_, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 0, sizeof(PushConstants), &push);
            g_ext().vkCmdTraceRaysKHR(m_cmdBuffer, &g_raygenSbt(), &g_missSbt(), &g_hitSbt(), &g_callableSbt(), width_, height_, 1);
        }
        currentSpp_ += samplesThisFrame;
    }

    // Tonemap pass
    float tonemapExposure = Options::EnvironmentExposure > 0.001f ? 1.0f / Options::EnvironmentExposure : 1.0f;

    vkCmdBindPipeline(m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapPipeline_);
    vkCmdBindDescriptorSets(m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapLayout_, 0, 1, &g_ctx().globalDescriptorSet, 0, nullptr);
    vkCmdPushConstants(m_cmdBuffer, tonemapLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &tonemapExposure);
    vkCmdDispatch(m_cmdBuffer, (width_ + 15) / 16, (height_ + 15) / 16, 1);

    VkMemoryBarrier tonemapBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT};
    vkCmdPipelineBarrier(m_cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &tonemapBarrier, 0, nullptr, 0, nullptr);

    // Blit to swapchain
    VkImage rtImage = rtOutputImages_[frameIndex].get();
    VkImage swapImage = g_swapchainImages()[swapImageIndex];

    VkImageMemoryBarrier barriers[2]{};
    barriers[0] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, rtImage, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    barriers[1] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, swapImage, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    vkCmdPipelineBarrier(m_cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

    VkImageBlit blit{};
    blit.srcSubresource = blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[1] = {width_, height_, 1};
    blit.dstOffsets[1] = {static_cast<int32_t>(g_ctx().width), static_cast<int32_t>(g_ctx().height), 1};

    vkCmdBlitImage(m_cmdBuffer, rtImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    VkImageMemoryBarrier presentBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 0, swapImage, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    vkCmdPipelineBarrier(m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

    vkEndCommandBuffer(m_cmdBuffer);

    return m_cmdBuffer;
}

void VulkanRenderer::setEnvironmentMap(const std::string& filename) noexcept {
    if (filename.empty()) return;

    VkDevice device = g_ctx().device;

    if (m_hasEnvironmentMap) {
        vkDestroyImageView(device, m_environmentMap.view, nullptr);
        vkDestroyImage(device, m_environmentMap.image, nullptr);
        vkFreeMemory(device, m_environmentMap.memory, nullptr);
        vkDestroySampler(device, m_environmentSampler, nullptr);
        m_environmentMap = {};
        m_environmentSampler = VK_NULL_HANDLE;
    }

    SDL_Surface* surface = IMG_Load(filename.c_str());
    if (surface) {
        SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
        SDL_DestroySurface(surface);
        surface = converted;

        int width = surface->w;
        int height = surface->h;

        std::vector<float> floatData(width * height * 4);
        uint8_t* pixels = static_cast<uint8_t*>(surface->pixels);
        int pitch = surface->pitch;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                uint8_t* pixel = pixels + y * pitch + x * 4;
                float r = powf(pixel[0] / 255.0f, 2.2f);
                float g = powf(pixel[1] / 255.0f, 2.2f);
                float b = powf(pixel[2] / 255.0f, 2.2f);
                float a = pixel[3] / 255.0f;
                int idx = (y * width + x) * 4;
                floatData[idx + 0] = r;
                floatData[idx + 1] = g;
                floatData[idx + 2] = b;
                floatData[idx + 3] = a;
            }
        }

        SDL_DestroySurface(surface);
        uploadEnvironmentMap(floatData.data(), width, height);
        m_hasEnvironmentMap = true;
        return;
    }

    int width, height, channels;
    float* data = stbi_loadf(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) return;

    for (int i = 0; i < width * height; ++i) {
        int idx = i * 4;
        data[idx + 0] *= 1.0f;  // Exposure in shader only
        data[idx + 1] *= 1.0f;
        data[idx + 2] *= 1.0f;
    }

    uploadEnvironmentMap(data, width, height);
    stbi_image_free(data);
    m_hasEnvironmentMap = true;
}

void VulkanRenderer::uploadEnvironmentMap(const float* data, int width, int height) noexcept {
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
        if (commandBuffer) vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
    };

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
    vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memReqs.size, findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

    void* mapped;
    vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mapped);
    memcpy(mapped, data, imageSize);
    vkUnmapMemory(device, stagingMemory);

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr, 0, VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr, VK_IMAGE_LAYOUT_UNDEFINED};
    vkCreateImage(device, &imageInfo, nullptr, &m_environmentMap.image);

    vkGetImageMemoryRequirements(device, m_environmentMap.image, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device, &allocInfo, nullptr, &m_environmentMap.memory);
    vkBindImageMemory(device, m_environmentMap.image, m_environmentMap.memory, 0);

    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, g_ctx().queueFamily};
    vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);

    VkCommandBufferAllocateInfo cmdAlloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
    vkAllocateCommandBuffers(device, &cmdAlloc, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, m_environmentMap.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy copyRegion{0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_environmentMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &commandBuffer, 0, nullptr};
    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, m_environmentMap.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, {}, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCreateImageView(device, &viewInfo, nullptr, &m_environmentMap.view);

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, nullptr, 0, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, VK_FALSE, 0.0f, VK_FALSE, VK_COMPARE_OP_NEVER, 0.0f, 1.0f, VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK, VK_FALSE};
    vkCreateSampler(device, &samplerInfo, nullptr, &m_environmentSampler);

    VkDescriptorImageInfo descImage{m_environmentSampler, m_environmentMap.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, g_ctx().globalDescriptorSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descImage};
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    cleanup();
}

Texture VulkanRenderer::createTextureImage(const float* pixels, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage) {
    VkDevice device = g_ctx().device;

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 16;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE};
    vkCreateBuffer(device, &bufInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memReqs.size, findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

    void* mapped;
    vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mapped);
    memcpy(mapped, pixels, imageSize);
    vkUnmapMemory(device, stagingMemory);

    Texture tex{};
    tex.width = width;
    tex.height = height;
    tex.format = format;
    tex.mipLevels = 1;

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr, 0, VK_IMAGE_TYPE_2D, format, {width, height, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | usage, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr, VK_IMAGE_LAYOUT_UNDEFINED};
    vkCreateImage(device, &imageInfo, nullptr, &tex.image);

    vkGetImageMemoryRequirements(device, tex.image, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device, &allocInfo, nullptr, &tex.memory);
    vkBindImageMemory(device, tex.image, tex.memory, 0);

    VkCommandPool cmdPool;
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, g_ctx().queueFamily};
    vkCreateCommandPool(device, &poolInfo, nullptr, &cmdPool);

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAlloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(cmd, &begin);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, tex.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {width, height, 1}};
    vkCmdCopyBufferToImage(cmd, stagingBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmd, 0, nullptr};
    vkQueueSubmit(g_ctx().graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_ctx().graphicsQueue);

    vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
    vkDestroyCommandPool(device, cmdPool, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, tex.image, VK_IMAGE_VIEW_TYPE_2D, format, {}, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCreateImageView(device, &viewInfo, nullptr, &tex.view);

    return tex;
}

Texture VulkanRenderer::createTextureImageFromFile(const std::string& filename, VkImageUsageFlags extraUsage) noexcept {
    SDL_Surface* surface = IMG_Load(filename.c_str());
    if (!surface) return {};

    SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
    SDL_DestroySurface(surface);
    if (!converted) return {};
    surface = converted;

    uint32_t width = static_cast<uint32_t>(surface->w);
    uint32_t height = static_cast<uint32_t>(surface->h);

    std::vector<float> floatPixels(width * height * 4);
    uint8_t* pixels = static_cast<uint8_t*>(surface->pixels);
    int pitch = surface->pitch;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t* src = pixels + y * pitch + x * 4;
            float r = powf(src[0] / 255.0f, 2.2f);
            float g = powf(src[1] / 255.0f, 2.2f);
            float b = powf(src[2] / 255.0f, 2.2f);
            float a = src[3] / 255.0f;
            size_t idx = (y * width + x) * 4;
            floatPixels[idx + 0] = r;
            floatPixels[idx + 1] = g;
            floatPixels[idx + 2] = b;
            floatPixels[idx + 3] = a;
        }
    }

    SDL_DestroySurface(surface);

    return createTextureImage(floatPixels.data(), width, height, VK_FORMAT_R32G32B32A32_SFLOAT, extraUsage);
}

VkShaderModule VulkanRenderer::loadShader(const std::string& filename) const noexcept {
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

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<uint32_t> code((fileSize + 3) / 4);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(code.data()), fileSize);

        VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, fileSize, code.data()};
        VkShaderModule module;
        if (vkCreateShaderModule(g_ctx().device, &createInfo, nullptr, &module) == VK_SUCCESS) {
            return module;
        }
    }
    fatal(("SHADER NOT FOUND: " + filename).c_str());
    return VK_NULL_HANDLE;
}

void VulkanRenderer::createRayTracingPipeline() {
    VkPushConstantRange pushConstant{VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 0, sizeof(PushConstants)};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &g_ctx().globalDescriptorSetLayout, 1, &pushConstant};
    vkCreatePipelineLayout(g_ctx().device, &pipelineLayoutInfo, nullptr, &pipelineLayout_);

    VkShaderModule raygenModule = loadShader("raygen.spv");
    VkShaderModule missModule = loadShader("miss.spv");
    VkShaderModule closestHitModule = loadShader("closesthit.spv");
    VkShaderModule anyHitModule = loadShader("anyhit.spv");

    VkPipelineShaderStageCreateInfo stages[4] = {};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygenModule, "main"};
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR, missModule, "main"};
    stages[2] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closestHitModule, "main"};
    stages[3] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, anyHitModule, "main"};

    VkRayTracingShaderGroupCreateInfoKHR groups[4] = {};
    groups[0] = {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR};
    groups[1] = {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR};
    groups[2] = {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR};
    groups[3] = {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, 3, VK_SHADER_UNUSED_KHR};

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = 4;
    pipelineInfo.pStages = stages;
    pipelineInfo.groupCount = 4;
    pipelineInfo.pGroups = groups;
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout = pipelineLayout_;
    g_ext().vkCreateRayTracingPipelinesKHR(g_ctx().device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_);

    vkDestroyShaderModule(g_ctx().device, raygenModule, nullptr);
    vkDestroyShaderModule(g_ctx().device, missModule, nullptr);
    vkDestroyShaderModule(g_ctx().device, closestHitModule, nullptr);
    vkDestroyShaderModule(g_ctx().device, anyHitModule, nullptr);
}

void VulkanRenderer::createShaderBindingTable() {
    if (g_raygenSbt().deviceAddress) return;

    const uint32_t handleSize = g_ctx().rtProps.shaderGroupHandleSize;
    const uint32_t handleAlign = g_ctx().rtProps.shaderGroupHandleAlignment;
    const uint32_t baseAlign = g_ctx().rtProps.shaderGroupBaseAlignment;

    const uint32_t alignedHandleSize = (handleSize + handleAlign - 1) & ~(handleAlign - 1);
    const uint32_t sbtStride = alignedHandleSize;

    VkDeviceSize sbtSize = static_cast<VkDeviceSize>(sbtStride) * 4;
    sbtSize = (sbtSize + baseAlign - 1) & ~(baseAlign - 1);

    uint64_t sbtHandle = BufferCreate(sbtSize);

    VkDeviceAddress sbtBaseAddress = BufferGetDeviceAddress(sbtHandle);

    std::vector<uint8_t> shaderHandles(4 * handleSize);
    g_ext().vkGetRayTracingShaderGroupHandlesKHR(g_ctx().device, pipeline_, 0, 4, shaderHandles.size(), shaderHandles.data());

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, sbtSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
    vkCreateBuffer(g_ctx().device, &bufferInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(g_ctx().device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memReqs.size, findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    vkAllocateMemory(g_ctx().device, &allocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(g_ctx().device, stagingBuffer, stagingMemory, 0);

    void* mapped;
    vkMapMemory(g_ctx().device, stagingMemory, 0, sbtSize, 0, &mapped);
    memset(mapped, 0, sbtSize);

    uint8_t* dst = static_cast<uint8_t*>(mapped);
    const uint8_t* src = shaderHandles.data();

    memcpy(dst + 0 * sbtStride, src + 0 * handleSize, handleSize);
    memcpy(dst + 1 * sbtStride, src + 1 * handleSize, handleSize);
    memcpy(dst + 2 * sbtStride, src + 2 * handleSize, handleSize);
    memcpy(dst + 3 * sbtStride, src + 3 * handleSize, handleSize);

    vkUnmapMemory(g_ctx().device, stagingMemory);

    VkCommandPool commandPool;
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, g_ctx().queueFamily};
    vkCreateCommandPool(g_ctx().device, &poolInfo, nullptr, &commandPool);

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAlloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
    vkAllocateCommandBuffers(g_ctx().device, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy copyRegion{0, g_ctx().offsets[sbtHandle], sbtSize};
    vkCmdCopyBuffer(cmd, stagingBuffer, g_ctx().poolBuffer, 1, &copyRegion);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmd, 0, nullptr};
    vkQueueSubmit(g_ctx().graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_ctx().graphicsQueue);

    vkFreeCommandBuffers(g_ctx().device, commandPool, 1, &cmd);
    vkDestroyCommandPool(g_ctx().device, commandPool, nullptr);
    vkDestroyBuffer(g_ctx().device, stagingBuffer, nullptr);
    vkFreeMemory(g_ctx().device, stagingMemory, nullptr);

    g_raygenSbt() = { sbtBaseAddress + 0 * sbtStride, sbtStride, sbtStride };
    g_missSbt() = { sbtBaseAddress + 1 * sbtStride, sbtStride, sbtStride };
    g_hitSbt() = { sbtBaseAddress + 3 * sbtStride, sbtStride, sbtStride };
    g_callableSbt() = { 0, 0, 0 };
}

// =============================================================================
// CORE ENGINE FUNCTIONS
// =============================================================================
inline void createSwapchain() noexcept {
    auto& ctx = g_ctx();
    VkDevice device = ctx.device;
    VkPhysicalDevice phys = ctx.physicalDevice;
    VkSurfaceKHR surface = ctx.surface;

    if (device == VK_NULL_HANDLE || surface == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(device);

    for (auto view : g_swapchainViews()) if (view) vkDestroyImageView(device, view, nullptr);
    g_swapchainViews().clear();

    if (g_swapchain()) vkDestroySwapchainKHR(device, g_swapchain(), nullptr);
    g_swapchain() = VK_NULL_HANDLE;
    g_swapchainImages().clear();

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps);

    int drawableW, drawableH;
    SDL_GetWindowSizeInPixels(g_window().get(), &drawableW, &drawableH);
    drawableW = std::max(drawableW, 1);
    drawableH = std::max(drawableH, 1);

    VkExtent2D extent = caps.currentExtent.width != UINT32_MAX ? caps.currentExtent : VkExtent2D{static_cast<uint32_t>(drawableW), static_cast<uint32_t>(drawableH)};
    extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);

    ctx.width = extent.width;
    ctx.height = extent.height;

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, formats.data());

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (const auto& f : formats) if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) chosenFormat = f;

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    uint32_t modeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, modes.data());

    bool hasMailbox = false, hasImmediate = false;
    for (auto mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) hasMailbox = true;
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) hasImmediate = true;
    }

    if (hasMailbox && Options::PreferMailboxForNoTearing) presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    else if (Options::ForceVSync) presentMode = VK_PRESENT_MODE_FIFO_KHR;
    else if (Options::ForceImmediateForMaxFps && hasImmediate) presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

    uint32_t imageCount = (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) ? 3 : 2;
    imageCount = std::max(imageCount, caps.minImageCount);
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, nullptr, 0, surface, imageCount, chosenFormat.format, chosenFormat.colorSpace, extent, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = presentMode;
    info.clipped = VK_TRUE;

    vkCreateSwapchainKHR(device, &info, nullptr, &g_swapchain());

    uint32_t imgCount;
    vkGetSwapchainImagesKHR(device, g_swapchain(), &imgCount, nullptr);
    g_swapchainImages().resize(imgCount);
    vkGetSwapchainImagesKHR(device, g_swapchain(), &imgCount, g_swapchainImages().data());

    g_swapchainViews().resize(imgCount);
    for (uint32_t i = 0; i < imgCount; ++i) {
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, g_swapchainImages()[i], VK_IMAGE_VIEW_TYPE_2D, chosenFormat.format, {}, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        vkCreateImageView(device, &viewInfo, nullptr, &g_swapchainViews()[i]);
    }
}

} // namespace RTX