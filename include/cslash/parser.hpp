#pragma once

#include <string>

#include "cslash/ast.hpp"

namespace cslash {

// Parses C/ source into a Program. Throws SyntaxError.
//
// Grammar notes (see SPEC.md in the public cslash repo):
//  * A statement ends at `;`, at end of input, or where a new `#directive`
//    starts (unless it follows a comma or `then:`, where it is the action).
//  * Items are separated by commas; `key: value` sets a parameter.
//  * A statement that starts with `key:` continues the previous directive,
//    so `from: "pr.cache";` and `then: create "x";` may sit on their own line.
//  * `lhs = rhs` sets the condition used by `#if`.
Program parse(const std::string& source);

}  // namespace cslash
