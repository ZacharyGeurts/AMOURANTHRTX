// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// AMOURANTH RTX — APOCALYPSE FINAL v10.3
// FULLY COMPILING — PURE EMPIRE - Inspired by Ellie Fier
// =============================================================================

#include <cstdint>
#include <ctime>
#include <unistd.h> 

double totalTime_ = 0.0;  // One true definition — starts at 0

uint64_t kStone1 = 0x9E37AF18C64D8A17UL ^ __builtin_ia32_rdtsc() ^ reinterpret_cast<uintptr_t>(&kStone1);
uint64_t kStone2 = 0xE4F8B29D71A3C56CUL ^ getpid() ^ (uint64_t)time(nullptr);