#pragma once
// Values: what every Fa value is (numbers, text, lists, maps ...).
#include "common.h"

// ═══════════════════════════ Values ═══════════════════════════

struct Obj { virtual ~Obj() {} };
enum class T { Nil, Bool, Int, Float, Str, List, Map, Func, Native };

struct Value {
    T t = T::Nil;
    bool b = false;
    int64_t i = 0;
    double d = 0;
    std::shared_ptr<Obj> o;
    static Value boolean(bool x) { Value v; v.t = T::Bool; v.b = x; return v; }
    static Value integer(int64_t x) { Value v; v.t = T::Int; v.i = x; return v; }
    static Value real(double x) { Value v; v.t = T::Float; v.d = x; return v; }
    static Value str(std::string s);
    static Value rawhtml(std::string s);
    static Value list(std::vector<Value> items = {});
};

struct StrObj : Obj { std::string s; bool html = false; };
struct ListObj : Obj { std::vector<Value> v; };
struct MapObj : Obj { std::vector<std::pair<Value, Value>> v; };

Value Value::str(std::string s) {
    Value v; v.t = T::Str;
    auto o = std::make_shared<StrObj>(); o->s = std::move(s); v.o = o; return v;
}
Value Value::rawhtml(std::string s) {
    Value v = Value::str(std::move(s));
    static_cast<StrObj*>(v.o.get())->html = true; return v;
}
Value Value::list(std::vector<Value> items) {
    Value v; v.t = T::List;
    auto o = std::make_shared<ListObj>(); o->v = std::move(items); v.o = o; return v;
}

static const std::string& S(const Value& v) { return static_cast<StrObj*>(v.o.get())->s; }
static std::vector<Value>& L(const Value& v) { return static_cast<ListObj*>(v.o.get())->v; }
static MapObj* M(const Value& v) { return static_cast<MapObj*>(v.o.get()); }

