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
#include <filesystem>
#include <unordered_map>
#include <memory>
#include "engine/camera.hpp"
#include "engine/logging.hpp"

namespace VulkanRTX {

class VulkanRenderer;

// ========================================================================
// 0. Global Constants
// ========================================================================
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

// ========================================================================
// 1. Strided Device Address Region
// ========================================================================
struct StridedDeviceAddressRegionKHR {
    VkDeviceAddress deviceAddress = 0;
    VkDeviceSize    stride        = 0;
    VkDeviceSize    size          = 0;
};

// ========================================================================
// 2. Shader Binding Table – FULL SUPPORT
// ========================================================================
struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen;
    VkStridedDeviceAddressRegionKHR miss;
    VkStridedDeviceAddressRegionKHR hit;
    VkStridedDeviceAddressRegionKHR callable;
    VkStridedDeviceAddressRegionKHR anyHit;
    VkStridedDeviceAddressRegionKHR shadowMiss;
    VkStridedDeviceAddressRegionKHR shadowAnyHit;
    VkStridedDeviceAddressRegionKHR intersection;
    VkStridedDeviceAddressRegionKHR volumetricAnyHit;
    VkStridedDeviceAddressRegionKHR midAnyHit;

    static VkStridedDeviceAddressRegionKHR emptyRegion() {
        return { .deviceAddress = 0, .stride = 0, .size = 0 };
    }

    static VkStridedDeviceAddressRegionKHR makeRegion(VkDeviceAddress base, VkDeviceSize size, VkDeviceSize stride) {
        return { .deviceAddress = base, .stride = stride, .size = size };
    }
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
// 4. MaterialData
// ========================================================================
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
        alignas(4)  uint32_t  showEnvMapOnly = 0;
    };
};

static_assert(sizeof(MaterialData) == 48, "MaterialData must be 48 bytes");
static_assert(sizeof(MaterialData::PushConstants) == 80, "PushConstants must be 80 bytes");

// ========================================================================
// 5. DimensionData
// ========================================================================
struct alignas(16) DimensionData {
    uint32_t screenWidth  = 0;
    uint32_t screenHeight = 0;
    uint32_t _pad0        = 0;
    uint32_t _pad1        = 0;
};
static_assert(sizeof(DimensionData) == 16, "DimensionData must be 16 bytes");

// ========================================================================
// 6. UniformBufferObject
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
// 7. DimensionState – CPU-side
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
// 8. Tonemap Push Constants
// ========================================================================
struct alignas(16) TonemapPushConstants {
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t _pad0  = 0;
    uint32_t _pad1  = 0;
};
static_assert(sizeof(TonemapPushConstants) == 16, "TonemapPushConstants must be 16 bytes");

// ========================================================================
// 9. Shader Paths — ALL SHADERS LOADED
// ========================================================================
inline std::unordered_map<std::string, std::string> getShaderBinPaths() {
    return {
        {"raygen",              "assets/shaders/raytracing/raygen.spv"},
        {"miss",                "assets/shaders/raytracing/miss.spv"},
        {"closesthit",          "assets/shaders/raytracing/closesthit.spv"},
        {"anyhit",              "assets/shaders/raytracing/anyhit.spv"},
        {"callable",            "assets/shaders/raytracing/callable.spv"},
        {"intersection",        "assets/shaders/raytracing/intersection.spv"},
        {"shadow_anyhit",       "assets/shaders/raytracing/shadow_anyhit.spv"},
        {"shadowmiss",          "assets/shaders/raytracing/shadowmiss.spv"},
        {"volumetric_anyhit",   "assets/shaders/raytracing/volumetric_anyhit.spv"},
        {"mid_anyhit",          "assets/shaders/raytracing/mid_anyhit.spv"},
        {"tonemap_compute",     "assets/shaders/compute/tonemap.spv"},
        {"tonemap_vert",        "assets/shaders/graphics/tonemap_vert.spv"},
        {"tonemap_frag",        "assets/shaders/graphics/tonemap_frag.spv"}
    };
}

inline std::unordered_map<std::string, std::string> getShaderSrcPaths() {
    return {
        {"raygen",              "assets/shaders/raytracing/raygen.rgen"},
        {"miss",                "assets/shaders/raytracing/miss.rmiss"},
        {"closesthit",          "assets/shaders/raytracing/closesthit.rchit"},
        {"anyhit",              "assets/shaders/raytracing/anyhit.rahit"},
        {"callable",            "assets/shaders/raytracing/callable.rcall"},
        {"intersection",        "assets/shaders/raytracing/intersection.rint"},
        {"shadow_anyhit",       "assets/shaders/raytracing/shadow_anyhit.rahit"},
        {"shadowmiss",          "assets/shaders/raytracing/shadowmiss.rmiss"},
        {"volumetric_anyhit",   "assets/shaders/raytracing/volumetric_anyhit.rahit"},
        {"mid_anyhit",          "assets/shaders/raytracing/mid_anyhit.rahit"},
        {"tonemap_compute",     "assets/shaders/compute/tonemap.comp"},
        {"tonemap_vert",        "assets/shaders/graphics/tonemap_vert.glsl"},
        {"tonemap_frag",        "assets/shaders/graphics/tonemap_frag.glsl"}
    };
}

