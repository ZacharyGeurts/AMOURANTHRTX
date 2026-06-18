#pragma once

// RTX-AMMOS DevKit — unified toolchain dispatch (GPU shell + AMMOFAT).

#include "FieldAmmoAsm.hpp"
#include "FieldAmmoCc.hpp"
#include "FieldFieldCc.hpp"
#include "FieldAmmoDbg.hpp"
#include "FieldAmmoDecomp.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoLink.hpp"
#include "FieldAmmoRun.hpp"
#include "FieldAmmoSys.hpp"
#include "FieldAmmoToolchain.hpp"
#include "FieldAmmoZip.hpp"

#include <cstdlib>
#include <string>
#include <vector>

namespace FieldAmmoTools {

inline std::string dosPath(const std::vector<std::string>& args, std::size_t idx,
                           const char* defaultExt) {
    if (idx >= args.size()) return {};
    std::string p = args[idx];
    if (p.find(':') == std::string::npos && p.find('\\') == std::string::npos)
        p = "C:\\" + p;
    if (p.find('.') == std::string::npos && defaultExt) p += defaultExt;
    return p;
}

inline std::string swapExt(const std::string& path, const char* ext) {
    const auto slash = path.find_last_of('\\');
    const auto dot = path.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        return path.substr(0, dot) + ext;
    return path + ext;
}

struct ToolResult {
    bool ok = false;
    std::string message;
};

inline ToolResult asmFile(const std::vector<std::string>& args) {
    const std::string src = dosPath(args, 1, ".ASM");
    if (src.empty()) return {false, "usage: AMMOASM file.asm"};
    auto r = FieldAmmoAsm::assemblePath(src.c_str());
    if (!r.ok) return {false, r.error};
    const std::string obj = swapExt(src, ".OBJ");
    if (!FieldAmmoFat::writeRootFile(obj.c_str(), r.object.data(), r.object.size()))
        return {false, "AMMOFAT write " + obj + " failed"};
    return {true, "assembled → " + obj};
}

inline ToolResult sysFile(const std::vector<std::string>& args) {
    const std::string src = dosPath(args, 1, ".ASM");
    if (src.empty()) return {false, "usage: AMMOSYS file.asm [minsize]"};
    std::size_t minSize = 160;
    if (args.size() >= 3) minSize = static_cast<std::size_t>(std::strtoul(args[2].c_str(), nullptr, 10));
    auto r = FieldAmmoSys::assembleSysFile(src.c_str(), minSize);
    if (!r.ok) return {false, r.error};
    const std::string sys = swapExt(src, ".SYS");
    if (!FieldAmmoFat::writeRootFile(sys.c_str(), r.sys.data(), r.sys.size()))
        return {false, "AMMOFAT write " + sys + " failed"};
    return {true, "driver → " + sys + " (" + std::to_string(r.sys.size()) + " bytes)"};
}

inline ToolResult linkFile(const std::vector<std::string>& args) {
    if (args.size() < 2) return {false, "usage: AMMOLINK a.obj [b.obj ...]"};
    std::vector<std::string> paths;
    for (std::size_t i = 1; i < args.size(); ++i) {
        std::string p = args[i];
        if (p.find(':') == std::string::npos && p.find('\\') == std::string::npos)
            p = "C:\\" + p;
        if (p.find('.') == std::string::npos) p += ".OBJ";
        paths.push_back(p);
    }
    FieldAmmoLink::Result r = (paths.size() == 1)
        ? FieldAmmoLink::linkPath(paths[0].c_str())
        : FieldAmmoLink::linkPaths(paths);
    if (!r.ok) return {false, r.error};
    const std::string com = swapExt(paths.back(), ".COM");
    if (!FieldAmmoFat::writeRootFile(com.c_str(), r.com.data(), r.com.size()))
        return {false, "AMMOFAT write " + com + " failed"};
    return {true, "linked " + std::to_string(paths.size()) + " module(s) → " + com
        + " (" + std::to_string(r.com.size()) + " bytes)"};
}

inline ToolResult fieldcFile(const std::vector<std::string>& args) {
    const std::string src = dosPath(args, 1, ".FLD");
    if (src.empty()) return {false, "usage: FIELDC file.fld"};
    auto r = FieldFieldCc::compilePath(src.c_str());
    if (!r.ok) return {false, r.error};
    const std::string obj = swapExt(src, ".OBJ");
    if (!FieldAmmoFat::writeRootFile(obj.c_str(), r.asmResult.object.data(), r.asmResult.object.size()))
        return {false, "AMMOFAT write " + obj + " failed"};
    return {true, "Field Compiler → " + obj};
}

inline ToolResult ccFile(const std::vector<std::string>& args) {
    const std::string src = dosPath(args, 1, ".C");
    if (src.empty()) return {false, "usage: AMMOCC file.c"};
    auto r = FieldAmmoCc::compilePath(src.c_str());
    if (!r.ok) return {false, r.error};
    const std::string obj = swapExt(src, ".OBJ");
    if (!FieldAmmoFat::writeRootFile(obj.c_str(), r.asmResult.object.data(), r.asmResult.object.size()))
        return {false, "AMMOFAT write " + obj + " failed"};
    return {true, "compiled → " + obj};
}

inline ToolResult zipFile(const std::vector<std::string>& args) {
    auto r = FieldAmmoZip::extractArgs(args);
    if (!r.ok) return {false, r.error};
    return {true, "extracted " + std::to_string(r.files) + " file(s), "
        + std::to_string(r.dirs) + " dir(s)"};
}

inline ToolResult decompFile(const std::vector<std::string>& args) {
    const std::string src = dosPath(args, 1, ".COM");
    if (src.empty()) return {false, "usage: AMMODECOMP file.com|.obj"};
    auto r = FieldAmmoDecomp::decompilePath(src.c_str());
    if (!r.ok) return {false, r.error};
    const std::string lst = swapExt(src, ".LST");
    if (!FieldAmmoFat::writeRootFile(lst.c_str(),
            reinterpret_cast<const std::uint8_t*>(r.listing.data()), r.listing.size()))
        return {false, "AMMOFAT write " + lst + " failed"};
    return {true, "decompiled → " + lst + " (" + std::to_string(r.listing.size()) + " bytes)"};
}

inline ToolResult buildAll(const std::vector<std::string>& args) {
    const std::string stem = (args.size() >= 2) ? dosPath(args, 1, nullptr) : "C:\\HELLO";
    std::string src = stem;
    if (src.find('.') == std::string::npos) src += ".ASM";
    auto a = FieldAmmoAsm::assemblePath(src.c_str());
    if (!a.ok) {
        std::string fld = swapExt(stem, ".FLD");
        auto f = FieldFieldCc::compilePath(fld.c_str());
        if (f.ok) {
            const std::string obj = swapExt(fld, ".OBJ");
            FieldAmmoFat::writeRootFile(obj.c_str(), f.asmResult.object.data(), f.asmResult.object.size());
            auto l = FieldAmmoLink::linkPath(obj.c_str());
            if (!l.ok) return {false, l.error};
            const std::string com = swapExt(obj, ".COM");
            FieldAmmoFat::writeRootFile(com.c_str(), l.com.data(), l.com.size());
            return {true, "BUILD " + com + " from " + fld};
        }
        std::string csrc = swapExt(src, ".C");
        auto c = FieldAmmoCc::compilePath(csrc.c_str());
        if (!c.ok) return {false, "build: " + a.error};
        const std::string obj = swapExt(csrc, ".OBJ");
        FieldAmmoFat::writeRootFile(obj.c_str(), c.asmResult.object.data(), c.asmResult.object.size());
        auto l = FieldAmmoLink::linkPath(obj.c_str());
        if (!l.ok) return {false, l.error};
        const std::string com = swapExt(obj, ".COM");
        FieldAmmoFat::writeRootFile(com.c_str(), l.com.data(), l.com.size());
        return {true, "BUILD " + com + " from " + csrc};
    }
    const std::string obj = swapExt(src, ".OBJ");
    FieldAmmoFat::writeRootFile(obj.c_str(), a.object.data(), a.object.size());
    auto l = FieldAmmoLink::linkPath(obj.c_str());
    if (!l.ok) return {false, l.error};
    const std::string com = swapExt(src, ".COM");
    FieldAmmoFat::writeRootFile(com.c_str(), l.com.data(), l.com.size());
    return {true, "BUILD " + com + " from " + src};
}

} // namespace FieldAmmoTools