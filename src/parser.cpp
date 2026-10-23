#include "cslash/parser.hpp"

#include "cslash/lexer.hpp"

namespace cslash {

namespace {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : t_(std::move(tokens)) {}

    Program run() {
        Program prog;
        NodePtr last;
        while (!at(Tok::End)) {
            if (at(Tok::Semi)) { ++p_; continue; }

            const std::size_t before = p_;
            if (at(Tok::Directive)) {
                last = directive();
                prog.statements.push_back(last);
            } else if (at(Tok::Key)) {
                if (!last) fail("'" + cur().text + ":' has no directive to continue");
                items(*last);
            } else {
                fail("unexpected '" + cur().text + "' (a statement starts with '#')");
            }
            if (at(Tok::Semi)) ++p_;
            if (p_ == before) fail("could not parse '" + cur().text + "'");
        }
        return prog;
    }

private:
    std::vector<Token> t_;
    std::size_t p_ = 0;

    const Token& cur() const { return t_[p_]; }
    bool at(Tok k) const { return t_[p_].kind == k; }

    [[noreturn]] void fail(const std::string& msg) const {
        throw SyntaxError(msg, cur().line);
    }

    NodePtr directive() {
        auto node = std::make_shared<Node>();
        node->name = cur().text;
        node->line = cur().line;
        ++p_;
        items(*node);
        return node;
    }

    // `then:` target: a `#directive` or a bare `name args` action.
    NodePtr action() {
        if (at(Tok::Directive)) return directive();
        if (at(Tok::Word)) {
            auto node = std::make_shared<Node>();
            node->name = cur().text;
            node->line = cur().line;
            ++p_;
            items(*node);
            return node;
        }
        fail("expected an action after 'then:'");
    }

    void item(Node& n) {
        if (at(Tok::Key)) {
            const std::string key = cur().text;
            ++p_;
            if (key == "then") {
                n.action = action();
                return;
            }
            if (at(Tok::String) || at(Tok::Word)) {
                n.params[key] = cur().text;
                ++p_;
                return;
            }
            fail("missing value after '" + key + ":'");
        }
        if (at(Tok::String) || at(Tok::Word)) {
            const std::string first = cur().text;
            ++p_;
            if (at(Tok::Equals)) {
                ++p_;
                if (!(at(Tok::String) || at(Tok::Word))) fail("missing value after '='");
                n.has_condition = true;
                n.condition = {first, cur().text};
                ++p_;
            } else {
                n.args.push_back(first);
            }
            return;
        }
        fail("unexpected '" + cur().text + "'");
    }

    void items(Node& n) {
        while (!at(Tok::End) && !at(Tok::Semi) && !at(Tok::Directive)) {
            const std::size_t before = p_;
            item(n);
            if (p_ == before) fail("could not parse '" + cur().text + "'");
            if (at(Tok::Comma)) {
                ++p_;
                if (at(Tok::Directive)) {
                    n.action = directive();
                    return;
                }
            }
        }
    }
};

}  // namespace

Program parse(const std::string& source) {
    return Parser(lex(source)).run();
}

}  // namespace cslash
