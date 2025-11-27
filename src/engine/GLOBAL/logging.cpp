#include <cstdint>
#include <ctime>
#include <unistd.h> 

uint64_t kStone1 = 0x517F3D6A9B8C4E2FULL ^ __builtin_ia32_rdtsc() ^ reinterpret_cast<uintptr_t>(&kStone1);
uint64_t kStone2 = 0xA1B2C3D4E5F60789ULL ^ getpid() ^ (uint64_t)time(nullptr);