#pragma once

// AMMOLINK v3 — single + multi-module AMMO link → DOS .COM.

#include "FieldAmmoAsm.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoObj.hpp"
#include "FieldDos.hpp"

#include <cstring>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace FieldAmmoLink {

struct Result {
    bool ok = false;
    std::string error;
    std::vector<std::uint8_t> com;
    std::uint32_t entryOffset = 0;
};

inline Result linkModule(const FieldAmmoObj::Module& m) {
    Result r;
    r.com.resize(m.code.size() + m.data.size());
    if (!m.code.empty()) std::memcpy(r.com.data(), m.code.data(), m.code.size());
    if (!m.data.empty()) std::memcpy(r.com.data() + m.code.size(), m.data.data(), m.data.size());
    r.ok = true;
    return r;
}

inline Result linkObjects(const std::vector<std::vector<std::uint8_t>>& blobs) {
    Result r;
    if (blobs.empty()) { r.error = "no objects"; return r; }
    std::vector<FieldAmmoObj::Module> mods;
    mods.reserve(blobs.size());
    for (const auto& b : blobs) {
        FieldAmmoObj::Module m;
        if (!FieldAmmoObj::unpack(b, m)) {
            r.error = "invalid AMMO object";
            return r;
        }
        mods.push_back(std::move(m));
    }

    std::vector<std::uint8_t> code;
    std::vector<std::uint8_t> data;
    std::unordered_map<std::string, std::uint32_t> globalExports;

    std::size_t codeBase = 0;
    for (std::size_t mi = 0; mi < mods.size(); ++mi) {
        const auto& m = mods[mi];
        for (const auto& e : m.exports) {
            std::string n(e.name, strnlen(e.name, 12));
            globalExports[n] = static_cast<std::uint32_t>(codeBase + e.codeOff);
        }
        if (mi + 1 == mods.size()) {
            const std::uint32_t relEntry = (m.entryIp >= m.org) ? (m.entryIp - m.org) : 0u;
            r.entryOffset = static_cast<std::uint32_t>(codeBase + relEntry);
        }
        code.insert(code.end(), m.code.begin(), m.code.end());
        codeBase += m.code.size();
    }
    for (const auto& m : mods) data.insert(data.end(), m.data.begin(), m.data.end());

    std::size_t modCodeBase = 0;
    std::size_t priorCode = 0;
    std::size_t priorData = 0;
    for (const auto& m : mods) {
        const std::uint32_t dataAdjust = static_cast<std::uint32_t>(priorCode + priorData);
        for (const auto& df : m.dataFixups) {
            const std::size_t fixSite = modCodeBase + df;
            if (fixSite + 1u >= code.size()) {
                r.error = "data fixup out of range";
                return r;
            }
            std::uint16_t imm = static_cast<std::uint16_t>(code[fixSite])
                | static_cast<std::uint16_t>(static_cast<std::uint16_t>(code[fixSite + 1u]) << 8);
            imm = static_cast<std::uint16_t>(imm + dataAdjust);
            code[fixSite] = static_cast<std::uint8_t>(imm & 0xFFu);
            code[fixSite + 1u] = static_cast<std::uint8_t>((imm >> 8) & 0xFFu);
        }
        for (const auto& im : m.imports) {
            std::string n(im.name, strnlen(im.name, 12));
            const auto it = globalExports.find(n);
            if (it == globalExports.end()) {
                r.error = "unresolved import: " + n;
                return r;
            }
            const std::uint32_t fixSite = static_cast<std::uint32_t>(modCodeBase + im.codeOff);
            const std::int32_t rel = static_cast<std::int32_t>(it->second)
                - static_cast<std::int32_t>(fixSite + 2u);
            if (rel < -32768 || rel > 32767) {
                r.error = "import rel out of range: " + n;
                return r;
            }
            code[fixSite] = static_cast<std::uint8_t>(rel & 0xFFu);
            code[fixSite + 1u] = static_cast<std::uint8_t>((rel >> 8) & 0xFFu);
        }
        modCodeBase += m.code.size();
        priorCode += m.code.size();
        priorData += m.data.size();
    }

    r.com.resize(code.size() + data.size());
    if (!code.empty()) std::memcpy(r.com.data(), code.data(), code.size());
    if (!data.empty()) std::memcpy(r.com.data() + code.size(), data.data(), data.size());
    r.ok = true;
    return r;
}

inline Result linkObject(const std::vector<std::uint8_t>& obj) {
    return linkObjects({obj});
}

inline Result linkPath(const char* objPath) {
    std::vector<std::uint8_t> obj;
    if (!FieldAmmoFat::readFile(objPath, obj) || obj.empty())
        return Result{false, "cannot read object", {}};
    return linkObject(obj);
}

inline Result linkPaths(const std::vector<std::string>& paths) {
    std::vector<std::vector<std::uint8_t>> blobs;
    for (const auto& p : paths) {
        std::vector<std::uint8_t> obj;
        if (!FieldAmmoFat::readFile(p.c_str(), obj) || obj.empty())
            return Result{false, "cannot read " + p, {}};
        blobs.push_back(std::move(obj));
    }
    return linkObjects(blobs);
}

inline Result linkAndWrite(const char* objPath, const char* comPath) {
    auto r = linkPath(objPath);
    if (!r.ok) return r;
    if (!FieldAmmoFat::writeRootFile(comPath, r.com.data(), r.com.size())) {
        r.ok = false;
        r.error = "AMMOFAT write failed";
        r.com.clear();
    }
    return r;
}

} // namespace FieldAmmoLink