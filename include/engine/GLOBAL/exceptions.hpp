// engine/GLOBAL/exceptions.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
#pragma once
#include <stdexcept>
#include <string>
#include <format>
#include <source_location>
#include <execinfo.h>
#include <cxxabi.h>
#include <cstdlib>

namespace Engine {

inline std::string demangle(const char* name) {
    int status = -1;
    char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    std::string result = (status == 0) ? demangled : name;
    free(demangled);
    return result;
}

inline std::string getBacktrace(int skip = 1) {
    void* callstack[128];
    int frames = backtrace(callstack, 128);
    char** symbols = backtrace_symbols(callstack, frames);

    std::string trace = "\n=== STACK TRACE ===\n";
    for (int i = skip; i < frames; ++i) {
        char* mangled = symbols[i];
        char* offset_begin = strchr(mangled, '(');
        char* offset_end = strchr(mangled, '+');
        if (offset_begin && offset_end && (offset_begin < offset_end)) {
            *offset_begin++ = '\0';
            *offset_end = '\0';
            std::string func = demangle(offset_begin);
            trace += std::format("  #{:<2} {} + {}\n", i - skip, func, offset_end + 1);
        } else {
            trace += std::format("  #{:<2} {}\n", i - skip, mangled);
        }
    }
    free(symbols);
    return trace;
}

class FatalError : public std::runtime_error {
public:
    FatalError(const std::string& msg,
               const std::source_location loc = std::source_location::current())
        : std::runtime_error("")
    {
        std::string full = std::format(
            "{}[FATAL ERROR]{} {}\n"
            "    → File: {}:{}\n"
            "    → Function: {}\n"
            "{}",
            Logging::Color::PLASMA_FUCHSIA, Logging::Color::RESET, msg,
            loc.file_name(), loc.line(),
            loc.function_name(),
            getBacktrace(2)  // skip this frame + runtime_error ctor
        );
        this->msg = full;
    }

    const char* what() const noexcept override {
        return msg.c_str();
    }

private:
    std::string msg;
};

} // namespace Engine

// MASTER MACRO — USE THIS EVERYWHERE
#define FATAL_THROW(msg) throw Engine::FatalError(msg)