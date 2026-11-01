// GUI backend using raylib: labels, buttons, rows and columns, a code editor, text views, lists.
// Kept in its own file so raylib's names never clash with the language.
#include "raylib.h"
#include "gui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

bool fa_gui_available() { return true; }

namespace {

typedef std::u32string U32;

// ════════════════════════ text helpers (UTF-8 <-> code points) ════════════════════════

std::string toU8(const U32& s) {
    std::string r;
    for (char32_t c : s) {
        if (c < 0x80) r += (char)c;
        else if (c < 0x800) { r += (char)(0xC0 | (c >> 6)); r += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { r += (char)(0xE0 | (c >> 12)); r += (char)(0x80 | ((c >> 6) & 0x3F)); r += (char)(0x80 | (c & 0x3F)); }
        else { r += (char)(0xF0 | (c >> 18)); r += (char)(0x80 | ((c >> 12) & 0x3F)); r += (char)(0x80 | ((c >> 6) & 0x3F)); r += (char)(0x80 | (c & 0x3F)); }
    }
    return r;
}

U32 toU32(const std::string& s) {
    U32 r;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        if (n > 0 && i + n >= s.size()) { r += U'\uFFFD'; break; }          // cut off in the middle of a character
        char32_t cp = n == 0 ? c : (c & (0x3F >> n));
        for (int k = 1; k <= n; k++) cp = (cp << 6) | (s[i + k] & 0x3F);
        r += cp; i += n + 1;
    }
    return r;
}

bool isIdStart(char32_t c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80; }
bool isDigit(char32_t c) { return c >= '0' && c <= '9'; }
bool isIdChar(char32_t c) { return isIdStart(c) || isDigit(c); }

// ════════════════════════ colors and options ════════════════════════

Color rgba(int r, int g, int b, int a = 255) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}; }
Color parseColor(const std::string& s, Color def) {
    if (s.size() == 7 && s[0] == '#') { unsigned v = (unsigned)strtoul(s.c_str() + 1, nullptr, 16); return rgba((v >> 16) & 255, (v >> 8) & 255, v & 255); }
    return def;
}
Color mixColor(Color a, Color b, float t) {
    return rgba((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t));
}
float luminance(Color c) { return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b; }

std::string optS(const GuiWidget& w, const char* k, const std::string& d = "") { auto it = w.opts.find(k); return it == w.opts.end() ? d : it->second; }
float optF(const GuiWidget& w, const char* k, float d) {
    auto it = w.opts.find(k);
    if (it == w.opts.end()) return d;
    char* e; float v = strtof(it->second.c_str(), &e);
    return (*e || it->second.empty()) ? d : v;
}
bool optB(const GuiWidget& w, const char* k) { auto it = w.opts.find(k); return it != w.opts.end() && it->second == "true"; }

// ════════════════════════ the text editor (the part that holds the text) ════════════════════════

struct Snap { std::vector<U32> lines; int row, col; };

struct Editor {
    std::vector<U32> lines{U32()};
    int row = 0, col = 0;            // cursor
    int arow = 0, acol = 0;          // other end of the selection (same as cursor = nothing selected)
    int wantCol = -1;                // column to keep while moving up and down
    float scrollX = 0, scrollY = 0;
    bool dirty = false, reveal = false;
    std::vector<Snap> undo, redo;
    int lastKind = 0;                // 1 = typing, 2 = deleting: neighbouring ones become one undo step
    double lastInput = 0;
};
std::map<std::string, Editor> g_editors;

bool hasSel(const Editor& e) { return e.row != e.arow || e.col != e.acol; }
void selRange(const Editor& e, int& r1, int& c1, int& r2, int& c2) {
    if (e.arow < e.row || (e.arow == e.row && e.acol <= e.col)) { r1 = e.arow; c1 = e.acol; r2 = e.row; c2 = e.col; }
    else { r1 = e.row; c1 = e.col; r2 = e.arow; c2 = e.acol; }
}
U32 selectedText(const Editor& e) {
    int r1, c1, r2, c2; selRange(e, r1, c1, r2, c2);
    if (r1 == r2) return e.lines[r1].substr(c1, c2 - c1);
    U32 out = e.lines[r1].substr(c1);
    for (int r = r1 + 1; r < r2; r++) { out += U'\n'; out += e.lines[r]; }
    out += U'\n'; out += e.lines[r2].substr(0, c2);
    return out;
}
void pushUndo(Editor& e, int kind) {
    if (kind != 0 && kind == e.lastKind) return;
    e.undo.push_back({e.lines, e.row, e.col});
    if (e.undo.size() > 300) e.undo.erase(e.undo.begin());
    e.redo.clear();
    e.lastKind = kind;
}
void deleteSel(Editor& e) {
    int r1, c1, r2, c2; selRange(e, r1, c1, r2, c2);
    if (r1 == r2) e.lines[r1].erase(c1, c2 - c1);
    else { e.lines[r1] = e.lines[r1].substr(0, c1) + e.lines[r2].substr(c2); e.lines.erase(e.lines.begin() + r1 + 1, e.lines.begin() + r2 + 1); }
    e.row = e.arow = r1; e.col = e.acol = c1;
    e.dirty = true; e.reveal = true; e.wantCol = -1;
}
void insertText(Editor& e, const U32& s) {
    if (hasSel(e)) deleteSel(e);
    U32 tail = e.lines[e.row].substr(e.col);
    e.lines[e.row].erase(e.col);
    size_t start = 0; bool first = true;
    for (size_t i = 0; i <= s.size(); i++) {
        if (i == s.size() || s[i] == U'\n') {
            U32 piece = s.substr(start, i - start);
            if (first) { e.lines[e.row] += piece; first = false; }
            else { e.lines.insert(e.lines.begin() + e.row + 1, piece); e.row++; }
            e.col = (int)e.lines[e.row].size();
            start = i + 1;
        }
    }
    e.lines[e.row] += tail;
    e.arow = e.row; e.acol = e.col;
    e.dirty = true; e.reveal = true; e.wantCol = -1;
}
void setCursor(Editor& e, int r, int c, bool extend) {
    r = std::max(0, std::min(r, (int)e.lines.size() - 1));
    c = std::max(0, std::min(c, (int)e.lines[r].size()));
    e.row = r; e.col = c;
    if (!extend) { e.arow = r; e.acol = c; }
    e.reveal = true;
}
std::string getText(const Editor& e) {
    std::string out;
    for (size_t i = 0; i < e.lines.size(); i++) { if (i) out += '\n'; out += toU8(e.lines[i]); }
    return out;
}
void setText(Editor& e, const std::string& text) {
    e.lines.clear();
    U32 all = toU32(text), cur;
    for (size_t i = 0; i < all.size(); i++) {
        char32_t c = all[i];
        if (c == U'\r') continue;
        if (c == U'\n') { e.lines.push_back(cur); cur.clear(); }
        else if (c == U'\t') cur += U"    ";
        else cur += c;
    }
    e.lines.push_back(cur);
    e.row = e.col = e.arow = e.acol = 0; e.wantCol = -1;
    e.scrollX = e.scrollY = 0; e.dirty = false; e.reveal = true;
    e.undo.clear(); e.redo.clear(); e.lastKind = 0;
}
int leadingSpaces(const U32& s) { int n = 0; while (n < (int)s.size() && s[n] == U' ') n++; return n; }

