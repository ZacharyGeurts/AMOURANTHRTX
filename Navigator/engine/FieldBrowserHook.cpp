// Field Browser Hook — OS default browser (Firefox/Chrome) positioned over the AmouranthOS panel.

#include "FieldBrowserHook.hpp"
#include "FieldDos.hpp"
#include "FieldDosViewport.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <signal.h>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {

std::string g_manifestBlob;
int g_lastGeom[4]{-1, -1, -1, -1};

bool readManifest() {
    g_manifestBlob.clear();
    const auto path = FieldDos::assetRoot() / "assets" / "browsers.json";
    if (!std::filesystem::exists(path)) return false;
    std::ifstream in(path);
    if (!in) return false;
    g_manifestBlob.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !g_manifestBlob.empty();
}

std::string extractString(const char* key) {
    const std::string needle = std::string("\"") + key + "\"";
    const std::size_t k = g_manifestBlob.find(needle);
    if (k == std::string::npos) return {};
    const std::size_t q0 = g_manifestBlob.find('"', k + needle.size());
    if (q0 == std::string::npos) return {};
    const std::size_t q1 = g_manifestBlob.find('"', q0 + 1);
    if (q1 == std::string::npos) return {};
    return g_manifestBlob.substr(q0 + 1, q1 - q0 - 1);
}

void shellAsync(const std::string& cmd) {
    std::string bg = cmd;
    if (bg.find('&') == std::string::npos)
        bg += " >/dev/null 2>&1 &";
    if (std::system(bg.c_str()) != 0) { /* fire-and-forget */ }
}

bool toolExists(const char* name) {
    return std::system(("command -v " + std::string(name) + " >/dev/null 2>&1").c_str()) == 0;
}

void positionBrowserWindow(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (g_lastGeom[0] == x && g_lastGeom[1] == y
        && g_lastGeom[2] == w && g_lastGeom[3] == h)
        return;
    g_lastGeom[0] = x;
    g_lastGeom[1] = y;
    g_lastGeom[2] = w;
    g_lastGeom[3] = h;

    std::ostringstream cmd;
    if (toolExists("wmctrl")) {
        cmd << "wmctrl -r 'AmouranthFieldWeb' -e 0," << x << ',' << y << ',' << w << ',' << h
            << " 2>/dev/null; ";
        cmd << "wmctrl -x -r 'AmouranthFieldWeb' 2>/dev/null; ";
        cmd << "wmctrl -l | grep -i firefox | tail -1 | awk '{print $1}' | "
               "xargs -r -I{} wmctrl -i -r {} -e 0," << x << ',' << y << ',' << w << ',' << h
            << " 2>/dev/null";
    } else if (toolExists("xdotool")) {
        cmd << "sleep 0.4; WID=$(xdotool search --classname AmouranthFieldWeb 2>/dev/null | tail -1); "
               "[ -z \"$WID\" ] && WID=$(xdotool search --class firefox 2>/dev/null | tail -1); "
               "[ -n \"$WID\" ] && xdotool windowmove $WID " << x << ' ' << y
            << " && xdotool windowsize $WID " << w << ' ' << h;
    } else {
        return;
    }
    shellAsync(cmd.str());
}

bool panelScreenRect(SDL_Window* hostWindow, int& x, int& y, int& w, int& h) {
    if (!hostWindow) return false;
    int winWpx = 0, winHpx = 0;
    SDL_GetWindowSize(hostWindow, &winWpx, &winHpx);
    FieldDosViewport::winW = static_cast<float>(winWpx);
    FieldDosViewport::winH = static_cast<float>(winHpx);
    const auto content = FieldDosViewport::contentRect();
    int winX = 0, winY = 0;
    SDL_GetWindowPosition(hostWindow, &winX, &winY);
    float scaleX = 1.f, scaleY = 1.f;
    SDL_GetWindowSizeInPixels(hostWindow, nullptr, nullptr);
    int pixelW = 0, pixelH = 0;
    SDL_GetWindowSizeInPixels(hostWindow, &pixelW, &pixelH);
    if (pixelW > 0 && pixelH > 0) {
        scaleX = static_cast<float>(pixelW) / std::max(FieldDosViewport::winW, 1.f);
        scaleY = static_cast<float>(pixelH) / std::max(FieldDosViewport::winH, 1.f);
    }
    x = winX + static_cast<int>(content.x0 * scaleX);
    y = winY + static_cast<int>(content.y0 * scaleY);
    w = static_cast<int>(content.w() * scaleX);
    h = static_cast<int>(content.h() * scaleY);
    return w > 64 && h > 64;
}

std::string launchCommand(const std::string& binary, const std::string& id,
                          const std::string& url, int x, int y, int w, int h) {
    std::ostringstream cmd;
    const std::string qurl = "'" + url + "'";
    if (id == "chromium" || id == "brave" || id == "edge") {
        cmd << '"' << binary << "\" --app=" << qurl
            << " --class=AmouranthFieldWeb"
            << " --window-name=AmouranthFieldWeb"
            << " --window-position=" << x << ',' << y
            << " --window-size=" << w << ',' << h
            << " --disable-infobars --no-first-run";
    } else if (id == "firefox") {
        cmd << '"' << binary << "\" -new-window " << qurl
            << " --class AmouranthFieldWeb"
            << " -width " << w << " -height " << h;
    } else {
        cmd << '"' << binary << "\" --new-window " << qurl;
    }
    return cmd.str();
}

} // namespace

