#pragma once

#include <cstddef>
#include <cstdint>

namespace FieldPlatform {

constexpr std::uint32_t GUEST_RAM_BYTES       = 0x04000000u;   // 64 MiB Field Die fast RAM (GPU)
constexpr std::uint64_t REPORTED_RAM_BYTES   = 0x100000000ull; // 4 GiB OS-visible RAM (BIOS/E820)
constexpr std::uint32_t REPORTED_RAM_KB      = 4u * 1024u * 1024u;
constexpr std::uint64_t HD_SIZE_BYTES         = 0x100000000ull; // 4 GiB logical C:
constexpr std::uint32_t HD_SIZE_BYTES32       = 0xFFFFF000u;    // uint32-safe (~4095 MiB UI)
constexpr std::uint32_t HD_PART_LBA         = 1u;
constexpr std::uint32_t HD_MIRROR_BYTE      = 0x01000000u; // C: hot mirror in Field Die RAM
constexpr std::uint32_t HD_MIRROR_MAX       = 32u * 1024u * 1024u;
constexpr std::uint32_t XMS_POOL_BYTE       = 0x03000000u; // after HD mirror window
constexpr std::uint32_t XMS_POOL_BYTES      = 8u * 1024u * 1024u; // 8 MiB fast XMS in die
constexpr std::uint32_t GPU_DATA_BYTE       = 0x03800000u; // AAFL staging (below 64 MiB cap)
constexpr std::uint16_t CONVENTIONAL_KB     = 640u;
constexpr std::uint16_t UMB_KB              = 384u;        // A0000h–EFFFFh (VGA/ROM carve-outs excluded)
constexpr std::uint16_t UMB_FREE_KB         = 352u;      // ~32K RTXHOST resident in upper blocks
constexpr std::uint16_t HMA_KB              = 64u;         // FFFF0h–FFFFFh with DOS=HIGH
constexpr std::uint16_t EXTENDED_KB         = 0xFC00u;     // INT 15 AH=88 (~63 MiB below 16M)
constexpr std::uint32_t XMS_KB              = REPORTED_RAM_KB - CONVENTIONAL_KB;
constexpr std::uint32_t VGA_PAL_RAM_BYTE  = 0x00098000u;

// RAID-0 stripe geometry — host + VRAM spill tiers, contiguous logical facade.
constexpr std::uint32_t RAID_STRIPE_BYTES   = 64u * 1024u;
constexpr std::uint32_t RAID_TICK_BUDGET    = 256u * 1024u; // bytes/frame background sync
constexpr std::uint32_t RAID_JOURNAL_CAP    = 4u * 1024u * 1024u;
constexpr std::uint32_t SPILL_RAM_BASE      = GUEST_RAM_BYTES; // linear spill above die

constexpr std::size_t DIE_HEADER_UINTS = 39u;
constexpr std::size_t DIE_RAM_UINTS    = GUEST_RAM_BYTES / 4u;
constexpr std::size_t DIE_TAIL_UINTS   = 1024u + 1024u + 2048u + 2048u + 1024u + 3u;
constexpr std::size_t FIELD_X86_DIE_UINTS = DIE_HEADER_UINTS + DIE_RAM_UINTS + DIE_TAIL_UINTS;
constexpr std::size_t FIELD_X86_DIE_CYCLE_OFFSET =
    (FIELD_X86_DIE_UINTS - 3u) * sizeof(std::uint32_t);

} // namespace FieldPlatform