void moveHorizontal(Editor& e, int dir, bool extend, bool word) {
    if (!extend && hasSel(e)) {
        int r1, c1, r2, c2; selRange(e, r1, c1, r2, c2);
        if (dir < 0) setCursor(e, r1, c1, false); else setCursor(e, r2, c2, false);
        e.wantCol = -1; return;
    }
    int r = e.row, c = e.col;
    if (dir < 0) {
        if (c == 0) { if (r > 0) { r--; c = (int)e.lines[r].size(); } }
        else if (word) {
            while (c > 0 && !isIdChar(e.lines[r][c - 1])) c--;
            while (c > 0 && isIdChar(e.lines[r][c - 1])) c--;
        } else c--;
    } else {
        int len = (int)e.lines[r].size();
        if (c >= len) { if (r + 1 < (int)e.lines.size()) { r++; c = 0; } }
        else if (word) {
            while (c < len && !isIdChar(e.lines[r][c])) c++;
            while (c < len && isIdChar(e.lines[r][c])) c++;
        } else c++;
    }
    setCursor(e, r, c, extend); e.wantCol = -1;
}
void moveVertical(Editor& e, int dRows, bool extend) {
    if (e.wantCol < 0) e.wantCol = e.col;
    int r = e.row + dRows;
    if (r < 0) { setCursor(e, 0, 0, extend); e.wantCol = -1; return; }
    if (r >= (int)e.lines.size()) { setCursor(e, (int)e.lines.size() - 1, (int)e.lines.back().size(), extend); e.wantCol = -1; return; }
    int want = e.wantCol;
    setCursor(e, r, std::min(want, (int)e.lines[r].size()), extend);
    e.wantCol = want;
}
void selectWordAt(Editor& e, int r, int c) {
    const U32& s = e.lines[r];
    int a = c, b = c;
    if (a < (int)s.size() && isIdChar(s[a])) { while (a > 0 && isIdChar(s[a - 1])) a--; while (b < (int)s.size() && isIdChar(s[b])) b++; }
    else if (a > 0 && isIdChar(s[a - 1])) { b = a; while (a > 0 && isIdChar(s[a - 1])) a--; }
    e.arow = r; e.acol = a; e.row = r; e.col = b; e.reveal = true;
}
void indentLines(Editor& e, bool outdent) {
    pushUndo(e, 0);
    int r1, c1, r2, c2; selRange(e, r1, c1, r2, c2);
    if (r2 > r1 && c2 == 0) r2--;
    for (int r = r1; r <= r2; r++) {
        int delta = 0;
        if (!outdent) { e.lines[r].insert(0, U"    "); delta = 4; }
        else { int n = std::min(4, leadingSpaces(e.lines[r])); e.lines[r].erase(0, n); delta = -n; }
        if (e.row == r) e.col = std::max(0, e.col + delta);
        if (e.arow == r) e.acol = std::max(0, e.acol + delta);
    }
    e.dirty = true; e.reveal = true; e.wantCol = -1;
}

// ════════════════════════ colors for Fa code ════════════════════════

enum { C_TEXT = 0, C_KEYWORD, C_STRING, C_INTERP, C_COMMENT, C_NUMBER, C_BUILTIN };

const std::set<std::string> KEYWORDS = {"def", "return", "if", "else", "while", "for", "in", "break", "continue",
                                        "true", "false", "nothing", "and", "or", "not"};
const std::set<std::string> BUILTINS = {
    "print", "ask", "len", "text", "number", "chr", "ord", "sqrt", "round", "floor", "abs", "min", "max", "random",
    "read_file", "write_file", "file_exists", "list_files", "delete_file", "run", "env", "wait", "now", "exit", "args",
    "route", "serve", "status", "content_type", "json", "page", "document", "h1", "h2", "h3", "p", "div", "span", "ul",
    "ol", "li", "pre", "bold", "italic", "code", "link", "image", "input", "br", "hr", "tag", "raw",
    "window", "label", "button", "row", "column", "editor", "textfield", "textview", "listbox", "spacer", "shortcut",
    "editor_get", "editor_set", "editor_dirty", "editor_clean", "editor_info", "here", "interpreter", "os"};

void highlightFa(const U32& s, std::vector<unsigned char>& cls) {
    cls.assign(s.size(), C_TEXT);
    size_t i = 0, n = s.size();
    while (i < n) {
        char32_t c = s[i];
        if (c == U'#') { for (; i < n; i++) cls[i] = C_COMMENT; break; }
        if (c == U'"') {
            size_t j = i + 1; cls[i] = C_STRING;
            while (j < n && s[j] != U'"') {
                if (s[j] == U'\\' && j + 1 < n) { cls[j] = cls[j + 1] = C_STRING; j += 2; continue; }
                if (s[j] == U'{') {                                   // {value} inside text
                    size_t k = j + 1; int depth = 1; bool inStr = false;
                    while (k < n && depth > 0) {
                        if (s[k] == U'"') inStr = !inStr;
                        else if (!inStr) { if (s[k] == U'{') depth++; else if (s[k] == U'}') depth--; }
                        k++;
                    }
                    if (depth == 0) { for (size_t m = j; m < k; m++) cls[m] = C_INTERP; j = k; continue; }
                }
                cls[j] = C_STRING; j++;
            }
            if (j < n) cls[j] = C_STRING;
            i = j + 1; continue;
        }
        if (isDigit(c)) {
            size_t j = i;
            while (j < n && (isDigit(s[j]) || (s[j] == U'.' && j + 1 < n && isDigit(s[j + 1])))) j++;
            for (size_t m = i; m < j; m++) cls[m] = C_NUMBER;
            i = j; continue;
        }
        if (isIdStart(c)) {
            size_t j = i;
            while (j < n && isIdChar(s[j])) j++;
            std::string w = toU8(s.substr(i, j - i));
            unsigned char k = KEYWORDS.count(w) ? C_KEYWORD : BUILTINS.count(w) ? C_BUILTIN : C_TEXT;
            for (size_t m = i; m < j; m++) cls[m] = k;
            i = j; continue;
        }
        i++;
    }
}

// ════════════════════════ look, fonts, layout ════════════════════════

