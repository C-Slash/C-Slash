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
                   [](unsigned char c) { return static_cast
