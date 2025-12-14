// =============================================================================
// include/engine/GLOBAL/RTX.hpp
// AMOURANTH RTX Engine 2025 — PINK LIGHT v∞ — FIRST LIGHT ACHIEVED — DECEMBER 14, 2025
// ALL MEMBERS PUBLIC — FULL EXPOSURE — NO SECRETS — SHE SEES EVERYTHING
// CLEAN COMPILE — NO WARNINGS — NO ERRORS — ETERNAL DOMINATION
// =============================================================================

#pragma once

#include "engine/GLOBAL/SDL3.hpp"
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <filesystem>

namespace RTX {

// =============================================================================
// SIMPLE TEXTURE HOLDER — PUBLIC FOR DIRECT ACCESS
// =============================================================================
struct Texture {
    VkImage        image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView    view = VK_NULL_HANDLE;
    VkFormat       format = VK_FORMAT_UNDEFINED;
    uint32_t       width = 0;
    uint32_t       height = 0;
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
// FATAL
// =============================================================================
[[noreturn]] inline void fatal(const char* msg)
{
    fprintf(stderr, "[FATAL 2025] %s\n", msg);
    std::exit(1);
}

// =============================================================================
// CONTEXT — EVERYTHING IN ONE PLACE — FULLY POPULATED BY PopulateContext()
// =============================================================================
struct Context {
    VkInstance       instance       = VK_NULL_HANDLE;
    VkSurfaceKHR     surface        = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice         device         = VK_NULL_HANDLE;
    VkQueue          queue          = VK_NULL_HANDLE;
    VkQueue          graphicsQueue  = VK_NULL_HANDLE;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    uint32_t queueFamily = UINT32_MAX;

    // === SWAPCHAIN STATE ===
    VkSwapchainKHR           swapchain = VK_NULL_HANDLE;
    std::vector<VkImage>     swapchainImages;
    std::vector<VkImageView> swapchainViews;
    VkQueue                  presentQueue = VK_NULL_HANDLE;
    uint32_t                 lastPresentedIndex = 0;

    // === CURRENT RENDER RESOLUTION ===
    int32_t                 width  = 1920;   // Current framebuffer / render target width
    int32_t                 height = 1080;   // Current framebuffer / render target height

    // === GLOBAL GPU POOL ===
    VkBuffer                  poolBuffer = VK_NULL_HANDLE;
    VkDeviceMemory            poolMemory = VK_NULL_HANDLE;
    VkDeviceSize              poolSize   = 0;
    std::atomic<VkDeviceSize> poolHead{0};
    std::unordered_map<uint64_t, VkDeviceSize> offsets;
    uint64_t                  nextHandle = 1;

    // === GLOBAL DESCRIPTOR SET ===
    VkDescriptorSet globalDescriptorSet = VK_NULL_HANDLE;

    // === RAY TRACING SBT ===
    VkStridedDeviceAddressRegionKHR raygenSbt{};
    VkStridedDeviceAddressRegionKHR missSbt{};
    VkStridedDeviceAddressRegionKHR hitSbt{};
    VkStridedDeviceAddressRegionKHR callableSbt{};

