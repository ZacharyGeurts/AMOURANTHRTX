// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#pragma once
#ifndef VULKAN_TYPES_HPP
#define VULKAN_TYPES_HPP

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <string>
#include <format>
#include <span>
#include <compare>

#include "engine/camera.hpp"
#include "engine/logging.hpp"

namespace VulkanRTX { class VulkanRenderer; }

// ================================================================
// 1. MaterialData – SSBO (matches raygen.rgen)
// ================================================================
struct alignas(16) MaterialData {
    alignas(16) glm::vec4 diffuse   = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    alignas(4)  float     specular  = 0.0f;
    alignas(4)  float     roughness = 0.5f;
    alignas(4)  float     metallic  = 0.0f;
    alignas(16) glm::vec4 emission  = glm::vec4(0.0f);

    struct PushConstants {
        alignas(16) glm::vec4 clearColor      = glm::vec4(0.0f);
        alignas(16) glm::vec3 cameraPosition = glm::vec3(0.0f);
        alignas(4)  float     _pad0          = 0.0f;
        alignas(16) glm::vec3 lightDirection = glm::vec3(0.0f, -1.0f, 0.0f);
        alignas(4)  float     lightIntensity = 1.0f;
        alignas(4)  uint32_t  samplesPerPixel = 1;
        alignas(4)  uint32_t  maxDepth        = 5;
        alignas(4)  uint32_t  maxBounces      = 3;
        alignas(4)  float     russianRoulette = 0.8f;
        alignas(8)  glm::vec2 resolution     = glm::vec2(1920, 1080);
    };
};

static_assert(sizeof(MaterialData) == 48, "MaterialData must be 48 bytes");
static_assert(sizeof(MaterialData::PushConstants) == 80, "PushConstants must be 80 bytes");

// ================================================================
// 2. DimensionData – SSBO (screen size)
// ================================================================
struct alignas(16) DimensionData {
    uint32_t screenWidth  = 0;
    uint32_t screenHeight = 0;
    uint32_t _pad0        = 0;
    uint32_t _pad1        = 0;
};
static_assert(sizeof(DimensionData) == 16, "DimensionData must be 16 bytes");

// ================================================================
// 3. UniformBufferObject – UBO (MUST BE 256 BYTES)
// ================================================================
struct alignas(16) UniformBufferObject {
    alignas(16) glm::mat4 viewInverse;
    alignas(16) glm::mat4 projInverse;
    alignas(16) glm::vec4 camPos;
    alignas(4)  float     time;
    alignas(4)  uint32_t  frame;
    alignas(4)  float     _pad[26];
};
static_assert(sizeof(UniformBufferObject) == 256, "UBO must be 256 bytes");

// ================================================================
// 4. DimensionState – CPU-side state
// ================================================================
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

// ================================================================
// 5. Shader Binding Table (SBT) - Fixed to match VulkanRenderer usage
// ================================================================
struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen{};
    VkStridedDeviceAddressRegionKHR miss{};
    VkStridedDeviceAddressRegionKHR hit{};
    VkStridedDeviceAddressRegionKHR callable{};
};

// ================================================================
// 6. Denoiser Push Constants (for compute shader)
// ================================================================
struct alignas(16) DenoisePushConstants {
    alignas(8)  glm::ivec2 imageSize     = glm::ivec2(0);     // 8 bytes
    alignas(4)  float      kernelRadius  = 1.0f;              // 4 bytes
    alignas(4)  uint32_t   _pad0         = 0;                 // 4 bytes → total 16
};
static_assert(sizeof(DenoisePushConstants) == 16, "DenoisePushConstants must be 16 bytes");

// ================================================================
// 7. AMOURANTH – camera + demo controller
// ================================================================
class AMOURANTH : public Camera {
public:
    explicit AMOURANTH(VulkanRTX::VulkanRenderer* renderer, int width, int height);
    ~AMOURANTH() override;

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

    void setMode(int mode);
    void setCurrentDimension(int dim);
    void adjustScale(float delta);
    void togglePause();
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

namespace std {
template<>
struct formatter<AMOURANTH, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const AMOURANTH& cam, FormatContext& ctx) const {
        return format_to(ctx.out(),
                         "AMOURANTH(dim={}, mode={}, scale={:.2f}, paused={})",
                         cam.getCurrentDimension(), cam.getMode(),
                         cam.getScale(), cam.isPaused());
    }
};
}

#endif // VULKAN_TYPES_HPP