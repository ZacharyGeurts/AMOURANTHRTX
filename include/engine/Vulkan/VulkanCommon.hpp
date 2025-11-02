// include/engine/Vulkan/VulkanCommon.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <string>
#include <format>
#include <span>
#include <compare>
#include "engine/camera.hpp"
#include "engine/logging.hpp"

namespace VulkanRTX {
    class VulkanRenderer;
}

// ========================================================================
// 0. Global Constants
// ========================================================================
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

// ========================================================================
// 1. Strided Device Address Region (exact Vulkan spec order)
// ========================================================================
struct StridedDeviceAddressRegionKHR {
    VkDeviceAddress deviceAddress = 0;
    VkDeviceSize    stride        = 0;  // 2nd field
    VkDeviceSize    size          = 0;  // 3rd field
};

// ========================================================================
// 2. Shader Binding Table
// ========================================================================
struct ShaderBindingTable {
    StridedDeviceAddressRegionKHR raygen;
    StridedDeviceAddressRegionKHR miss;
    StridedDeviceAddressRegionKHR hit;
    StridedDeviceAddressRegionKHR callable;
};

// ========================================================================
// 3. Per-Frame Resources
// ========================================================================
struct Frame {
    VkCommandBuffer         commandBuffer           = VK_NULL_HANDLE;
    VkDescriptorSet         rayTracingDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet         graphicsDescriptorSet   = VK_NULL_HANDLE;
    VkDescriptorSet         computeDescriptorSet    = VK_NULL_HANDLE;
    VkSemaphore             imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore             renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence                 fence                   = VK_NULL_HANDLE;
};

// ========================================================================
// 4. MaterialData – SSBO (matches raygen.rgen)
// ========================================================================
struct alignas(16) MaterialData {
    alignas(16) glm::vec4 diffuse   = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    alignas(4)  float     specular  = 0.0f;
    alignas(4)  float     roughness = 0.5f;
    alignas(4)  float     metallic  = 0.0f;
    alignas(16) glm::vec4 emission  = glm::vec4(0.0f);

    struct PushConstants {
        alignas(16) glm::vec4 clearColor      = glm::vec4(0.0f);        // 16 → 0-16
        alignas(16) glm::vec3 cameraPosition = glm::vec3(0.0f);        // 12 → 16-28
        alignas(4)  float     _pad0          = 0.0f;                    // 4  → 28-32
        alignas(16) glm::vec3 lightDirection = glm::vec3(0.0f, -1.0f, 0.0f); // 12 → 32-44
        alignas(4)  float     lightIntensity = 1.0f;                    // 4  → 44-48
        alignas(4)  uint32_t  samplesPerPixel = 1;                      // 4  → 48-52
        alignas(4)  uint32_t  maxDepth        = 5;                      // 4  → 52-56
        alignas(4)  uint32_t  maxBounces      = 3;                      // 4  → 56-60
        alignas(4)  float     russianRoulette = 0.8f;                   // 4  → 60-64
        alignas(8)  glm::vec2 resolution     = glm::vec2(1920, 1080);  // 8  → 64-72
        alignas(4)  uint32_t  showEnvMapOnly = 0;                       // 4  → 72-76
    };
};

static_assert(sizeof(MaterialData) == 48, "MaterialData must be 48 bytes");
static_assert(sizeof(MaterialData::PushConstants) == 80, "PushConstants must be 80 bytes (96 with padding)");

// ========================================================================
// 5. DimensionData – SSBO (screen size)
// ========================================================================
struct alignas(16) DimensionData {
    uint32_t screenWidth  = 0;
    uint32_t screenHeight = 0;
    uint32_t _pad0        = 0;
    uint32_t _pad1        = 0;
};
static_assert(sizeof(DimensionData) == 16, "DimensionData must be 16 bytes");

// ========================================================================
// 6. UniformBufferObject – UBO (MUST BE 256 BYTES)
// ========================================================================
struct alignas(16) UniformBufferObject {
    alignas(16) glm::mat4 viewInverse;
    alignas(16) glm::mat4 projInverse;
    alignas(16) glm::vec4 camPos;
    alignas(4)  float     time;
    alignas(4)  uint32_t  frame;
    alignas(4)  float     _pad[26];
};
static_assert(sizeof(UniformBufferObject) == 256, "UBO must be 256 bytes");

// ========================================================================
// 7. DimensionState – CPU-side state
// ========================================================================
struct DimensionState {
    int       dimension = 0;
    float     scale     = 1.0f;
    glm::vec3 position  = glm::vec3(0.0f);
    float     intensity = 1.0f;

    std::string toString() const {
        return std::format(
            "Dim: {}, Scale: {:.3f}, Pos: ({:.2f}, {:.2f}, {:.2f}), Intensity: {:.3f}",
            dimension, scale, position.x, position.y, position.z, intensity);
    }

