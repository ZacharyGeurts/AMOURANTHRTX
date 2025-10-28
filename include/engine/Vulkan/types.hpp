// engine/Vulkan/types.hpp
// AMOURANTH RTX Engine – Core type definitions (no UE namespace)
// © 2025 Zachary Geurts – CC BY-NC 4.0
#pragma once
#ifndef AMOURANTH_TYPES_HPP
#define AMOURANTH_TYPES_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <format>
#include <span>
#include <compare>                     // ← REQUIRED for = default operator==

#include "engine/camera.hpp"
#include "engine/logging.hpp"

namespace VulkanRTX { class VulkanRenderer; }   // forward decl

/* --------------------------------------------------------------------- *
 *  1. DimensionData – SSBO data
 * --------------------------------------------------------------------- */
struct DimensionData {
    alignas(16) glm::vec3 position = glm::vec3(0.0f);
    alignas(16) glm::vec3 scale    = glm::vec3(1.0f);
    alignas(16) glm::vec4 color    = glm::vec4(1.0f);
	alignas(4)  int32_t   dimension = 0; 
};

/* --------------------------------------------------------------------- *
 *  2. UniformBufferObject – per-frame UBO
 * --------------------------------------------------------------------- */
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 camPos;  // Camera position + w=1.0
    alignas(4)  float     time;    // Animation / shader time
};

/* --------------------------------------------------------------------- *
 *  3. DimensionState – CPU visualisation state
 * --------------------------------------------------------------------- */
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

    // ← CRITICAL: Enables std::vector<DimensionState> ==, !=, etc.
    bool operator==(const DimensionState& other) const = default;
};

/* --------------------------------------------------------------------- *
 *  4. AMOURANTH – camera + demo controller
 * --------------------------------------------------------------------- */
class AMOURANTH : public Camera {
public:
    explicit AMOURANTH(VulkanRTX::VulkanRenderer* renderer, int width, int height);
    ~AMOURANTH() override;

    // Camera interface
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

    // Demo-specific
    void setMode(int mode);
    void setCurrentDimension(int dim);
    void adjustScale(float delta);
    void togglePause();
    void updateDimensionBuffer(VkDevice device, uint32_t currentFrame);

    // Getters
    int   getCurrentDimension() const { return currentDimension_; }
    float getScale() const            { return scale_; }
    bool  isPaused() const            { return paused_; }
    const std::vector<DimensionState>& getDimensions() const { return dimensions_; }

    // Ray-tracing SBT
    PFN_vkCmdTraceRaysKHR getVkCmdTraceRaysKHR() const { return vkCmdTraceRaysKHR_; }
    VkStridedDeviceAddressRegionKHR getRaygenSBT() const   { return raygenSBT_; }
    VkStridedDeviceAddressRegionKHR getMissSBT() const     { return missSBT_; }
    VkStridedDeviceAddressRegionKHR getHitSBT() const      { return hitSBT_; }
    VkStridedDeviceAddressRegionKHR getCallableSBT() const { return callableSBT_; }

private:
    VulkanRTX::VulkanRenderer* renderer_;
    int width_, height_;

    // Mode / UI
    int   mode_ = 0;
    int   currentDimension_ = 0;
    float scale_ = 1.0f;
    bool  paused_ = false;

    // Camera state
    glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 5.0f);
    glm::vec3 front_    = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up_       = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw_   = -90.0f;
    float pitch_ = 0.0f;
    float fov_   = 45.0f;
    float sensitivity_ = 0.1f;
    float speed_ = 2.5f;

    // Visualisation
    std::vector<DimensionState> dimensions_;

    // UBOs – correct top-level type
    std::vector<UniformBufferObject> ubos_;

    // Vulkan resources
    VkBuffer       dimensionBuffer_       = VK_NULL_HANDLE;
    VkDeviceMemory dimensionBufferMemory_ = VK_NULL_HANDLE;

    // Ray tracing
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR_ = nullptr;
    VkStridedDeviceAddressRegionKHR raygenSBT_   = {};
    VkStridedDeviceAddressRegionKHR missSBT_     = {};
    VkStridedDeviceAddressRegionKHR hitSBT_      = {};
    VkStridedDeviceAddressRegionKHR callableSBT_ = {};

    void updateCameraVectors();
    void createDimensionBuffer(VkDevice device);
};

/* --------------------------------------------------------------------- *
 *  std::format support for AMOURANTH
 * --------------------------------------------------------------------- */
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
} // namespace std

#endif // AMOURANTH_TYPES_HPP