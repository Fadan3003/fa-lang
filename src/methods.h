#pragma once
// Things you can do with a value using a dot:  text.upper()   list.add(x)   map.keys()   player.hp
#include "interp.h"

inline Value Interp::field(const Value& obj, const std::string& name) {
    if (obj.t == T::Map) {
        int f = mapFind(M(obj), Value::str(name));               // player.hp  is  player["hp"]
        if (f >= 0) return M(obj)->v[f].second;
        if (name == "keys") return native(name, [obj](std::vector<Value>&) { std::vector<Value> r; for (auto& kv : M(obj)->v) r.push_back(kv.first); return Value::list(r); });
        if (name == "values") return native(name, [obj](std::vector<Value>&) { std::vector<Value> r; for (auto& kv : M(obj)->v) r.push_back(kv.second); return Value::list(r); });
        if (name == "has") return native(name, [obj](std::vector<Value>& a) { need(a, 1, "has"); return Value::boolean(mapFind(M(obj), a[0]) >= 0); });
        if (name == "get") return native(name, [obj](std::vector<Value>& a) {          // map.get("key")  or  map.get("key", default)
            if (a.empty() || a.size() > 2) throw FaError("get needs a key and optionally a default: map.get(\"key\", 0)");
            int f = mapFind(M(obj), a[0]);
            if (f >= 0) return M(obj)->v[f].second;
            return a.size() == 2 ? a[1] : Value(); });
        if (name == "remove") return native(name, [obj](std::vector<Value>& a) {
            need(a, 1, "remove"); int f = mapFind(M(obj), a[0]); if (f >= 0) M(obj)->v.erase(M(obj)->v.begin() + f); return Value(); });
        throw FaError("this map has no '" + name + "'. (Maps can also: keys values has get remove)");
    }
    if (obj.t == T::List) {
        if (name == "add") return native(name, [obj](std::vector<Value>& a) { need(a, 1, "add"); L(obj).push_back(a[0]); return Value(); });
        if (name == "pop") return native(name, [obj](std::vector<Value>&) {
            if (L(obj).empty()) throw FaError("can't pop from an empty list");
            Value v = L(obj).back(); L(obj).pop_back(); return v; });
        if (name == "join") return native(name, [obj](std::vector<Value>& a) {
            std::string sep = a.empty() ? "" : show(a[0]), r; bool first = true;
            for (auto& x : L(obj)) { if (!first) r += sep; first = false; r += show(x); } return Value::str(r); });
        if (name == "contains") return native(name, [obj](std::vector<Value>& a) {
            need(a, 1, "contains"); for (auto& x : L(obj)) if (equals(x, a[0])) return Value::boolean(true); return Value::boolean(false); });
        if (name == "sort") return native(name, [obj](std::vector<Value>&) {
            std::stable_sort(L(obj).begin(), L(obj).end(), [](const Value& x, const Value& y) { return cmpValues(x, y, "sort") < 0; });
            return Value(); });
        if (name == "reverse") return native(name, [obj](std::vector<Value>&) { std::reverse(L(obj).begin(), L(obj).end()); return Value(); });
        if (name == "slice") return native(name, [obj](std::vector<Value>& a) {
            int64_t lo, hi; sliceRange((int64_t)L(obj).size(), a, lo, hi);
            return Value::list(std::vector<Value>(L(obj).begin() + lo, L(obj).begin() + hi)); });
        throw FaError("a list has no '" + name + "'. Lists can: add pop join contains sort reverse slice");
    }
    if (obj.t == T::Str) {
        if (name == "upper") return native(name, [obj](std::vector<Value>&) { return Value::str(mapCase(S(obj), true)); });
        if (name == "lower") return native(name, [obj](std::vector<Value>&) { return Value::str(mapCase(S(obj), false)); });
        if (name == "trim") return native(name, [obj](std::vector<Value>&) {
            const std::string& s = S(obj); size_t a = s.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) return Value::str("");
            return Value::str(s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1)); });
        if (name == "split") return native(name, [obj](std::vector<Value>& a) {
            std::string sep = a.empty() ? " " : show(a[0]); const std::string& s = S(obj);
            std::vector<Value> out;
            if (sep.empty()) { for (auto& c : utf8Chars(s)) out.push_back(Value::str(c)); return Value::list(out); }
            size_t pos = 0, f;
            while ((f = s.find(sep, pos)) != std::string::npos) { out.push_back(Value::str(s.substr(pos, f - pos))); pos = f + sep.size(); }
            out.push_back(Value::str(s.substr(pos))); return Value::list(out); });
        if (name == "contains") return native(name, [obj](std::vector<Value>& a) { need(a, 1, "contains"); return Value::boolean(S(obj).find(show(a[0])) != std::string::npos); });
        if (name == "replace") return native(name, [obj](std::vector<Value>& a) {
            need(a, 2, "replace"); std::string s = S(obj), from = show(a[0]), to = show(a[1]);
            if (from.empty()) return Value::str(s);
            size_t pos = 0; while ((pos = s.find(from, pos)) != std::string::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
            return Value::str(s); });
        if (name == "starts_with") return native(name, [obj](std::vector<Value>& a) { need(a, 1, "starts_with"); return Value::boolean(S(obj).rfind(show(a[0]), 0) == 0); });
        if (name == "ends_with") return native(name, [obj](std::vector<Value>& a) {
            need(a, 1, "ends_with"); std::string e = show(a[0]); const std::string& s = S(obj);
            return Value::boolean(s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0); });
        if (name == "slice") return native(name, [obj](std::vector<Value>& a) {
            auto c = utf8Chars(S(obj)); int64_t lo, hi; sliceRange((int64_t)c.size(), a, lo, hi);
            std::string r; for (int64_t i = lo; i < hi; i++) r += c[i]; return Value::str(r); });
        throw FaError("text has no '" + name + "'. Text can: upper lower trim split contains replace starts_with ends_with slice");
    }
    throw FaError("can't use '." + name + "' on " + typeName(obj));
}
