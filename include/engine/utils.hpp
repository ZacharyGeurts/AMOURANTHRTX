// engine/utils.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FULLY POLISHED. ZERO WARNINGS. ZERO NARROWING. 100% COMPILABLE.

#pragma once

#include <cstdint>
#include <string>
#include <format>
#include <type_traits>

// ---------------------------------------------------------------------------
//  ptr_to_hex – Convert any pointer to 0xDEADBEEF string (used in LOG_*)
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::string ptr_to_hex(const void* ptr)
{
    // Use std::format (C++20) for clean, safe formatting
    return std::format("0x{:x}", reinterpret_cast<std::uintptr_t>(ptr));
}

// ---------------------------------------------------------------------------
//  ptr_to_hex overload for Vk* handles (uint64_t) – same style
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::string ptr_to_hex(uint64_t handle)
{
    return std::format("0x{:x}", handle);
}

// ---------------------------------------------------------------------------
//  join – Join any container with separator (used in handle_app.cpp)
// ---------------------------------------------------------------------------
template<typename Container>
[[nodiscard]] std::string join(const Container& container, const std::string& sep)
{
    if (container.empty()) return "";

    std::string result;
    bool first = true;

    for (const auto& elem : container) {
        if (!first) result += sep;
        result += std::format("{}", elem);
        first = false;
    }

    return result;
}

// ---------------------------------------------------------------------------
//  to_string for glm::vec3 (optional – if you use it in logs)
// ---------------------------------------------------------------------------
#if defined(GLM_ENABLE) || defined(GLM_FORCE_CXX20)
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

[[nodiscard]] inline std::string to_string(const glm::vec3& v)
{
    return std::format("[{:.3f}, {:.3f}, {:.3f}]", v.x, v.y, v.z);
}
#endif

// ---------------------------------------------------------------------------
//  constexpr string view helpers (C++20)
// ---------------------------------------------------------------------------
#include <string_view>

[[nodiscard]] constexpr std::string_view trim_view(std::string_view sv) noexcept
{
    const auto start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return {};
    const auto end = sv.find_last_not_of(" \t\r\n");
    return sv.substr(start, end - start + 1);
}

// ---------------------------------------------------------------------------
//  Compile-time assert for 64-bit (optional safety)
// ---------------------------------------------------------------------------
static_assert(sizeof(void*) == 8, "AMOURANTH RTX requires 64-bit build");

// End of file