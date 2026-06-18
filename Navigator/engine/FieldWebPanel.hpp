#pragma once

// Field Web Panel — in-panel browser (no external host window). Renders into VGA FB.

#include <cstdint>
#include <string>

namespace FieldWebPanel {

constexpr std::uint32_t FB_BASE = 0x000A0000u;
constexpr int FB_W = 320;
constexpr int FB_H = 200;

inline bool active = false;
inline bool loading = false;
inline bool urlBarFocus = false;
inline std::string currentUrl;
inline std::string pageTitle;
inline std::string statusLine;
inline int scrollY = 0;

void open(std::uint8_t* ram, const char* url);
void close(std::uint8_t* ram) noexcept;
void navigate(const char* url);
void pump(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept;
void tick(std::uint8_t* ram, const bool* keys) noexcept;
void packDataBus(std::uint32_t* bus) noexcept;

} // namespace FieldWebPanel