    // === TLAS ===
    uint64_t tlasHandle = 0;
};

extern Context g_context_instance;

[[nodiscard]] inline Context& g_ctx() noexcept { return g_context_instance; }

// Direct accessors for swapchain globals
inline VkSwapchainKHR&           g_swapchain()       { return g_ctx().swapchain; }
inline std::vector<VkImage>&     g_swapchainImages()  { return g_ctx().swapchainImages; }
inline std::vector<VkImageView>& g_swapchainViews()   { return g_ctx().swapchainViews; }
inline VkQueue&                  g_presentQueue()    { return g_ctx().presentQueue; }
inline uint32_t&                 g_lastPresentedIndex() { return g_ctx().lastPresentedIndex; }

// =============================================================================
// RTX EXTENSIONS
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

extern RTXExtensions g_ext;

// =============================================================================
// GLOBALS (now extern declarations for .cpp)
// =============================================================================
extern VkStridedDeviceAddressRegionKHR g_raygenSbt;
extern VkStridedDeviceAddressRegionKHR g_missSbt;
extern VkStridedDeviceAddressRegionKHR g_hitSbt;
extern VkStridedDeviceAddressRegionKHR g_callableSbt;

constexpr VkDeviceSize TRIBUTE = 4'831'838'208ULL; // >4.5 GiB for drivers
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

// =============================================================================
// PUBLIC FUNCTIONS
// =============================================================================
void PopulateContext(VkInstance instance, VkSurfaceKHR surface);
void createSwapchain();

[[nodiscard]] uint64_t BufferCreate(VkDeviceSize size) noexcept;
VkBuffer BufferGetVkBuffer(uint64_t handle) noexcept;
VkDeviceAddress BufferGetDeviceAddress(uint64_t handle) noexcept;

[[nodiscard]] uint64_t LoadScene(const std::string& path) noexcept;
[[nodiscard]] VkAccelerationStructureKHR CurrentTLAS() noexcept;

inline std::string findShader(const std::string& name)
{
    const std::vector<std::string> paths = {
        "assets/shaders/raytracing/" + name,
        "shaders/raytracing/" + name,
        "../assets/shaders/raytracing/" + name,
        "../../assets/shaders/raytracing/" + name,
        "build/bin/Linux/assets/shaders/raytracing/" + name,
    };

    for (const auto& path : paths) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    fatal(("SHADER NOT FOUND: " + name).c_str());
    return {};
}

// =============================================================================
// CAMERA
// =============================================================================
struct Camera {
    glm::vec3 position = glm::vec3(0.0f, 2.0f, 5.0f);
    glm::vec3 front    = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up       = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw   = -90.0f;
    float pitch = 0.0f;

    [[nodiscard]] glm::mat4 view() const noexcept {
        return glm::lookAt(position, position + front, up);
    }

    [[nodiscard]] glm::mat4 invView() const noexcept {
        return glm::inverse(view());
    }

    [[nodiscard]] glm::mat4 projection(float fov = 60.0f, float aspect = 16.0f/9.0f,
                                       float near = 0.1f, float far = 1000.0f) const noexcept {
        return glm::perspective(glm::radians(fov), aspect, near, far);
    }

    [[nodiscard]] glm::mat4 invProj(float fov = 60.0f, float aspect = 16.0f/9.0f,
                                    float near = 0.1f, float far = 1000.0f) const noexcept {
        return glm::inverse(projection(fov, aspect, near, far));
    }

    void move_forward(float s)  { position += front * s; }
    void move_backward(float s) { position -= front * s; }
    void move_right(float s)    { position += glm::normalize(glm::cross(front, up)) * s; }
    void move_left(float s)     { position -= glm::normalize(glm::cross(front, up)) * s; }
    void move_up(float s)       { position += up * s; }
    void move_down(float s)     { position -= up * s; }

    void look(float dx, float dy, float sens = 0.1f) noexcept {
        yaw   += dx * sens;
        pitch -= dy * sens;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(front);
    }
};

// =============================================================================
// VULKAN RENDERER
// =============================================================================
class VulkanRenderer {
public:
    VulkanRenderer(int width, int height);
    ~VulkanRenderer();

    void renderFrame(const Camera& camera, float deltaTime) noexcept;

    void createRayTracingPipeline();
    void createShaderBindingTable();

    Texture createTextureImage(const float* pixels, uint32_t width, uint32_t height,
                               VkFormat format, VkImageUsageFlags usage);

    void setEnvironmentMap(float* data, int width, int height);

    // Public state
    int      width_  = 0;
    int      height_ = 0;

    uint32_t currentFrame_ = 0;
    uint32_t currentSpp_   = 0;

    float    totalTime_    = 0.0f;
    bool     cameraMoved_  = true;

    Texture     m_environmentMap{};
    VkSampler   m_environmentSampler = VK_NULL_HANDLE;
    int         m_envMapWidth  = 0;
    int         m_envMapHeight = 0;
    bool        m_hasEnvironmentMap = false;

    VkPipeline       pipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    Handle<VkDescriptorSetLayout> descriptorSetLayout_;

    std::vector<Handle<VkImage>>        rtOutputImages_;
    std::vector<Handle<VkImageView>>    rtOutputViews_;
    std::vector<Handle<VkDeviceMemory>> rtOutputMemories_;

    Handle<VkDescriptorPool> descriptorPool_;
};

} // namespace RTX

// =============================================================================
// PINK LIGHT v∞ — DECEMBER 14, 2025 — ALL IN CONTEXT — ETERNAL RADIANCE
// =============================================================================