struct Ui {
    bool simple = true;                                  // only labels and buttons: big, centered, old look
    int labelFs = 22, buttonFs = 20, monoFs = 15;
    float labelH = 28, buttonH = 46, rowH = 26;
    Font font{}, buttonFont{}, mono{};
    float charW = 9, lineH = 22;
    Color bg{244, 245, 247, 255}, text{34, 34, 40, 255}, accent{37, 99, 235, 255};
    bool dark = false;
} ui;

const int SIMPLE_W = 520, SIMPLE_TOP = 36, SIMPLE_GAP = 14;

bool endsWith(const std::string& s, const char* e) { std::string t = e; return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0; }

std::string findFontFile(const char* envName, const char* fcQuery, const std::vector<const char*>& candidates) {
    if (const char* own = getenv(envName)) if (FileExists(own)) return own;
#ifndef _WIN32
    std::string cmd = std::string("fc-match -f '%{file}' ") + fcQuery + " 2>/dev/null";
    if (FILE* p = popen(cmd.c_str(), "r")) {
        char buf[1024]; std::string path;
        while (fgets(buf, sizeof buf, p)) path += buf;
        pclose(p);
        if (!path.empty() && (endsWith(path, ".ttf") || endsWith(path, ".otf")) && FileExists(path.c_str())) return path;
    }
#endif
    for (const char* c : candidates) if (FileExists(c)) return c;
    return "";
}

Font loadUiFont(const std::string& file, int size) {
    if (file.empty()) return GetFontDefault();
    std::vector<int> cps;
    auto range = [&](int a, int b) { for (int c = a; c <= b; c++) cps.push_back(c); };
    range(32, 126); range(160, 383); range(0x391, 0x3C9); range(0x400, 0x45F);          // Latin, Greek, Cyrillic
    range(0x2013, 0x2014); range(0x2018, 0x201E); range(0x2022, 0x2022); range(0x2026, 0x2026);   // dashes, quotes, bullet, ...
    cps.push_back(0x20AC); cps.push_back(0x2713); cps.push_back(0x25CF);
    Font f = LoadFontEx(file.c_str(), size, cps.data(), (int)cps.size());
    if (f.texture.id == 0 || f.glyphCount == 0) return GetFontDefault();
    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    return f;
}
void unloadUiFont(Font& f) { if (f.texture.id != GetFontDefault().texture.id) UnloadFont(f); }

float textW(const Font& f, const std::string& s, float size) { return MeasureTextEx(f, s.c_str(), size, 0).x; }

bool isShortcut(const GuiWidget& w) { return w.kind == "shortcut"; }
bool isSingle(const GuiWidget& w) { return w.kind == "editor" && optB(w, "single"); }
bool isFlex(const GuiWidget& w) { return (w.kind == "editor" && !isSingle(w)) || w.kind == "textview" || w.kind == "listbox" || w.kind == "spacer"; }

float prefW(const GuiWidget& w) {
    float ow = optF(w, "width", -1);
    if (ow >= 0) return ow;
    if (w.kind == "label") return textW(ui.font, w.text, (float)ui.labelFs);
    if (w.kind == "button") return std::max(ui.simple ? 200.0f : 0.0f, textW(ui.buttonFont, w.text, (float)ui.buttonFs) + (ui.simple ? 56.0f : 28.0f));
    if (w.kind == "row" || w.kind == "column") {
        float pad = optF(w, "padding", 0), gap = optF(w, "gap", 0), s = 0; int n = 0;
        for (auto& c : w.children) { if (isShortcut(c)) continue; float p = prefW(c); s = w.kind == "row" ? s + p : std::max(s, p); n++; }
        return s + (w.kind == "row" ? gap * std::max(0, n - 1) : 0) + 2 * pad;
    }
    return 0;
}
float prefH(const GuiWidget& w) {
    float oh = optF(w, "height", -1);
    if (oh >= 0) return oh;
    if (w.kind == "label") return ui.simple ? ui.labelH : ui.rowH;
    if (w.kind == "button") return ui.buttonH;
    if (isSingle(w)) return 30;
    if (w.kind == "row" || w.kind == "column") {
        float pad = optF(w, "padding", 0), gap = optF(w, "gap", 0), s = 0; int n = 0;
        for (auto& c : w.children) { if (isShortcut(c)) continue; float p = prefH(c); s = w.kind == "column" ? s + p : std::max(s, p); n++; }
        return s + (w.kind == "column" ? gap * std::max(0, n - 1) : 0) + 2 * pad;
    }
    return 0;
}
float growOf(const GuiWidget& w, bool horiz) {
    auto it = w.opts.find("grow");
    if (it != w.opts.end()) return std::max(0.0f, (float)atof(it->second.c_str()));
    if (optF(w, horiz ? "width" : "height", -1) >= 0) return 0;
    if (isFlex(w)) return 1;
    if (w.kind == "row" || w.kind == "column") return (horiz ? prefW(w) : prefH(w)) <= 0 ? 1 : 0;
    return 0;
}

int g_keyCounter = 0;
void layoutNode(GuiWidget& w, float x, float y, float W, float H) {
    w.x = x; w.y = y; w.w = W; w.h = H;
    std::string id = optS(w, "id");
    w.key = !id.empty() ? id : (w.kind + "#" + std::to_string(g_keyCounter++));
    if (w.kind != "row" && w.kind != "column") return;
    bool horiz = w.kind == "row";
    float pad = optF(w, "padding", 0), gap = optF(w, "gap", 0);
    std::string align = optS(w, "align", "left");
    float ix = x + pad, iy = y + pad, iw = std::max(0.0f, W - 2 * pad), ih = std::max(0.0f, H - 2 * pad);
    std::vector<GuiWidget*> kids;
    for (auto& c : w.children) if (!isShortcut(c)) kids.push_back(&c);
    if (kids.empty()) return;
    float total = (horiz ? iw : ih) - gap * (float)(kids.size() - 1);
    float fixedSum = 0, growSum = 0;
    std::vector<float> size(kids.size()), grow(kids.size());
    for (size_t i = 0; i < kids.size(); i++) {
        grow[i] = growOf(*kids[i], horiz);
        if (grow[i] > 0) growSum += grow[i];
        else { size[i] = horiz ? prefW(*kids[i]) : prefH(*kids[i]); fixedSum += size[i]; }
    }
    float remaining = std::max(0.0f, total - fixedSum);
    for (size_t i = 0; i < kids.size(); i++) if (grow[i] > 0) size[i] = growSum > 0 ? remaining * grow[i] / growSum : 0;
    float pos = horiz ? ix : iy, cross = horiz ? ih : iw;
    for (size_t i = 0; i < kids.size(); i++) {
        GuiWidget& c = *kids[i];
        bool textual = c.kind == "label" || c.kind == "button";
        float explicitCross = optF(c, horiz ? "height" : "width", -1);
        float cs = explicitCross >= 0 ? std::min(cross, explicitCross) : textual ? std::min(cross, horiz ? prefH(c) : prefW(c)) : cross;
        float off = 0;
        if (cs < cross) off = horiz ? (cross - cs) / 2 : (align == "center" ? (cross - cs) / 2 : align == "right" ? cross - cs : 0);
        if (horiz) layoutNode(c, pos, iy + off, size[i], cs);
        else layoutNode(c, ix + off, pos, cs, size[i]);
        pos += size[i] + gap;
    }
}

