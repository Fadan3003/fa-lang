#pragma once
// Variable scopes (Env) and function objects.
#include "value.h"
#include "ast.h"

struct Env;
using EnvP = std::shared_ptr<Env>;
struct Env {
    std::unordered_map<std::string, Value> m;
    EnvP parent;
    Value* find(const std::string& n) {
        for (Env* e = this; e; e = e->parent.get()) {
            auto it = e->m.find(n);
            if (it != e->m.end()) return &it->second;
        }
        return nullptr;
    }
};

struct FuncObj : Obj {                                          // a function you wrote with def, or a block of lines
    std::string name; NP node;
    bool isBlock = false;                                       // a block: the indented lines under a call
    EnvP env;                                                   // blocks see the variables around them
};
using NativeFn = std::function<Value(std::vector<Value>&)>;
struct NativeObj : Obj { std::string name; NativeFn f; bool children = false; };   // a built-in function (children: lines below it become its values)

static FuncObj* F(const Value& v) { return static_cast<FuncObj*>(v.o.get()); }
