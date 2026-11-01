#pragma once
// The evaluator: runs statements, evaluates expressions, calls functions.
#include "objects.h"
#include "helpers.h"
#include "html.h"
#include "lexer.h"
#include "parser.h"
#include "text.h"

enum class Ctl { None, Break, Continue, Return };

struct Interp {
    EnvP builtins = std::make_shared<Env>();   // built-in functions: print, len, page ...
    EnvP g = std::make_shared<Env>();          // your own variables and functions
    Value ret;
    int depth = 0;
    std::vector<std::string> cliArgs;
    std::vector<std::vector<Value>*> collectors;              // lines inside a page / window block put their results here
    std::vector<std::pair<std::string, Value>> routes;        // route "/" ...
    int respStatus = 200;                                     // set by  status 404
    std::string respType;                                     // set by  content_type "text/plain"
    std::string sourceName, sourceText;                       // the program file that is running (for  show_code)
    std::map<std::string, std::string> routeCode;             // the Fa code written under each route
    std::map<std::string, int> routeLine;                     // and the line of the program file it starts on
    mutable std::vector<std::string> srcLines;
    std::mt19937_64 rng{(unsigned long long)std::chrono::steady_clock::now().time_since_epoch().count()};
    static constexpr int MAX_DEPTH = 2000;

    Interp() { g->parent = builtins; install(); }

    // ── running programs ──
    void runSource(const std::string& src) {
        auto prog = Parser(tokenize(src)).program();
        for (auto& s : prog) { if (exec(*s, g) == Ctl::Return) break; }
    }

    void replLine(const std::string& src) {
        auto prog = Parser(tokenize(src)).program();
        if (prog.size() == 1 && prog[0]->k == K::ExprS) {
            Value v = ev(*prog[0]->a[0], g);
            if (v.t != T::Nil) std::cout << show(v, true) << '\n';
            return;
        }
        for (auto& s : prog) exec(*s, g);
    }

    // ── statements ──
    Ctl execList(const std::vector<NP>& stmts, const EnvP& env) {
        for (auto& s : stmts) { Ctl c = exec(*s, env); if (c != Ctl::None) return c; }
        return Ctl::None;
    }

    // adds "line N:" to errors
    Ctl exec(const Node& n, const EnvP& env) {
        try { return exec2(n, env); }
        catch (FaError& e) {
            std::string m = e.what();
            if (m.rfind("line ", 0) == 0) throw;
            throw FaError("line " + std::to_string(n.line) + ": " + m);
        }
    }

    Ctl exec2(const Node& n, const EnvP& env) {
        switch (n.k) {
            case K::Assign: assign(*n.a[0], ev(*n.a[1], env), env); break;
            case K::Fn: {
                auto f = std::make_shared<FuncObj>(); f->name = n.s; f->node = std::make_shared<Node>(n);
                Value v; v.t = T::Func; v.o = f; env->m[n.s] = v; break;
            }
            case K::ExprS: {
                Value v = ev(*n.a[0], env);
                // a function's name alone on a line calls it:   spacer   br   save
                const Node& e = *n.a[0];
                if ((e.k == K::Var || e.k == K::Field) && (v.t == T::Native || v.t == T::Func)) v = call(v, {});
                // inside a page / window block every line that produces something is added to it
                if (!collectors.empty() && collectors.back() && v.t != T::Nil) collectors.back()->push_back(std::move(v));
                break;
            }
            case K::Return: ret = n.a.empty() ? Value() : ev(*n.a[0], env); return Ctl::Return;
            case K::If:
                if (truthy(ev(*n.a[0], env))) return execList(n.body, env);
                if (n.flag) return execList(n.body2, env);
                break;
            case K::While:
                while (truthy(ev(*n.a[0], env))) {
                    Ctl c = execList(n.body, env);
                    if (c == Ctl::Break) break;
                    if (c == Ctl::Return) return c;
                }
                break;
            case K::For: return execFor(n, env);
            case K::Break: return Ctl::Break;
            case K::Continue: return Ctl::Continue;
            default: ev(n, env);
        }
        return Ctl::None;
    }