GuiWidget* hitTest(GuiWidget& w, Vector2 p) {
    if (isShortcut(w)) return nullptr;
    if (!(p.x >= w.x && p.x < w.x + w.w && p.y >= w.y && p.y < w.y + w.h)) return nullptr;
    for (size_t i = w.children.size(); i-- > 0;) if (GuiWidget* h = hitTest(w.children[i], p)) return h;
    return &w;
}
GuiWidget* findByKey(GuiWidget& w, const std::string& key) {
    if (w.key == key) return &w;
    for (auto& c : w.children) if (GuiWidget* f = findByKey(c, key)) return f;
    return nullptr;
}

// scroll positions of text views and lists
std::map<std::string, float> g_scroll;
std::map<std::string, size_t> g_textHash;

// ════════════════════════ one frame: input and drawing ════════════════════════

struct Frame {
    Vector2 mouse{0, 0};
    float wheelY = 0, wheelX = 0;
    int cursor = MOUSE_CURSOR_DEFAULT;
    std::string focus;               // key of the editor that has the keyboard
    std::string dragging;            // key of the editor where a mouse selection started
    double lastClick = 0; int lastClickRow = -1, lastClickCol = -1;
    bool rebuild = false;
    std::function<void(int, int)> onEvent;
} fr;

void fireEvent(int id, int item) { if (id >= 0) { fr.onEvent(id, item); } fr.rebuild = true; }

bool inRect(Vector2 p, const GuiWidget& w) { return p.x >= w.x && p.x < w.x + w.w && p.y >= w.y && p.y < w.y + w.h; }

struct EditorMetrics { float gutterW, textX, textTop, viewW, viewH; };
EditorMetrics editorMetrics(const GuiWidget& w, const Editor& e) {
    EditorMetrics m;
    bool single = isSingle(w);
    int digits = 2; for (size_t n = e.lines.size(); n >= 100; n /= 10) digits++;
    m.gutterW = single ? 0 : (digits + 1) * ui.charW + 14;
    float pad = single ? 8.0f : 10.0f;
    m.textX = w.x + m.gutterW + pad;
    m.textTop = w.y + (single ? (w.h - ui.lineH) / 2 : 8.0f);
    m.viewW = w.w - m.gutterW - pad;
    m.viewH = single ? ui.lineH : w.h - 8.0f;
    return m;
}

void clampScroll(const GuiWidget& w, Editor& e) {
    EditorMetrics m = editorMetrics(w, e);
    float contentH = e.lines.size() * ui.lineH + ui.lineH * 2;
    size_t longest = 0; for (auto& l : e.lines) longest = std::max(longest, l.size());
    float contentW = longest * ui.charW + ui.charW * 4;
    e.scrollY = std::max(0.0f, std::min(e.scrollY, std::max(0.0f, contentH - m.viewH)));
    e.scrollX = std::max(0.0f, std::min(e.scrollX, std::max(0.0f, contentW - m.viewW)));
}
void revealCursor(const GuiWidget& w, Editor& e) {
    EditorMetrics m = editorMetrics(w, e);
    float cy = e.row * ui.lineH, cx = e.col * ui.charW;
    if (cy < e.scrollY) e.scrollY = cy;
    if (cy + ui.lineH > e.scrollY + m.viewH) e.scrollY = cy + ui.lineH - m.viewH;
    if (cx < e.scrollX) e.scrollX = std::max(0.0f, cx - ui.charW * 3);
    if (cx + ui.charW * 2 > e.scrollX + m.viewW) e.scrollX = cx + ui.charW * 2 - m.viewW + ui.charW * 3;
    clampScroll(w, e);
    e.reveal = false;
}

void mousePosToCursor(const GuiWidget& w, Editor& e, Vector2 p, int& r, int& c) {
    EditorMetrics m = editorMetrics(w, e);
    r = (int)std::floor((p.y - m.textTop + e.scrollY) / ui.lineH);
    r = std::max(0, std::min(r, (int)e.lines.size() - 1));
    c = (int)std::floor((p.x - m.textX + e.scrollX) / ui.charW + 0.5f);
    c = std::max(0, std::min(c, (int)e.lines[r].size()));
}

bool ctrlDown() { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER); }
bool shiftDown() { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
bool altDown() { return IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT); }
bool pressed(int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); }

