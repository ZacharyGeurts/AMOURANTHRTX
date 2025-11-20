// include/modes/RenderMode1.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — PURE RTX PATH TRACING — scene.obj IN VALHALLA
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"   // ← Brings in RTX::Handle + g_ctx()
#include "engine/GLOBAL/StoneKey.hpp"     // ← g_device(), g_instance(), etc.
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <chrono>

using namespace RTX;

class RenderMode1 {
public:
    RenderMode1(uint32_t width, uint32_t height);
    ~RenderMode1();

    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    uint32_t width_, height_;
    uint32_t frameCount_ = 0;
    std::chrono::steady_clock::time_point lastFrame_;

    uint64_t uniformBuf_ = 0;
    uint64_t accumulationBuf_ = 0;
    VkDeviceSize accumSize_ = 0;

    RTX::Handle<VkImage>        accumImage_;
    RTX::Handle<VkImageView>    accumView_;
    RTX::Handle<VkImage>        outputImage_;
    RTX::Handle<VkImageView>    outputView_;
    RTX::Handle<VkDeviceMemory> accumMem_;
    RTX::Handle<VkDeviceMemory> outputMem_;

    bool sceneLoaded_ = false;

    void initResources();
    void cleanupResources();
    void updateUniforms(float deltaTime);
    void traceRays(VkCommandBuffer cmd);
    void accumulateAndToneMap(VkCommandBuffer cmd);
    void loadSceneFromDisk();
};