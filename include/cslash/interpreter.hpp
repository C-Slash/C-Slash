#pragma once

#include <filesystem>
#include <iosfwd>
#include <set>
#include <string>

#include "cslash/ast.hpp"
#include "cslash/runtime.hpp"

namespace cslash {

struct Options {
    // In strict mode a missing #import file or pr.cache is an error;
    // otherwise it is reported as a warning and skipped.
    bool strict = false;
    std::ostream* out = nullptr;  // progress messages (default: std::cout)
    std::ostream* err = nullptr;  // warnings (default: std::cerr)
};

class Interpreter {
public:
    Interpreter(Runtime& runtime, Options options = {});

    // Parses and runs a file. Throws Error subclasses on failure.
    void run_file(const fs::path& file);

    // Parses and runs source text.
    void run_source(const std::string& source);

    // True if the file exists and parses as C/.
    static bool is_valid(const fs::path& file);

    // Result of the most recent #if (false before any #if).
    bool last_condition() const { return last_condition_; }

private:
    Runtime& rt_;
    Options opt_;
    std::set<fs::path> imported_;
    std::set<fs::path> caches_;
    bool last_condition_ = false;
    int depth_ = 0;

    void exec(const Node& n);
    void exec_import(const Node& n);
    void exec_include(const Node& n);
    void exec_create(const Node& n);
    void exec_convert(const Node& n);
    void exec_run(const Node& n);
    void exec_byte_generate(const Node& n);
    bool eval_condition(const Node& n);
    void ensure_cache(const std::string& name);

    std::ostream& out() const;
    std::ostream& err() const;
};

}  // namespace cslash