// keyboard input for the editor that has the focus. Returns true when something changed.
void editorKeys(GuiWidget& w, Editor& e) {
    bool single = isSingle(w);
    bool ctrl = ctrlDown(), shift = shiftDown(), alt = altDown();
    double t = GetTime();
    auto touched = [&]() { e.lastInput = t; };

    // typed characters
    for (int ch = GetCharPressed(); ch != 0; ch = GetCharPressed()) {
        if (ch < 32 || ch == 127 || ctrl) continue;
        pushUndo(e, 1); insertText(e, U32(1, (char32_t)ch)); touched();
    }
    if (ctrl && !alt) {
        if (IsKeyPressed(KEY_A)) { e.arow = 0; e.acol = 0; e.row = (int)e.lines.size() - 1; e.col = (int)e.lines.back().size(); e.reveal = true; touched(); }
        if (IsKeyPressed(KEY_C) && hasSel(e)) SetClipboardText(toU8(selectedText(e)).c_str());
        if (IsKeyPressed(KEY_X) && hasSel(e)) { SetClipboardText(toU8(selectedText(e)).c_str()); pushUndo(e, 0); deleteSel(e); touched(); }
        if (IsKeyPressed(KEY_V)) {
            const char* clip = GetClipboardText();
            if (clip) {
                U32 s = toU32(clip), clean;
                for (char32_t c : s) { if (c == U'\r') continue; if (c == U'\t') clean += U"    "; else if (single && c == U'\n') clean += U' '; else clean += c; }
                pushUndo(e, 0); insertText(e, clean); touched();
            }
        }
        if (pressed(KEY_Z) && !shift) {
            if (!e.undo.empty()) { e.redo.push_back({e.lines, e.row, e.col}); Snap s = e.undo.back(); e.undo.pop_back(); e.lines = s.lines; e.row = e.arow = s.row; e.col = e.acol = s.col; e.lastKind = 0; e.dirty = true; e.reveal = true; touched(); }
        }
        if (pressed(KEY_Y) || (pressed(KEY_Z) && shift)) {
            if (!e.redo.empty()) { e.undo.push_back({e.lines, e.row, e.col}); Snap s = e.redo.back(); e.redo.pop_back(); e.lines = s.lines; e.row = e.arow = s.row; e.col = e.acol = s.col; e.lastKind = 0; e.dirty = true; e.reveal = true; touched(); }
        }
    }
    if (pressed(KEY_LEFT)) { moveHorizontal(e, -1, shift, ctrl); e.lastKind = 0; touched(); }
    if (pressed(KEY_RIGHT)) { moveHorizontal(e, +1, shift, ctrl); e.lastKind = 0; touched(); }
    if (!single) {
        if (pressed(KEY_UP)) { moveVertical(e, -1, shift); e.lastKind = 0; touched(); }
        if (pressed(KEY_DOWN)) { moveVertical(e, +1, shift); e.lastKind = 0; touched(); }
        int page = std::max(1, (int)(w.h / ui.lineH) - 1);
        if (pressed(KEY_PAGE_UP)) { moveVertical(e, -page, shift); touched(); }
        if (pressed(KEY_PAGE_DOWN)) { moveVertical(e, +page, shift); touched(); }
    }
    if (pressed(KEY_HOME)) {
        if (ctrl && !single) setCursor(e, 0, 0, shift);
        else { int first = leadingSpaces(e.lines[e.row]); setCursor(e, e.row, e.col == first ? 0 : first, shift); }
        e.wantCol = -1; e.lastKind = 0; touched();
    }
    if (pressed(KEY_END)) {
        if (ctrl && !single) setCursor(e, (int)e.lines.size() - 1, (int)e.lines.back().size(), shift);
        else setCursor(e, e.row, (int)e.lines[e.row].size(), shift);
        e.wantCol = -1; e.lastKind = 0; touched();
    }
    if (pressed(KEY_BACKSPACE)) {
        if (hasSel(e)) { pushUndo(e, 0); deleteSel(e); }
        else if (e.col > 0) { pushUndo(e, 2); e.lines[e.row].erase(e.col - 1, 1); e.col--; e.acol = e.col; e.dirty = true; e.reveal = true; }
        else if (e.row > 0 && !single) {
            pushUndo(e, 0); int len = (int)e.lines[e.row - 1].size();
            e.lines[e.row - 1] += e.lines[e.row]; e.lines.erase(e.lines.begin() + e.row); e.row--; e.col = len; e.arow = e.row; e.acol = e.col; e.dirty = true; e.reveal = true;
        }
        e.wantCol = -1; touched();
    }
    if (pressed(KEY_DELETE)) {
        if (hasSel(e)) { pushUndo(e, 0); deleteSel(e); }
        else if (e.col < (int)e.lines[e.row].size()) { pushUndo(e, 2); e.lines[e.row].erase(e.col, 1); e.dirty = true; e.reveal = true; }
        else if (e.row + 1 < (int)e.lines.size() && !single) { pushUndo(e, 0); e.lines[e.row] += e.lines[e.row + 1]; e.lines.erase(e.lines.begin() + e.row + 1); e.dirty = true; e.reveal = true; }
        e.wantCol = -1; touched();
    }
    if (!single && !ctrl && (pressed(KEY_ENTER) || pressed(KEY_KP_ENTER))) {
        pushUndo(e, 0);
        int sr1, sc1, sr2, sc2; selRange(e, sr1, sc1, sr2, sc2);          // when nothing is selected this is the cursor
        const U32& line = e.lines[sr1];
        U32 before = line.substr(0, std::min((int)line.size(), sc1));
        int indent = leadingSpaces(before);
        std::string trimmed = toU8(before); size_t a = trimmed.find_first_not_of(' ');
        trimmed = a == std::string::npos ? "" : trimmed.substr(a);
        std::string firstWord = trimmed.substr(0, trimmed.find_first_of(" \t"));
        bool opens = (firstWord == "def" || firstWord == "if" || firstWord == "else" || firstWord == "while" || firstWord == "for") ||
                     (!trimmed.empty() && trimmed.back() == ':');
        U32 ins = U"\n" + U32(indent + (opens ? 4 : 0), U' ');
        insertText(e, ins); touched();
    }
    if (pressed(KEY_TAB) && !single) {
        int r1, c1, r2, c2; selRange(e, r1, c1, r2, c2);
        if (shift) indentLines(e, true);
        else if (r1 != r2) indentLines(e, false);
        else { pushUndo(e, 0); insertText(e, U"    "); }
        touched();
    }
}

