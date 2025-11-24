// =============================================================================
// include/modes/RenderMode4.hpp
// AMOURANTH RTX © 2025 — Camera-Tinted Clear — Based on RenderMode1
// PINK PHOTONS ETERNAL — VALHALLA v∞
// =============================================================================

#pragma once

#include "main.hpp"                         // ONE TRUE HEADER
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <chrono>

namespace Engine {

class RenderMode4 {
public:
    RenderMode4(uint32_t width, uint32_t height);
    ~RenderMode4();

    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    uint32_t width_, height_;

    RTX::Handle<VkImage>        outputImage_;
    RTX::Handle<VkImageView>    outputView_;
    RTX::Handle<VkDeviceMemory> outputMem_;

    void initResources();
    void cleanupResources();
    void clearCameraTinted(VkCommandBuffer cmd);
    glm::vec3 normalizePosition(glm::vec3 pos) const;
};

} // namespace Engine