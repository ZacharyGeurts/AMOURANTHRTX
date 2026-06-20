#pragma once

#include <cstdint>

namespace FieldCdRomHost {

bool mountDevice(const char* devPath, int& outFd, std::uint32_t& outSectors,
                 char* labelOut, std::size_t labelCap) noexcept;
bool readSector2048(int fd, std::uint32_t lba, std::uint8_t* out2048) noexcept;
bool detectOptical(char* pathOut, std::size_t pathCap) noexcept;

} // namespace FieldCdRomHost