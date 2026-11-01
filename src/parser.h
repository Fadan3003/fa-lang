#pragma once
// Parser: turns tokens into the syntax tree.
//
//   name = value                  def name a, b       (the lines below it, indented, are its body)
//   if / else if / else           while       for x in list       return value      break      continue
//   print "hi"                    a call without ( ) when it is a whole statement
#include "ast.h"
#include "lexer.h"

struct Parser {
    std::vector<Token> t;
    size_t p = 0;
    int nest = 0;          // inside ( ) [ ] { }: lines may continue freely
    explicit Parser(std::vector<Token> toks) : t(std::move(toks)) {}

    Token& peek() { return t[p]; }
    Token& next() { return t[p++]; }
    bool isOp(const char* v) { return peek().k == TK::Op && peek().s == v; }
    bool isKw(const char* v) { return peek().k == TK::Kw && peek().s == v; }
    // same, but only when still on the same line (a new line starts a new statement)
    bool isOpHere(const char* v) { return isOp(v) && (nest > 0 || !peek().nl); }
    bool isKwHere(const char* v) { return isKw(v) && (nest > 0 || !peek().nl); }
    bool acceptOp(const char* v) { if (isOp(v)) { p++; return true; } return false; }
    bool acceptKw(const char* v) { if (isKw(v)) { p++; return true; } return false; }
    [[noreturn]] void error(const std::string& m) { throw FaError("line " + std::to_string(peek().line) + ": " + m); }
    std::string shown() {
        switch (peek().k) {
            case TK::Eof: return "the end of the file";
            case TK::Num: return "a number";
            case TK::Str: return "some text";
            case TK::Indent: return "an indented line";
            case TK::Dedent: return "the end of the block";
            default: return "'" + peek().s + "'";
        }
    }
    void expectOp(const char* v) { if (!acceptOp(v)) error(std::string("expected '") + v + "' but found " + shown()); }
    std::string ident() {
        if (peek().k != TK::Id) error("expected a name but found " + shown());
        return next().s;
    }

    // ── statements ──
    std::vector<NP> program() {
        std::vector<NP> out;
        while (peek().k != TK::Eof) out.push_back(statement());
        return out;
    }

    // an indented block under a header line like  "if x"  or  "def f".  A ":" after the header is allowed (like Python).
    std::vector<NP> block(const std::string& what, int line) {
        acceptOp(":");
        if (peek().k != TK::Indent) throw FaError("line " + std::to_string(line) + ": this " + what + " needs indented lines below it");
        next();
        std::vector<NP> out;
        while (peek().k != TK::Dedent && peek().k != TK::Eof) out.push_back(statement());
        if (peek().k == TK::Dedent) next();
        return out;
    }

    NP statement() {
        int line = peek().line;
        if (peek().k == TK::Indent) error("this line is indented, but nothing above it asks for that");
        if (peek().k == TK::Kw) {
            std::string k = peek().s;
            if (k == "def") {
                next(); NP s = mk(K::Fn, line); s->s = ident();
                if (acceptOp("(")) {                                   // def add(a, b)   or   def add a, b
                    nest++;
                    while (!isOp(")")) { s->names.push_back(ident()); if (!acceptOp(",")) break; }
                    nest--; expectOp(")");
                } else {
                    while (peek().k == TK::Id && !peek().nl) { s->names.push_back(ident()); if (!acceptOp(",")) break; }
                }
                s->body = block("function '" + s->s + "'", line);
                return s;
            }
            if (k == "return") {
                next(); NP s = mk(K::Return, line);
                Token& nt = peek();
                if (!(nt.k == TK::Eof || nt.nl)) s->a.push_back(withBlock(maybeCommand(expr(), line), line));
                return s;
            }
            if (k == "if") { next(); return ifRest(line); }
            if (k == "while") {
                next(); NP s = mk(K::While, line); s->a.push_back(expr());
                s->body = block("while loop", line); return s;
            }
            if (k == "for") {
                next(); NP s = mk(K::For, line); s->s = ident();
                if (!acceptKw("in")) error("expected 'in', like:  for item in list");
                s->a.push_back(expr());
                s->body = block("for loop", line); return s;
            }
            if (k == "break") { next(); return mk(K::Break, line); }
            if (k == "continue") { next(); return mk(K::Continue, line); }
            if (k == "else") error("'else' has no matching 'if' right before it");
        }
        return exprStmt();
    }

