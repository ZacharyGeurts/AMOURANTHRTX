// Host libx86emu throughput — AMOURANTHRTX CPU backend vs DOSBox-class emulators.
// Build: g++-14 -std=c++20 -O3 -I Navigator/engine -I third_party/libx86emu/include \
//   scripts/bench_x86_host.cpp build/libx86emu.a -o build/bin/Linux/bench_x86_host

#include "FieldAmmoFat.hpp"
#include "FieldBios.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

static bool hasPrompt(const std::uint8_t* ram) {
    for (int row = 0; row < 25; ++row) {
        for (int col = 0; col < 76; ++col) {
            const std::uint32_t off = 0xB8000u + static_cast<std::uint32_t>((row * 80 + col) * 2);
            const char d = static_cast<char>(ram[off]);
            if ((d == 'C' || d == 'A') && ram[off + 2] == ':' && ram[off + 4] == '\\' && ram[off + 6] == '>')
                return true;
        }
    }
    return false;
}

int main() {
    constexpr int kBenchFrames = 300;
    constexpr int kBenchMaxSec = 20;
    constexpr int kBootMaxRounds = 120;

    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);

    FieldDosConfig::applyPreset(FieldDosConfig::Preset::Full);
    const std::uint32_t kCyclesPerFrame = FieldDosConfig::cfg.cyclesRun;

    const auto floppy = FieldDos::defaultImagePath(".");
    const auto hd = FieldDos::defaultHdPath(".");
    if (!FieldDos::loadHdImage(hd)) {
        std::fprintf(stderr, "FAIL hd load %s\n", hd.string().c_str());
        return 1;
    }
    if (!FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, floppy)) {
        std::fprintf(stderr, "FAIL floppy load\n");
        return 1;
    }
    if (!FieldDos::mirrorHdToGuestGpu(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4)) {
        std::fprintf(stderr, "FAIL hd mirror\n");
        return 1;
    }
    FieldAmmoFat::mount();

    auto* ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldX86Emu::Ctx ctx{};

    auto boot0 = Clock::now();
    bool settled = false;
    for (int round = 0; round < kBootMaxRounds; ++round) {
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 8'388'608u);
        if (FieldBios::guestBootSettled || hasPrompt(ram)) {
            settled = true;
            break;
        }
    }
    auto boot1 = Clock::now();
    const double bootMs = std::chrono::duration<double, std::milli>(boot1 - boot0).count();

    if (!settled) {
        std::fprintf(stderr, "FAIL boot not settled in %d rounds (ticks=%llu)\n",
            kBootMaxRounds, static_cast<unsigned long long>(ctx.ticks));
        return 1;
    }

    // Keep the guest busy (idle C:\> + HLT can stall TSC and look like a hang).
    static const std::uint16_t kWakeKeys[] = {
        'D', 'I', 'R', 0x0D, 'D', 'I', 'R', 0x0D,
    };
    for (std::uint16_t key : kWakeKeys)
        FieldBios::enqueueHostKey(key);

    const std::uint64_t ticksBefore = ctx.ticks;
    FieldX86Emu::benchForceExecute = true;
    auto bench0 = Clock::now();
    const auto benchDeadline = bench0 + std::chrono::seconds(kBenchMaxSec);
    int framesDone = 0;
    for (int f = 0; f < kBenchFrames && Clock::now() < benchDeadline; ++f) {
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, kCyclesPerFrame);
        ++framesDone;
    }
    auto bench1 = Clock::now();
    FieldX86Emu::benchForceExecute = false;

    if (framesDone == 0) {
        std::fprintf(stderr, "FAIL bench produced 0 frames in %ds\n", kBenchMaxSec);
        return 1;
    }

    const double benchSec = std::chrono::duration<double>(bench1 - bench0).count();
    const std::uint64_t ticksRan = ctx.ticks - ticksBefore;
    const double cyclesPerSec = static_cast<double>(ticksRan) / benchSec;
    const double framesPerSec = static_cast<double>(framesDone) / benchSec;
    const double msPerFrame = benchSec * 1000.0 / static_cast<double>(framesDone);

    std::printf("METRIC backend=host_libx86emu\n");
    std::printf("METRIC cycles_per_frame=%u\n", kCyclesPerFrame);
    std::printf("METRIC bench_frames=%d\n", framesDone);
    std::printf("METRIC boot_ms=%.1f\n", bootMs);
    std::printf("METRIC wall_sec=%.6f\n", benchSec);
    std::printf("METRIC cycles_total=%llu\n", static_cast<unsigned long long>(ticksRan));
    std::printf("METRIC cycles_per_sec=%.0f\n", cyclesPerSec);
    std::printf("METRIC frames_per_sec=%.2f\n", framesPerSec);
    std::printf("METRIC ms_per_frame=%.3f\n", msPerFrame);
    std::printf("METRIC effective_mips=%.2f\n", cyclesPerSec / 1'000'000.0);
    std::printf("METRIC cpu_util_est=100.0\n");
    std::printf("METRIC gpu_util_est=0.0\n");
    std::printf("OK bench_x86_host\n");
    return 0;
}