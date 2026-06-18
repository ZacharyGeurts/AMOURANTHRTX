// QA + host bake: assemble RTXFL field-layer .SYS drivers from DRIVERS/*.ASM
#include "FieldAmmoSys.hpp"
#include "FieldDos.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct DriverSpec {
    const char* asmName;
    const char* sysName;
    std::size_t minSize;
    const char* signature;
};

static const DriverSpec kDrivers[] = {
    {"RTXKERNEL.ASM", "RTXKERNEL.SYS", 256, "SUPRCR01"},
    {"HIMEM.ASM",     "HIMEM.SYS",     160, "HIMEM630"},
    {"RTXDRV.ASM",    "RTXDRV.SYS",    160, "RTXDRV01"},
    {"RTXVGA.ASM",    "RTXVGA.SYS",    160, "RTXVGA1"},
    {"RTXSB.ASM",     "RTXSB.SYS",     160, "RTXSB001"},
    {"AMMOFAT.ASM",   "AMMOFAT.SYS",   160, "AMMOFAT1"},
    {"RTXCD.ASM",     "RTXCD.SYS",     160, "RTXCD001"},
    {"RTXHOST.ASM",   "RTXHOST.SYS",   160, "RTXHOST1"},
};

static bool readFile(const fs::path& p, std::vector<std::uint8_t>& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const auto n = f.tellg();
    if (n <= 0) return false;
    out.resize(static_cast<std::size_t>(n));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(out.data()), n);
    return f.good();
}

static bool writeFile(const fs::path& p, const std::vector<std::uint8_t>& data) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data.data()),
        static_cast<std::streamsize>(data.size()));
    return f.good();
}

int main(int argc, char** argv) {
    const fs::path root = (argc >= 2) ? fs::path(argv[1])
        : fs::current_path().parent_path();
    const fs::path srcDir = root / "assets" / "dos" / "ammo" / "DRIVERS";
    const fs::path outDir = (argc >= 3) ? fs::path(argv[2])
        : root / "assets" / "dos" / "ammo";

    int fails = 0;
    for (const auto& spec : kDrivers) {
        const fs::path asmPath = srcDir / spec.asmName;
        std::vector<std::uint8_t> src;
        if (!readFile(asmPath, src)) {
            std::fprintf(stderr, "FAIL missing %s\n", asmPath.string().c_str());
            ++fails;
            continue;
        }
        auto r = FieldAmmoSys::assembleSysSource(
            reinterpret_cast<const char*>(src.data()), src.size(), spec.minSize);
        if (!r.ok) {
            std::fprintf(stderr, "FAIL assemble %s: %s\n", spec.asmName, r.error.c_str());
            ++fails;
            continue;
        }
        const auto sigNeedle = std::string(spec.signature);
        const auto it = std::search(r.sys.begin(), r.sys.end(),
            sigNeedle.begin(), sigNeedle.end());
        if (it == r.sys.end()) {
            std::fprintf(stderr, "FAIL %s missing signature %s\n", spec.sysName, spec.signature);
            ++fails;
            continue;
        }
        const fs::path outPath = outDir / spec.sysName;
        if (!writeFile(outPath, r.sys)) {
            std::fprintf(stderr, "FAIL write %s\n", outPath.string().c_str());
            ++fails;
            continue;
        }
        std::printf("OK %s %zu bytes (AMMOASM)\n", spec.sysName, r.sys.size());
    }
    return fails ? 1 : 0;
}