namespace FieldBrowserHook {

bool refreshManifest() noexcept {
    const auto root = FieldDos::assetRoot();
    const auto script = root / "scripts" / "detect_browsers.py";
    if (!std::filesystem::exists(script)) return false;
    const std::string cmd = "python3 \"" + script.string() + "\"";
    return std::system(cmd.c_str()) == 0;
}

bool resolveDefault(std::string& binary, std::string& id, std::string& label) noexcept {
    if (!readManifest() && !refreshManifest())
        readManifest();
    binary = extractString("default_binary");
    id = extractString("default_id");
    if (binary.empty() || !std::filesystem::exists(binary)) {
        for (std::size_t pos = 0; ; ) {
            const std::size_t block = g_manifestBlob.find("\"binary\"", pos);
            if (block == std::string::npos) break;
            const std::size_t q0 = g_manifestBlob.find('"', block + 9);
            const std::size_t q1 = g_manifestBlob.find('"', q0 + 1);
            if (q0 != std::string::npos && q1 != std::string::npos) {
                binary = g_manifestBlob.substr(q0 + 1, q1 - q0 - 1);
                const std::size_t idPos = g_manifestBlob.rfind("\"id\"", block);
                if (idPos != std::string::npos && idPos > block - 200) {
                    const std::size_t iq0 = g_manifestBlob.find('"', idPos + 5);
                    const std::size_t iq1 = g_manifestBlob.find('"', iq0 + 1);
                    if (iq0 != std::string::npos && iq1 != std::string::npos)
                        id = g_manifestBlob.substr(iq0 + 1, iq1 - iq0 - 1);
                }
                if (std::filesystem::exists(binary)) break;
                binary.clear();
            }
            pos = block + 8;
        }
    }
    if (binary.empty()) return false;
    for (std::size_t pos = 0; ; ) {
        const std::size_t block = g_manifestBlob.find("\"id\"", pos);
        if (block == std::string::npos) break;
        const std::size_t binBlock = g_manifestBlob.find(binary, block);
        if (binBlock != std::string::npos && binBlock < block + 400) {
            const std::size_t q0 = g_manifestBlob.find('"', block + 5);
            const std::size_t q1 = g_manifestBlob.find('"', q0 + 1);
            if (q0 != std::string::npos && q1 != std::string::npos)
                id = g_manifestBlob.substr(q0 + 1, q1 - q0 - 1);
            const std::size_t lpos = g_manifestBlob.rfind("\"label\"", block);
            if (lpos != std::string::npos && lpos > block - 120) {
                const std::size_t lq0 = g_manifestBlob.find('"', lpos + 8);
                const std::size_t lq1 = g_manifestBlob.find('"', lq0 + 1);
                if (lq0 != std::string::npos && lq1 != std::string::npos)
                    label = g_manifestBlob.substr(lq0 + 1, lq1 - q0 - 1);
            }
            break;
        }
        pos = block + 4;
    }
    if (label.empty()) label = id.empty() ? "Browser" : id;
    if (id.empty()) id = "default";
    return true;
}

bool open(const char* url, SDL_Window* hostWindow) noexcept {
    if (!url || !url[0]) url = "https://amouranth.com";
    close();
    std::string bin, id, label;
    if (!resolveDefault(bin, id, label) && !refreshManifest()) {
        std::fprintf(stderr, "[BrowserHook] no default browser — run detect_browsers.py\n");
        return false;
    }
    int x = 100, y = 100, w = 960, h = 540;
    panelScreenRect(hostWindow, x, y, w, h);

    const std::string cmd = launchCommand(bin, id, url, x, y, w, h);
    std::fprintf(stderr, "[BrowserHook] %s → %s\n", label.c_str(), url);
    if (std::system((cmd + " >/dev/null 2>&1 &").c_str()) != 0) { /* fire-and-forget */ }

    active = true;
    hooked = true;
    browserId = id;
    browserLabel = label;
    browserPath = bin;
    currentUrl = url;
    childPid = 0;
    g_lastGeom[0] = -1;
    positionBrowserWindow(x, y, w, h);
    return true;
}

void syncGeometry(SDL_Window* hostWindow) noexcept {
    if (!active || !hooked) return;
    int x = 0, y = 0, w = 0, h = 0;
    if (!panelScreenRect(hostWindow, x, y, w, h)) return;
    positionBrowserWindow(x, y, w, h);
}

void close() noexcept {
    if (childPid > 0) {
        kill(static_cast<pid_t>(childPid), SIGTERM);
        childPid = 0;
    }
    active = false;
    hooked = false;
    g_lastGeom[0] = -1;
}

void pump(SDL_Window* hostWindow) noexcept {
    if (!active) return;
    syncGeometry(hostWindow);
}

} // namespace FieldBrowserHook