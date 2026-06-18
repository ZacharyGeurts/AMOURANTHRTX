// Field Web Panel — libcurl fetch + VGA render inside AmouranthOS display window.

#include <SDL3/SDL.h>

#include "FieldWebPanel.hpp"
#include "FieldInput.hpp"
#include "FieldVga.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#if defined(HAVE_LIBCURL)
#include <curl/curl.h>
#endif

namespace {

constexpr int CHAR_W = 8;
constexpr int CHAR_H = 8;
constexpr int COLS = FieldWebPanel::FB_W / CHAR_W;
constexpr int CHROME_ROWS = 2;
constexpr int VIEW_ROWS = (FieldWebPanel::FB_H / CHAR_H) - CHROME_ROWS;

constexpr std::uint8_t COL_BG      = 0u;
constexpr std::uint8_t COL_CHROME  = 1u;
constexpr std::uint8_t COL_TEXT    = 2u;
constexpr std::uint8_t COL_LINK    = 3u;
constexpr std::uint8_t COL_DIM     = 4u;
constexpr std::uint8_t COL_ERR     = 5u;
constexpr std::uint8_t COL_OK      = 6u;
constexpr std::uint8_t COL_SELECT   = 7u;

struct LinkHit {
    std::string href;
    int line = 0;
    int col = 0;
    int len = 0;
};

std::vector<std::string> g_lines;
std::vector<LinkHit> g_links;
std::deque<std::string> g_history;
std::string g_urlDraft;
std::string g_rawHtml;
bool g_dirty = true;
int g_hoverLine = -1;

// Minimal 8x8 ASCII font (32..126), 8 bytes per glyph (top row first).
constexpr std::uint8_t kFont8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // !
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // "
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // #
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // $
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // %
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // &
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // '
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // (
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x06,0x00}, // ,
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // .
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // /
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // 0
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // 1
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // 2
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // 3
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // 4
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // 5
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // 6
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // 7
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // 8
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // 9
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // :
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x06,0x00}, // ;
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // <
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // =
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // >
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // ?
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // @
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // A
    {0x1F,0x36,0x36,0x1E,0x36,0x36,0x1F,0x00}, // B
    {0x1E,0x33,0x03,0x03,0x03,0x33,0x1E,0x00}, // C
    {0x1F,0x36,0x33,0x33,0x33,0x36,0x1F,0x00}, // D
    {0x3F,0x03,0x03,0x1F,0x03,0x03,0x3F,0x00}, // E
    {0x3F,0x03,0x03,0x1F,0x03,0x03,0x03,0x00}, // F
    {0x1E,0x33,0x03,0x3B,0x33,0x33,0x1E,0x00}, // G
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // H
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // I
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // J
    {0x67,0x33,0x1B,0x0E,0x1B,0x33,0x67,0x00}, // K
    {0x03,0x03,0x03,0x03,0x03,0x03,0x3F,0x00}, // L
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // M
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // N
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // O
    {0x1F,0x33,0x33,0x1F,0x03,0x03,0x03,0x00}, // P
    {0x1C,0x36,0x63,0x63,0x6F,0x36,0x5C,0x00}, // Q
    {0x1F,0x33,0x33,0x1F,0x1B,0x33,0x73,0x00}, // R
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // S
    {0x3F,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x00}, // T
    {0x33,0x33,0x33,0x33,0x33,0x33,0x1E,0x00}, // U
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // X
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x0C,0x00}, // Y
    {0x3F,0x31,0x18,0x0C,0x06,0x23,0x3F,0x00}, // Z
    {0x0E,0x06,0x06,0x06,0x06,0x06,0x0E,0x00}, // [
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // backslash
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x0E,0x00}, // ]
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // a
    {0x03,0x03,0x1F,0x33,0x33,0x33,0x1F,0x00}, // b
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // c
    {0x30,0x30,0x3E,0x33,0x33,0x33,0x6E,0x00}, // d
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, // e
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, // f
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // g
    {0x03,0x03,0x1F,0x33,0x33,0x33,0x33,0x00}, // h
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // i
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // j
    {0x03,0x03,0x33,0x1B,0x0F,0x1B,0x33,0x00}, // k
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // l
    {0x00,0x00,0x63,0x77,0x7F,0x6B,0x63,0x00}, // m
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // n
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // o
    {0x00,0x00,0x1F,0x33,0x33,0x1F,0x03,0x03}, // p
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x30}, // q
    {0x00,0x00,0x1B,0x37,0x33,0x03,0x03,0x00}, // r
    {0x00,0x00,0x1E,0x03,0x1E,0x30,0x1F,0x00}, // s
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // t
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // u
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // v
    {0x00,0x00,0x63,0x6B,0x7F,0x36,0x22,0x00}, // w
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // x
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // y
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // z
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // {
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // |
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // }
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // ~
};