inline std::vector<std::string> getRayTracingBinPaths() {
    auto binPaths = getShaderBinPaths();
    return {
        binPaths.at("raygen"),
        binPaths.at("miss"),
        binPaths.at("closesthit"),
        binPaths.at("anyhit"),
        binPaths.at("callable"),
        binPaths.at("intersection"),
        binPaths.at("shadow_anyhit"),
        binPaths.at("shadowmiss"),
        binPaths.at("volumetric_anyhit"),
        binPaths.at("mid_anyhit")
    };
}

// ========================================================================
// 10. findShaderPath
// ========================================================================
inline std::string findShaderPath(const std::string& logicalName) {
    using namespace Logging::Color;
    LOG_DEBUG_CAT("Vulkan", ">>> RESOLVING SHADER '{}'", logicalName);

    auto binPaths = getShaderBinPaths();
    auto binIt = binPaths.find(logicalName);
    if (binIt == binPaths.end()) {
        LOG_ERROR_CAT("Vulkan", "  --> UNKNOWN SHADER NAME '{}'", logicalName);
        throw std::runtime_error("Unknown shader name: " + logicalName);
    }
    std::filesystem::path binPath = std::filesystem::current_path() / binIt->second;
    if (std::filesystem::exists(binPath)) {
        LOG_DEBUG_CAT("Vulkan", "  --> FOUND IN BIN: {}", binPath.string());
        return binPath.string();
    }

    auto srcPaths = getShaderSrcPaths();
    auto srcIt = srcPaths.find(logicalName);
    if (srcIt == srcPaths.end()) {
        LOG_ERROR_CAT("Vulkan", "  --> NO SOURCE-TREE ENTRY FOR '{}'", logicalName);
        throw std::runtime_error("Unknown shader name: " + logicalName);
    }
    const auto projectRoot = std::filesystem::current_path().parent_path().parent_path().parent_path();
    const std::filesystem::path srcPath = projectRoot / srcIt->second;

    if (std::filesystem::exists(srcPath)) {
        LOG_DEBUG_CAT("Vulkan", "  --> FOUND IN SRC: {}", srcPath.string());
        return srcPath.string();
    }

    LOG_ERROR_CAT("Vulkan",
                  "  --> SHADER NOT FOUND!\n"
                  "      BIN: {}\n"
                  "      SRC: {}", binPath.string(), srcPath.string());

    throw std::runtime_error("Shader file missing: " + logicalName);
}

// ========================================================================
// 11. AMOURANTH – camera + demo controller
// ========================================================================
class AMOURANTH : public Camera {
public:
    explicit AMOURANTH(VulkanRenderer* renderer, int width, int height);
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

    void setCurrentDimension(int dim);
    void adjustScale(float delta);
    void updateDimensionBuffer(VkDevice device, uint32_t currentFrame);

    int   getCurrentDimension() const { return currentDimension_; }
    float getScale() const            { return scale_; }
    bool  isPaused() const            { return paused_; }
    const std::vector<DimensionState>& getDimensions() const { return dimensions_; }

    PFN_vkCmdTraceRaysKHR getVkCmdTraceRaysKHR() const { return vkCmdTraceRaysKHR_; }
    const VkStridedDeviceAddressRegionKHR& getRaygenSBT() const   { return raygenSBT_; }
    const VkStridedDeviceAddressRegionKHR& getMissSBT() const     { return missSBT_; }
    const VkStridedDeviceAddressRegionKHR& getHitSBT() const      { return hitSBT_; }
    const VkStridedDeviceAddressRegionKHR& getCallableSBT() const { return callableSBT_; }
    const VkStridedDeviceAddressRegionKHR& getAnyHitSBT() const   { return anyHitSBT_; }
    const VkStridedDeviceAddressRegionKHR& getShadowMissSBT() const { return shadowMissSBT_; }
    const VkStridedDeviceAddressRegionKHR& getShadowAnyHitSBT() const { return shadowAnyHitSBT_; }
    const VkStridedDeviceAddressRegionKHR& getIntersectionSBT() const { return intersectionSBT_; }
    const VkStridedDeviceAddressRegionKHR& getVolumetricAnyHitSBT() const { return volumetricAnyHitSBT_; }
    const VkStridedDeviceAddressRegionKHR& getMidAnyHitSBT() const { return midAnyHitSBT_; }

private:
    VulkanRenderer* renderer_;
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
    VkStridedDeviceAddressRegionKHR anyHitSBT_   = {};
    VkStridedDeviceAddressRegionKHR shadowMissSBT_ = {};
    VkStridedDeviceAddressRegionKHR shadowAnyHitSBT_ = {};
    VkStridedDeviceAddressRegionKHR intersectionSBT_ = {};
    VkStridedDeviceAddressRegionKHR volumetricAnyHitSBT_ = {};
    VkStridedDeviceAddressRegionKHR midAnyHitSBT_ = {};

    void updateCameraVectors();
    void createDimensionBuffer(VkDevice device);
};

} // namespace VulkanRTX

// ========================================================================
// 12. std::formatter for AMOURANTH — OUTSIDE VulkanRTX
// ========================================================================
template<>
struct std::formatter<VulkanRTX::AMOURANTH, char> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const VulkanRTX::AMOURANTH& cam, FormatContext& ctx) const {
        return std::format_to(ctx.out(),
                              "AMOURANTH(dim={}, mode={}, scale={:.2f}, paused={}, pos=({:.2f},{:.2f},{:.2f}))",
                              cam.getCurrentDimension(), cam.getMode(),
                              cam.getScale(), cam.isPaused(),
                              cam.getPosition().x, cam.getPosition().y, cam.getPosition().z);
    }
};