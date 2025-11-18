#pragma once

#include <cstdint>  // ← REQUIRED for uint64_t

struct ImDrawData;
using VkCommandBuffer = struct VkCommandBuffer_T*;

namespace RTX {

struct ImGuiStoneKeyShield {
    static void newFrame();
    static void renderDrawData(ImDrawData* draw_data, VkCommandBuffer cmd);

private:
    static bool stonekey_active_;
    static uint64_t frameNumber();
};

}  // namespace RTX