#pragma once

// RTX-DOS 7.0 embedded boot slice — linked when FIELD_DOS_EMBED_HD is defined.

#include <cstddef>
#include <cstdint>

namespace FieldDosEmbed {

const unsigned char* slice() noexcept;
std::size_t sliceSize() noexcept;
std::size_t fullHdSize() noexcept;

} // namespace FieldDosEmbed