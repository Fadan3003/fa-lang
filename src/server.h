#pragma once
// serve 8080: a small web server on raw sockets. The pages are the routes you defined:
//     route "/"
//         page "Hello"
//             h1 "Hello"
// "*" catches every other address. Inside a route these variables exist:
//     path (the address without ?...), method ("GET" / "POST"), query (a map of the ?a=1&b=2 values), body (text sent with a POST)
#include "interp.h"
#include "html.h"
#include "obfuscate.h"

static std::string urlDecode(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '+') r += ' ';
        else if (s[i] == '%' && i + 2 < s.size() && isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) {
            r += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2;
        } else r += s[i];
    }
    return r;
}

static Value parseQuery(const std::string& qs) {
    Value m; m.t = T::Map; auto mo = std::make_shared<MapObj>(); m.o = mo;
    size_t pos = 0;
    while (pos < qs.size()) {
        size_t amp = qs.find('&', pos);
        std::string pair = qs.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        size_t eq = pair.find('=');
        std::string k = urlDecode(pair.substr(0, eq)), v = eq == std::string::npos ? "" : urlDecode(pair.substr(eq + 1));
        if (!k.empty()) mo->v.push_back({Value::str(k), Value::str(v)});
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return m;
}

inline Value Interp::serve(std::vector<Value>& a) {
    if (a.empty() || a.size() > 2 || a[0].t != T::Int || (a.size() == 2 && a[1].t != T::Map))
        throw FaError("serve needs a port and optionally options, like:  serve 8080   or   serve 8080, {obfuscate: true}");
    int showCode = -1;                                        // serve 8080, {show_code: true}  (0 = off, 1 = the code of the page, 2 = the whole file; not set: the whole file when code_root is on)
    int codeRoot = 1;                                         // on by default: the inspector (F12) starts with your Fa file instead of html / head / body
                                                              // serve 8080, {code_root: false}  turns it off;  "sources": only the file in the Sources tab
    Obfuscator obf;                                           // serve 8080, {obfuscate: true, keep: ["name"]}
    obf.on = true;                                            // on by default: the inspector (F12) shows no readable page
    if (a.size() == 2) {
        int f = mapFind(M(a[1]), Value::str("obfuscate"));
        if (f >= 0) {
            const Value& v = M(a[1])->v[f].second;
            if (v.t == T::Bool) obf.on = v.b;
            else if (v.t == T::Str && S(v) == "light") { obf.on = true; obf.light = true; }
            else throw FaError("obfuscate: true, false or \"light\"");
        }
        f = mapFind(M(a[1]), Value::str("keep"));
        if (f >= 0) {
            const Value& v = M(a[1])->v[f].second;
            if (v.t != T::List) throw FaError("keep needs a list of names, like:  keep: [\"menu\", \"code\"]");
            for (auto& x : L(v)) obf.keep.insert(show(x));
        }
        f = mapFind(M(a[1]), Value::str("show_code"));
        if (f >= 0) {
            const Value& v = M(a[1])->v[f].second;
            if (v.t == T::Bool) showCode = v.b ? 1 : 0;
            else if (v.t == T::Str && S(v) == "file") showCode = 2;
            else throw FaError("show_code: true, false or \"file\"");
        }
        f = mapFind(M(a[1]), Value::str("code_root"));
        if (f >= 0) {
            const Value& v = M(a[1])->v[f].second;
            if (v.t == T::Bool) codeRoot = v.b ? 1 : 0;
            else if (v.t == T::Str && S(v) == "sources") codeRoot = 2;
            else throw FaError("code_root: true, false or \"sources\"");
        }
        for (auto& kv : M(a[1])->v) {
            std::string k = show(kv.first);
            if (k != "obfuscate" && k != "keep" && k != "show_code" && k != "code_root")
                throw FaError("serve doesn't know the option '" + k + "' (it knows: obfuscate, keep, show_code, code_root)");
        }
    }
    if (showCode < 0) showCode = codeRoot ? 2 : 0;                    // code_root shows the whole file (show_code: true narrows it to the route)
    if (obf.on) obf.salt = base36(std::random_device{}() ^ (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count(), 8);
    if (routes.empty())
        throw FaError("there are no pages yet - add some before serve, like:\n    route \"/\"\n        page \"Hello\"\n            h1 \"Hello\"");
    int port = (int)a[0].i;
#ifdef _WIN32
    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
#endif
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);                  // only this computer can reach the server
    if (bind(s, (sockaddr*)&addr, sizeof addr) != 0) throw FaError("can't use port " + std::to_string(port) + " (is something else using it?)");
    listen(s, 16);
    std::cout << "Serving on http://localhost:" << port << "  (Ctrl+C to stop)" << std::endl;
    if (codeRoot) std::cout << "Inspector (F12): no html / head / body" << (codeRoot == 1 ? ", your Fa code stands there instead" : "") << " (the file is also in the Sources tab).  Turn off: serve " << port << ", {code_root: false}" << std::endl;
    if (showCode == 2) std::cout << "WARNING: the WHOLE file " << sourceName << " is sent to every visitor. Keep passwords and keys out of it!  (only the route: show_code: true, nothing: show_code: false)" << std::endl;
    if (showCode == 1) std::cout << "WARNING: the Fa code of each page is readable for every visitor (Sources tab). Keep secrets out of those lines." << std::endl;
    if (codeRoot) std::cout << "Pages with their own script need:  serve " << port << ", {code_root: false, obfuscate: \"light\"}" << std::endl;
    if (obf.on) std::cout << "Source protection is on: " << (obf.light ? "the page source is encoded" : "the page source is encoded and tags, classes and ids are renamed")
                          << "  (turn off with:  serve " << port << ", {obfuscate: false})" << std::endl;
    std::set<std::string> warned;
    for (;;) {
        sock_t c = accept(s, nullptr, nullptr);
        if (c == BAD_SOCK) continue;
        // read the request: first the head, then as much body as Content-Length says
        std::string req; char buf[4096];
        while (req.find("\r\n\r\n") == std::string::npos && req.size() < 65536) {
            int n = (int)recv(c, buf, sizeof buf, 0);
            if (n <= 0) break;
            req.append(buf, n);
        }
        size_t he = req.find("\r\n\r\n");
        std::string head = he == std::string::npos ? req : req.substr(0, he);
        std::string reqBody = he == std::string::npos ? "" : req.substr(he + 4);
        size_t want = 0;
        {
            std::string low = head; for (auto& ch : low) ch = (char)tolower((unsigned char)ch);
            size_t cl = low.find("content-length:");
            if (cl != std::string::npos) want = (size_t)strtoull(low.c_str() + cl + 15, nullptr, 10);
            if (want > 50000000) want = 50000000;
        }
        while (reqBody.size() < want) {
            int n = (int)recv(c, buf, sizeof buf, 0);
            if (n <= 0) break;
            reqBody.append(buf, n);
        }
        std::string method, full;
        { std::istringstream ls(head); ls >> method >> full; }
        size_t qpos = full.find('?');
        std::string path = full.substr(0, qpos);
        Value query = parseQuery(qpos == std::string::npos ? "" : full.substr(qpos + 1));

        std::string ctype = "text/html; charset=utf-8", body, codeKey;
        respStatus = 200; respType = "";
        try {
            Value h; bool found = false;
            for (auto& r : routes) if (r.first == path) { h = r.second; found = true; codeKey = r.first; }
            if (!found && path != "/favicon.ico") for (auto& r : routes) if (r.first == "*") { h = r.second; found = true; codeKey = r.first; }   // "*" doesn't catch the browser's icon request
            if (!found && path == "/favicon.ico") {
                respStatus = 204;                                          // browsers ask for an icon on their own: answer "none" instead of an error
            } else if (!found) {
                respStatus = 404; body = pageHtml("Not found", "<h1>404</h1><p>No page at " + escapeHtml(path) + "</p>");
            } else {
                Value r;
                if (isBlock(h)) {
                    Value returned;
                    auto kids = collect(h, &returned, {{"path", Value::str(path)}, {"method", Value::str(method)},
                                                       {"query", query}, {"body", Value::str(reqBody)}});
                    if (returned.t != T::Nil) r = returned;                       // the route used  return
                    else if (kids.size() == 1) r = kids[0];                       // the route made one page
                    else if (kids.size() > 1) { std::string all; for (auto& k : kids) all += renderHtml(k); r = Value::rawhtml(all); }
                } else {
                    std::vector<Value> args;
                    if (h.t == T::Func && F(h)->node->names.size() >= 1) args.push_back(Value::str(path));
                    r = call(h, args);
                }
                if ((r.t == T::Map || r.t == T::List) && respType.empty()) {      // data is sent as JSON
                    ctype = "application/json"; toJson(r, body);
                } else {
                    if (r.t != T::Nil) body = show(r);                            // a route that shows nothing sends an empty page
                    if (!respType.empty()) ctype = respType;
                }
            }
        } catch (FaError& e) {
            respStatus = 500; ctype = "text/plain; charset=utf-8"; body = std::string("Error in your program: ") + e.what();
            std::cerr << "Error: " << e.what() << std::endl;
        }
        if (respStatus == 204) ctype = "text/plain";
        bool isHtml = ctype.rfind("text/html", 0) == 0 && !body.empty();
        std::string codeHeader, codeText, fileText; bool haveCode = false;
        std::string fileName = sourceName.empty() ? "program.fa" : sourceName;
        if (showCode > 0 && isHtml && (showCode == 2 ? !sourceText.empty() : !codeKey.empty())) {   // the Fa code, readable in the inspector
            haveCode = true;
            if (showCode == 2) { codeHeader = "# " + sourceName; codeText = sourceText; fileText = sourceText; }
            else {
                codeHeader = "# " + sourceName + ", route \"" + codeKey + "\""; codeText = routeCode[codeKey];
                int ln = routeLine.count(codeKey) ? routeLine[codeKey] : 1;   // empty lines in front, so the numbers match the real file
                fileText = std::string(ln > 1 ? ln - 1 : 0, '\n') + codeText;
            }
        }
        fileText.erase(std::remove(fileText.begin(), fileText.end(), '\r'), fileText.end());      // Windows line endings are not needed
        if (codeRoot && isHtml) {                                           // the page goes into <fa-view>, the code becomes the root of the document
            Obfuscator t = obf; if (!t.on) { t.on = true; t.light = true; }  // (without protection the page is only tidied up)
            std::vector<std::string> lines; std::string all = (haveCode && codeRoot == 1) ? codeHeader + "\n" + codeText : "", line;
            bool ownScript = body.find("<script") != std::string::npos;
            for (size_t i = 0; !all.empty() && i <= all.size(); i++) {
                if (i == all.size() || all[i] == '\n') {
                    size_t lead = 0; while (lead < line.size() && line[lead] == ' ') lead++;
                    std::string indent; for (size_t k = 0; k < lead; k++) indent += "\xE2\xA0\x80";
                    lines.push_back(line.empty() ? std::string("\xE2\xA0\x80") : indent + line.substr(lead)); line.clear();   // an empty line becomes an empty braille cell, so the gap stays visible
                } else if (all[i] != '\r') line += all[i];
            }
            if (ownScript && warned.insert(path).second)
                std::cerr << "Note: the page " << path << " has its own script. While the inspector shows your code (code_root) a script can't reach the page (it lives inside <fa-view>).\n"
                          << "      Use  serve " << port << ", {code_root: false, obfuscate: \"light\"}  for pages with scripts." << std::endl;
            body = t.rootPage(t.html(body), lines, haveCode ? fileName : "", haveCode ? fileText : "");
        } else {
            bool ownScript = body.find("<script") != std::string::npos;     // (before our own script is added)
            if (haveCode) body = Obfuscator::withSourceMap(body, fileName, fileText);
            if (obf.on) {                                                   // hide the source of pages (and the style sheets that go with them)
                if (ctype.rfind("text/html", 0) == 0) {
                    if (!obf.light && ownScript && warned.insert(path).second)
                        std::cerr << "Note: the page " << path << " has its own script. While names are scrambled a script can't find elements by id, class or tag name.\n"
                                  << "      Use  serve " << port << ", {obfuscate: \"light\"}  (or list the names:  keep: [\"menu\"] ) for pages with scripts." << std::endl;
                    body = obf.page(body);
                } else if (!obf.light && ctype.rfind("text/css", 0) == 0) body = obf.css(body);
            }
        }
        int status = respStatus;
        const char* text = status == 200 ? "OK" : status == 201 ? "Created" : status == 204 ? "No Content" : status == 301 ? "Moved Permanently" :
            status == 302 ? "Found" : status == 400 ? "Bad Request" : status == 403 ? "Forbidden" :
            status == 404 ? "Not Found" : status == 500 ? "Internal Server Error" : "OK";
        std::string resp = "HTTP/1.1 " + std::to_string(status) + " " + text + "\r\nContent-Type: " + ctype +
            "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" + body;
        size_t sent = 0;
        while (sent < resp.size()) {
#ifdef MSG_NOSIGNAL
            int n = (int)send(c, resp.data() + sent, resp.size() - sent, MSG_NOSIGNAL);
#else
            int n = (int)send(c, resp.data() + sent, (int)(resp.size() - sent), 0);
#endif
            if (n <= 0) break;
            sent += n;
        }
        CLOSESOCK(c);
    }
    return Value();
}