    Ctl execFor(const Node& n, const EnvP& env) {
        const Node& it = *n.a[0];
        auto run = [&](const Value& x) -> Ctl { setVar(n.s, x, env); return execList(n.body, env); };
        if (it.k == K::Range) {                                  // 1..5 counts 1, 2, 3, 4, 5
            Value a = ev(*it.a[0], env), b = ev(*it.a[1], env);
            if (a.t != T::Int || b.t != T::Int) throw FaError("a range needs whole numbers, like 1..10");
            for (int64_t i = a.i; i <= b.i; i++) {
                Ctl c = run(Value::integer(i));
                if (c == Ctl::Break) break;
                if (c == Ctl::Return) return c;
            }
            return Ctl::None;
        }
        Value v = ev(it, env);
        std::vector<Value> items;
        if (v.t == T::List) items = L(v);
        else if (v.t == T::Map) for (auto& kv : M(v)->v) items.push_back(kv.first);
        else if (v.t == T::Str) for (auto& c : utf8Chars(S(v))) items.push_back(Value::str(c));
        else throw FaError("'for' needs a list, text, map or range like 1..10 (got " + typeName(v) + ")");
        for (auto& x : items) {
            Ctl c = run(x);
            if (c == Ctl::Break) break;
            if (c == Ctl::Return) return c;
        }
        return Ctl::None;
    }

    // "name = value": changes the variable if it exists (in the function or globally), otherwise creates it
    void setVar(const std::string& name, Value v, const EnvP& env) {
        for (Env* e = env.get(); e && e != builtins.get(); e = e->parent.get()) {
            auto it = e->m.find(name);
            if (it != e->m.end()) { it->second = std::move(v); return; }
        }
        env->m[name] = std::move(v);
    }

    void assign(const Node& target, Value val, const EnvP& env) {
        if (target.k == K::Var) { setVar(target.s, std::move(val), env); return; }
        if (target.k == K::Index) {
            Value obj = ev(*target.a[0], env), idx = ev(*target.a[1], env);
            if (obj.t == T::List) {
                if (idx.t != T::Int) throw FaError("list positions are whole numbers");
                int64_t i = idx.i, n = (int64_t)L(obj).size();
                if (i < 0) i += n;
                if (i < 0 || i >= n) throw FaError("position " + std::to_string(idx.i) + " is outside the list (it has " + std::to_string(n) + " items)");
                L(obj)[i] = std::move(val);
            } else if (obj.t == T::Map) {
                int f = mapFind(M(obj), idx);
                if (f >= 0) M(obj)->v[f].second = std::move(val); else M(obj)->v.push_back({idx, std::move(val)});
            } else throw FaError("can't put a value into a " + typeName(obj));
            return;
        }
        Value obj = ev(*target.a[0], env);                       // player.hp = 5
        if (obj.t != T::Map) throw FaError("only maps have named parts, like player.hp (got " + typeName(obj) + ")");
        int f = mapFind(M(obj), Value::str(target.s));
        if (f >= 0) M(obj)->v[f].second = std::move(val); else M(obj)->v.push_back({Value::str(target.s), std::move(val)});
    }

    // ── expressions ──
    Value ev(const Node& n, const EnvP& env) {
        switch (n.k) {
            case K::Lit: return n.lit;
            case K::Var: {
                Value* v = env->find(n.s);
                if (!v) throw FaError("'" + n.s + "' doesn't exist yet - give it a value first, like:  " + n.s + " = 0");
                return *v;
            }
            case K::Interp: {
                std::string out;
                for (auto& p : n.a) out += show(ev(*p, env));
                return Value::str(out);
            }
            case K::Bin: { Value a = ev(*n.a[0], env); Value b = ev(*n.a[1], env); return binop(n.s, a, b); }
            case K::And: { Value l = ev(*n.a[0], env); return truthy(l) ? ev(*n.a[1], env) : l; }
            case K::Or: { Value l = ev(*n.a[0], env); return truthy(l) ? l : ev(*n.a[1], env); }
            case K::Not: return Value::boolean(!truthy(ev(*n.a[0], env)));
            case K::Neg: {
                Value v = ev(*n.a[0], env);
                if (v.t == T::Int) return Value::integer(-v.i);
                if (v.t == T::Float) return Value::real(-v.d);
                throw FaError("can't put a minus in front of " + typeName(v));
            }
            case K::Range: {
                Value a = ev(*n.a[0], env), b = ev(*n.a[1], env);
                if (a.t != T::Int || b.t != T::Int) throw FaError("a range needs whole numbers, like 1..10");
                std::vector<Value> out;
                for (int64_t i = a.i; i <= b.i; i++) out.push_back(Value::integer(i));
                return Value::list(std::move(out));
            }
            case K::ListL: {
                std::vector<Value> out;
                for (auto& x : n.a) out.push_back(ev(*x, env));
                return Value::list(std::move(out));
            }
            case K::MapL: {
                Value v; v.t = T::Map; auto m = std::make_shared<MapObj>(); v.o = m;
                for (size_t i = 0; i + 1 < n.a.size(); i += 2) {
                    Value k = ev(*n.a[i], env), val = ev(*n.a[i + 1], env);
                    int f = mapFind(m.get(), k);
                    if (f >= 0) m->v[f].second = val; else m->v.push_back({k, val});
                }
                return v;
            }
            case K::Call: {
                Value fn = ev(*n.a[0], env);
                std::vector<Value> args;
                for (size_t i = 1; i < n.a.size(); i++) args.push_back(ev(*n.a[i], env));
                if (!n.body.empty()) args.push_back(makeBlock(n, env));      // the indented lines below the call
                return call(fn, args);
            }
            case K::Index: return index(ev(*n.a[0], env), ev(*n.a[1], env));
            case K::Field: return field(ev(*n.a[0], env), n.s);
            default: throw FaError("internal error: bad expression");
        }
    }

