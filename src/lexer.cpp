#include "cslash/lexer.hpp"

#include <cctype>

namespace cslash {

#include "cslash/lexer.hpp"

#include <cctype>

namespace cslash {

namespace {

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

bool is_name_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool is_word_stop(char c) {
    return is_space(c) || c == ',' || c == ';' || c == '=' || c == '"' || c == '\'';
}

}  // namespace

std::vector<Token> lex(const std::string& src) {
    std::vector<Token> out;
    const std::size_t n = src.size();
    std::size_t i = 0;
    int line = 1;

    while (i < n) {
        const char c = src[i];

        if (c == '\n') {
            ++line;
            ++i;
            continue;
        }
        if (is_space(c)) {
            ++i;
            continue;
        }

        // Comment: `*` then optional blanks then `//`, to end of line.
        if (c == '*') {
            std::size_t j = i + 1;
            while (j < n && (src[j] == ' ' || src[j] == '\t')) ++j;
            if (j + 1 < n && src[j] == '/' && src[j + 1] == '/') {
                while (i < n && src[i] != '\n') ++i;
                continue;
            }
            throw SyntaxError("stray '*' (comments are written '* //')", line);
        }

        if (c == ',') { out.push_back({Tok::Comma, ",", line}); ++i; continue; }
        if (c == ';') { out.push_back({Tok::Semi, ";", line}); ++i; continue; }
        if (c == '=') { out.push_back({Tok::Equals, "=", line}); ++i; continue; }

        if (c == '"' || c == '\'') {
            const char quote = c;
            const int start_line = line;
            std::string text;
            ++i;
            bool closed = false;
            while (i < n) {
                if (src[i] == quote) { closed = true; ++i; break; }
                if (src[i] == '\n') ++line;
                text.push_back(src[i]);
                ++i;
            }
            if (!closed) throw SyntaxError("unterminated string", start_line);
            out.push_back({Tok::String, text, start_line});
            continue;
        }

        if (c == '#') {
            ++i;
            std::string name;
            while (i < n && is_name_char(src[i])) name.push_back(src[i++]);
            if (name.empty()) throw SyntaxError("expected a directive name after '#'", line);
            if (i < n && src[i] == ':') ++i;
            out.push_back({Tok::Directive, name, line});
            continue;
        }

        // Bare word or `key:`.
        std::string word;
        while (i < n && !is_word_stop(src[i])) word.push_back(src[i++]);
        if (word.empty()) throw SyntaxError(std::string("unexpected character '") + c + "'", line);
        if (word.size() > 1 && word.back() == ':') {
            word.pop_back();
            out.push_back({Tok::Key, word, line});
        } else {
            out.push_back({Tok::Word, word, line});
        }
    }

    out.push_back({Tok::End, "", line});
    return out;
}

}  // namespace cslash {

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

bool is_name_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool is_word_stop(char c) {
    return is_space(c) || c == ',' || c == ';' || c == '=' || c == '"' || c == '\'';
}

}  // namespace

std::vector<Token> lex(const std::string& src) {
    std::vector<Token> out;
    const std::size_t n = src.size();
    std::size_t i = 0;
    int line = 1;

    while (i < n) {
        const char c = src[i];

        if (c == '\n') {
            ++line;
            ++i;
            continue;
        }
        if (is_space(c)) {
            ++i;
            continue;
        }

        // Comment: `*` then optional blanks then `//`, to end of line.
        if (c == '*') {
            std::size_t j = i + 1;
            while (j < n && (src[j] == ' ' || src[j] == '\t')) ++j;
            if (j + 1 < n && src[j] == '/' && src[j + 1] == '/') {
                while (i < n && src[i] != '\n') ++i;
                continue;
            }
            throw SyntaxError("stray '*' (comments are written '* //')", line);
        }

        if (c == ',') { out.push_back({Tok::Comma, ",", line}); ++i; continue; }
        if (c == ';') { out.push_back({Tok::Semi, ";", line}); ++i; continue; }
        if (c == '=') { out.push_back({Tok::Equals, "=", line}); ++i; continue; }

        if (c == '"' || c == '\'') {
            const char quote = c;
            const int start_line = line;
            std::string text;
            ++i;
            bool closed = false;
            while (i < n) {
                if (src[i] == quote) { closed = true; ++i; break; }
                if (src[i] == '\n') ++line;
                text.push_back(src[i]);
                ++i;
            }
            if (!closed) throw SyntaxError("unterminated string", start_line);
            out.push_back({Tok::String, text, start_line});
            continue;
        }

        if (c == '#') {
            ++i;
            std::string name;
            whi
