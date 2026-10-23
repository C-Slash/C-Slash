#pragma once

#include <string>
#include <vector>

#include "cslash/ast.hpp"

namespace cslash {

enum class Tok { Directive, Key, Word, String, Comma, Semi, Equals, End };

struct Token {
    Tok kind;
    std::string text;
    int line;
};

// Splits C/ source into tokens. Comments (`* // ...`) are dropped.
// Single and double quotes are interchangeable.
std::vector<Token> lex(const std::string& source);

}  // namespace cslash
