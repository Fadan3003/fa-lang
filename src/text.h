#pragma once
// Characters: UTF-8 handling, upper/lower case for umlauts, Greek, Cyrillic.
#include "common.h"

// ═══════════════════════════ Text (UTF-8 characters) ═══════════════════════════

static std::string utf8Encode(uint32_t c) {
    std::string r;
    if (c < 0x80) r += (char)c;
    else if (c < 0x800) { r += (char)(0xC0 | (c >> 6)); r += (char)(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) { r += (char)(0xE0 | (c >> 12)); r += (char)(0x80 | ((c >> 6) & 0x3F)); r += (char)(0x80 | (c & 0x3F)); }
    else { r += (char)(0xF0 | (c >> 18)); r += (char)(0x80 | ((c >> 12) & 0x3F)); r += (char)(0x80 | ((c >> 6) & 0x3F)); r += (char)(0x80 | (c & 0x3F)); }
    return r;
}

static bool isAscii(const std::string& s) { for (unsigned char c : s) if (c >= 0x80) return false; return true; }

// splits text into characters (not bytes), so "ü" is one character
static std::vector<std::string> utf8Chars(const std::string& s) {
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size();) {
        size_t len = 1; unsigned char c = s[i];
        if (c >= 0xF0) len = 4; else if (c >= 0xE0) len = 3; else if (c >= 0xC0) len = 2;
        out.push_back(s.substr(i, len)); i += len;
    }
    return out;
}

// the number (code point) of one character, e.g. "ü" -> 252
static uint32_t utf8Code(const std::string& s) {
    if (s.empty()) return 0;
    unsigned char c = s[0];
    if (c < 0x80) return c;
    int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
    uint32_t cp = c & (0x3F >> n);
    for (int i = 1; i <= n && i < (int)s.size(); i++) cp = (cp << 6) | (s[i] & 0x3F);
    return cp;
}

// upper/lower case for Latin (with ä ö ü é ...), Greek and Cyrillic
static uint32_t upperCp(uint32_t c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32;
    if (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2) return c - 32;
    if (c >= 0x430 && c <= 0x44F) return c - 32;
    if (c >= 0x450 && c <= 0x45F) return c - 80;
    return c;
}
static uint32_t lowerCp(uint32_t c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) return c + 32;
    if (c >= 0x410 && c <= 0x42F) return c + 32;
    if (c >= 0x400 && c <= 0x40F) return c + 80;
    return c;
}
static std::string mapCase(const std::string& s, bool up) {
    std::string r;
    for (auto& ch : utf8Chars(s)) r += utf8Encode(up ? upperCp(utf8Code(ch)) : lowerCp(utf8Code(ch)));
    return r;
}