void drawEditor(GuiWidget& w, Editor& e, bool focused) {
    bool single = isSingle(w);
    Color bg = parseColor(optS(w, "background"), single ? mixColor(ui.bg, ui.text, ui.dark ? 0.10f : 0.04f) : ui.bg);
    Color fg = parseColor(optS(w, "color"), ui.text);
    bool darkBg = luminance(bg) < 128;
    Color muted = mixColor(bg, fg, 0.45f), lineHi = mixColor(bg, fg, 0.06f), selCol = rgba(ui.accent.r, ui.accent.g, ui.accent.b, darkBg ? 140 : 90);
    Color palette[7];
    if (darkBg) { palette[C_TEXT] = fg; palette[C_KEYWORD] = rgba(197, 134, 192); palette[C_STRING] = rgba(206, 145, 120); palette[C_INTERP] = rgba(86, 156, 214);
                  palette[C_COMMENT] = rgba(106, 153, 85); palette[C_NUMBER] = rgba(181, 206, 168); palette[C_BUILTIN] = rgba(220, 220, 170); }
    else { palette[C_TEXT] = fg; palette[C_KEYWORD] = rgba(175, 0, 219); palette[C_STRING] = rgba(163, 21, 21); palette[C_INTERP] = rgba(4, 81, 165);
           palette[C_COMMENT] = rgba(0, 128, 0); palette[C_NUMBER] = rgba(9, 134, 88); palette[C_BUILTIN] = rgba(121, 94, 38); }
    bool fa = optS(w, "language") == "fa";

    Rectangle r = {w.x, w.y, w.w, w.h};
    DrawRectangleRec(r, bg);
    if (single) DrawRectangleLinesEx(r, 1, focused ? ui.accent : mixColor(bg, fg, 0.25f));
    if (e.reveal) revealCursor(w, e); else clampScroll(w, e);
    EditorMetrics m = editorMetrics(w, e);

    // mouse wheel
    if (inRect(fr.mouse, w) && !single) {
        if (fr.wheelY != 0) e.scrollY -= fr.wheelY * ui.lineH * 3;
        if (fr.wheelX != 0) e.scrollX -= fr.wheelX * ui.charW * 6;
        clampScroll(w, e);
    }
    int firstRow = std::max(0, (int)std::floor(e.scrollY / ui.lineH));
    int lastRow = std::min((int)e.lines.size() - 1, (int)std::floor((e.scrollY + m.viewH) / ui.lineH));
    int firstCol = std::max(0, (int)std::floor(e.scrollX / ui.charW) - 1);
    int visCols = (int)(m.viewW / ui.charW) + 4;

    // gutter: line numbers
    if (!single) {
        BeginScissorMode((int)w.x, (int)w.y, (int)m.gutterW, (int)w.h);
        DrawRectangle((int)w.x, (int)w.y, (int)m.gutterW, (int)w.h, mixColor(bg, fg, darkBg ? 0.03f : 0.05f));
        for (int row = firstRow; row <= lastRow; row++) {
            std::string num = std::to_string(row + 1);
            float y = m.textTop + row * ui.lineH - e.scrollY;
            Color nc = row == e.row ? fg : muted;
            DrawTextEx(ui.mono, num.c_str(), Vector2{(float)(int)(w.x + m.gutterW - 10 - num.size() * ui.charW), (float)(int)y + (ui.lineH - ui.monoFs) / 2}, (float)ui.monoFs, 0, nc);
        }
        DrawLine((int)(w.x + m.gutterW), (int)w.y, (int)(w.x + m.gutterW), (int)(w.y + w.h), mixColor(bg, fg, 0.15f));
        EndScissorMode();
    }

    // text
    BeginScissorMode((int)(w.x + m.gutterW + 1), (int)w.y, (int)(w.w - m.gutterW - 1), (int)w.h);
    int r1, c1, r2, c2; selRange(e, r1, c1, r2, c2);
    bool sel = hasSel(e);
    std::vector<unsigned char> cls;
    for (int row = firstRow; row <= lastRow; row++) {
        const U32& line = e.lines[row];
        float y = m.textTop + row * ui.lineH - e.scrollY;
        if (!single && row == e.row && !sel && focused) DrawRectangle((int)(w.x + m.gutterW + 1), (int)y, (int)(w.w - m.gutterW), (int)ui.lineH, lineHi);
        if (sel && row >= r1 && row <= r2) {
            int a = row == r1 ? c1 : 0, b = row == r2 ? c2 : (int)line.size() + 1;
            DrawRectangle((int)(m.textX + a * ui.charW - e.scrollX), (int)y, (int)((b - a) * ui.charW), (int)ui.lineH, selCol);
        }
        if (fa) highlightFa(line, cls); else cls.assign(line.size(), C_TEXT);
        int end = std::min((int)line.size(), firstCol + visCols);
        int i = firstCol;
        while (i < end) {
            int j = i; while (j < end && cls[j] == cls[i]) j++;
            std::string seg = toU8(line.substr(i, j - i));
            DrawTextEx(ui.mono, seg.c_str(), Vector2{(float)(int)(m.textX + i * ui.charW - e.scrollX), (float)(int)y + (ui.lineH - ui.monoFs) / 2}, (float)ui.monoFs, 0, palette[cls[i]]);
            i = j;
        }
    }
    if (single && e.lines.size() == 1 && e.lines[0].empty() && !optS(w, "placeholder").empty())
        DrawTextEx(ui.mono, optS(w, "placeholder").c_str(), Vector2{m.textX, (float)(int)m.textTop + (ui.lineH - ui.monoFs) / 2}, (float)ui.monoFs, 0, muted);
    if (focused && std::fmod(GetTime() - e.lastInput, 1.06) < 0.58) {
        float cx = m.textX + e.col * ui.charW - e.scrollX, cy = m.textTop + e.row * ui.lineH - e.scrollY;
        DrawRectangle((int)cx, (int)cy + 2, 2, (int)ui.lineH - 4, fg);
    }
    EndScissorMode();

    // scrollbars (just an indicator)
    if (!single) {
        float contentH = e.lines.size() * ui.lineH + ui.lineH * 2;
        if (contentH > m.viewH) {
            float th = std::max(24.0f, w.h * (m.viewH / contentH)), ty = w.y + (w.h - th) * (e.scrollY / std::max(1.0f, contentH - m.viewH));
            DrawRectangleRounded(Rectangle{w.x + w.w - 9, ty, 6, th}, 0.5f, 4, mixColor(bg, fg, 0.25f));
        }
    }
}

void drawTextView(GuiWidget& w) {
    Color bg = parseColor(optS(w, "background"), mixColor(ui.bg, ui.text, ui.dark ? 0.05f : 0.04f));
    Color fg = parseColor(optS(w, "color"), ui.text);
    DrawRectangle((int)w.x, (int)w.y, (int)w.w, (int)w.h, bg);
    size_t h = std::hash<std::string>()(w.text);
    if (g_textHash[w.key] != h) { g_textHash[w.key] = h; g_scroll[w.key] = 0; }        // new text: start at the top
    std::vector<std::string> lines; size_t start = 0;
    for (size_t i = 0; i <= w.text.size(); i++) if (i == w.text.size() || w.text[i] == '\n') { std::string l = w.text.substr(start, i - start); if (!l.empty() && l.back() == '\r') l.pop_back(); lines.push_back(l); start = i + 1; }
    float contentH = lines.size() * ui.lineH + 12, viewH = w.h;
    float& sc = g_scroll[w.key];
    if (inRect(fr.mouse, w) && fr.wheelY != 0) sc -= fr.wheelY * ui.lineH * 3;
    sc = std::max(0.0f, std::min(sc, std::max(0.0f, contentH - viewH)));
    BeginScissorMode((int)w.x, (int)w.y, (int)w.w, (int)w.h);
    for (size_t i = 0; i < lines.size(); i++) {
        float y = w.y + 6 + i * ui.lineH - sc;
        if (y + ui.lineH < w.y || y > w.y + w.h) continue;
        DrawTextEx(ui.mono, lines[i].c_str(), Vector2{(float)(int)(w.x + 10), (float)(int)y + (ui.lineH - ui.monoFs) / 2}, (float)ui.monoFs, 0, fg);
    }
    EndScissorMode();
    if (contentH > viewH) {
        float th = std::max(24.0f, w.h * (viewH / contentH)), ty = w.y + (w.h - th) * (sc / std::max(1.0f, contentH - viewH));
        DrawRectangleRounded(Rectangle{w.x + w.w - 9, ty, 6, th}, 0.5f, 4, mixColor(bg, fg, 0.25f));
    }
}

