// Host optical drive bridge — USB/DVD block device for MSCDEX.

#include "FieldCdRomHost.hpp"

#include <cstdio>
#include <cstring>

#if defined(__linux__)

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

bool sameDevice(const char* a, const char* b) noexcept {
    if (!a || !b) return false;
    struct stat sa{}, sb{};
    if (stat(a, &sa) != 0 || stat(b, &sb) != 0) return false;
    return sa.st_rdev == sb.st_rdev;
}

bool parsePvd(const std::uint8_t* pvd, std::uint32_t& sectors,
              char* labelOut, std::size_t labelCap) noexcept {
    if (!pvd || pvd[0] != 1u || std::memcmp(pvd + 1, "CD001", 5) != 0)
        return false;
    sectors = static_cast<std::uint32_t>(
        static_cast<unsigned>(pvd[80])
        | (static_cast<unsigned>(pvd[81]) << 8)
        | (static_cast<unsigned>(pvd[82]) << 16)
        | (static_cast<unsigned>(pvd[83]) << 24));
    if (sectors == 0u) return false;
    if (labelOut && labelCap > 0) {
        char lbl[33]{};
        std::memcpy(lbl, pvd + 40, 32);
        for (int i = 31; i >= 0; --i) {
            if (lbl[i] == ' ') lbl[i] = '\0';
            else break;
        }
        std::snprintf(labelOut, labelCap, "%s", lbl[0] ? lbl : "HOST_CD");
    }
    return true;
}

bool probeDevice(const char* path, int& outFd, std::uint32_t& outSectors,
                 char* labelOut, std::size_t labelCap) noexcept {
    if (!path || !path[0]) return false;
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    std::uint8_t pvd[2048]{};
    const off_t off = static_cast<off_t>(16) * 2048;
    const ssize_t n = pread(fd, pvd, sizeof pvd, off);
    if (n != static_cast<ssize_t>(sizeof pvd) || !parsePvd(pvd, outSectors, labelOut, labelCap)) {
        close(fd);
        return false;
    }
    outFd = fd;
    return true;
}

} // namespace

namespace FieldCdRomHost {

bool readSector2048(int fd, std::uint32_t lba, std::uint8_t* out2048) noexcept {
    if (fd < 0 || !out2048) return false;
    const off_t off = static_cast<off_t>(lba) * 2048;
    return pread(fd, out2048, 2048, off) == 2048;
}

bool mountDevice(const char* devPath, int& outFd, std::uint32_t& outSectors,
                 char* labelOut, std::size_t labelCap) noexcept {
    outFd = -1;
    outSectors = 0u;
    return probeDevice(devPath, outFd, outSectors, labelOut, labelCap);
}

bool detectOptical(char* pathOut, std::size_t pathCap) noexcept {
    if (!pathOut || pathCap == 0) return false;
    const char* candidates[] = {
        "/dev/cdrom",
        "/dev/dvd",
        "/dev/sr0",
        "/dev/sr1",
        "/dev/sr2",
        nullptr
    };
    std::vector<const char*> unique;
    for (int i = 0; candidates[i]; ++i) {
        bool dup = false;
        for (const char* prior : unique) {
            if (sameDevice(candidates[i], prior)) {
                dup = true;
                break;
            }
        }
        if (!dup)
            unique.push_back(candidates[i]);
    }
    int fd = -1;
    std::uint32_t sectors = 0u;
    char label[33]{};
    for (const char* path : unique) {
        if (probeDevice(path, fd, sectors, label, sizeof label)) {
            std::snprintf(pathOut, pathCap, "%s", path);
            close(fd);
            return true;
        }
    }
    return false;
}

} // namespace FieldCdRomHost

#else // !__linux__

namespace FieldCdRomHost {

bool readSector2048(int /*fd*/, std::uint32_t /*lba*/, std::uint8_t* /*out2048*/) noexcept {
    return false;
}

bool mountDevice(const char* /*devPath*/, int& outFd, std::uint32_t& outSectors,
                 char* /*labelOut*/, std::size_t /*labelCap*/) noexcept {
    outFd = -1;
    outSectors = 0u;
    return false;
}

bool detectOptical(char* /*pathOut*/, std::size_t /*pathCap*/) noexcept {
    return false;
}

} // namespace FieldCdRomHost

#endif