#pragma once
// Lexer: turns source text into tokens. Indented blocks become Indent / Dedent tokens (like Python).
#include "common.h"
#include "text.h"

// ═══════════════════════════ Lexer ═══════════════════════════

enum class TK { Num, Str, Id, Kw, Op, Indent, Dedent, Eof };
struct Token {
    TK k; std::string s; bool isFloat = false; int64_t i = 0; double d = 0; int line = 1; bool nl = false;
};

static const std::unordered_set<std::string> KEYWORDS = {
    "def", "return", "if", "else", "while", "for", "in", "break", "continue",
    "true", "false", "nothing", "and", "or", "not"};
static const char* TWO_OPS[] = {"==", "!=", "<=", ">=", "..", "+=", "-=", "*=", "/="};
static const std::string ONE_OPS = "+-*/%<>=()[]{},.:";

// blocks=false: no indentation handling (used for the {values} inside text)
static std::vector<Token> tokenize(const std::string& src, int line = 1, bool blocks = true) {
    std::vector<Token> toks;
    size_t i = 0, n = src.size();
    bool nl = false, atStart = true;
    int col = 0, depth = 0;                       // depth: how many ( [ { are open (lines inside them don't count for indentation)
    std::vector<int> indents = {0};
    auto special = [&](TK k) { Token t; t.k = k; t.line = line; t.nl = true; toks.push_back(t); };
    auto push = [&](Token t) { t.line = line; t.nl = nl; nl = false; toks.push_back(std::move(t)); };
    while (i < n) {
        unsigned char c = src[i];
        if (c == '\n') { line++; nl = true; atStart = true; col = 0; i++; continue; }
        if (c >= 0xC2) {                                // other kinds of spaces (non-breaking, figure space ...): code copied out of the inspector uses them
            int sl = 0;
            if (c == 0xC2 && i + 1 < n && (unsigned char)src[i + 1] == 0xA0) sl = 2;
            else if (c == 0xE2 && i + 2 < n && (unsigned char)src[i + 1] == 0x80 &&
                     (((unsigned char)src[i + 2] >= 0x80 && (unsigned char)src[i + 2] <= 0x8A) || (unsigned char)src[i + 2] == 0xAF)) sl = 3;
            else if (c == 0xE2 && i + 2 < n && (unsigned char)src[i + 1] == 0xA0 && (unsigned char)src[i + 2] == 0x80) sl = 3;      // U+2800, used by show_code
            if (sl) { if (atStart) col++; i += sl; continue; }
        }
        if (c == ' ') { if (atStart) col++; i++; continue; }
        if (c == '\t') { if (atStart) col += 4; i++; continue; }
        if (c == '\r') { i++; continue; }
        if (c == '#') { while (i < n && src[i] != '\n') i++; continue; }      // # starts a comment
        if (atStart) {                                // first real thing on a line: compare its indentation
            atStart = false;
            if (blocks && depth == 0) {
                bool dedented = false;
                while (col < indents.back()) { indents.pop_back(); special(TK::Dedent); dedented = true; }
                if (col > indents.back()) {
                    if (dedented) throw FaError("line " + std::to_string(line) + ": this line is indented differently than any line above it");
                    indents.push_back(col); special(TK::Indent);
                }
            }
        }
        if (isdigit(c)) {
            size_t j = i;
            while (j < n && isdigit((unsigned char)src[j])) j++;
            Token t; t.k = TK::Num;
            if (j + 1 < n && src[j] == '.' && isdigit((unsigned char)src[j + 1])) {
                j++;
                while (j < n && isdigit((unsigned char)src[j])) j++;
                t.isFloat = true; t.d = strtod(src.substr(i, j - i).c_str(), nullptr);
            } else {
                t.i = strtoll(src.substr(i, j - i).c_str(), nullptr, 10);
            }
            push(t); i = j; continue;
        }
        if (isalpha(c) || c == '_' || c >= 0x80) {
            size_t j = i;
            while (j < n && (isalnum((unsigned char)src[j]) || src[j] == '_' || (unsigned char)src[j] >= 0x80)) j++;
            Token t; t.s = src.substr(i, j - i); t.k = KEYWORDS.count(t.s) ? TK::Kw : TK::Id;
            push(t); i = j; continue;
        }
        if (c == '"') {
            size_t j = i + 1; std::string buf; int startLine = line;
            while (j < n && src[j] != '"') {
                if (src[j] == '\\' && j + 1 < n) {
                    char e = src[j + 1];
                    if (e == 'u' && j + 2 < n && src[j + 2] == '{') {          // \u{1F600}
                        size_t close = src.find('}', j + 3);
                        if (close == std::string::npos) throw FaError("line " + std::to_string(line) + ": \\u{...} is never closed");
                        buf += utf8Encode((uint32_t)strtoul(src.substr(j + 3, close - j - 3).c_str(), nullptr, 16));
                        j = close + 1; continue;
                    }
                    buf += e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e == '0' ? '\0' : e == '{' ? '\x01' : e == '}' ? '\x02' : e;
                    j += 2;
                } else if (src[j] == '{' && j + 1 < n && src[j + 1] != '"') {
                    // {value} inside text. The value may contain "quoted text" itself:  "file {query.get("file")}"
                    size_t k = j + 1; int depth = 1; bool ok = false;
                    while (k < n && src[k] != '\n') {
                        char ch = src[k];
                        if (ch == '"') {
                            k++;
                            while (k < n && src[k] != '"' && src[k] != '\n') { if (src[k] == '\\') k++; k++; }
                            if (k >= n || src[k] != '"') break;
                            k++; continue;
                        }
                        if (ch == '{') depth++;
                        else if (ch == '}') { depth--; if (depth == 0) { ok = true; break; } }
                        k++;
                    }
                    if (ok) { buf.append(src, j, k - j + 1); j = k + 1; } else buf += src[j++];
                } else { if (src[j] == '\n') line++; buf += src[j++]; }
            }
            if (j >= n) throw FaError("line " + std::to_string(startLine) + ": string is never closed");
            Token t; t.k = TK::Str; t.s = buf;
            int saved = line; line = startLine; push(t); line = saved;
            i = j + 1; continue;
        }
        bool matched = false;
        if (i + 1 < n) {
            for (const char* op : TWO_OPS)
                if (src[i] == op[0] && src[i + 1] == op[1]) {
                    Token t; t.k = TK::Op; t.s = op; push(t); i += 2; matched = true; break;
                }
        }
        if (matched) continue;
        if (ONE_OPS.find((char)c) != std::string::npos) {
            if (c == '(' || c == '[' || c == '{') depth++;
            else if ((c == ')' || c == ']' || c == '}') && depth > 0) depth--;
            Token t; t.k = TK::Op; t.s = std::string(1, (char)c); push(t); i++; continue;
        }
        throw FaError("line " + std::to_string(line) + ": unexpected character '" + std::string(1, (char)c) + "'");
    }
    while (indents.size() > 1) { indents.pop_back(); special(TK::Dedent); }
    Token e; e.k = TK::Eof; e.nl = true; e.line = line; toks.push_back(e);
    return toks;
}

