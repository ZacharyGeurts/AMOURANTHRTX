// S1E1 bot playthrough — GPU Doom raycast path (same assets as qa_doom_gpu_test).
#include "FieldDoomGpu.hpp"
#include "FieldPlatform.hpp"
#include "FieldVga.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

int main() {
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);
    auto* ram = FieldDoomGpu::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * sizeof(std::uint32_t));

    if (!FieldDoomGpu::blitTitleToGuest(ram)) {
        std::fprintf(stderr, "FAIL blitTitleToGuest\n");
        return 1;
    }
    if (ram[0x449u] != 0x13u) {
        std::fprintf(stderr, "FAIL boot: mode=%u\n", static_cast<unsigned>(ram[0x449u]));
        return 1;
    }

    FieldDoomGpu::beginS1e1(ram);
    std::printf("METRIC s1e1_boot=1\n");
    std::printf("S1E1 boot ok level=%u\n",
        static_cast<unsigned>(FieldDoomGpu::doomState(ram, FieldDoomGpu::ST_LEVEL)));

    constexpr int maxFrames = 12000;
    for (int f = 0; f < maxFrames; ++f) {
        FieldDoomGpu::stepS1e1(ram, 0u, true);
        if (FieldDoomGpu::episodeComplete(ram)) {
            std::printf("METRIC s1e1_complete=1\n");
            std::printf("METRIC s1e1_frames=%d\n", f + 1);
            std::printf("S1E1 complete: frames=%d levels=%d\n", f + 1, FieldDoomGpu::S1E1_LEVELS);
            return 0;
        }
        if (f > 0 && (f % 500) == 0)
            std::printf("  progress frame=%d level=%u\n", f,
                static_cast<unsigned>(FieldDoomGpu::doomState(ram, FieldDoomGpu::ST_LEVEL)));
    }

    std::fprintf(stderr, "FAIL timeout level=%u frames=%u\n",
        static_cast<unsigned>(FieldDoomGpu::doomState(ram, FieldDoomGpu::ST_LEVEL)),
        static_cast<unsigned>(FieldDoomGpu::doomState(ram, FieldDoomGpu::ST_FRAMES)));
    return 1;
}