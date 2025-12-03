// include/modes/RenderMode6.hpp   (same for 7,8,9 — just change the number)
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

class RenderMode7
{
public:
    RenderMode7(uint32_t width, uint32_t height);
    ~RenderMode7() = default;

    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    void updateUniforms(float deltaTime);
    void traceRays(VkCommandBuffer cmd);

    uint32_t width_;
    uint32_t height_;
    uint64_t frameCount_ = 0;
};