void loadWebPalette() noexcept {
    static const std::uint8_t rgb[8][3] = {
        {252,252,252}, {24,48,96}, {16,16,16}, {32,96,200},
        {120,120,120}, {200,32,32}, {32,160,64}, {64,128,255},
    };
    for (int i = 0; i < 8; ++i) {
        FieldVga::palette[static_cast<std::size_t>(i) * 3u + 0u] = rgb[i][0];
        FieldVga::palette[static_cast<std::size_t>(i) * 3u + 1u] = rgb[i][1];
        FieldVga::palette[static_cast<std::size_t>(i) * 3u + 2u] = rgb[i][2];
    }
}

void putPixel(std::uint8_t* ram, int x, int y, std::uint8_t col) noexcept {
    if (x < 0 || y < 0 || x >= FieldWebPanel::FB_W || y >= FieldWebPanel::FB_H) return;
    const std::uint32_t off = FieldWebPanel::FB_BASE
        + static_cast<std::uint32_t>(y * FieldWebPanel::FB_W + x);
    if (off < FieldVga::VGA_FB + 0x10000u)
        ram[off] = col;
}

void fillRect(std::uint8_t* ram, int x, int y, int w, int h, std::uint8_t col) noexcept {
    for (int py = y; py < y + h; ++py)
        for (int px = x; px < x + w; ++px)
            putPixel(ram, px, py, col);
}

void drawChar(std::uint8_t* ram, int x, int y, char ch, std::uint8_t fg, std::uint8_t bg) noexcept {
    if (ch < 32 || ch > 126) ch = '?';
    const auto& glyph = kFont8x8[static_cast<std::size_t>(ch - 32)];
    for (int row = 0; row < 8; ++row) {
        const std::uint8_t bits = glyph[row];
        for (int col = 0; col < 8; ++col)
            putPixel(ram, x + col, y + row, (bits & (0x80 >> col)) ? fg : bg);
    }
}

void drawText(std::uint8_t* ram, int x, int y, std::string_view text,
              std::uint8_t fg, std::uint8_t bg, int maxCols = COLS) noexcept {
    int col = 0;
    for (char ch : text) {
        if (col >= maxCols) break;
        drawChar(ram, x + col * CHAR_W, y, ch, fg, bg);
        ++col;
    }
}

std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string stripTags(std::string html) {
    std::string out;
    out.reserve(html.size());
    bool inTag = false;
    for (char c : html) {
        if (c == '<') { inTag = true; continue; }
        if (c == '>') { inTag = false; out.push_back(' '); continue; }
        if (!inTag) out.push_back(c);
    }
    return out;
}

std::string extractBetween(const std::string& hay, const char* open, const char* close) {
    const std::size_t a = toLower(hay).find(toLower(open));
    if (a == std::string::npos) return {};
    const std::size_t b = hay.find('>', a);
    if (b == std::string::npos) return {};
    const std::size_t c = toLower(hay).find(toLower(close), b);
    if (c == std::string::npos) return {};
    return hay.substr(b + 1, c - b - 1);
}

std::string resolveUrl(const std::string& base, std::string href) {
    while (!href.empty() && (href.front() == ' ' || href.front() == '"')) href.erase(href.begin());
    while (!href.empty() && (href.back() == ' ' || href.back() == '"')) href.pop_back();
    if (href.rfind("http://", 0) == 0 || href.rfind("https://", 0) == 0) return href;
    if (href.rfind("//", 0) == 0) return "https:" + href;
    if (href.front() == '/') {
        const auto schemeEnd = base.find("://");
        const auto hostStart = schemeEnd == std::string::npos ? 0 : schemeEnd + 3;
        const auto pathStart = base.find('/', hostStart);
        return base.substr(0, pathStart == std::string::npos ? base.size() : pathStart) + href;
    }
    const auto slash = base.rfind('/');
    const std::string prefix = slash == std::string::npos ? base : base.substr(0, slash + 1);
    return prefix + href;
}

