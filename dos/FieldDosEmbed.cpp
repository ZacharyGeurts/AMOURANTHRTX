// RTX-DOS 7.0 embedded boot slice — linked via objcopy from dos_slice.bin.

#include "FieldDosEmbed.hpp"
#include "FieldPlatform.hpp"

#include <cstddef>
#include <cstdint>

#if defined(FIELD_DOS_EMBED_HD) && defined(FIELD_DOS_EMBED_LINKED)

extern "C" {
extern const std::uint8_t _binary_dos_slice_bin_start[];
extern const std::uint8_t _binary_dos_slice_bin_end[];
}

namespace FieldDosEmbed {

const unsigned char* slice() noexcept {
    return _binary_dos_slice_bin_start;
}

std::size_t sliceSize() noexcept {
    return static_cast<std::size_t>(
        _binary_dos_slice_bin_end - _binary_dos_slice_bin_start);
}

std::size_t fullHdSize() noexcept { return static_cast<std::size_t>(FieldPlatform::HD_SIZE_BYTES); }

} // namespace FieldDosEmbed

#else

namespace FieldDosEmbed {

const unsigned char* slice() noexcept { return nullptr; }
std::size_t sliceSize() noexcept { return 0; }
std::size_t fullHdSize() noexcept { return 0; }

} // namespace FieldDosEmbed

#endif