    Value index(const Value& obj, const Value& idx) {
        if (obj.t == T::List || obj.t == T::Str) {
            if (idx.t != T::Int) throw FaError("positions are whole numbers");
            std::vector<std::string> chars;
            bool ascii = obj.t == T::List || isAscii(S(obj));
            if (!ascii) chars = utf8Chars(S(obj));
            int64_t i = idx.i, n = obj.t == T::List ? (int64_t)L(obj).size() : ascii ? (int64_t)S(obj).size() : (int64_t)chars.size();
            if (i < 0) i += n;
            if (i < 0 || i >= n) throw FaError("position " + std::to_string(idx.i) + " doesn't exist (size " + std::to_string(n) + ", positions start at 0)");
            if (obj.t == T::List) return L(obj)[i];
            return Value::str(ascii ? std::string(1, S(obj)[i]) : chars[i]);
        }
        if (obj.t == T::Map) {
            int f = mapFind(M(obj), idx);
            if (f < 0) throw FaError("no key " + show(idx, true) + " in this map");
            return M(obj)->v[f].second;
        }
        throw FaError("can't use [] on " + typeName(obj));
    }

    Value binop(const std::string& op, const Value& a, const Value& b) {
        if (op == "==") return Value::boolean(equals(a, b));
        if (op == "!=") return Value::boolean(!equals(a, b));
        if (op == "<") return Value::boolean(cmpValues(a, b, op) < 0);
        if (op == ">") return Value::boolean(cmpValues(a, b, op) > 0);
        if (op == "<=") return Value::boolean(cmpValues(a, b, op) <= 0);
        if (op == ">=") return Value::boolean(cmpValues(a, b, op) >= 0);
        char c = op[0];
        bool bothInt = a.t == T::Int && b.t == T::Int;
        if (c == '+') {
            if (a.t == T::Str || b.t == T::Str) return Value::str(show(a) + show(b));
            if (a.t == T::List && b.t == T::List) { auto r = L(a); r.insert(r.end(), L(b).begin(), L(b).end()); return Value::list(r); }
        }
        if (c == '*' && a.t == T::Str && b.t == T::Int) {
            std::string r; for (int64_t i = 0; i < b.i; i++) r += S(a); return Value::str(r);
        }
        if (!isNum(a) || !isNum(b)) throw FaError("can't use '" + op + "' on " + typeName(a) + " and " + typeName(b));
        switch (c) {
            case '+': return bothInt ? Value::integer(a.i + b.i) : Value::real(asD(a) + asD(b));
            case '-': return bothInt ? Value::integer(a.i - b.i) : Value::real(asD(a) - asD(b));
            case '*': return bothInt ? Value::integer(a.i * b.i) : Value::real(asD(a) * asD(b));
            case '/':
                if (asD(b) == 0) throw FaError("can't divide by zero");
                if (bothInt && a.i % b.i == 0) return Value::integer(a.i / b.i);
                return Value::real(asD(a) / asD(b));
            case '%':
                if (asD(b) == 0) throw FaError("can't divide by zero");
                if (bothInt) { int64_t r = a.i % b.i; if (r != 0 && ((r < 0) != (b.i < 0))) r += b.i; return Value::integer(r); }
                return Value::real(std::fmod(asD(a), asD(b)));
        }
        throw FaError("unknown operator " + op);
    }

    // ── blocks: the indented lines below a call ──
    static bool isBlock(const Value& v) { return v.t == T::Func && F(v)->isBlock; }

    Value makeBlock(const Node& call, const EnvP& env) {
        auto f = std::make_shared<FuncObj>(); f->name = "block"; f->isBlock = true; f->env = env;
        f->node = std::make_shared<Node>(); f->node->k = K::Fn; f->node->line = call.line; f->node->body = call.body;
        Value v; v.t = T::Func; v.o = f; return v;
    }

