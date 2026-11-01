#pragma once
// The syntax tree the parser produces.
#include "value.h"

enum class K {
    // statements
    Assign, Fn, ExprS, Return, If, While, For, Break, Continue,
    // expressions
    Lit, Var, Interp, Bin, And, Or, Not, Neg, Range, ListL, MapL, Call, Index, Field
};

struct Node;
using NP = std::shared_ptr<Node>;
struct Node {
    K k;
    int line = 0;
    std::string s;                    // name or operator
    std::vector<NP> a;                // child expressions
    std::vector<std::string> names;   // function parameters
    std::vector<NP> body, body2;      // blocks
    Value lit;
    bool flag = false;                // if: has an else part
};
static NP mk(K k, int line) { auto n = std::make_shared<Node>(); n->k = k; n->line = line; return n; }
