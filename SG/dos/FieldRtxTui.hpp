#pragma once

// RTX modal TUI helpers — line editor keys and active-modal pump dispatch.

#include <cstdint>
#include <string>

namespace FieldRtxTui {

inline bool handleLineEditKey(std::uint16_t key, std::string& buf, std::size_t maxLen) noexcept {
    switch (key & 0xFFu) {
    case 0x08:
        if (!buf.empty()) buf.pop_back();
        return true;
    case 0x0D:
        return true;
    case 0x1B:
        return true;
    default:
        if ((key & 0xFF00u) == 0) {
            const char ch = static_cast<char>(key & 0xFFu);
            if (ch >= 32 && ch < 127 && buf.size() < maxLen) buf += ch;
        }
        return true;
    }
}

template<typename PaintFn, typename KeyFn, typename MouseFn>
inline void pumpModal(bool& active, std::uint8_t* ram, PaintFn paint, KeyFn onKey,
                        MouseFn onMouse, std::uint32_t hostKey, bool keyDown) noexcept {
    if (!active || !ram) return;
    onMouse(ram);
    const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
    if (keyDown && hostKey != 0u)
        onKey(ram, key);
    else
        paint(ram);
}

} // namespace FieldRtxTui