    // after 'if': condition, block, optional else / else if
    NP ifRest(int line) {
        NP s = mk(K::If, line);
        s->a.push_back(expr());
        s->body = block("if", line);
        if (acceptKw("else")) {
            s->flag = true;
            if (isKw("if")) { int l = peek().line; next(); s->body2.push_back(ifRest(l)); }
            else s->body2 = block("else", line);
        }
        return s;
    }

    // does the next token start a value on the same line?  (then "print x" is a call without parentheses)
    bool startsArg() {
        Token& tk = peek();
        if (tk.nl) return false;
        switch (tk.k) {
            case TK::Str: case TK::Num: case TK::Id: return true;
            case TK::Kw: return tk.s == "true" || tk.s == "false" || tk.s == "nothing" || tk.s == "not";
            case TK::Op: return tk.s == "{";
            default: return false;
        }
    }
    // a call can have indented lines below it:   page "Title"   /   ul   /   button "Go"
    NP withBlock(NP e, int line) {
        if (isOp(":") && !peek().nl && (e->k == K::Call || e->k == K::Var || e->k == K::Field)) next();
        if (peek().k != TK::Indent) return e;
        if (e->k != K::Call && e->k != K::Var && e->k != K::Field) return e;      // statement() reports the stray indent
        if (e->k != K::Call) { NP c = mk(K::Call, line); c->a.push_back(e); e = c; }
        e->body = block("call", line);
        return e;
    }
    NP maybeCommand(NP e, int line) {
        if ((e->k != K::Var && e->k != K::Field) || !startsArg()) return e;
        NP c = mk(K::Call, line); c->a.push_back(e);
        do { c->a.push_back(expr()); } while (acceptOp(","));
        return c;
    }

    NP exprStmt() {
        int line = peek().line;
        NP e = maybeCommand(expr(), line);
        const char* ops[] = {"=", "+=", "-=", "*=", "/="};
        for (const char* op : ops) {
            if (isOp(op)) {
                if (e->k != K::Var && e->k != K::Index && e->k != K::Field)
                    error("you can only put a value into a name, a list item or a map key");
                next();
                NP val = maybeCommand(expr(), line);
                if (std::string(op) != "=") {
                    NP b = mk(K::Bin, line); b->s = std::string(1, op[0]); b->a = {e, val}; val = b;
                }
                NP s = mk(K::Assign, line); s->a = {e, val}; return s;
            }
        }
        NP s = mk(K::ExprS, line); s->a.push_back(withBlock(e, line)); return s;
    }

