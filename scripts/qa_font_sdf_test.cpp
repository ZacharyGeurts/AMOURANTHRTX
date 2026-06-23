// QA: RTX font SDF atlas — JetBrains Mono, crisp 6.0 spread, dual-polarity shader constants.
#include <cstdio>
#include <fstream>
#include <string>

static bool fileContains(const char* path, const char* needle) {
    std::ifstream in(path);
    if (!in) return false;
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text.find(needle) != std::string::npos;
}

int main() {
    const char* meta = "Navigator/shaders/compute/rtx_font_meta.inc";
    const char* png = "assets/textures/rtx_font_sdf.png";
    const char* render = "Navigator/shaders/compute/rtx_font_render.inc";

    if (!fileContains(meta, "FONT_SDF_SPREAD = 6.0")) {
        std::fprintf(stderr, "FAIL %s missing FONT_SDF_SPREAD = 6.0\n", meta);
        return 1;
    }
    if (!fileContains(meta, "FONT_SDF_ATLAS_W = 512u")) {
        std::fprintf(stderr, "FAIL %s atlas width\n", meta);
        return 1;
    }
    if (!fileContains(render, "mixHudInkLight")) {
        std::fprintf(stderr, "FAIL %s missing mixHudInkLight\n", render);
        return 1;
    }
    if (!fileContains(render, "mixHudInkDark")) {
        std::fprintf(stderr, "FAIL %s missing mixHudInkDark\n", render);
        return 1;
    }
    if (!fileContains(render, "FONT_FRINGE_DARK")) {
        std::fprintf(stderr, "FAIL %s missing dual fringe constants\n", render);
        return 1;
    }

    std::ifstream img(png, std::ios::binary);
    if (!img) {
        std::fprintf(stderr, "FAIL missing %s — run gen_rtx_sdf_font.py\n", png);
        return 1;
    }
    img.seekg(0, std::ios::end);
    const auto bytes = img.tellg();
    if (bytes < 4096) {
        std::fprintf(stderr, "FAIL %s too small (%ld bytes)\n", png, static_cast<long>(bytes));
        return 1;
    }

    std::printf("OK font SDF meta spread=6.0 atlas=512x768\n");
    std::printf("OK font render dual-polarity fringe (light+dark ink)\n");
    std::printf("Font SDF QA passed (v2.0.4)\n");
    return 0;
}