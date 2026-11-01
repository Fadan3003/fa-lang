#pragma once
// The built-in functions. Everything you can use without defining it yourself.
//
//   show and ask:   print  ask  len  text  number  chr  ord
//   math:           sqrt  round  floor  abs  min  max  random
//   files:          read_file  write_file  file_exists  list_files  delete_file
//   system:         run  env  wait  now  exit  args      and the values  here  interpreter  os
//   web:            route  serve  status  content_type  json  page  document  h1 h2 h3 p div span ul ol li
//                   pre bold italic code link image input br hr tag raw
//   windows:        window  label  button  row  column  editor  textfield  textview  listbox  spacer  shortcut
//                   editor_get  editor_set  editor_dirty  editor_clean  editor_info
#include "interp.h"
#include "html.h"
#include "gui/gui.h"

// ── window widgets are plain maps {kind: ..., text: ..., opts: ..., children: ...}; window() turns them into GuiWidget ──
static void put(Value& m, const char* key, Value v) { M(m)->v.push_back({Value::str(key), std::move(v)}); }
static Value newWidget(const char* kind) { Value m; m.t = T::Map; m.o = std::make_shared<MapObj>(); put(m, "kind", Value::str(kind)); return m; }
static bool isWidget(const Value& v) { return v.t == T::Map && mapFind(M(v), Value::str("kind")) >= 0; }
static std::map<std::string, std::string> widgetOpts(const Value& m) {
    std::map<std::string, std::string> out;
    if (m.t == T::Map) for (auto& kv : M(m)->v) out[show(kv.first)] = show(kv.second);
    return out;
}
static const char* WIDGET_LIST = "label, button, row, column, editor, textfield, textview, listbox, spacer and shortcut";