    // runs the lines of a block; they can see and change the variables around them
    Value runBlock(FuncObj* f, const std::vector<std::pair<std::string, Value>>& locals = {}) {
        if (depth >= MAX_DEPTH) throw FaError("too much nesting");
        EnvP e = std::make_shared<Env>(); e->parent = f->env;
        for (auto& kv : locals) e->m[kv.first] = kv.second;
        depth++;
        Ctl c;
        try { c = execList(f->node->body, e); } catch (...) { depth--; throw; }
        depth--;
        if (c == Ctl::Return) { Value r = std::move(ret); ret = Value(); return r; }
        return Value();
    }

    // runs a block and returns everything its lines produced (used by page, window, route ...)
    std::vector<Value> collect(const Value& blk, Value* returned = nullptr, const std::vector<std::pair<std::string, Value>>& locals = {}) {
        std::vector<Value> out;
        collectors.push_back(&out);
        Value r;
        try { r = runBlock(F(blk), locals); } catch (...) { collectors.pop_back(); throw; }
        collectors.pop_back();
        if (returned) *returned = r;
        return out;
    }

    Value call(const Value& fn, std::vector<Value> args) {
        if (fn.t == T::Func) {
            FuncObj* f = F(fn);
            if (f->isBlock) {
                if (!args.empty()) throw FaError("a block of lines can't take values");
                return runBlock(f);
            }
            const Node& n = *f->node;
            if (args.size() != n.names.size()) {
                std::string names; for (size_t i = 0; i < n.names.size(); i++) names += (i ? ", " : "") + n.names[i];
                throw FaError(f->name + " needs " + std::to_string(n.names.size()) + " value(s)" + (names.empty() ? "" : " (" + names + ")") +
                              ", but got " + std::to_string(args.size()));
            }
            if (depth >= MAX_DEPTH) throw FaError("too much recursion - a function keeps calling itself without stopping");
            EnvP e = std::make_shared<Env>(); e->parent = g;       // a function sees your global variables, plus its own
            for (size_t i = 0; i < args.size(); i++) e->m[n.names[i]] = args[i];
            depth++;
            collectors.push_back(nullptr);                          // lines inside a function don't add to a page around the call: use return
            Ctl c;
            try { c = execList(n.body, e); } catch (...) { collectors.pop_back(); depth--; throw; }
            collectors.pop_back(); depth--;
            if (c == Ctl::Return) { Value r = std::move(ret); ret = Value(); return r; }
            return Value();
        }
        if (fn.t == T::Native) {
            NativeObj* no = static_cast<NativeObj*>(fn.o.get());
            if (no->children && !args.empty() && isBlock(args.back())) {   // the lines below become the values (children)
                Value blk = args.back(); args.pop_back();
                for (auto& c : collect(blk)) args.push_back(std::move(c));
            }
            return no->f(args);
        }
        throw FaError("that is not a function, so it can't be called with ( )");
    }

    // ── helpers for the standard library ──
    Value native(const std::string& name, NativeFn f, bool children = false) {
        auto o = std::make_shared<NativeObj>(); o->name = name; o->f = std::move(f); o->children = children;
        Value v; v.t = T::Native; v.o = o; return v;
    }
    static void need(std::vector<Value>& a, size_t n, const char* name) {
        if (a.size() != n) throw FaError(std::string(name) + " needs " + std::to_string(n) + " value(s), but got " + std::to_string(a.size()));
    }
    void def(const char* name, NativeFn f, bool children = false) { builtins->m[name] = native(name, std::move(f), children); }

    // the statement that starts on this line of the program file, with everything indented below it
    std::string blockSource(int line) const {
        if (srcLines.empty()) {
            std::string cur;
            for (char ch : sourceText) { if (ch == '\n') { srcLines.push_back(cur); cur.clear(); } else if (ch != '\r') cur += ch; }
            srcLines.push_back(cur);
        }
        if (line < 1 || line > (int)srcLines.size()) return "";
        auto indentOf = [](const std::string& s) { size_t n = 0; while (n < s.size() && (s[n] == ' ' || s[n] == '\t')) n++; return n; };
        size_t base = indentOf(srcLines[line - 1]);
        int last = line;
        for (size_t i = line; i < srcLines.size(); i++) {
            if (srcLines[i].find_first_not_of(" \t") == std::string::npos) continue;      // empty line
            if (indentOf(srcLines[i]) <= base) break;
            last = (int)i + 1;
        }
        std::string out;
        for (int i = line; i <= last; i++) out += (i > line ? "\n" : "") + srcLines[i - 1];
        return out;
    }

    Value field(const Value& obj, const std::string& name);     // methods.h
    void install();                                              // stdlib.h
    Value window(std::vector<Value>& a);                         // stdlib.h
    Value serve(std::vector<Value>& a);                          // server.h
};
