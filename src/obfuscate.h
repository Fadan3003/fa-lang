#pragma once
// Hides the source of web pages:     serve 8080, {obfuscate: true}     or     serve 8080, {obfuscate: "light"}
//
//   "light": every page is sent encoded. "View source" and the network tab show only noise.
//            The page itself is not changed, so any script keeps working.
//   true:    on top of that every tag gets a meaningless name made of letters and digits (like  zk3p9x2 ),
//            classes and ids become attributes with such names (the words "class" and "id" disappear), and CSS is rewritten to match.
//            The inspector (F12) shows noise too, and the page still looks and works the same.
//            Scripts of your own that look things up by id, class or tag name need those names in  keep: [...]  (or use "light").
//
// Nothing can stay secret from the browser: whatever is shown on screen can be read by someone who tries hard enough.
#include "html.h"
#include <set>
#include <random>

static uint64_t fnv64(const std::string& s) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
    return h;
}
static std::string base36(uint64_t v, size_t len) {
    const char* d = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::string r;
    for (size_t i = 0; i < len; i++) { r += d[v % 36]; v /= 36; }
    return r;
}
static std::string lowerAscii(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

// what a tag looks like by default, so a renamed tag still looks the same
static const std::map<std::string, std::string>& tagDefaults() {
    static const std::map<std::string, std::string> m = {
        {"div", "display:block"}, {"section", "display:block"}, {"article", "display:block"}, {"header", "display:block"},
        {"footer", "display:block"}, {"nav", "display:block"}, {"main", "display:block"}, {"aside", "display:block"},
        {"p", "display:block;margin:1em 0"},
        {"h1", "display:block;font-size:2em;margin:.67em 0;font-weight:bold"},
        {"h2", "display:block;font-size:1.5em;margin:.83em 0;font-weight:bold"},
        {"h3", "display:block;font-size:1.17em;margin:1em 0;font-weight:bold"},
        {"h4", "display:block;margin:1.33em 0;font-weight:bold"},
        {"h5", "display:block;font-size:.83em;margin:1.67em 0;font-weight:bold"},
        {"h6", "display:block;font-size:.67em;margin:2.33em 0;font-weight:bold"},
        {"ul", "display:block;margin:1em 0;padding-left:40px;list-style-type:disc"},
        {"ol", "display:block;margin:1em 0;padding-left:40px;list-style-type:decimal"},
        {"li", "display:list-item"}, {"span", ""},
        {"b", "font-weight:bolder"}, {"strong", "font-weight:bolder"}, {"i", "font-style:italic"}, {"em", "font-style:italic"},
        {"code", "font-family:monospace"}, {"small", "font-size:smaller"},
        {"pre", "display:block;font-family:monospace;white-space:pre;margin:1em 0"},
        {"blockquote", "display:block;margin:1em 40px"},
        {"table", "display:table;border-spacing:2px"}, {"thead", "display:table-header-group"}, {"tbody", "display:table-row-group"},
        {"tfoot", "display:table-footer-group"}, {"tr", "display:table-row"}, {"td", "display:table-cell;padding:1px"},
        {"th", "display:table-cell;padding:1px;font-weight:bold;text-align:center"},
    };
    return m;
}

struct Obfuscator {
    bool on = false;
    bool light = false;                 // only encode the page, don't rename anything
    std::string salt;                   // new for every start of the server: names are the same on all pages while it runs
    std::set<std::string> keep;         // class / id names that must stay as they are

    // ---- names ----
    std::string name(const std::string& n) const {                  // the value of an id that must stay (for anchors and labels)
        if (n.empty() || keep.count(n)) return n;
        return "q" + base36(fnv64(salt + "n" + n), 6);
    }
    std::string mark(char kind, const std::string& n) const {       // class "box" -> the attribute  k3f9a1z  (kind: c = class, i = id)
        return "k" + base36(fnv64(salt + "a" + kind + n), 6);
    }
    bool scrambles(const std::string& tag) const { return !light && tagDefaults().count(tag) > 0; }
    std::string tagName(const std::string& tag) const {            // letters and digits only, never a real tag name
        return scrambles(tag) ? "z" + base36(fnv64(salt + "t" + tag), 6) : tag;
    }

    // ---- CSS: rename .class, #id and tag names in selectors; drop comments and extra spaces ----
    std::string css(const std::string& s) const {
        std::string out, header;         // header: the text before the current { (to see whether it is an @rule)
        size_t i = 0, n = s.size();
        std::vector<char> ctx = {'S'};   // S: rules inside, D: declarations inside
        auto idStart = [](unsigned char c) { return isalpha(c) || c == '_' || c == '-' || c >= 0x80; };
        auto idChar = [](unsigned char c) { return isalnum(c) || c == '_' || c == '-' || c >= 0x80; };
        auto skipQuoted = [&](size_t from) { char q = s[from]; size_t j = from + 1; while (j < n && s[j] != q) { if (s[j] == '\\') j++; j++; } return std::min(j + 1, n); };
        while (i < n) {
            char c = s[i];
            if (c == '/' && i + 1 < n && s[i + 1] == '*') { size_t e = s.find("*/", i + 2); i = e == std::string::npos ? n : e + 2; continue; }
            if (c == '"' || c == '\'') { size_t e = skipQuoted(i); out += s.substr(i, e - i); header += s.substr(i, e - i); i = e; continue; }
            if (isspace((unsigned char)c)) {
                while (i < n && isspace((unsigned char)s[i])) i++;
                if (!out.empty() && out.back() != ' ' && out.back() != '{' && out.back() != '}' && out.back() != ';') { out += ' '; header += ' '; }
                continue;
            }
            if (c == '{') {
                char kind = 'D';
                if (ctx.back() == 'S' && !header.empty() && header[0] == '@') {
                    size_t e = 1; while (e < header.size() && idChar((unsigned char)header[e])) e++;
                    std::string at = lowerAscii(header.substr(1, e - 1));
                    if (at == "media" || at == "supports" || at == "layer" || at == "container" || at == "document" || at == "scope") kind = 'S';
                }
                ctx.push_back(kind); out += '{'; header.clear(); i++; continue;
            }
            if (c == '}') { if (ctx.size() > 1) ctx.pop_back(); out += '}'; header.clear(); i++; continue; }
            if (c == ';') { out += ';'; header.clear(); i++; continue; }
            if (ctx.back() == 'D') {          // declarations: copy as they are (keep url(...) in one piece)
                if ((c == 'u' || c == 'U') && lowerAscii(s.substr(i, 4)) == "url(") {
                    size_t j = i + 4;
                    while (j < n && s[j] != ')') { if (s[j] == '"' || s[j] == '\'') j = skipQuoted(j); else j++; }
                    j = std::min(j + 1, n); out += s.substr(i, j - i); i = j; continue;
                }
                out += c; i++; continue;
            }
            bool atHeader = !header.empty() && header[0] == '@';
            if ((c == '.' || c == '#') && i + 1 < n && idStart((unsigned char)s[i + 1]) && !atHeader) {
                size_t j = i + 1; while (j < n && idChar((unsigned char)s[j])) j++;
                std::string id = s.substr(i + 1, j - i - 1);
                if (keep.count(id)) out += std::string(1, c) + id;
                else if (c == '.') out += "[" + mark('c', id) + "]";                              // .box   -> [k3f9a1z]
                else out += ":is(#zz0,[" + mark('i', id) + "])";                                  // #menu  -> same weight as an id
                header += s.substr(i, j - i); i = j; continue;
            }
            if (c == ':') {                    // a:hover  li::before - the word after : stays
                size_t j = i + 1; if (j < n && s[j] == ':') j++;
                while (j < n && idChar((unsigned char)s[j])) j++;
                out += s.substr(i, j - i); header += s.substr(i, j - i); i = j; continue;
            }
            if (c == '[') {                    // [type="text"] stays
                size_t j = i + 1; while (j < n && s[j] != ']') { if (s[j] == '"' || s[j] == '\'') j = skipQuoted(j); else j++; }
                j = std::min(j + 1, n); out += s.substr(i, j - i); header += s.substr(i, j - i); i = j; continue;
            }
            if (isdigit((unsigned char)c)) {   // 2n+1  50%  1.5em
                size_t j = i; while (j < n && (isalnum((unsigned char)s[j]) || s[j] == '.' || s[j] == '%')) j++;
                out += s.substr(i, j - i); header += s.substr(i, j - i); i = j; continue;
            }
            if (idStart((unsigned char)c)) {
                size_t j = i; while (j < n && idChar((unsigned char)s[j])) j++;
                std::string w = s.substr(i, j - i);
                out += (!atHeader && scrambles(lowerAscii(w))) ? tagName(lowerAscii(w)) : w;
                header += w; i = j; continue;
            }
            out += c; header += c; i++;
        }
        return out;
    }

    // ---- HTML: rename tags, turn classes and ids into odd attributes, drop comments and extra spaces ----
    struct Attr { std::string name, value; bool hasValue = false; };
    struct TagInfo { std::string name; std::vector<Attr> attrs; size_t end = 0; bool closing = false; };

    static bool nameChar(char c) { return isalnum((unsigned char)c) || c == '-' || c == '_' || c == ':'; }

    // reads the tag that starts at src[i] == '<'
    static bool parseTagAt(const std::string& src, size_t i, TagInfo& t) {
        size_t n = src.size();
        t = TagInfo();
        t.closing = i + 1 < n && src[i + 1] == '/';
        size_t p = i + (t.closing ? 2 : 1);
        if (p >= n || !isalpha((unsigned char)src[p])) return false;
        size_t q = p; while (q < n && nameChar(src[q])) q++;
        t.name = lowerAscii(src.substr(p, q - p));
        if (t.closing) { size_t e = src.find('>', q); t.end = e == std::string::npos ? n : e + 1; return true; }
        size_t k = q;
        while (k < n && src[k] != '>') {
            while (k < n && isspace((unsigned char)src[k])) k++;
            if (k >= n || src[k] == '>') break;
            if (src[k] == '/') { k++; continue; }
            size_t a = k; while (k < n && !isspace((unsigned char)src[k]) && src[k] != '=' && src[k] != '>' && src[k] != '/') k++;
            Attr at; at.name = lowerAscii(src.substr(a, k - a));
            if (at.name.empty()) { k++; continue; }
            while (k < n && isspace((unsigned char)src[k])) k++;
            if (k < n && src[k] == '=') {
                at.hasValue = true; k++; while (k < n && isspace((unsigned char)src[k])) k++;
                if (k < n && (src[k] == '"' || src[k] == '\'')) { char qc = src[k++]; size_t s0 = k; while (k < n && src[k] != qc) k++; at.value = src.substr(s0, k - s0); if (k < n) k++; }
                else { size_t s0 = k; while (k < n && !isspace((unsigned char)src[k]) && src[k] != '>') k++; at.value = src.substr(s0, k - s0); }
            }
            t.attrs.push_back(at);
        }
        t.end = k < n ? k + 1 : n;
        return true;
    }

    // ids that something points to (href="#x", label for="x" ...) must stay real ids, with a scrambled value
    static void collectRefs(const std::string& src, std::set<std::string>& refs) {
        TagInfo t;
        for (size_t i = src.find('<'); i != std::string::npos; i = src.find('<', i + 1)) {
            if (!parseTagAt(src, i, t) || t.closing) continue;
            for (auto& a : t.attrs) {
                if (!a.hasValue) continue;
                if (a.name == "href" && a.value.size() > 1 && a.value[0] == '#') refs.insert(a.value.substr(1));
                else if (a.name == "for" || a.name == "list" || a.name == "form") refs.insert(a.value);
                else if (a.name == "aria-labelledby" || a.name == "aria-describedby" || a.name == "aria-controls") {
                    std::string tok;
                    for (size_t k = 0; k <= a.value.size(); k++) {
                        if (k == a.value.size() || isspace((unsigned char)a.value[k])) { if (!tok.empty()) refs.insert(tok); tok.clear(); }
                        else tok += a.value[k];
                    }
                }
            }
        }
    }

    std::string emitAttrs(const std::vector<Attr>& attrs, const std::set<std::string>& refs) const {
        std::string out, markers;
        auto quote = [](const std::string& v) { std::string r; for (char ch : v) { if (ch == '"') r += "&quot;"; else r += ch; } return r; };
        for (auto& a : attrs) {
            if (!a.hasValue) { out += " " + a.name; continue; }
            std::string v = a.value;
            if (!light) {
                if (a.name == "class") {                                   // class="box big"  ->  k3f9a1z kq81z0   (no class word)
                    std::string kept, tok;
                    for (size_t k = 0; k <= v.size(); k++) {
                        if (k == v.size() || isspace((unsigned char)v[k])) {
                            if (!tok.empty()) { if (keep.count(tok)) kept += (kept.empty() ? "" : " ") + tok; else markers += " " + mark('c', tok); }
                            tok.clear();
                        } else tok += v[k];
                    }
                    if (!kept.empty()) out += " class=\"" + kept + "\"";
                    continue;
                }
                if (a.name == "id") {
                    if (keep.count(v)) out += " id=\"" + quote(v) + "\"";
                    else { if (refs.count(v)) out += " id=\"" + name(v) + "\""; markers += " " + mark('i', v); }
                    continue;
                }
                if (a.name == "for") v = name(v);
                else if (a.name == "href" && v.size() > 1 && v[0] == '#') v = "#" + name(v.substr(1));
            }
            out += " " + a.name + "=\"" + quote(v) + "\"";
        }
        return out + markers;
    }

    std::string html(const std::string& src) const {
        std::string out; out.reserve(src.size());
        std::set<std::string> used, refs;
        if (!light) collectRefs(src, refs);
        int preDepth = 0;
        size_t i = 0, n = src.size();
        while (i < n) {
            if (src[i] != '<') {                                          // text
                size_t e = src.find('<', i); if (e == std::string::npos) e = n;
                std::string t = src.substr(i, e - i);
                if (preDepth == 0) {
                    std::string c; bool sp = false;
                    for (char ch : t) { if (isspace((unsigned char)ch)) sp = true; else { if (sp) c += ' '; sp = false; c += ch; } }
                    if (sp) c += ' ';
                    if (c == " " && t.find('\n') != std::string::npos) c.clear();       // formatting between tags
                    t = c;
                }
                out += t; i = e; continue;
            }
            if (src.compare(i, 4, "<!--") == 0) { size_t e = src.find("-->", i + 4); i = e == std::string::npos ? n : e + 3; continue; }
            if (i + 1 < n && src[i + 1] == '!') { size_t e = src.find('>', i); if (e == std::string::npos) e = n - 1; out += src.substr(i, e - i + 1); i = e + 1; continue; }
            TagInfo t;
            if (!parseTagAt(src, i, t)) { out += '<'; i++; continue; }
            if (t.closing) {
                out += "</" + tagName(t.name) + ">";
                if ((t.name == "pre" || t.name == "fa-code") && preDepth > 0) preDepth--;
                i = t.end; continue;
            }
            if (scrambles(t.name)) used.insert(t.name);
            out += "<" + tagName(t.name) + emitAttrs(t.attrs, refs) + ">";
            if (t.name == "script" || t.name == "style" || t.name == "textarea" || t.name == "title") {   // text inside these is not HTML
                size_t e = t.end; std::string close = "</" + t.name;
                while (e < n && lowerAscii(src.substr(e, close.size())) != close) e++;
                std::string inner = src.substr(t.end, e - t.end);
                out += (t.name == "style" && !light) ? css(inner) : inner;
                size_t gt = src.find('>', e); i = gt == std::string::npos ? n : gt + 1;
                out += "</" + t.name + ">"; continue;
            }
            if (t.name == "pre" || t.name == "fa-code") preDepth++;
            i = t.end;
        }
        if (!used.empty()) {                                               // default looks of the renamed tags (easy for your own CSS to beat)
            std::string rules;
            for (auto& t : used) { const std::string& d = tagDefaults().at(t); if (!d.empty()) rules += ":where(" + tagName(t) + "){" + d + "}"; }
            if (!rules.empty()) {
                std::string st = "<style>" + rules + "</style>";
                size_t h = out.find("<head");
                size_t gt = h == std::string::npos ? std::string::npos : out.find('>', h);
                if (gt != std::string::npos) out.insert(gt + 1, st);
                else if (out.compare(0, 2, "<!") == 0 && out.find('>') != std::string::npos) out.insert(out.find('>') + 1, st);
                else out = st + out;
            }
        }
        return out;
    }

    // ---- sending: the page becomes a tiny loader plus an encoded blob ----
    static std::string base64(const std::string& in) {
        const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string r; size_t i = 0;
        while (i + 2 < in.size()) {
            unsigned v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8) | (unsigned char)in[i + 2];
            r += t[(v >> 18) & 63]; r += t[(v >> 12) & 63]; r += t[(v >> 6) & 63]; r += t[v & 63]; i += 3;
        }
        if (i + 1 == in.size()) { unsigned v = (unsigned char)in[i] << 16; r += t[(v >> 18) & 63]; r += t[(v >> 12) & 63]; r += "=="; }
        else if (i + 2 == in.size()) { unsigned v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8); r += t[(v >> 18) & 63]; r += t[(v >> 12) & 63]; r += t[(v >> 6) & 63]; r += '='; }
        return r;
    }

    std::string wrap(const std::string& page) const {
        static std::mt19937_64 rng{std::random_device{}()};
        int key[8]; std::string cipher = page, keys;
        for (int k = 0; k < 8; k++) { key[k] = 1 + (int)(rng() % 255); keys += (k ? "," : "") + std::to_string(key[k]); }
        for (size_t i = 0; i < cipher.size(); i++) cipher[i] = (char)((unsigned char)cipher[i] ^ key[i % 8]);
        // decodes the page, builds it in a separate document, then replaces this one (so the loader is gone afterwards)
        std::string js =
            "(function(){var d=\"" + base64(cipher) + "\",k=[" + keys + "],b=atob(d),u=new Uint8Array(b.length),i,j;"
            "for(i=0;i<b.length;i++)u[i]=b.charCodeAt(i)^k[i%8];"
            "var p=new DOMParser().parseFromString(new TextDecoder().decode(u),\"text/html\"),r=document.importNode(p.documentElement,true),"
            "s=r.querySelectorAll(\"script\");"
            "for(i=0;i<s.length;i++){var n=document.createElement(\"script\"),a=s[i].attributes;"
            "for(j=0;j<a.length;j++)n.setAttribute(a[j].name,a[j].value);n.async=false;n.text=s[i].text;s[i].replaceWith(n)}"
            "document.replaceChild(r,document.documentElement)})()";
        return "<!DOCTYPE html><html><head><meta charset=\"utf-8\"></head><body><script>" + js +
               "</script><noscript>This page needs JavaScript.</noscript></body></html>";
    }

    std::string page(const std::string& pageHtml) const { return wrap(html(pageHtml)); }

    // ---- the program file as a plain file in the Sources tab (it removes itself from the page again) ----
    static std::string withSourceMap(const std::string& html, const std::string& name, const std::string& text) {
        std::string json = "{\"version\":3,\"sources\":[" + jsonQuote(name) + "],\"sourcesContent\":[" + jsonQuote(text) + "],\"names\":[],\"mappings\":\"\"}";
        std::string tag = "<script>(function(){var s=document.createElement(\"script\");s.textContent=\"//# sourceMappingURL=data:application/json;base64," +
                          base64(json) + "\\n\";(document.head||document.documentElement).appendChild(s);s.remove();if(document.currentScript)document.currentScript.remove()})()</script>";
        size_t b = html.rfind("</body>");
        return b != std::string::npos ? html.substr(0, b) + tag + html.substr(b) : html + tag;
    }

    // ---- code root: the inspector shows your Fa code instead of html / head / body ----
    static std::string jsonQuote(const std::string& s) {
        std::string r = "\"";
        for (unsigned char c : s) {
            if (c == '"') r += "\\\""; else if (c == '\\') r += "\\\\";
            else if (c == '\n') r += "\\n"; else if (c == '\r') r += "\\r"; else if (c == '\t') r += "\\t";
            else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); r += b; }
            else r += (char)c;
        }
        return r + "\"";
    }

    struct PageParts { std::string lang, head, attrs, body; };
    static PageParts splitPage(const std::string& h) {                     // <html lang> <head>..</head> <body attrs>..</body>
        PageParts r; TagInfo t;
        size_t hp = h.find("<html");
        if (hp != std::string::npos && parseTagAt(h, hp, t)) for (auto& a : t.attrs) if (a.name == "lang") r.lang = a.value;
        size_t hs = h.find("<head"), he = h.find("</head>");
        if (hs != std::string::npos && parseTagAt(h, hs, t) && he != std::string::npos && he >= t.end) r.head = h.substr(t.end, he - t.end);
        size_t bs = h.find("<body");
        if (bs != std::string::npos && parseTagAt(h, bs, t)) {
            r.attrs = h.substr(bs + 5, t.end - 1 - (bs + 5));
            size_t be = h.rfind("</body>");
            r.body = h.substr(t.end, (be == std::string::npos || be < t.end ? h.size() : be) - t.end);
        } else {                                                           // just a piece of HTML, no page around it
            size_t from = 0; if (h.compare(0, 2, "<!") == 0) { size_t g = h.find('>'); if (g != std::string::npos) from = g + 1; }
            r.body = h.substr(from);
        }
        return r;
    }

    // html: the page, already transformed by html().  code: the lines of Fa code to show.
    std::string rootPage(const std::string& html, const std::vector<std::string>& code, const std::string& fileName = "", const std::string& fileText = "") const {
        PageParts p = splitPage(html);
        std::string codeJs;
        for (size_t i = 0; i < code.size(); i++) codeJs += (i ? "," : "") + jsonQuote(code[i]);
        std::string json = "{\"l\":" + jsonQuote(p.lang) + ",\"c\":[" + codeJs + "],\"h\":" + jsonQuote(p.head) +
                           ",\"a\":" + jsonQuote(p.attrs) + ",\"b\":" + jsonQuote(p.body) +
                           ",\"f\":" + jsonQuote(fileName) + ",\"s\":" + jsonQuote(fileText) + "}";
        static std::mt19937_64 rng{std::random_device{}()};
        int key[8]; std::string cipher = json, keys;
        for (int k = 0; k < 8; k++) { key[k] = 1 + (int)(rng() % 255); keys += (k ? "," : "") + std::to_string(key[k]); }
        for (size_t i = 0; i < cipher.size(); i++) cipher[i] = (char)((unsigned char)cipher[i] ^ key[i % 8]);
        // The page is built inside <fa-view> (a closed shadow root); the root element of the document is <fa-code>,
        // which carries the code as plain text lines (made invisible with line-height:0 and a transparent color). So the inspector starts with your code, not with html / head / body.
        std::string js =
            "(function(){var d=\"" + base64(cipher) + "\",k=[" + keys + "],b=atob(d),u=new Uint8Array(b.length),i;"
            "for(i=0;i<b.length;i++)u[i]=b.charCodeAt(i)^k[i%8];"
            "var P=JSON.parse(new TextDecoder().decode(u)),R=document.createElement(\"fa-code\");"
            // the program file as a plain file in the Sources tab (a source map without any mappings)
            "if(P.f){var sc=document.createElement(\"script\");sc.textContent=\"//# sourceMappingURL=data:application/json;base64,\"+"
            "btoa(unescape(encodeURIComponent(JSON.stringify({version:3,sources:[P.f],sourcesContent:[P.s],names:[],mappings:\"\"}))))+\"\\n\";"
            "document.head.appendChild(sc);sc.remove()}"
            "for(i=0;i<P.c.length;i++)R.appendChild(document.createTextNode(P.c[i]));"
            "var X=function(s){return new DOMParser().parseFromString(\"<!DOCTYPE html>\"+s,\"text/html\")},"
            "hd=X(\"<html><head>\"+P.h+\"</head><body></body></html>\"),tt=hd.head.querySelector(\"title\");"
            "if(tt){R.appendChild(document.importNode(tt,true));tt.remove()}"
            "var H=document.createElement(\"fa-view\"),S=H.attachShadow({mode:\"closed\"}),c=hd.head.childNodes,j;"
            "try{var q=new CSSStyleSheet();q.replaceSync(\":host{all:initial;display:block}\");S.adoptedStyleSheets=[q]}"
            "catch(e){var z=document.createElement(\"style\");z.textContent=\":host{all:initial;display:block}\";S.appendChild(z)}"
            "for(i=0;i<c.length;i++)S.appendChild(document.importNode(c[i],true));"
            "var B=document.createElement(\"body\"),a=X(\"<html><body \"+P.a+\"></body></html>\").body.attributes;"
            "for(i=0;i<a.length;i++)B.setAttribute(a[i].name,a[i].value);if(P.l&&!B.hasAttribute(\"lang\"))B.setAttribute(\"lang\",P.l);"
            "var n=X(\"<html><head></head><body>\"+P.b+\"</body></html>\").body.childNodes;"
            "for(i=0;i<n.length;i++)B.appendChild(document.importNode(n[i],true));"
            "S.appendChild(B);R.appendChild(H);document.replaceChild(R,document.documentElement);"
            "if(document.doctype)document.doctype.remove();"
            // links like href=\"#menu\" can't find ids inside a shadow root by themselves
            "S.addEventListener(\"click\",function(e){var t=e.composedPath().find(function(x){return x.tagName===\"A\"});"
            "if(!t)return;var h=t.getAttribute(\"href\")||\"\";if(h.charAt(0)!==\"#\")return;e.preventDefault();"
            "var g=h.length>1?S.getElementById(h.slice(1)):null;if(g)g.scrollIntoView();else if(h===\"#\")window.scrollTo(0,0)});"
            // a page background set on <body> must still fill the whole window: copy it to the root, again when a style sheet has loaded
            "var G=new CSSStyleSheet(),F=function(){var m=getComputedStyle(B);"
            "G.replaceSync(\"fa-code{line-height:0;color:transparent;background-color:\"+m.backgroundColor+\";background-image:\"+m.backgroundImage+\"}\")};"
            "try{document.adoptedStyleSheets=[G];F();"
            "S.querySelectorAll(\"link\").forEach(function(l){l.addEventListener(\"load\",F)});window.addEventListener(\"load\",F)}catch(e){R.style.cssText=\"line-height:0;color:transparent\"}"
            "})()";
        return "<!DOCTYPE html><html><head><meta charset=\"utf-8\"></head><body><script>" + js +
               "</script><noscript>This page needs JavaScript.</noscript></body></html>";
    }
};
