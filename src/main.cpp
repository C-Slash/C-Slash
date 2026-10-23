#include <filesystem>
#include <iostream>
#include <string>

#include "cslash/interpreter.hpp"
#include "cslash/parser.hpp"
#include "cslash/runtime.hpp"

#ifndef CSLASH_VERSION
#define CSLASH_VERSION "dev"
#endif

namespace {

void usage(std::ostream& os) {
    os << "cslash " << CSLASH_VERSION << "\n\n"
       << "usage:\n"
       << "  cslash run <file.cslash> [--cache <pr.cache>] [--strict]\n"
       << "  cslash check <file.cslash>\n"
       << "  cslash version\n";
}

}  // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    if (argc < 2) { usage(std::cerr); return 2; }

    const std::string cmd = argv[1];
    if (cmd == "version" || cmd == "--version") {
        std::cout << "cslash " << CSLASH_VERSION << "\n";
        return 0;
    }
    if (cmd == "help" || cmd == "--help" || cmd == "-h") { usage(std::cout); return 0; }
    if ((cmd != "run" && cmd != "check") || argc < 3) { usage(std::cerr); return 2; }

    const fs::path file = argv[2];
    std::string cache;
    cslash::Options opt;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--strict") opt.strict = true;
        else if (a == "--cache" && i + 1 < argc) cache = argv[++i];
        else { std::cerr << "unknown option: " << a << "\n"; usage(std::cerr); return 2; }
    }

    try {
        if (cmd == "check") {
            if (!cslash::Interpreter::is_valid(file)) {
                std::cerr << "invalid: " << file.string() << "\n";
                return 1;
            }
            std::cout << "ok: " << file.string() << "\n";
            return 0;
        }

        const fs::path abs = fs::absolute(file);
        cslash::Runtime rt(abs.parent_path());
        if (!cache.empty()) rt.load_cache(cache);
        cslash::Interpreter interp(rt, opt);
        interp.run_file(abs);
        return 0;
    } catch (const cslash::SyntaxError& e) {
        std::cerr << "syntax error (line " << e.line << "): " << e.what() << "\n";
        return 1;
    } catch (const cslash::Error& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}
