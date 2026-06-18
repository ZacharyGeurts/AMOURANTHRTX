#pragma once

// AMMOSYS — assemble + link RTXFL field-layer .SYS drivers (DevKit / host bake).

#include "FieldAmmoAsm.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoLink.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace FieldAmmoSys {

constexpr std::uint8_t kRtxflMagic[] = {'R', 'T', 'X', 'F', 'L'};
constexpr std::size_t kInitOff = 48;

struct Result {
    bool ok = false;
    std::string error;
    std::vector<std::uint8_t> sys;
};

inline bool verifyRtxfl(const std::vector<std::uint8_t>& sys) noexcept {
    if (sys.size() < 30) return false;
    if (sys[0] != 0xEBu) return false;
    return std::memcmp(sys.data() + 19, kRtxflMagic, sizeof(kRtxflMagic)) == 0;
}

inline Result linkFlat(const std::vector<std::uint8_t>& obj, std::size_t minSize) {
    Result r;
    auto linked = FieldAmmoLink::linkObject(obj);
    if (!linked.ok) {
        r.error = linked.error;
        return r;
    }
    r.sys = std::move(linked.com);
    if (r.sys.size() < minSize)
        r.sys.resize(minSize, 0);
    if (!verifyRtxfl(r.sys)) {
        r.error = "missing RTXFL header — driver .ASM must include RTXFL device header";
        r.sys.clear();
        return r;
    }
    r.ok = true;
    return r;
}

inline Result assembleSysSource(const char* src, std::size_t len, std::size_t minSize = 160) {
    Result r;
    auto asmR = FieldAmmoAsm::assembleSource(src, len);
    if (!asmR.ok) {
        r.error = asmR.error;
        return r;
    }
    return linkFlat(asmR.object, minSize);
}

inline Result assembleSysFile(const char* path, std::size_t minSize = 160) {
    std::vector<std::uint8_t> src;
    if (!FieldAmmoFat::readFile(path, src) || src.empty())
        return Result{false, "cannot read driver source", {}};
    return assembleSysSource(reinterpret_cast<const char*>(src.data()), src.size(), minSize);
}

} // namespace FieldAmmoSys