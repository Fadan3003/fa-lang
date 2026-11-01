#pragma once
// Value helpers: printing, equality, comparing, truthiness.
#include "objects.h"
#include "text.h"

static std::string fmtFloat(double d) {
    if (std::isnan(d)) return "nan";
    if (std::isinf(d)) return d < 0 ? "-inf" : "inf";
    char buf[64]; snprintf(buf, sizeof buf, "%.15g", d);
    std::string s = buf;
    if (s.find_first_of(".e") == std::string::npos) s += ".0";
    return s;
}

static std::string show(const Value& v, bool quoted = false) {
    switch (v.t) {
        case T::Nil: return "nothing";
        case T::Bool: return v.b ? "true" : "false";
        case T::Int: return std::to_string(v.i);
        case T::Float: return fmtFloat(v.d);
        case T::Str: return quoted ? "\"" + S(v) + "\"" : S(v);
        case T::List: {
            std::string r = "[";
            bool first = true;
            for (auto& x : L(v)) { if (!first) r += ", "; first = false; r += show(x, true); }
            return r + "]";
        }
        case T::Map: {
            std::string r = "{";
            bool first = true;
            for (auto& kv : M(v)->v) { if (!first) r += ", "; first = false; r += show(kv.first, true) + ": " + show(kv.second, true); }
            return r + "}";
        }
        default: return "<function>";
    }
}

static std::string typeName(const Value& v) {
    switch (v.t) {
        case T::Nil: return "nothing";
        case T::Bool: return "true/false";
        case T::Int: case T::Float: return "number";
        case T::Str: return "text";
        case T::List: return "list";
        case T::Map: return "map";
        default: return "function";
    }
}

static bool isNum(const Value& v) { return v.t == T::Int || v.t == T::Float; }
static double asD(const Value& v) { return v.t == T::Int ? (double)v.i : v.d; }

// only false and nothing count as "no"; everything else is "yes"
static bool truthy(const Value& v) {
    if (v.t == T::Nil) return false;
    if (v.t == T::Bool) return v.b;
    return true;
}

static bool equals(const Value& a, const Value& b) {
    if (isNum(a) && isNum(b)) return (a.t == T::Int && b.t == T::Int) ? a.i == b.i : asD(a) == asD(b);
    if (a.t != b.t) return false;
    switch (a.t) {
        case T::Nil: return true;
        case T::Bool: return a.b == b.b;
        case T::Str: return S(a) == S(b);
        case T::List: {
            auto &x = L(a), &y = L(b);
            if (x.size() != y.size()) return false;
            for (size_t i = 0; i < x.size(); i++) if (!equals(x[i], y[i])) return false;
            return true;
        }
        case T::Map: {
            auto &x = M(a)->v, &y = M(b)->v;
            if (x.size() != y.size()) return false;
            for (size_t i = 0; i < x.size(); i++) if (!equals(x[i].first, y[i].first) || !equals(x[i].second, y[i].second)) return false;
            return true;
        }
        default: return a.o == b.o;
    }
}

static int mapFind(MapObj* m, const Value& key) {
    for (size_t i = 0; i < m->v.size(); i++) if (equals(m->v[i].first, key)) return (int)i;
    return -1;
}

static int cmpValues(const Value& a, const Value& b, const std::string& op) {
    if (isNum(a) && isNum(b)) {
        if (a.t == T::Int && b.t == T::Int) return a.i < b.i ? -1 : a.i > b.i ? 1 : 0;
        double x = asD(a), y = asD(b); return x < y ? -1 : x > y ? 1 : 0;
    }
    if (a.t == T::Str && b.t == T::Str) { int c = S(a).compare(S(b)); return c < 0 ? -1 : c > 0 ? 1 : 0; }
    throw FaError("can't use '" + op + "' on " + typeName(a) + " and " + typeName(b));
}

// start/end for slice(a, b): negative counts from the end, values are clamped
static void sliceRange(int64_t n, const std::vector<Value>& a, int64_t& lo, int64_t& hi) {
    lo = a.size() > 0 && a[0].t == T::Int ? a[0].i : 0;
    hi = a.size() > 1 && a[1].t == T::Int ? a[1].i : n;
    if (lo < 0) lo += n;
    if (hi < 0) hi += n;
    lo = std::max<int64_t>(0, std::min(lo, n));
    hi = std::max<int64_t>(lo, std::min(hi, n));
}