    // ── expressions ──
    NP expr() { return pOr(); }
    NP bin(const std::string& op, NP l, NP r, int line) {
        NP n = mk(K::Bin, line); n->s = op; n->a = {std::move(l), std::move(r)}; return n;
    }
    NP pOr() {
        NP l = pAnd();
        while (isKwHere("or")) { int ln = next().line; NP n = mk(K::Or, ln); n->a = {l, pAnd()}; l = n; }
        return l;
    }
    NP pAnd() {
        NP l = pEq();
        while (isKwHere("and")) { int ln = next().line; NP n = mk(K::And, ln); n->a = {l, pEq()}; l = n; }
        return l;
    }
    NP pEq() {
        NP l = pCmp();
        while (isOpHere("==") || isOpHere("!=")) { Token o = next(); l = bin(o.s, l, pCmp(), o.line); }
        return l;
    }
    NP pCmp() {
        NP l = pRange();
        while (isOpHere("<") || isOpHere(">") || isOpHere("<=") || isOpHere(">=")) { Token o = next(); l = bin(o.s, l, pRange(), o.line); }
        return l;
    }
    NP pRange() {
        NP l = pAdd();
        if (isOpHere("..")) { int ln = next().line; NP n = mk(K::Range, ln); n->a = {l, pAdd()}; return n; }
        return l;
    }
    NP pAdd() {
        NP l = pMul();
        while (isOpHere("+") || isOpHere("-")) { Token o = next(); l = bin(o.s, l, pMul(), o.line); }
        return l;
    }
    NP pMul() {
        NP l = pUnary();
        while (isOpHere("*") || isOpHere("/") || isOpHere("%")) { Token o = next(); l = bin(o.s, l, pUnary(), o.line); }
        return l;
    }
    NP pUnary() {
        if (isOp("-")) { int ln = next().line; NP n = mk(K::Neg, ln); n->a.push_back(pUnary()); return n; }
        if (isKw("not")) { int ln = next().line; NP n = mk(K::Not, ln); n->a.push_back(pUnary()); return n; }
        return pPostfix();
    }
    NP pPostfix() {
        NP e = pPrimary();
        for (;;) {
            Token& tk = peek();
            if (tk.k == TK::Op && tk.s == "(" && (nest > 0 || !tk.nl)) {
                next(); NP c = mk(K::Call, tk.line); c->a.push_back(e);
                nest++;
                while (!isOp(")")) { c->a.push_back(expr()); if (!acceptOp(",")) break; }
                nest--; expectOp(")"); e = c;
            } else if (tk.k == TK::Op && tk.s == "[" && (nest > 0 || !tk.nl)) {
                next(); NP c = mk(K::Index, tk.line); nest++; c->a = {e, expr()}; nest--; expectOp("]"); e = c;
            } else if (tk.k == TK::Op && tk.s == "." && (nest > 0 || !tk.nl)) {
                next(); NP c = mk(K::Field, tk.line); c->a.push_back(e);
                if (peek().k == TK::Kw) c->s = next().s; else c->s = ident();      // m.for works too
                e = c;
            } else return e;
        }
    }
    NP pPrimary() {
        Token tk = next();
        if (tk.k == TK::Num) {
            NP n = mk(K::Lit, tk.line);
            n->lit = tk.isFloat ? Value::real(tk.d) : Value::integer(tk.i); return n;
        }
        if (tk.k == TK::Str) return stringNode(tk.s, tk.line);
        if (tk.k == TK::Id) { NP n = mk(K::Var, tk.line); n->s = tk.s; return n; }
        if (tk.k == TK::Kw) {
            if (tk.s == "true" || tk.s == "false") { NP n = mk(K::Lit, tk.line); n->lit = Value::boolean(tk.s == "true"); return n; }
            if (tk.s == "nothing") return mk(K::Lit, tk.line);
        }
        if (tk.k == TK::Op) {
            if (tk.s == "(") { nest++; NP e = expr(); nest--; expectOp(")"); return e; }
            if (tk.s == "[") {
                NP n = mk(K::ListL, tk.line);
                nest++;
                while (!isOp("]")) { n->a.push_back(expr()); if (!acceptOp(",")) break; }
                nest--; expectOp("]"); return n;
            }
            if (tk.s == "{") {
                NP n = mk(K::MapL, tk.line);
                nest++;
                while (!isOp("}")) {
                    if ((peek().k == TK::Id || peek().k == TK::Kw) && t[p + 1].k == TK::Op && t[p + 1].s == ":") {   // {name: "x"} means {"name": "x"}  (also for words like for, in, not)
                        NP key = mk(K::Lit, peek().line); key->lit = Value::str(peek().s); next(); n->a.push_back(key);
                    } else n->a.push_back(expr());
                    expectOp(":"); n->a.push_back(expr());
                    if (!acceptOp(",")) break;
                }
                nest--; expectOp("}"); return n;
            }
        }
        p--;
        error("didn't expect " + shown() + " here");
    }

    // "Hello {name}" -> text with values inside
    NP stringNode(const std::string& s, int line) {
        auto fix = [](std::string x) { for (auto& c : x) { if (c == '\x01') c = '{'; else if (c == '\x02') c = '}'; } return x; };
        NP n = mk(K::Interp, line);
        std::string lit; bool any = false;
        auto flush = [&]() {
            if (lit.empty()) return;
            NP l = mk(K::Lit, line); l->lit = Value::str(fix(lit)); n->a.push_back(l); lit.clear();
        };
        size_t i = 0;
        while (i < s.size()) {
            if (s[i] == '{') {
                size_t j = i + 1;
                while (j < s.size() && s[j] != '{' && s[j] != '}') j++;
                if (j < s.size() && s[j] == '}' && j > i + 1) {
                    flush();
                    Parser sub(tokenize(s.substr(i + 1, j - i - 1), line, false));
                    n->a.push_back(sub.expr());
                    any = true; i = j + 1; continue;
                }
            }
            lit += s[i++];
        }
        if (!any) { NP l = mk(K::Lit, line); l->lit = Value::str(fix(s)); return l; }
        flush();
        return n;
    }
};
