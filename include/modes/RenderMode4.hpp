// include/modes/RenderMode4.hpp
#pragma once

#include <vulkan/vulkan.h>

class RenderMode4 {
public:
    RenderMode4(uint32_t width, uint32_t height);
    ~RenderMode4();

    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);
};