void wrapWords(const std::string& text, std::vector<std::string>& out) {
    std::string line;
    auto flush = [&]() {
        if (!line.empty()) { out.push_back(line); line.clear(); }
    };
    for (std::size_t i = 0; i < text.size(); ) {
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
        if (i >= text.size()) break;
        std::size_t j = i;
        while (j < text.size() && !std::isspace(static_cast<unsigned char>(text[j]))) ++j;
        const std::string word = text.substr(i, j - i);
        if (line.empty()) line = word;
        else if (line.size() + 1 + word.size() <= static_cast<std::size_t>(COLS - 1))
            line += ' ' + word;
        else { flush(); line = word; }
        i = j;
    }
    flush();
}

void parseHtml(const std::string& html, const std::string& baseUrl) {
    g_lines.clear();
    g_links.clear();
    FieldWebPanel::pageTitle = extractBetween(html, "<title", "</title>");
    if (FieldWebPanel::pageTitle.empty())
        FieldWebPanel::pageTitle = baseUrl;

    std::string body = html;
    for (const char* tag : {"<script", "<style", "<noscript"}) {
        for (;;) {
            const std::string low = toLower(body);
            const std::size_t a = low.find(tag);
            if (a == std::string::npos) break;
            const std::size_t b = low.find("</", a);
            const std::size_t c = b == std::string::npos ? body.size() : low.find('>', b);
            body.erase(a, (c == std::string::npos ? body.size() : c + 1) - a);
        }
    }

    std::size_t pos = 0;
    while (pos < body.size()) {
        const std::string low = toLower(body);
        const std::size_t h1 = low.find("<h1", pos);
        const std::size_t a = low.find("<a ", pos);
        const std::size_t p = low.find("<p", pos);
        const std::size_t br = low.find("<br", pos);
        std::size_t next = body.size();
        enum Kind { End, H1, A, P, BR } kind = End;
        if (h1 != std::string::npos && h1 < next) { next = h1; kind = H1; }
        if (a != std::string::npos && a < next)   { next = a; kind = A; }
        if (p != std::string::npos && p < next)   { next = p; kind = P; }
        if (br != std::string::npos && br < next) { next = br; kind = BR; }

        if (kind == End) {
            wrapWords(stripTags(body.substr(pos)), g_lines);
            break;
        }

        if (next > pos) {
            const std::string chunk = stripTags(body.substr(pos, next - pos));
            if (!chunk.empty()) wrapWords(chunk, g_lines);
        }

        if (kind == BR) {
            g_lines.emplace_back("");
            pos = body.find('>', next);
            pos = pos == std::string::npos ? body.size() : pos + 1;
            continue;
        }

        const std::size_t gt = body.find('>', next);
        if (gt == std::string::npos) break;
        const std::string tag = body.substr(next, gt - next + 1);
        const std::size_t end = toLower(body).find("</a>", gt);
        const std::size_t endTag = kind == A
            ? (end == std::string::npos ? body.size() : end)
            : toLower(body).find("</p>", gt);
        const std::size_t contentEnd = endTag == std::string::npos ? body.size() : endTag;
        std::string content = stripTags(body.substr(gt + 1, contentEnd - gt - 1));

        if (kind == H1 && !content.empty()) {
            g_lines.emplace_back("");
            g_lines.push_back(content);
            g_lines.emplace_back("");
        } else if (kind == A) {
            const std::string lowTag = toLower(tag);
            const std::size_t hrefPos = lowTag.find("href=");
            std::string href;
            if (hrefPos != std::string::npos) {
                const char q = tag[hrefPos + 5];
                const std::size_t v0 = hrefPos + 6;
                const std::size_t v1 = tag.find(q, v0);
                if (v1 != std::string::npos) href = tag.substr(v0, v1 - v0);
            }
            if (content.empty()) content = href;
            if (!content.empty()) {
                LinkHit hit;
                hit.href = resolveUrl(baseUrl, href.empty() ? content : href);
                hit.line = static_cast<int>(g_lines.size());
                hit.col = 2;
                hit.len = static_cast<int>(std::min<std::size_t>(content.size(), COLS - 3));
                g_lines.push_back("> " + content);
                g_links.push_back(std::move(hit));
            }
        } else if (!content.empty()) {
            wrapWords(content, g_lines);
            g_lines.emplace_back("");
        }

        pos = contentEnd;
        if (kind == A && end != std::string::npos) pos = end + 4;
        else if (kind == P && endTag != std::string::npos) pos = endTag + 4;
    }

    if (g_lines.empty())
        g_lines.push_back("(empty page)");
}

