// GPU Doom QA — mode 13h assets in guest RAM, host S1E1 raycast smoke test (no libx86emu).
#include "FieldDoom.hpp"
#include "FieldDoomGpu.hpp"
#include "FieldPlatform.hpp"
#include "FieldVga.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

static int countFbNz(const std::uint8_t* ram, int y0, int y1) {
    int nz = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = 0; x < 320; ++x)
            if (ram[FieldDoomGpu::DOOM_FB_BYTE + static_cast<std::uint32_t>(y * 320 + x)]) ++nz;
    return nz;
}

static bool writeMode13Ppm(const std::filesystem::path& path, const std::uint8_t* ram) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n320 200\n255\n";
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 320; ++x) {
            const std::uint8_t idx =
                ram[FieldDoomGpu::DOOM_FB_BYTE + static_cast<std::uint32_t>(y * 320 + x)];
            const std::size_t off = static_cast<std::size_t>(idx) * 3u;
            const std::uint8_t* pal = ram + FieldDoomGpu::DOOM_PAL_BYTE;
            out.put(static_cast<char>(pal[off]));
            out.put(static_cast<char>(pal[off + 1]));
            out.put(static_cast<char>(pal[off + 2]));
        }
    }
    return static_cast<bool>(out);
}

int main(int argc, char** argv) {
    const std::filesystem::path outDir = (argc >= 2) ? argv[1] : "build/grabs/doom";
    std::filesystem::create_directories(outDir);

    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);
    auto* ram = FieldDoomGpu::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * sizeof(std::uint32_t));

    if (!FieldDoomGpu::blitTitleToGuest(ram)) {
        std::fprintf(stderr, "FAIL FieldDoomGpu::blitTitleToGuest (GPU assets)\n");
        return 1;
    }

    const std::uint8_t mode = ram[0x449u];
    const int titleNz = countFbNz(ram, 0, 200);
    std::fprintf(stderr, "GPU launch mode=0x%02X titleNz=%d\n",
        static_cast<unsigned>(mode), titleNz);

    if (mode != 0x13u) {
        std::fprintf(stderr, "FAIL VGA mode not 13h (got 0x%02X)\n", static_cast<unsigned>(mode));
        return 1;
    }
    if (titleNz < 8000) {
        std::fprintf(stderr, "FAIL title framebuffer too empty (nz=%d)\n", titleNz);
        return 1;
    }

    const auto titlePpm = outDir / "gpu_title.ppm";
    if (!writeMode13Ppm(titlePpm, ram)) {
        std::fprintf(stderr, "FAIL write %s\n", titlePpm.string().c_str());
        return 1;
    }

    FieldDoomGpu::beginS1e1(ram);
    for (int f = 0; f < 48; ++f)
        FieldDoomGpu::stepS1e1(ram, FieldDoomGpu::KEY_UP, false);
    const int playNz = countFbNz(ram, 0, 200);
    std::fprintf(stderr, "playNz=%d level=%u\n", playNz,
        static_cast<unsigned>(FieldDoomGpu::doomState(ram, FieldDoomGpu::ST_LEVEL)));

    if (playNz < 4000) {
        std::fprintf(stderr, "FAIL play framebuffer too empty (nz=%d)\n", playNz);
        return 1;
    }

    const auto playPpm = outDir / "gpu_play.ppm";
    if (!writeMode13Ppm(playPpm, ram)) {
        std::fprintf(stderr, "FAIL write %s\n", playPpm.string().c_str());
        return 1;
    }

    std::printf("PASS gpu_doom mode13 titleNz=%d playNz=%d\n", titleNz, playNz);
    return 0;
}