    bool operator==(const DimensionState& other) const = default;
};

// ========================================================================
// 8. Denoiser Push Constants (for compute shader)
// ========================================================================
struct alignas(16) DenoisePushConstants {
    uint32_t width = 0;
    uint32_t height = 0;
    float kernelRadius = 1.0f;
    uint32_t _pad0 = 0;  // Padding to 16 bytes
};
static_assert(sizeof(DenoisePushConstants) == 16, "DenoisePushConstants must be 16 bytes");

// ========================================================================
// 9. AMOURANTH – camera + demo controller
// ========================================================================
class AMOURANTH : public VulkanRTX::Camera {
public:
    explicit AMOURANTH(VulkanRTX::VulkanRenderer* renderer, int width, int height);
    ~AMOURANTH() override;

    // --- Required pure virtual overrides from Camera ---
    glm::mat4 getViewMatrix() const override;
    glm::mat4 getProjectionMatrix() const override;
    int       getMode() const override;
    glm::vec3 getPosition() const override;
    void      setPosition(const glm::vec3& pos) override;
    void      setOrientation(float yaw, float pitch) override;
    void      update(float deltaTime) override;
    void      moveForward(float speed) override;
    void      moveRight(float speed) override;
    void      moveUp(float speed) override;
    void      rotate(float yawDelta, float pitchDelta) override;
    void      setFOV(float fov) override;
    float     getFOV() const override;
    void      setMode(int mode) override;
    void      setAspectRatio(float aspectRatio) override;
    void      moveCamera(float x, float y, float z,
                         std::source_location loc = std::source_location::current()) override;
    void      rotateCamera(float yaw, float pitch,
                           std::source_location loc = std::source_location::current()) override;
    void      moveUserCam(float dx, float dy, float dz) override;
    void      togglePause() override;
    void      updateZoom(bool zoomIn) override;
    void      setUserData(void* data) override;
    void*     getUserData() const override;

    // --- Custom AMOURANTH methods ---
    void setCurrentDimension(int dim);
    void adjustScale(float delta);
    void updateDimensionBuffer(VkDevice device, uint32_t currentFrame);

    int   getCurrentDimension() const { return currentDimension_; }
    float getScale() const            { return scale_; }
    bool  isPaused() const            { return paused_; }
    const std::vector<DimensionState>& getDimensions() const { return dimensions_; }

    PFN_vkCmdTraceRaysKHR getVkCmdTraceRaysKHR() const { return vkCmdTraceRaysKHR_; }
    VkStridedDeviceAddressRegionKHR getRaygenSBT() const   { return raygenSBT_; }
    VkStridedDeviceAddressRegionKHR getMissSBT() const     { return missSBT_; }
    VkStridedDeviceAddressRegionKHR getHitSBT() const      { return hitSBT_; }
    VkStridedDeviceAddressRegionKHR getCallableSBT() const { return callableSBT_; }

private:
    VulkanRTX::VulkanRenderer* renderer_;
    int width_, height_;

    int   mode_ = 0;
    int   currentDimension_ = 0;
    float scale_ = 1.0f;
    bool  paused_ = false;

    glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 5.0f);
    glm::vec3 front_    = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up_       = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw_   = -90.0f;
    float pitch_ = 0.0f;
    float fov_   = 45.0f;
    float sensitivity_ = 0.1f;
    float speed_ = 2.5f;

    std::vector<DimensionState> dimensions_;
    std::vector<UniformBufferObject> ubos_;

    VkBuffer       dimensionBuffer_       = VK_NULL_HANDLE;
    VkDeviceMemory dimensionBufferMemory_ = VK_NULL_HANDLE;

    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR_ = nullptr;
    VkStridedDeviceAddressRegionKHR raygenSBT_   = {};
    VkStridedDeviceAddressRegionKHR missSBT_     = {};
    VkStridedDeviceAddressRegionKHR hitSBT_      = {};
    VkStridedDeviceAddressRegionKHR callableSBT_ = {};

    void updateCameraVectors();
    void createDimensionBuffer(VkDevice device);
};

// ========================================================================
// 10. std::formatter for AMOURANTH (pretty logging)
// ========================================================================
namespace std {
template<>
struct formatter<AMOURANTH, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const AMOURANTH& cam, FormatContext& ctx) const {
        return format_to(ctx.out(),
                         "AMOURANTH(dim={}, mode={}, scale={:.2f}, paused={}, pos=({:.2f},{:.2f},{:.2f}))",
                         cam.getCurrentDimension(), cam.getMode(),
                         cam.getScale(), cam.isPaused(),
                         cam.getPosition().x, cam.getPosition().y, cam.getPosition().z);
    }
};
} // VULKAN_COMMON_HPP