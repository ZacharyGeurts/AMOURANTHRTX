#pragma once

// Field browser — hooks OS default browser (Firefox/Chrome) into the DOS panel; curl panel fallback.

#include "FieldBrowserHook.hpp"
#include "FieldDos.hpp"
#include "FieldWebPanel.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace FieldAmmoBrowser {

inline std::filesystem::path manifestPath() noexcept {
    return FieldDos::assetRoot() / "assets" / "browsers.json";
}

inline bool readManifest(std::string& out) noexcept {
    out.clear();
    const auto path = manifestPath();
    if (!std::filesystem::exists(path)) return false;
    std::ifstream in(path);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !out.empty();
}

inline std::string extractString(const std::string& blob, const char* key, std::size_t from = 0) noexcept {
    const std::string needle = std::string("\"") + key + "\"";
    const std::size_t k = blob.find(needle, from);
    if (k == std::string::npos) return {};
    const std::size_t q0 = blob.find('"', k + needle.size());
    if (q0 == std::string::npos) return {};
    const std::size_t q1 = blob.find('"', q0 + 1);
    if (q1 == std::string::npos) return {};
    return blob.substr(q0 + 1, q1 - q0 - 1);
}

inline std::string defaultBinary() noexcept {
    std::string blob;
    if (!readManifest(blob)) return {};
    const std::string def = extractString(blob, "default_binary");
    if (!def.empty() && std::filesystem::exists(def)) return def;
    for (std::size_t pos = 0; ; ) {
        const std::size_t block = blob.find("\"default\": true", pos);
        if (block == std::string::npos) break;
        const std::string bin = extractString(blob, "binary", block - 80);
        if (!bin.empty() && std::filesystem::exists(bin)) return bin;
        pos = block + 12;
    }
    const std::size_t block = blob.find("\"binary\"");
    if (block != std::string::npos) {
        const std::string bin = extractString(blob, "binary", block);
        if (!bin.empty() && std::filesystem::exists(bin)) return bin;
    }
    return {};
}

inline bool refreshManifest() noexcept {
    return FieldBrowserHook::refreshManifest();
}

inline bool openUrl(const char* url, std::uint8_t* ram = nullptr,
                    SDL_Window* hostWindow = nullptr) noexcept {
    if (!url || !url[0]) url = "https://amouranth.com";
    if (!defaultBinary().empty() || refreshManifest()) {
        if (FieldBrowserHook::open(url, hostWindow)) {
            if (ram) {
                FieldWebPanel::active = false;
            }
            return true;
        }
    }
    std::fprintf(stderr, "[AmmoBrowser] hook failed — offline Field Web panel\n");
    FieldWebPanel::open(ram, url);
    return FieldWebPanel::active;
}

inline void close() noexcept {
    FieldBrowserHook::close();
    FieldWebPanel::close(nullptr);
}

inline bool isActive() noexcept {
    return FieldBrowserHook::active || FieldWebPanel::active;
}

} // namespace FieldAmmoBrowser