#include "cslash/interpreter.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>

#include "cslash/parser.hpp"
#include "cslash/png.hpp"

namespace cslash {

namespace {

constexpr int kMaxDepth = 32;

std::string read_text(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw Error("cannot read file: " + file.string());
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void write_bytes(const fs::path& file, const void* data, std::size_t size) {
    std::error_code ec;
    if (file.has_parent_path()) fs::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) throw Error("cannot write file: " + file.string());
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
}
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

const char* language_of(const std::string& ext) {
    static const struct { const char* ext; const char* lang; } table[] = {
        {".js", "JavaScript"}, {".ts", "TypeScript"},
        {".java", "Java"}, {".class", "Java bytecode"}, {".jar", "Java archive"},
        {".c", "C"}, {".cpp", "C++"}, {".hpp", "C++"},
        {".cs", "C#"}, {".csx", "C#"}, {".csproj", "C#"}, {".sln", "C#"}, {".slnx", "C#"},
    };
    for (const auto& row : table) {
        if (ext == row.ext) return row.lang;
    }
    return "data";
}

}  // namespace

Interpreter::Interpreter(Runtime& runtime, Options options)
    : rt_(runtime), opt_(options) {}

std::ostream& Interpreter::out() const { return opt_.out ? *opt_.out : std::cout; }
std::ostream& Interpreter::err() const { return opt_.err ? *opt_.err : std::cerr; }

bool Interpreter::is_valid(const fs::path& file) {
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) return false;
    try {
        parse(read_text(file));
        return true;
    } catch (const Error&) {
        return false;
    }
}

void Interpreter::run_file(const fs::path& file) {
    try {
        run_source(read_text(file));
    } catch (const SyntaxError& e) {
        throw SyntaxError(file.filename().string() + ": " + e.what(), e.line);
    }
}
void Interpreter::run_source(const std::string& source) {
    if (depth_ >= kMaxDepth) throw Error("nesting too deep (possible #run/#include loop)");
    ++depth_;
    try {
        const Program prog = parse(source);
        for (const auto& stmt : prog.statements) exec(*stmt);
    } catch (...) {
        --depth_;
        throw;
    }
    --depth_;
}

void Interpreter::ensure_cache(const std::string& name) {
    fs::path found;
    std::error_code ec;
    for (const fs::path& base : {rt_.script_dir(), fs::current_path(ec)}) {
        const fs::path candidate = base / name;
        if (fs::is_regular_file(candidate, ec)) { found = candidate; break; }
    }
    if (found.empty()) {
        if (opt_.strict) throw Error("pr.cache not found: " + name);
        err() << "warning: cache not found, using defaults: " << name << "\n";
        return;
    }
    const fs::path key = fs::weakly_canonical(found, ec);
    if (caches_.insert(key).second) {
        rt_.load_cache(found);
        out() << "[cslash] loaded cache: " << found.string() << "\n";
    }
}

void Interpreter::exec(const Node& n) {
    const auto from = n.params.find("from");
    if (from != n.params.end()) ensure_cache(from->second);

    if (n.name == "import") exec_import(n);
    else if (n.name == "include") exec_include(n);
    else if (n.name == "create") exec_create(n);
    else if (n.name == "convert") exec_convert(n);
    else if (n.name == "run") exec_run(n);
    else if (n.name == "byte_generate") exec_byte_generate(n);
    else if (n.name == "print") {
        if (n.args.empty()) throw SyntaxError("#print needs some text", n.line);
        for (std::size_t i = 0; i < n.args.size(); ++i) {
            out() << (i == 0 ? "" : " ") << n.args[i];
        }
        out() << "\n";
    }
    else if (n.name == "if") {
        last_condition_ = eval_condition(n);
        if (last_condition_ && n.action) exec(*n.action);
        return;
    } else {
        throw SyntaxError("unknown directive '" + n.name + "'", n.line);
    }

    if (n.action) exec(*n.action);
}
void Interpreter::exec_import(const Node& n) {
    if (n.args.empty()) throw SyntaxError("#import needs a file", n.line);
    for (const auto& arg : n.args) {
        const fs::path path = rt_.resolve(arg, Access::Read);
        std::error_code ec;
        if (!fs::is_regular_file(path, ec)) {
            if (opt_.strict) throw Error("import not found: " + arg);
            err() << "warning: import not found, skipped: " << arg << "\n";
            continue;
        }
        const fs::path key = fs::weakly_canonical(path, ec);
        if (!imported_.insert(key).second) continue;  // loaded once only

        if (lower(path.extension().string()) == ".cslash") {
            run_file(path);
        } else {
            // Spec files such as pr.txt are recorded; grammar extensions are applied by
            // a later build of the lexer.
            out() << "[cslash] imported: " << path.string() << "\n";
        }
    }
}

