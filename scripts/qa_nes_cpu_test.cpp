// QA: NES 6502 frame budget + opcode sanity (no full GPU path).
#include "CHIPS/Nes/FieldNes.hpp"
#include "CHIPS/Nes/FieldNesAccuracy.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace Nes = FieldChips::Nes;

static std::vector<std::uint8_t> makeMinimalInes() {
    std::vector<std::uint8_t> rom(16 + 16384 + 8192, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = 1;  // 16 KiB PRG
    rom[5] = 1;  // 8 KiB CHR
    rom[6] = 1;  // vertical mirror
    rom[7] = 0;
    // Reset vector at $FFFC -> $8000
    rom[16 + 16384 - 4] = 0x00;
    rom[16 + 16384 - 3] = 0x80;
    // Infinite loop at $8000: JMP $8000
    rom[16] = 0x4C;
    rom[17] = 0x00;
    rom[18] = 0x80;
    return rom;
}

static bool test_frame_budget() {
    Nes::State s{};
    const auto rom = makeMinimalInes();
    if (!Nes::loadInes(s, rom.data(), rom.size())) {
        std::fprintf(stderr, "FAIL loadInes minimal cart\n");
        return false;
    }
    const int before = s.cpu.totalCycles;
    Nes::runFrameCpu(s, Nes::ntscFrameCpuCycles());
    const int ran = s.cpu.totalCycles - before;
    if (ran < 29776 || ran > 29784) {
        std::fprintf(stderr, "FAIL NTSC frame budget ran=%d (want ~29780)\n", ran);
        return false;
    }
    if (s.ppu.frame < 1) {
        std::fprintf(stderr, "FAIL PPU frame counter not advanced\n");
        return false;
    }
    std::printf("OK NTSC frame budget cpu=%d ppu.frame=%d\n", ran, s.ppu.frame);
    return true;
}

static bool test_accuracy_tier() {
    Nes::State s{};
    const auto rom = makeMinimalInes();
    if (!Nes::loadInes(s, rom.data(), rom.size())) return false;
    Nes::gAccuracyStats = {};
    for (int i = 0; i < 100; ++i)
        Nes::runAccuracyFrame(s, Nes::AccuracyTier::FrameSync);
    if (Nes::gAccuracyStats.frames != 100u) {
        std::fprintf(stderr, "FAIL accuracy stats frames=%u\n", Nes::gAccuracyStats.frames);
        return false;
    }
    if (s.ppu.frame < 100) {
        std::fprintf(stderr, "FAIL expected >=100 PPU frames got %d\n", s.ppu.frame);
        return false;
    }
    std::printf("OK accuracy tier 100 frames ppu.frame=%d totalCpu=%llu\n",
        s.ppu.frame, static_cast<unsigned long long>(s.cpu.totalCycles));
    return true;
}

static bool test_bmi_branch() {
    Nes::State s{};
    s.cpu.pc = 0x0200;
    s.cpu.status = 0xA4; // N set
    s.ram[0x200] = 0x30; // BMI
    s.ram[0x201] = 0x05; // +5
    s.ram[0x206] = 0xEA; // NOP landing
    const int cyc = Nes::stepCpu(s);
    if (s.cpu.pc != 0x0207) {
        std::fprintf(stderr, "FAIL BMI branch pc=0x%04x (want 0x0207)\n", s.cpu.pc);
        return false;
    }
    if (cyc < 3) {
        std::fprintf(stderr, "FAIL BMI cycles=%d\n", cyc);
        return false;
    }
    std::printf("OK BMI branch pc=0x%04x cycles=%d\n", s.cpu.pc, cyc);
    return true;
}

static bool test_cmp_carry() {
    Nes::State s{};
    s.cpu.pc = 0x0300;
    s.cpu.a = 0x50;
    s.ram[0x300] = 0xC9; // CMP #$40
    s.ram[0x301] = 0x40;
    Nes::stepCpu(s);
    if ((s.cpu.status & 0x01) == 0) {
        std::fprintf(stderr, "FAIL CMP carry not set (status=0x%02x)\n", s.cpu.status);
        return false;
    }
    std::printf("OK CMP carry set (status=0x%02x)\n", s.cpu.status);
    return true;
}

static bool test_nestest_rom(const char* path) {
    if (!path || !path[0]) {
        std::printf("SKIP nestest ROM (set AMOURANTHRTX_NESTEST_ROM=path)\n");
        return true;
    }
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::fprintf(stderr, "SKIP nestest open failed: %s\n", path);
        return true;
    }
    std::vector<std::uint8_t> data;
    std::uint8_t buf[4096];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        data.insert(data.end(), buf, buf + n);
    std::fclose(f);
    Nes::State s{};
    if (!Nes::loadInes(s, data.data(), data.size())) {
        std::fprintf(stderr, "FAIL nestest loadInes\n");
        return false;
    }
    for (int i = 0; i < 30000; ++i)
        Nes::runAccuracyFrame(s, Nes::AccuracyTier::FrameSync);
    std::printf("OK nestest soak 30000 frames totalCpu=%llu ppu.frame=%d\n",
        static_cast<unsigned long long>(s.cpu.totalCycles), s.ppu.frame);
    return true;
}

int main() {
    bool ok = test_frame_budget()
        && test_accuracy_tier()
        && test_bmi_branch()
        && test_cmp_carry()
        && test_nestest_rom(std::getenv("AMOURANTHRTX_NESTEST_ROM"));
    if (!ok) return 1;
    std::printf("NES CPU accuracy QA PASSED\n");
    return 0;
}