const float LIST_ROW = 28;
void drawListBox(GuiWidget& w) {
    Color bg = parseColor(optS(w, "background"), ui.bg);
    Color fg = parseColor(optS(w, "color"), ui.text);
    DrawRectangle((int)w.x, (int)w.y, (int)w.w, (int)w.h, bg);
    float& sc = g_scroll[w.key];
    float contentH = w.items.size() * LIST_ROW;
    if (inRect(fr.mouse, w) && fr.wheelY != 0) sc -= fr.wheelY * LIST_ROW * 2;
    sc = std::max(0.0f, std::min(sc, std::max(0.0f, contentH - w.h)));
    std::string selected = optS(w, "selected");
    BeginScissorMode((int)w.x, (int)w.y, (int)w.w, (int)w.h);
    for (size_t i = 0; i < w.items.size(); i++) {
        float y = w.y + i * LIST_ROW - sc;
        if (y + LIST_ROW < w.y || y > w.y + w.h) continue;
        Rectangle row = {w.x, y, w.w, LIST_ROW};
        bool hover = CheckCollisionPointRec(fr.mouse, row) && inRect(fr.mouse, w);
        if (w.items[i] == selected) DrawRectangleRec(row, mixColor(bg, fg, 0.16f));
        else if (hover) DrawRectangleRec(row, mixColor(bg, fg, 0.08f));
        DrawTextEx(ui.font, w.items[i].c_str(), Vector2{(float)(int)(w.x + 14), (float)(int)(y + (LIST_ROW - ui.labelFs) / 2)}, (float)ui.labelFs, 0, w.items[i] == selected ? fg : mixColor(fg, bg, 0.12f));
    }
    EndScissorMode();
}

void drawNode(GuiWidget& w) {
    if (isShortcut(w)) return;
    std::string bgOpt = optS(w, "background");
    if (!bgOpt.empty() && (w.kind == "row" || w.kind == "column"))
        DrawRectangle((int)w.x, (int)w.y, (int)w.w, (int)w.h, parseColor(bgOpt, ui.bg));
    if (w.kind == "row" || w.kind == "column") { for (auto& c : w.children) drawNode(c); return; }
    Color fg = parseColor(optS(w, "color"), ui.text);
    if (w.kind == "label") {
        float th = (float)ui.labelFs;
        DrawTextEx(ui.font, w.text.c_str(), Vector2{(float)(int)w.x, (float)(int)(w.y + (w.h - th) / 2)}, th, 0, fg);
    } else if (w.kind == "button") {
        bool hover = inRect(fr.mouse, w);
        Color base = parseColor(optS(w, "background"), ui.accent);
        Color col = hover ? mixColor(base, rgba(0, 0, 0), 0.22f) : base;
        DrawRectangleRounded(Rectangle{w.x, w.y, w.w, w.h}, ui.simple ? 0.22f : 0.18f, 10, col);
        Color tc = parseColor(optS(w, "color"), rgba(255, 255, 255));
        float tw = textW(ui.buttonFont, w.text, (float)ui.buttonFs);
        DrawTextEx(ui.buttonFont, w.text.c_str(), Vector2{(float)(int)(w.x + (w.w - tw) / 2), (float)(int)(w.y + (w.h - ui.buttonFs) / 2)}, (float)ui.buttonFs, 0, tc);
        if (hover) fr.cursor = MOUSE_CURSOR_POINTING_HAND;
    } else if (w.kind == "editor") {
        Editor& e = g_editors[w.text];
        drawEditor(w, e, fr.focus == w.key);
        if (inRect(fr.mouse, w)) fr.cursor = MOUSE_CURSOR_IBEAM;
    } else if (w.kind == "textview") {
        drawTextView(w);
    } else if (w.kind == "listbox") {
        drawListBox(w);
        if (inRect(fr.mouse, w)) fr.cursor = MOUSE_CURSOR_POINTING_HAND;
    }
}

bool allSimple(const GuiWidget& root) {
    for (auto& c : root.children) if (c.kind != "label" && c.kind != "button" && c.kind != "shortcut") return false;
    return true;
}
int simpleHeight(const GuiWidget& root) {
    int h = SIMPLE_TOP;
    for (auto& c : root.children) if (!isShortcut(c)) h += (int)(c.kind == "button" ? 46 : 28) + SIMPLE_GAP;
    return std::max(180, h + SIMPLE_TOP / 2);
}

int keyFromName(const std::string& n) {
    if (n.size() == 1 && n[0] >= 'a' && n[0] <= 'z') return KEY_A + (n[0] - 'a');
    if (n.size() == 1 && n[0] >= '0' && n[0] <= '9') return KEY_ZERO + (n[0] - '0');
    if (n == "enter" || n == "return") return KEY_ENTER;
    if (n == "space") return KEY_SPACE;
    if (n == "escape" || n == "esc") return KEY_ESCAPE;
    if (n == "tab") return KEY_TAB;
    if (n.size() >= 2 && n[0] == 'f') { int k = atoi(n.c_str() + 1); if (k >= 1 && k <= 12) return KEY_F1 + (k - 1); }
    return KEY_NULL;
}
// shortcut "ctrl+s" -> fires when exactly these keys are pressed
bool shortcutPressed(const std::string& spec) {
    bool ctrl = false, shift = false, alt = false; int key = KEY_NULL;
    std::string part;
    for (size_t i = 0; i <= spec.size(); i++) {
        if (i == spec.size() || spec[i] == '+') {
            for (auto& ch : part) ch = (char)tolower((unsigned char)ch);
            if (part == "ctrl" || part == "strg" || part == "cmd") ctrl = true;
            else if (part == "shift") shift = true;
            else if (part == "alt") alt = true;
            else key = keyFromName(part);
            part.clear();
        } else part += spec[i];
    }
    return key != KEY_NULL && IsKeyPressed(key) && ctrlDown() == ctrl && shiftDown() == shift && altDown() == alt;
}
void collectShortcuts(GuiWidget& w, std::vector<GuiWidget*>& out) {
    if (isShortcut(w)) out.push_back(&w);
    for (auto& c : w.children) collectShortcuts(c, out);
}

}  // namespace

// ════════════════════════ public functions ════════════════════════

void fa_gui_editor_set(const std::string& name, const std::string& text) { setText(g_editors[name], text); }
std::string fa_gui_editor_get(const std::string& name) { auto it = g_editors.find(name); return it == g_editors.end() ? "" : getText(it->second); }
bool fa_gui_editor_dirty(const std::string& name) { auto it = g_editors.find(name); return it != g_editors.end() && it->second.dirty; }
void fa_gui_editor_clean(const std::string& name) { auto it = g_editors.find(name); if (it != g_editors.end()) it->second.dirty = false; }
void fa_gui_editor_info(const std::string& name, int& line, int& column, int& lines) {
    auto it = g_editors.find(name);
    if (it == g_editors.end()) { line = 1; column = 1; lines = 1; return; }
    line = it->second.row + 1; column = it->second.col + 1; lines = (int)it->second.lines.size();
}

