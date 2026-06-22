#pragma once

// Minimal x86_64 ELF64 loader for Field Die guest KILROY programs.

#include "FieldKilroyCompat.hpp"

#include "FieldPlatform.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace FieldKilroy {

namespace Elf64 {

constexpr std::uint8_t  EI_MAG0 = 0;
constexpr std::uint8_t  EI_MAG1 = 1;
constexpr std::uint8_t  EI_MAG2 = 2;
constexpr std::uint8_t  EI_MAG3 = 3;
constexpr std::uint8_t  EI_CLASS = 4;
constexpr std::uint8_t  ELFCLASS64 = 2;
constexpr std::uint16_t ET_EXEC = 2;
constexpr std::uint16_t EM_X86_64 = 62;
constexpr std::uint32_t PT_LOAD = 1;

struct Ehdr {
    std::uint8_t  e_ident[16];
    std::uint16_t e_type;
    std::uint16_t e_machine;
    std::uint32_t e_version;
    std::uint64_t e_entry;
    std::uint64_t e_phoff;
    std::uint64_t e_shoff;
    std::uint32_t e_flags;
    std::uint16_t e_ehsize;
    std::uint16_t e_phentsize;
    std::uint16_t e_phnum;
    std::uint16_t e_shentsize;
    std::uint16_t e_shnum;
    std::uint16_t e_shstrndx;
};

struct Phdr {
    std::uint32_t p_type;
    std::uint32_t p_flags;
    std::uint64_t p_offset;
    std::uint64_t p_vaddr;
    std::uint64_t p_paddr;
    std::uint64_t p_filesz;
    std::uint64_t p_memsz;
    std::uint64_t p_align;
};

} // namespace Elf64

struct LoadedImage {
    bool ok = false;
    std::uint64_t entry = 0;
    std::uint64_t stackTop = 0;
    std::uint32_t loadBase = 0;
    char error[128]{};
};

inline constexpr std::uint32_t LINUX_LOAD_BASE = 0x00200000u;
inline constexpr std::uint32_t LINUX_STACK_TOP = 0x00F00000u;
inline constexpr std::uint32_t LINUX_STACK_SIZE = 256u * 1024u;

inline bool isElf64(const std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size < 16) return false;
    return data[Elf64::EI_MAG0] == 0x7F && data[Elf64::EI_MAG1] == 'E'
        && data[Elf64::EI_MAG2] == 'L' && data[Elf64::EI_MAG3] == 'F'
        && data[Elf64::EI_CLASS] == Elf64::ELFCLASS64;
}

inline LoadedImage loadIntoGuest(std::uint8_t* ram, const std::uint8_t* data,
                                 std::size_t size) noexcept {
    LoadedImage out{};
    if (!ram || !data) {
        std::snprintf(out.error, sizeof(out.error), "null ram or image");
        return out;
    }
    if (!isElf64(data, size)) {
        std::snprintf(out.error, sizeof(out.error), "not ELF64");
        return out;
    }

    const auto* eh = reinterpret_cast<const Elf64::Ehdr*>(data);
    if (eh->e_type != Elf64::ET_EXEC && eh->e_type != 3) {
        std::snprintf(out.error, sizeof(out.error), "unsupported ELF type %u", eh->e_type);
        return out;
    }
    if (eh->e_machine != Elf64::EM_X86_64) {
        std::snprintf(out.error, sizeof(out.error), "not x86_64");
        return out;
    }

    const std::uint32_t base = LINUX_LOAD_BASE;
    for (std::uint16_t i = 0; i < eh->e_phnum; ++i) {
        const auto* ph = reinterpret_cast<const Elf64::Phdr*>(
            data + eh->e_phoff + static_cast<std::uint64_t>(i) * eh->e_phentsize);
        if (ph->p_type != Elf64::PT_LOAD) continue;
        const std::uint64_t vaddr = ph->p_vaddr;
        const std::uint32_t dest = base + static_cast<std::uint32_t>(vaddr & 0xFFFFFu);
        if (dest + ph->p_memsz > FieldPlatform::GUEST_RAM_BYTES) {
            std::snprintf(out.error, sizeof(out.error), "segment overflow at %08x", dest);
            return out;
        }
        if (ph->p_filesz)
            std::memcpy(ram + dest, data + ph->p_offset, static_cast<std::size_t>(ph->p_filesz));
        if (ph->p_memsz > ph->p_filesz)
            std::memset(ram + dest + ph->p_filesz, 0,
                        static_cast<std::size_t>(ph->p_memsz - ph->p_filesz));
    }

    out.ok = true;
    out.loadBase = base;
    out.entry = base + static_cast<std::uint32_t>(eh->e_entry & 0xFFFFFu);
    out.stackTop = LINUX_STACK_TOP;
    activate(1);
    std::fprintf(stderr,
        "[FieldKilroy] ELF loaded entry=0x%llx stack=0x%llx base=0x%08x\n",
        static_cast<unsigned long long>(out.entry),
        static_cast<unsigned long long>(out.stackTop),
        out.loadBase);
    return out;
}

inline LoadedImage loadFromVector(std::uint8_t* ram,
                                  const std::vector<std::uint8_t>& image) noexcept {
    return loadIntoGuest(ram, image.data(), image.size());
}

} // namespace FieldKilroy