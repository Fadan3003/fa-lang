#pragma once
// HTML builders and JSON encoding (used by the html and json modules).
#include "helpers.h"

// ═══════════════════════════ HTML & JSON helpers ═══════════════════════════

static std::string escapeHtml(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '&': r += "&amp;"; break; case '<': r += "&lt;"; break; case '>': r += "&gt;"; break;
            case '"': r += "&quot;"; break; case '\'': r += "&#39;"; break; default: r += c;
        }
    }
    return r;
}

// Text is escaped (safe) unless it was built by a tag function or raw(...).
static std::string renderHtml(const Value& v) {
    switch (v.t) {
        case T::Nil: return "";
        case T::Str: return static_cast<StrObj*>(v.o.get())->html ? S(v) : escapeHtml(S(v));
        case T::List: { std::string r; for (auto& x : L(v)) r += renderHtml(x); return r; }
        default: return escapeHtml(show(v));
    }
}

static std::string attrString(const Value& m) {
    std::string r;
    for (auto& kv : M(m)->v) {
        const Value& val = kv.second;
        if (val.t == T::Nil || (val.t == T::Bool && !val.b)) continue;
        r += " " + show(kv.first);
        if (val.t != T::Bool) r += "=\"" + escapeHtml(show(val)) + "\"";
    }
    return r;
}

static Value buildTag(const std::string& tag, const std::vector<Value>& a, size_t start, bool isVoid, const std::string& extra = "") {
    std::string attrs = extra;
    if (a.size() > start && a[start].t == T::Map) { attrs += attrString(a[start]); start++; }
    std::string out = "<" + tag + attrs + ">";
    if (!isVoid) {
        for (size_t i = start; i < a.size(); i++) out += renderHtml(a[i]);
        out += "</" + tag + ">";
    }
    return Value::rawhtml(out);
}

static std::string pageHtml(const std::string& title, const std::string& body) {
    return "<!DOCTYPE html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>" + escapeHtml(title) +
           "</title><style>body{font-family:system-ui,sans-serif;max-width:720px;margin:40px auto;padding:0 16px;"
           "line-height:1.6;color:#222}a{color:#2563eb}button{padding:8px 16px;font-size:1rem;cursor:pointer}"
           "code,pre{background:#f3f4f6;padding:2px 6px;border-radius:4px}</style></head><body>" + body + "</body></html>";
}

static void toJson(const Value& v, std::string& out) {
    switch (v.t) {
        case T::Bool: out += v.b ? "true" : "false"; break;
        case T::Int: out += std::to_string(v.i); break;
        case T::Float: out += fmtFloat(v.d); break;
        case T::Str: {
            out += '"';
            for (unsigned char c : S(v)) {
                if (c == '"') out += "\\\""; else if (c == '\\') out += "\\\\";
                else if (c == '\n') out += "\\n"; else if (c == '\r') out += "\\r"; else if (c == '\t') out += "\\t";
                else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); out += b; }
                else out += (char)c;
            }
            out += '"'; break;
        }
        case T::List: {
            out += '['; bool first = true;
            for (auto& x : L(v)) { if (!first) out += ','; first = false; toJson(x, out); }
            out += ']'; break;
        }
        case T::Map: {
            out += '{'; bool first = true;
            for (auto& kv : M(v)->v) { if (!first) out += ','; first = false; toJson(Value::str(show(kv.first)), out); out += ':'; toJson(kv.second, out); }
            out += '}'; break;
        }
        default: out += "null";
    }
}