#if defined(HAVE_LIBCURL)
size_t curlWrite(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

bool fetchUrl(const std::string& url, std::string& outHtml, std::string& err) {
    outHtml.clear();
    err.clear();
    CURL* curl = curl_easy_init();
    if (!curl) { err = "curl init failed"; return false; }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outHtml);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "FieldWebPanel/1.0 (AmouranthOS)");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    const CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) err = curl_easy_strerror(res);
    curl_easy_cleanup(curl);
    return res == CURLE_OK && !outHtml.empty();
}
#else
bool fetchUrl(const std::string& url, std::string& outHtml, std::string& err) {
    (void)url;
    outHtml.clear();
    err = "libcurl not linked — rebuild with libcurl4-openssl-dev";
    return false;
}
#endif

void renderPanel(std::uint8_t* ram) noexcept {
    if (!ram) return;
    FieldVga::setMode(FieldVga::MODE_VGA_13, ram);
    ram[0x449u] = FieldVga::MODE_VGA_13;
    loadWebPalette();
    FieldVga::syncPaletteToGuest(ram);

    fillRect(ram, 0, 0, FieldWebPanel::FB_W, CHROME_ROWS * CHAR_H, COL_CHROME);
    const std::string chrome = FieldWebPanel::urlBarFocus
        ? "> " + g_urlDraft + "_"
        : FieldWebPanel::currentUrl;
    drawText(ram, 0, 0, chrome, COL_TEXT, COL_CHROME, COLS);
    drawText(ram, 0, CHAR_H, FieldWebPanel::statusLine, COL_DIM, COL_CHROME, COLS);

    const int contentY = CHROME_ROWS * CHAR_H;
    fillRect(ram, 0, contentY, FieldWebPanel::FB_W,
             FieldWebPanel::FB_H - contentY, COL_BG);

    for (int row = 0; row < VIEW_ROWS; ++row) {
        const int srcLine = FieldWebPanel::scrollY + row;
        if (srcLine < 0 || srcLine >= static_cast<int>(g_lines.size())) continue;
        const std::string& line = g_lines[static_cast<std::size_t>(srcLine)];
        std::uint8_t fg = COL_TEXT;
        std::uint8_t bg = COL_BG;
        for (const auto& link : g_links) {
            if (link.line == srcLine) {
                fg = COL_LINK;
                if (srcLine == g_hoverLine) { fg = COL_SELECT; bg = COL_CHROME; }
                break;
            }
        }
        drawText(ram, 0, contentY + row * CHAR_H, line, fg, bg, COLS);
    }
}

LinkHit* linkAtLine(int line) noexcept {
    for (auto& link : g_links)
        if (link.line == line) return &link;
    return nullptr;
}

void doNavigate(const std::string& url) {
    if (url.empty()) return;
    FieldWebPanel::loading = true;
    FieldWebPanel::statusLine = "Loading...";
    g_dirty = true;

    std::string html;
    std::string err;
    const bool ok = fetchUrl(url, html, err);
    if (ok) {
        if (!FieldWebPanel::currentUrl.empty())
            g_history.push_back(FieldWebPanel::currentUrl);
        FieldWebPanel::currentUrl = url;
        g_rawHtml = std::move(html);
        parseHtml(g_rawHtml, url);
        FieldWebPanel::statusLine = FieldWebPanel::pageTitle + " — " + url;
        FieldWebPanel::scrollY = 0;
    } else {
        g_lines = {"Failed to load page:", err, "", "Press F5 to retry."};
        g_links.clear();
        FieldWebPanel::statusLine = "Error: " + err;
    }
    FieldWebPanel::loading = false;
    g_dirty = true;
}

} // namespace