inline void Interp::install() {
    // ── show and ask ──
    def("print", [](std::vector<Value>& a) {
        std::string out;
        for (size_t i = 0; i < a.size(); i++) { if (i) out += ' '; out += show(a[i]); }
        std::cout << out << std::endl; return Value(); });   // flush, so output shows up at once even in files and pipes
    def("ask", [](std::vector<Value>& a) {
        if (!a.empty()) std::cout << show(a[0]) << std::flush;
        std::string line; std::getline(std::cin, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return Value::str(line); });
    def("len", [](std::vector<Value>& a) {
        need(a, 1, "len");
        if (a[0].t == T::List) return Value::integer((int64_t)L(a[0]).size());
        if (a[0].t == T::Str) return Value::integer((int64_t)utf8Chars(S(a[0])).size());
        if (a[0].t == T::Map) return Value::integer((int64_t)M(a[0])->v.size());
        throw FaError("len needs a list, text or map (got " + typeName(a[0]) + ")"); });
    def("text", [](std::vector<Value>& a) { need(a, 1, "text"); return Value::str(show(a[0])); });
    def("number", [](std::vector<Value>& a) {
        need(a, 1, "number");
        if (isNum(a[0])) return a[0];
        if (a[0].t == T::Str) {
            std::string s = S(a[0]);
            size_t b = s.find_first_not_of(" \t"), e = s.find_last_not_of(" \t");
            s = b == std::string::npos ? "" : s.substr(b, e - b + 1);
            char* end;
            if (!s.empty() && s.find_first_of(".eE") == std::string::npos) {
                long long v = strtoll(s.c_str(), &end, 10);
                if (!*end) return Value::integer(v);
            }
            if (!s.empty()) { double d = strtod(s.c_str(), &end); if (!*end) return Value::real(d); }
            throw FaError("can't turn " + show(a[0], true) + " into a number");
        }
        throw FaError("can't turn " + typeName(a[0]) + " into a number"); });
    def("chr", [](std::vector<Value>& a) {
        need(a, 1, "chr");
        if (a[0].t != T::Int || a[0].i < 0 || a[0].i > 0x10FFFF) throw FaError("chr needs a character number, like chr(252)");
        return Value::str(utf8Encode((uint32_t)a[0].i)); });
    def("ord", [](std::vector<Value>& a) {
        need(a, 1, "ord");
        if (a[0].t != T::Str || S(a[0]).empty()) throw FaError("ord needs a character, like ord(\"ü\")");
        return Value::integer((int64_t)utf8Code(S(a[0]))); });

    // ── math ──
    def("sqrt", [](std::vector<Value>& a) { need(a, 1, "sqrt"); if (!isNum(a[0])) throw FaError("sqrt needs a number"); return Value::real(std::sqrt(asD(a[0]))); });
    def("round", [](std::vector<Value>& a) { need(a, 1, "round"); if (!isNum(a[0])) throw FaError("round needs a number"); return Value::integer((int64_t)std::llround(asD(a[0]))); });
    def("floor", [](std::vector<Value>& a) { need(a, 1, "floor"); if (!isNum(a[0])) throw FaError("floor needs a number"); return Value::integer((int64_t)std::floor(asD(a[0]))); });
    def("abs", [](std::vector<Value>& a) {
        need(a, 1, "abs");
        if (a[0].t == T::Int) return Value::integer(a[0].i < 0 ? -a[0].i : a[0].i);
        if (a[0].t == T::Float) return Value::real(std::fabs(a[0].d));
        throw FaError("abs needs a number"); });
    auto pick = [](std::vector<Value>& a, const char* nm, bool wantMax) {
        std::vector<Value> items;
        if (a.size() == 1 && a[0].t == T::List) items = L(a[0]); else items = a;
        if (items.empty()) throw FaError(std::string(nm) + " needs some numbers, like " + nm + "(3, 9)");
        Value best = items[0];
        for (size_t i = 1; i < items.size(); i++) { int c = cmpValues(items[i], best, nm); if (wantMax ? c > 0 : c < 0) best = items[i]; }
        return best; };
    def("min", [pick](std::vector<Value>& a) { return pick(a, "min", false); });
    def("max", [pick](std::vector<Value>& a) { return pick(a, "max", true); });
    def("random", [this](std::vector<Value>& a) {
        if (a.empty()) return Value::real(std::uniform_real_distribution<double>(0.0, 1.0)(rng));
        if (a.size() != 2 || a[0].t != T::Int || a[1].t != T::Int || a[1].i < a[0].i)
            throw FaError("random() gives 0 to 1;  random(1, 6) gives a whole number from 1 to 6");
        return Value::integer(std::uniform_int_distribution<int64_t>(a[0].i, a[1].i)(rng)); });

    // ── files ──
    def("read_file", [](std::vector<Value>& a) {
        need(a, 1, "read_file"); std::ifstream f(show(a[0]), std::ios::binary);
        if (!f) throw FaError("can't open " + show(a[0], true));
        std::stringstream ss; ss << f.rdbuf(); return Value::str(ss.str()); });
    def("write_file", [](std::vector<Value>& a) {
        need(a, 2, "write_file"); std::ofstream f(show(a[0]), std::ios::binary);
        if (!f) throw FaError("can't write " + show(a[0], true));
        f << show(a[1]); return Value(); });
    def("file_exists", [](std::vector<Value>& a) { need(a, 1, "file_exists"); return Value::boolean(std::ifstream(show(a[0])).good()); });
    def("list_files", [](std::vector<Value>& a) {                 // names of the files in a folder, sorted
        need(a, 1, "list_files");
        std::vector<std::string> names;
        try {
            for (auto& e : std::filesystem::directory_iterator(show(a[0])))
                if (e.is_regular_file()) names.push_back(e.path().filename().string());
        } catch (std::exception&) { throw FaError("can't read the folder " + show(a[0], true)); }
        std::sort(names.begin(), names.end());
        std::vector<Value> out; for (auto& n : names) out.push_back(Value::str(n));
        return Value::list(out); });
    def("delete_file", [](std::vector<Value>& a) {
        need(a, 1, "delete_file"); std::error_code ec;
        if (!std::filesystem::remove(show(a[0]), ec) || ec) throw FaError("can't delete " + show(a[0], true));
        return Value(); });

    // ── system ──
    def("run", [](std::vector<Value>& a) {
        need(a, 1, "run"); FILE* p = POPEN(show(a[0]).c_str(), "r");
        if (!p) throw FaError("can't start that command");
        std::string out; char buf[512]; size_t n;
        while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
        PCLOSE(p); return Value::str(out); });
    def("env", [](std::vector<Value>& a) {
        need(a, 1, "env"); const char* v = getenv(show(a[0]).c_str());
        return v ? Value::str(v) : Value(); });
    def("wait", [](std::vector<Value>& a) {
        need(a, 1, "wait"); if (!isNum(a[0])) throw FaError("wait needs seconds, like wait(0.5)");
        std::this_thread::sleep_for(std::chrono::duration<double>(asD(a[0]))); return Value(); });
    def("now", [](std::vector<Value>&) {
        using namespace std::chrono;
        return Value::real(duration<double>(system_clock::now().time_since_epoch()).count()); });
    def("exit", [](std::vector<Value>& a) { std::cout.flush(); exit(a.empty() || a[0].t != T::Int ? 0 : (int)a[0].i); return Value(); });
    def("args", [this](std::vector<Value>&) {
        std::vector<Value> r; for (auto& x : cliArgs) r.push_back(Value::str(x)); return Value::list(r); });

    // ── web ──
    def("route", [this](std::vector<Value>& a) {
        if (a.size() != 2 || a[0].t != T::Str || (!isBlock(a[1]) && a[1].t != T::Func))
            throw FaError("route needs an address and the lines of the page below it, like:\n    route \"/\"\n        page \"Hello\"\n            h1 \"Hello\"");
        routeCode[S(a[0])] = blockSource(F(a[1])->node->line);        // for  serve 8080, {show_code: true}
        routeLine[S(a[0])] = F(a[1])->node->line;
        for (auto& r : routes) if (r.first == S(a[0])) { r.second = a[1]; return Value(); }
        routes.push_back({S(a[0]), a[1]}); return Value(); });
    def("serve", [this](std::vector<Value>& a) { return serve(a); });
    def("status", [this](std::vector<Value>& a) {
        need(a, 1, "status"); if (a[0].t != T::Int) throw FaError("status needs a number like 404");
        respStatus = (int)a[0].i; return Value(); });
    def("content_type", [this](std::vector<Value>& a) {          // content_type "text/plain" - what kind of data a route sends
        need(a, 1, "content_type"); respType = show(a[0]); return Value(); });
    def("json", [](std::vector<Value>& a) { need(a, 1, "json"); std::string o; toJson(a[0], o); return Value::str(o); });
    const char* tags[][2] = {{"h1", "h1"}, {"h2", "h2"}, {"h3", "h3"}, {"p", "p"}, {"div", "div"}, {"ul", "ul"}, {"ol", "ol"},
                             {"li", "li"}, {"span", "span"}, {"pre", "pre"}, {"bold", "b"}, {"italic", "i"}, {"code", "code"}};
    for (auto& tg : tags) { std::string html = tg[1]; def(tg[0], [html](std::vector<Value>& a) { return buildTag(html, a, 0, false); }, true); }
    def("br", [](std::vector<Value>& a) { return buildTag("br", a, 0, true); });
    def("hr", [](std::vector<Value>& a) { return buildTag("hr", a, 0, true); });
    def("input", [](std::vector<Value>& a) { return buildTag("input", a, 0, true); });
    def("link", [](std::vector<Value>& a) {
        if (a.empty()) throw FaError("link(address, text) needs an address");
        return buildTag("a", a, 1, false, " href=\"" + escapeHtml(show(a[0])) + "\""); }, true);
    def("image", [](std::vector<Value>& a) {
        if (a.empty()) throw FaError("image(address) needs an address");
        bool alt = a.size() > 1 && a[1].t == T::Str;
        return buildTag("img", a, alt ? 2 : 1, true,
            " src=\"" + escapeHtml(show(a[0])) + "\"" + (alt ? " alt=\"" + escapeHtml(S(a[1])) + "\"" : "")); });
    def("tag", [](std::vector<Value>& a) {                       // tag "table" for anything not built in
        if (a.empty() || a[0].t != T::Str) throw FaError("tag needs a tag name like \"table\"");
        for (unsigned char c : S(a[0])) if (!isalnum(c)) throw FaError("a tag name has only letters and digits");
        return buildTag(S(a[0]), a, 1, false); }, true);
    def("raw", [](std::vector<Value>& a) { need(a, 1, "raw"); return Value::rawhtml(show(a[0])); });
    def("page", [](std::vector<Value>& a) {
        if (a.empty()) throw FaError("page needs a title, like:  page \"My page\"");
        std::string body; for (size_t i = 1; i < a.size(); i++) body += renderHtml(a[i]);
        return Value::rawhtml(pageHtml(show(a[0]), body)); }, true);

    // document "Title", {css: "/a.css", js: "/a.js"}  +  lines below: a bare page for web apps (no built-in styling)
    def("document", [](std::vector<Value>& a) {
        if (a.empty()) throw FaError("document needs a title, like:  document \"My app\"");
        size_t start = 1; std::string head, tail;
        auto each = [](const Value& v, const std::function<void(const std::string&)>& f) {
            if (v.t == T::List) for (auto& x : L(v)) f(show(x)); else f(show(v)); };
        if (a.size() > 1 && a[1].t == T::Map) {
            start = 2; int k;
            if ((k = mapFind(M(a[1]), Value::str("css"))) >= 0)
                each(M(a[1])->v[k].second, [&](const std::string& x) { head += "<link rel=\"stylesheet\" href=\"" + escapeHtml(x) + "\">"; });
            if ((k = mapFind(M(a[1]), Value::str("js"))) >= 0)
                each(M(a[1])->v[k].second, [&](const std::string& x) { tail += "<script src=\"" + escapeHtml(x) + "\"></script>"; });
        }
        std::string body; for (size_t i = start; i < a.size(); i++) body += renderHtml(a[i]);
        return Value::rawhtml("<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>" + escapeHtml(show(a[0])) + "</title>" +
            head + "</head><body>" + body + tail + "</body></html>"); }, true);

    // ── windows ──
    def("window", [this](std::vector<Value>& a) { return window(a); });
    def("label", [](std::vector<Value>& a) {                     // label "text" [, {options}]
        if (a.empty() || a.size() > 2 || (a.size() == 2 && a[1].t != T::Map))
            throw FaError("label needs a text and optionally options, like:  label \"Hello\", {color: \"#ffffff\"}");
        Value w = newWidget("label"); put(w, "text", Value::str(show(a[0])));
        if (a.size() == 2) put(w, "opts", a[1]);
        return w; });
    def("button", [](std::vector<Value>& a) {                    // button "text" [, {options}]  + the lines to run on a click below it
        if (a.size() < 2 || a.size() > 3 || !isBlock(a.back()) || (a.size() == 3 && a[1].t != T::Map))
            throw FaError("button needs a text and the lines to run on a click below it, like:\n    button \"Go\"\n        print \"clicked\"");
        Value w = newWidget("button"); put(w, "text", Value::str(show(a[0])));
        if (a.size() == 3) put(w, "opts", a[1]);
        put(w, "action", a.back());
        return w; });
    for (const char* kind : {"row", "column"}) {                 // row {gap: 8}  /  column {width: 200}  + the widgets inside below
        std::string k = kind;
        def(kind, [k](std::vector<Value>& a) {
            Value w = newWidget(k.c_str()); size_t start = 0;
            if (!a.empty() && a[0].t == T::Map && !isWidget(a[0])) { put(w, "opts", a[0]); start = 1; }
            std::vector<Value> kids;
            for (size_t i = start; i < a.size(); i++) {
                if (!isWidget(a[i])) throw FaError(k + " can only contain " + WIDGET_LIST + " lines");
                kids.push_back(a[i]);
            }
            put(w, "children", Value::list(kids));
            return w; }, true);
    }
    def("editor", [](std::vector<Value>& a) {                    // editor "name" [, {options}]  a text editor that keeps its own text
        if (a.empty() || a.size() > 2 || (a.size() == 2 && a[1].t != T::Map)) throw FaError("editor needs a name, like:  editor \"code\"");
        Value w = newWidget("editor"); put(w, "text", Value::str(show(a[0])));
        if (a.size() == 2) put(w, "opts", a[1]);
        return w; });
    def("textfield", [](std::vector<Value>& a) {                 // textfield "name" [, {options}]  [+ lines to run on Enter]
        size_t n = a.size(); Value action;
        if (n > 0 && isBlock(a[n - 1])) { action = a[n - 1]; n--; }
        if (n < 1 || n > 2 || (n == 2 && a[1].t != T::Map)) throw FaError("textfield needs a name, like:  textfield \"search\"");
        Value w = newWidget("editor"); put(w, "text", Value::str(show(a[0])));
        Value opts; opts.t = T::Map; opts.o = std::make_shared<MapObj>();
        if (n == 2) M(opts)->v = M(a[1])->v;
        M(opts)->v.push_back({Value::str("single"), Value::boolean(true)});
        put(w, "opts", opts);
        if (action.t != T::Nil) put(w, "action", action);
        return w; });
    def("textview", [](std::vector<Value>& a) {                  // textview text [, {options}]  read-only text that can scroll
        if (a.empty() || a.size() > 2 || (a.size() == 2 && a[1].t != T::Map)) throw FaError("textview needs a text, like:  textview output");
        Value w = newWidget("textview"); put(w, "text", Value::str(show(a[0])));
        if (a.size() == 2) put(w, "opts", a[1]);
        return w; });
    def("listbox", [](std::vector<Value>& a) {                   // listbox names [, {selected: name}]  + lines to run on a click (item is the clicked entry)
        size_t n = a.size(); Value action;
        if (n > 0 && isBlock(a[n - 1])) { action = a[n - 1]; n--; }
        if (n < 1 || n > 2 || a[0].t != T::List || (n == 2 && a[1].t != T::Map))
            throw FaError("listbox needs a list, like:\n    listbox names, {selected: current}\n        open item");
        Value w = newWidget("listbox"); put(w, "items", a[0]);
        if (n == 2) put(w, "opts", a[1]);
        if (action.t != T::Nil) put(w, "action", action);
        return w; });
    def("spacer", [](std::vector<Value>& a) {                    // spacer: empty space that pushes the others apart
        if (a.size() > 1 || (a.size() == 1 && a[0].t != T::Map)) throw FaError("spacer takes only options, like:  spacer {width: 20}");
        Value w = newWidget("spacer"); if (a.size() == 1) put(w, "opts", a[0]);
        return w; });
    def("shortcut", [](std::vector<Value>& a) {                  // shortcut "ctrl+s"  + the lines to run
        if (a.size() != 2 || a[0].t != T::Str || !isBlock(a[1]))
            throw FaError("shortcut needs the keys and the lines to run, like:\n    shortcut \"ctrl+s\"\n        save()");
        Value w = newWidget("shortcut"); put(w, "text", a[0]); put(w, "action", a[1]);
        return w; });

    // the text inside a text editor
    def("editor_get", [](std::vector<Value>& a) { need(a, 1, "editor_get"); return Value::str(fa_gui_editor_get(show(a[0]))); });
    def("editor_set", [](std::vector<Value>& a) { need(a, 2, "editor_set"); fa_gui_editor_set(show(a[0]), show(a[1])); return Value(); });
    def("editor_dirty", [](std::vector<Value>& a) { need(a, 1, "editor_dirty"); return Value::boolean(fa_gui_editor_dirty(show(a[0]))); });
    def("editor_clean", [](std::vector<Value>& a) { need(a, 1, "editor_clean"); fa_gui_editor_clean(show(a[0])); return Value(); });
    def("editor_info", [](std::vector<Value>& a) {               // {line: 3, column: 7, lines: 20}
        need(a, 1, "editor_info"); int line, col, lines; fa_gui_editor_info(show(a[0]), line, col, lines);
        Value m; m.t = T::Map; m.o = std::make_shared<MapObj>();
        put(m, "line", Value::integer(line)); put(m, "column", Value::integer(col)); put(m, "lines", Value::integer(lines));
        return m; });
}

// window "Title" [, {options}]  with widgets below it. The lines run again after every click, key or edit,
// so the window always shows the current values.  Options: width height background color accent
inline Value Interp::window(std::vector<Value>& a) {
    if (!fa_gui_available())
        throw FaError("this fa was built without windows. Install the window version with:  sh install.sh gui   (needs raylib: sh install-raylib.sh)");
    if (a.size() < 2 || a.size() > 3 || !isBlock(a.back()) || (a.size() == 3 && a[1].t != T::Map))
        throw FaError("window needs a title and what to show below it, like:\n    window \"My app\"\n        label \"Hello\"");
    std::string title = show(a[0]);
    std::map<std::string, std::string> wopts;
    if (a.size() == 3) wopts = widgetOpts(a[1]);
    Value blk = a.back();

    struct Handler { Value block; std::vector<std::string> items; };
    std::vector<Handler> handlers;
    std::function<GuiWidget(const Value&)> convert = [&](const Value& v) -> GuiWidget {
        if (!isWidget(v)) throw FaError(std::string("a window can only contain ") + WIDGET_LIST + " lines");
        auto get = [&](const char* k) -> const Value* { int f = mapFind(M(v), Value::str(k)); return f < 0 ? nullptr : &M(v)->v[f].second; };
        GuiWidget g;
        g.kind = S(*get("kind"));
        if (const Value* t = get("text")) g.text = show(*t);
        if (const Value* o = get("opts")) g.opts = widgetOpts(*o);
        if (const Value* it = get("items")) for (auto& x : L(*it)) g.items.push_back(show(x));
        if (const Value* ch = get("children")) for (auto& c : L(*ch)) g.children.push_back(convert(c));
        if (const Value* ac = get("action")) { g.id = (int)handlers.size(); handlers.push_back({*ac, g.items}); }
        return g;
    };
    auto build = [&]() -> GuiWidget {
        handlers.clear();
        GuiWidget root; root.kind = "column";
        for (auto& w : collect(blk)) root.children.push_back(convert(w));
        return root;
    };
    auto onEvent = [&](int id, int item) {
        if (id < 0 || id >= (int)handlers.size()) return;
        Handler h = handlers[id];
        std::vector<std::pair<std::string, Value>> locals;                  // in a listbox: item (the entry) and index
        if (item >= 0 && item < (int)h.items.size()) { locals.push_back({"item", Value::str(h.items[item])}); locals.push_back({"index", Value::integer(item)}); }
        runBlock(F(h.block), locals);
    };
    fa_gui_run(title, wopts, build, onEvent);
    return Value();
}