void fa_gui_run(const std::string& title, const std::map<std::string, std::string>& wopts,
                const std::function<GuiWidget()>& build, const std::function<void(int, int)>& onEvent) {
    SetTraceLogLevel(LOG_ERROR);                  // hide raylib's startup chatter
    GuiWidget root = build();                     // build first, so a mistake in your code doesn't flash an empty window
    ui.simple = allSimple(root);
    GuiWidget windowCfg; windowCfg.opts = wopts;
    ui.bg = parseColor(optS(windowCfg, "background"), rgba(244, 245, 247));
    ui.text = parseColor(optS(windowCfg, "color"), rgba(34, 34, 40));
    ui.accent = parseColor(optS(windowCfg, "accent"), rgba(37, 99, 235));
    ui.dark = luminance(ui.bg) < 128;
    if (ui.simple) { ui.labelFs = 22; ui.buttonFs = 20; ui.buttonH = 46; ui.labelH = 28; }
    else { ui.labelFs = 16; ui.buttonFs = 15; ui.buttonH = 32; ui.rowH = 24; }
    ui.monoFs = 15; ui.lineH = (float)(int)(ui.monoFs * 1.5f);

    int W = (int)optF(windowCfg, "width", ui.simple ? SIMPLE_W : 1000), H = (int)optF(windowCfg, "height", ui.simple ? (float)simpleHeight(root) : 680);
    SetConfigFlags(FLAG_MSAA_4X_HINT | (ui.simple ? 0 : FLAG_WINDOW_RESIZABLE));
    InitWindow(W, H, title.c_str());
    if (!ui.simple) { SetWindowMinSize(560, 360); SetExitKey(KEY_NULL); }      // Esc must not close an editor
    SetTargetFPS(60);

    std::string uiFile = findFontFile("FA_FONT", "sans", {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/dejavu/DejaVuSans.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf", "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf", "/usr/share/fonts/noto/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf", "/Library/Fonts/Arial.ttf"});
    std::string monoFile = findFontFile("FA_MONO_FONT", "monospace", {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", "/usr/share/fonts/dejavu/DejaVuSansMono.ttf", "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf", "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf", "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
        "C:/Windows/Fonts/consola.ttf", "C:/Windows/Fonts/cour.ttf", "/Library/Fonts/Courier New.ttf"});
    ui.font = loadUiFont(uiFile, ui.labelFs);
    ui.buttonFont = loadUiFont(uiFile, ui.buttonFs);
    ui.mono = loadUiFont(monoFile, ui.monoFs);
    ui.charW = MeasureTextEx(ui.mono, "M", (float)ui.monoFs, 0).x;
    if (ui.charW < 1) ui.charW = 9;

    fr.onEvent = onEvent;
    auto cleanup = [&]() { unloadUiFont(ui.font); unloadUiFont(ui.buttonFont); unloadUiFont(ui.mono); };
    try {
        while (!WindowShouldClose()) {
            // 1. layout for the current window size
            g_keyCounter = 0;
            if (ui.simple) { root.opts["gap"] = std::to_string(SIMPLE_GAP); root.opts["align"] = "center"; }
            if (ui.simple) layoutNode(root, 0, (float)SIMPLE_TOP, (float)GetScreenWidth(), (float)(GetScreenHeight() - SIMPLE_TOP));
            else layoutNode(root, 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight());

            // 2. input
            fr.mouse = GetMousePosition();
            fr.wheelY = GetMouseWheelMoveV().y; fr.wheelX = GetMouseWheelMoveV().x;
            if (shiftDown() && fr.wheelX == 0) { fr.wheelX = fr.wheelY; fr.wheelY = 0; }
            fr.cursor = MOUSE_CURSOR_DEFAULT;
            fr.rebuild = false;

            std::vector<GuiWidget*> shortcuts; collectShortcuts(root, shortcuts);
            bool fired = false;
            for (GuiWidget* s : shortcuts) if (!fired && s->id >= 0 && shortcutPressed(s->text)) { fireEvent(s->id, -1); fired = true; }

            if (!fired && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                GuiWidget* h = hitTest(root, fr.mouse);
                fr.focus.clear(); fr.dragging.clear();
                if (h) {
                    if (h->kind == "button") fireEvent(h->id, -1);
                    else if (h->kind == "listbox") {
                        int item = (int)std::floor((fr.mouse.y - h->y + g_scroll[h->key]) / LIST_ROW);
                        if (item >= 0 && item < (int)h->items.size()) fireEvent(h->id, item);
                    } else if (h->kind == "editor") {
                        Editor& e = g_editors[h->text];
                        fr.focus = h->key; fr.dragging = h->key;
                        int r, c; mousePosToCursor(*h, e, fr.mouse, r, c);
                        double now = GetTime();
                        if (now - fr.lastClick < 0.35 && r == fr.lastClickRow && c == fr.lastClickCol) { selectWordAt(e, r, c); fr.lastClick = 0; fr.dragging.clear(); }      // a word is selected: don't let the held button drag the cursor away
                        else { setCursor(e, r, c, shiftDown()); e.wantCol = -1; fr.lastClick = now; fr.lastClickRow = r; fr.lastClickCol = c; }
                        e.lastKind = 0; e.lastInput = now; fr.rebuild = true;
                    }
                }
            } else if (!fr.dragging.empty() && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                if (GuiWidget* h = findByKey(root, fr.dragging)) {
                    Editor& e = g_editors[h->text];
                    int r, c; mousePosToCursor(*h, e, fr.mouse, r, c);
                    if (r != e.row || c != e.col) { e.row = r; e.col = c; e.reveal = true; fr.rebuild = true; }
                }
            }
            if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) fr.dragging.clear();

            if (!fr.focus.empty() && !fired) {
                if (GuiWidget* h = findByKey(root, fr.focus)) {
                    Editor& e = g_editors[h->text];
                    int br = e.row, bc = e.col, bl = (int)e.lines.size(); bool bd = e.dirty;
                    editorKeys(*h, e);
                    if (isSingle(*h) && !ctrlDown() && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) fireEvent(h->id, -1);
                    if (br != e.row || bc != e.col || bl != (int)e.lines.size() || bd != e.dirty) fr.rebuild = true;
                }
            }

            // 3. build the window again with the new values
            if (fr.rebuild) {
                root = build();
                g_keyCounter = 0;
                if (ui.simple) {
                    int h = simpleHeight(root);
                    if (h != GetScreenHeight() && !wopts.count("height")) SetWindowSize(GetScreenWidth(), h);
                    root.opts["gap"] = std::to_string(SIMPLE_GAP); root.opts["align"] = "center";
                    layoutNode(root, 0, (float)SIMPLE_TOP, (float)GetScreenWidth(), (float)(GetScreenHeight() - SIMPLE_TOP));
                } else layoutNode(root, 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight());
            }

            // 4. draw
            BeginDrawing();
            ClearBackground(ui.bg);
            drawNode(root);
            EndDrawing();
            SetMouseCursor(fr.cursor);
        }
    } catch (...) { cleanup(); CloseWindow(); throw; }
    cleanup();
    CloseWindow();
}