void Interpreter::exec_include(const Node& n) {
    if (n.args.empty()) throw SyntaxError("#include needs a file", n.line);
    for (const auto& arg : n.args) {
        const fs::path path = rt_.resolve(arg, Access::Read);
        run_source(read_text(path));  // raw paste: runs again on every include
    }
}

void Interpreter::exec_create(const Node& n) {
    if (n.args.empty()) throw SyntaxError("#create needs a file name", n.line);
    const auto to = n.params.find("to");
    for (const auto& arg : n.args) {
        const std::string spec = (to != n.params.end()) ? to->second + "/" + arg : arg;
        const fs::path path = rt_.resolve(spec, Access::Write);
        std::error_code ec;
        if (fs::exists(path, ec)) {
            out() << "[cslash] exists: " << path.string() << "\n";
            continue;
        }
        write_bytes(path, "", 0);
        out() << "[cslash] created: " << path.string() << "\n";
    }
}

void Interpreter::exec_convert(const Node& n) {
    if (n.args.empty()) throw SyntaxError("#convert needs a source file", n.line);
    const auto to = n.params.find("to");
    if (to == n.params.end() || to->second != "cslash") {
        throw SyntaxError("#convert supports only 'to: cslash'", n.line);
    }

    const fs::path src = rt_.resolve(n.args[0], Access::Read);
    std::error_code ec;
    if (!fs::is_regular_file(src, ec)) throw Error("convert: file not found: " + src.string());

    const std::string ext = lower(src.extension().string());
    if (ext == ".cslash") throw Error("convert: already a .cslash file: " + src.string());

    fs::path dest = src;
    dest.replace_extension(".cslash");
    dest = rt_.resolve(dest.string(), Access::Write);

    const std::string lang = language_of(ext);

    // The generated .cslash file is a wrapper: it records the source language and
    // imports the original file, so C/ orchestrates the foreign code in place.
    const std::string spec = rt_.to_spec(src);
    if (spec.find('"') != std::string::npos && spec.find('\'') != std::string::npos) {
        throw Error("convert: path contains both quote types: " + spec);
    }
    const char quote = (spec.find('"') == std::string::npos) ? '"' : '\'';

    std::ostringstream body;
    body << "* // converted by cslash from " << src.filename().string() << " (" << lang << ")\n";
    body << "* // language: " << lang << "\n";
    body << "#import: " << quote << spec << quote << ";\n";
    const std::string result = body.str();
    write_bytes(dest, result.data(), result.size());
    out() << "[cslash] converted: " << src.string() << " -> " << dest.string() << "\n";
}

void Interpreter::exec_run(const Node& n) {
    if (n.args.empty()) throw SyntaxError("#run needs a file", n.line);
    const fs::path path = rt_.resolve(n.args[0], Access::Read);
    if (lower(path.extension().string()) != ".cslash") {
        throw Error("run: not a .cslash file: " + path.string());
    }
    if (!is_valid(path)) throw Error("run: file is missing or invalid: " + path.string());
    out() << "[cslash] running: " << path.string() << "\n";
    run_file(path);
}

void Interpreter::exec_byte_generate(const Node& n) {
    if (n.args.empty()) throw SyntaxError("#byte_generate needs a file name", n.line);
    const auto to = n.params.find("to");
    for (const auto& arg : n.args) {
        const std::string spec = (to != n.params.end()) ? to->second + "/" + arg : arg;
        const fs::path path = rt_.resolve(spec, Access::Write);
        if (lower(path.extension().string()) == ".png") {
            // Dark green placeholder badge, 32x32.
            const auto png = make_png(32, 32, 0x0F, 0x51, 0x32);
            write_bytes(path, png.data(), png.size());
        } else {
            write_bytes(path, "", 0);
        }
        out() << "[cslash] generated: " << path.string() << "\n";
    }
}

bool Interpreter::eval_condition(const Node& n) {
    if (!n.has_condition) throw SyntaxError("#if needs 'lhs = rhs'", n.line);
    const std::string& lhs = n.condition.lhs;
    const std::string& rhs = n.condition.rhs;
    std::error_code ec;

    if (rhs == "valid") {
        return is_valid(rt_.resolve(lhs, Access::Read));
    }

    // `"file" = dir/x`: the file exists in directory dir/x (or dir/x is that file).
    const fs::path base = rt_.resolve(rhs, Access::Read);
    if (fs::is_directory(base, ec)) {
        return fs::exists(base / fs::path(lhs).filename(), ec);
    }
    return fs::exists(base, ec) && base.filename() == fs::path(lhs).filename();
}

}  // namespace cslash