namespace FieldWebPanel {

void navigate(const char* url) {
    if (!url || !url[0]) return;
    std::string u = url;
    if (u.rfind("http://", 0) != 0 && u.rfind("https://", 0) != 0)
        u = "https://" + u;
    doNavigate(u);
}

void open(std::uint8_t* ram, const char* url) {
    active = true;
    loading = false;
    urlBarFocus = false;
    scrollY = 0;
    g_urlDraft.clear();
    g_history.clear();
    g_hoverLine = -1;
    currentUrl.clear();
    if (!url || !url[0]) url = "https://amouranth.com";
    navigate(url);
    if (ram) renderPanel(ram);
    std::fprintf(stderr, "[FieldWeb] Panel open %s\n", currentUrl.c_str());
}

void close(std::uint8_t* ram) noexcept {
    active = false;
    loading = false;
    urlBarFocus = false;
    g_lines.clear();
    g_links.clear();
    g_history.clear();
    if (ram)
        FieldVga::setMode(FieldVga::MODE_TEXT_80, ram);
}

void pump(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept {
    if (!active || !keyDown) return;
    if (key == 0x011Bu) { close(ram); return; }
    if ((key & 0xFFu) == 27u) { close(ram); return; }

    if (urlBarFocus) {
        const char ch = static_cast<char>(key & 0xFFu);
        if (key == 0x0E08u || key == 0x007Fu) {
            if (!g_urlDraft.empty()) g_urlDraft.pop_back();
        } else if (key == 0x1C0Du || key == 0x000Du) {
            urlBarFocus = false;
            navigate(g_urlDraft.c_str());
        } else if (key == 0x011Bu) {
            urlBarFocus = false;
        } else if (ch >= 32 && ch < 127) {
            if (g_urlDraft.size() < 240u) g_urlDraft.push_back(ch);
        }
        g_dirty = true;
        if (ram) renderPanel(ram);
        return;
    }

    if (key == 0x3B00u || (key & 0xFFu) == 'l' || (key & 0xFFu) == 'L') {
        urlBarFocus = true;
        g_urlDraft = currentUrl;
        g_dirty = true;
        if (ram) renderPanel(ram);
        return;
    }
    if (key == 0x3C00u || key == 0x0E08u) {
        if (!g_history.empty()) {
            const std::string prev = g_history.back();
            g_history.pop_back();
            navigate(prev.c_str());
            if (ram) renderPanel(ram);
        }
        return;
    }
    if (key == 0x3F00u) {
        navigate(currentUrl.c_str());
        if (ram) renderPanel(ram);
        return;
    }
    if (key == 0x4800u) {
        scrollY = std::max(0, scrollY - 1);
        g_dirty = true;
    } else if (key == 0x5000u) {
        scrollY = std::min(std::max(0, static_cast<int>(g_lines.size()) - VIEW_ROWS), scrollY + 1);
        g_dirty = true;
    } else if (key == 0x4900u) {
        scrollY = std::max(0, scrollY - VIEW_ROWS);
        g_dirty = true;
    } else if (key == 0x5100u) {
        scrollY = std::min(std::max(0, static_cast<int>(g_lines.size()) - VIEW_ROWS),
                           scrollY + VIEW_ROWS);
        g_dirty = true;
    } else if (key == 0x1C0Du || key == 0x000Du) {
        if (g_hoverLine >= 0) {
            if (LinkHit* hit = linkAtLine(g_hoverLine))
                navigate(hit->href.c_str());
        }
        g_dirty = true;
    }
    if (ram && g_dirty) renderPanel(ram);
}

void tick(std::uint8_t* ram, const bool* keys) noexcept {
    if (!active || !ram) return;

    const auto& mouse = FieldInput::state.mouse;
    if (mouse.installed) {
        const int my = mouse.y - CHROME_ROWS * CHAR_H;
        int newHover = -1;
        if (my >= 0) {
            const int row = my / CHAR_H + scrollY;
            if (linkAtLine(row)) newHover = row;
        }
        if (newHover != g_hoverLine) {
            g_hoverLine = newHover;
            g_dirty = true;
        }
        if ((mouse.buttons & 1u) && !(mouse.prevButtons & 1u) && g_hoverLine >= 0) {
            if (LinkHit* hit = linkAtLine(g_hoverLine))
                navigate(hit->href.c_str());
            g_dirty = true;
        }
    }

    if (keys) {
        if (keys[SDL_SCANCODE_UP]) pump(ram, 0x4800u, true);
        else if (keys[SDL_SCANCODE_DOWN]) pump(ram, 0x5000u, true);
        else if (keys[SDL_SCANCODE_PAGEUP]) pump(ram, 0x4900u, true);
        else if (keys[SDL_SCANCODE_PAGEDOWN]) pump(ram, 0x5100u, true);
        else if (keys[SDL_SCANCODE_RETURN]) pump(ram, 0x1C0Du, true);
        else if (keys[SDL_SCANCODE_BACKSPACE]) pump(ram, 0x0E08u, true);
        else if (keys[SDL_SCANCODE_F5]) pump(ram, 0x3F00u, true);
        else if (keys[SDL_SCANCODE_L] && (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]))
            pump(ram, 0x3B00u, true);
    }

    if (g_dirty) renderPanel(ram);
}

void packDataBus(std::uint32_t* bus) noexcept {
    if (!bus || !active) return;
    bus[38] = (static_cast<std::uint32_t>(scrollY) & 0xFFFFu)
            | (loading ? (1u << 16u) : 0u)
            | (urlBarFocus ? (1u << 17u) : 0u);
    bus[39] = static_cast<std::uint32_t>(g_lines.size());
}

} // namespace FieldWebPanel