#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "engine/GLOBAL/StoneKey.hpp"

class RenderMode2 {
public:
    RenderMode2(uint32_t width, uint32_t height);
    ~RenderMode2();

    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t w, uint32_t h);

    const char* getName() const {
        return "RTX::CUBE OF ETERNAL BALLZ — STONEKEY Ω — PINK PHOTONS ETERNAL";
    }

private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    float totalTime_ = 0.0f;

    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_  = VK_NULL_HANDLE;
    VkDeviceAddress vertexAddr_ = 0;
    VkDeviceAddress indexAddr_  = 0;
};