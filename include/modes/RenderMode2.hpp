// include/modes/RenderMode2.hpp
// ENVMAP GAZE MODE — FULL-SCREEN HDR SKY — THE EMPIRE BEHOLDS THE INFINITE
// DECEMBER 15, 2025 — v16.0 — PURE SKY DISPLAY

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class RenderMode2 {
public:
    RenderMode2(uint32_t w, uint32_t h);
    ~RenderMode2() = default;

    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t w, uint32_t h);

private:
    uint32_t width_;
    uint32_t height_;
    uint32_t frameIndex_{0};
    float    totalTime_{0.0f};
};