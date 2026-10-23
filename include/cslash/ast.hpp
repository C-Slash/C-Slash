#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace cslash {

// Base class for every error raised by the SDK.
struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Raised for lexing and parsing problems. `line` is 1-based.
struct SyntaxError : Error {
    SyntaxError(const std::string& message, int line_number)
        : Error(message), line(line_number) {}
    int line;
};

struct Node;
using NodePtr = std::shared_ptr<Node>;

struct Condition {
    std::string lhs;
    std::string rhs;
};

// One directive, for example `#create: "a.txt", to: dir/default;`.
struct Node {
    std::string name;                           // directive name, without '#'
    std::vector<std::string> args;              // positional arguments
    std::map<std::string, std::string> params;  // key: value pairs (to, from, ...)
    bool has_condition = false;                 // set by `lhs = rhs`
    Condition condition;
    NodePtr action;                             // `then:` or comma-chained directive
    int line = 0;
};

struct Program {
    std::vector<NodePtr> statements;
};

}  // namespace cslash
