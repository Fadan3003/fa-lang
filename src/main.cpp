// Fa - your own programming language. Standalone C++17, no dependencies.
//
// Build:    sh build.sh        (or: make)         add "gui" for window support (needs raylib)
// Install:  sh install.sh      then type:  fa file.fa
//
// Source layout (read in this order):
//   common.h   shared includes          value.h    what a value is
//   ast.h      syntax tree              objects.h  scopes and functions
//   text.h     UTF-8 / characters       lexer.h    source -> tokens
//   parser.h   tokens -> syntax tree    helpers.h  printing, equality, compare
//   html.h     html + json builders     interp.h   the evaluator
//   methods.h  text/list/map methods    stdlib.h   built-in functions
//   server.h   web server               obfuscate.h  hides the source of web pages
//   gui/       optional window backend (raylib)

#include "common.h"
#include "value.h"
#include "ast.h"
#include "objects.h"
#include "text.h"
#include "lexer.h"
#include "parser.h"
#include "helpers.h"
#include "html.h"
#include "interp.h"
#include "methods.h"
#include "stdlib.h"
#include "server.h"

// the interactive prompt: a block (def / if / while / for) is finished with an empty line, like in Python
static int repl(Interp& in) {
    std::cout << "Fa 0.18 - type code, Ctrl+D to quit\n";
    std::string buf;
    bool startsBlock = false;                       // first line ended with ':'  (like  route "/":  )
    for (;;) {
        std::cout << (buf.empty() ? ">>> " : "... ") << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) { std::cout << "\n"; return 0; }
        bool blank = line.find_first_not_of(" \t\r") == std::string::npos;
        if (buf.empty()) { size_t e = line.find_last_not_of(" \t\r"); startsBlock = e != std::string::npos && line[e] == ':'; }
        buf += line + "\n";
        try {
            int open = 0; bool block = startsBlock;
            auto toks = tokenize(buf, 1, false);
            for (size_t i = 0; i < toks.size(); i++) {
                auto& tk = toks[i];
                if (i == 0 && tk.k == TK::Kw && (tk.s == "def" || tk.s == "if" || tk.s == "while" || tk.s == "for")) block = true;
                if (tk.k == TK::Op) {
                    if (tk.s == "{" || tk.s == "(" || tk.s == "[") open++;
                    else if (tk.s == "}" || tk.s == ")" || tk.s == "]") open--;
                }
            }
            if (open > 0) continue;
            if (block && !blank) continue;
            if (blank && buf.find_first_not_of(" \t\r\n") == std::string::npos) { buf.clear(); continue; }
            in.replLine(buf);
        } catch (FaError& e) { std::cerr << "Error: " << e.what() << '\n'; }
        buf.clear();
    }
}

static const char* NEW_TEMPLATE = R"FA(# @NAME@ - eine Webseite nur mit Fa
besucher = 0

route "/"
    besucher += 1
    page "@NAME@"
        h1 "Willkommen bei @NAME@"
        p "Besucher Nummer {besucher}."

# Im Inspektor (F12) steht standardmäßig diese ganze Datei statt html/head/body.
# Ausschalten:  serve 8080, {code_root: false}
serve 8080
)FA";

static void usage() {
    std::cout <<
        "Fa 0.18 - a small, easy programming language\n\n"
        "  fa                 interactive prompt\n"
        "  fa file.fa         run a program        (same: fa run file.fa)\n"
        "  fa new name        create name.fa with a starter website\n"
        "  fa version         show the version\n"
        "  fa help            show this text\n";
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001); SetConsoleCP(65001);   // show ä ö ü correctly in the Windows console
#endif
    std::string cmd = argc >= 2 ? argv[1] : "";
    if (cmd == "version" || cmd == "--version") { std::cout << "Fa 0.18\n"; return 0; }
    if (cmd == "help" || cmd == "--help" || cmd == "-h") { usage(); return 0; }
    if (cmd == "new") {
        if (argc < 3) { std::cerr << "usage: fa new name\n"; return 1; }
        std::string name = argv[2];
        if (name.size() > 3 && name.compare(name.size() - 3, 3, ".fa") == 0) name.resize(name.size() - 3);
        std::string path = name + ".fa";
        if (std::ifstream(path)) { std::cerr << "Error: " << path << " already exists\n"; return 1; }
        std::string text = NEW_TEMPLATE;
        for (size_t p; (p = text.find("@NAME@")) != std::string::npos;) text.replace(p, 6, name);
        std::ofstream(path) << text;
        std::cout << "created " << path << "\nrun it with:  fa " << path << "\n";
        return 0;
    }
    int fileArg = (cmd == "run") ? 2 : 1;
    if (cmd == "run" && argc < 3) { std::cerr << "usage: fa run file.fa\n"; return 1; }
    Interp in;
    for (int i = fileArg + 1; i < argc; i++) in.cliArgs.push_back(argv[i]);
    // values every program can use: os (windows / linux / macos), interpreter (path to fa itself), here (folder of the program)
#ifdef _WIN32
    in.builtins->m["os"] = Value::str("windows");
#elif defined(__APPLE__)
    in.builtins->m["os"] = Value::str("macos");
#else
    in.builtins->m["os"] = Value::str("linux");
#endif
    {
        std::string exe = argv[0];
        try { if (exe.find('/') != std::string::npos || exe.find('\\') != std::string::npos) exe = std::filesystem::absolute(exe).string(); } catch (...) {}
        in.builtins->m["interpreter"] = Value::str(exe);
    }
    in.builtins->m["here"] = Value::str(".");
    if (argc < 2) return repl(in);
    std::string path = argv[fileArg];
    std::ifstream f(path, std::ios::binary);
    if (!f && !(path.size() > 3 && path.compare(path.size() - 3, 3, ".fa") == 0)) { path += ".fa"; f.open(path, std::ios::binary); }
    if (!f) { std::cerr << "Error: can't open " << argv[fileArg] << '\n'; return 1; }
    try { in.builtins->m["here"] = Value::str(std::filesystem::absolute(std::filesystem::path(path)).parent_path().string()); } catch (...) {}
    std::stringstream ss; ss << f.rdbuf();
    std::string src = ss.str();
    if (src.rfind("\xEF\xBB\xBF", 0) == 0) src = src.substr(3);
    in.sourceText = src;
    try { in.sourceName = std::filesystem::path(path).filename().string(); } catch (...) { in.sourceName = path; }
    try { in.runSource(src); }
    catch (FaError& e) { std::cout.flush(); std::cerr << "Error: " << e.what() << '\n'; return 1; }
    return 0;
}
