#include "cslash/runtime.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace cslash {

namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

bool is_comment(const std::string& line) {
    if (line.empty() || line[0] != '*') return false;
    const auto j = line.find_first_not_of(" \t", 1);
    return j != std::string::npos && line.compare(j, 2, "//") == 0;
}

bool inside(const fs::path& root, const fs::path& p) {
    std::error_code ec;
    const fs::path r = fs::weakly_canonical(root, ec);
    if (ec) return false;
    const fs::path q = fs::weakly_canonical(p, ec);
    if (ec) return false;
    auto ri = r.begin();
    auto qi = q.begin();
    for (; ri != r.end(); ++ri, ++qi) {
        if (qi == q.end() || *ri != *qi) return false;
    }
    return true;
}

}  // namespace

std::map<std::string, std::string> parse_cache(const std::string& text) {
    std::map<std::string, std::string> out;
    std::istringstream in(text);
    std::string raw;
    int line_no = 0;
    while (std::getline(in, raw)) {
        ++line_no;
        std::string line = trim(raw);
        if (line.empty() || line == "{" || line == "}" || is_comment(line)) continue;

        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            throw SyntaxError("pr.cache: expected 'name = path'", line_no);
        }
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (!value.empty() && (value.front() == '"' || value.front() == '\'') &&
            value.back() == value.front() && value.size() >= 2) {
            value = value.substr(1, value.size() - 2);
        }
        if (key.rfind("dir/", 0) != 0 || key.size() == 4) {
            throw SyntaxError("pr.cache: key must look like dir/NAME", line_no);
        }
        if (value.empty()) throw SyntaxError("pr.cache: empty path", line_no);
        out[key.substr(4)] = value;
    }
    return out;
}

fs::path default_downloads() {
#if defined(_WIN32)
    const char* home = std::getenv("USERPROFILE");
    return fs::path(home ? home : ".") / "Downloads";
#elif defined(__ANDROID__)
    return fs::path("/storage/emulated/0/Download");
#else
    const char* home = std::getenv("HOME");
    return fs::path(home ? home : ".") / "Downloads";
#endif
}

Runtime::Runtime(fs::path script_dir) : script_dir_(std::move(script_dir)) {
    dirs_["default"] = default_downloads();
}

void Runtime::load_cache(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw Error("cannot read pr.cache: " + file.string());
    std::stringstream buf;
    buf << in.rdbuf();

    for (const auto& [name, value] : parse_cache(buf.str())) {
        fs::path p(value);
#if defined(__ANDROID__)
        // pr.cache on Android writes storage/emulated/0/... without a leading '/'.
        if (!p.is_absolute() && value.rfind("storage/", 0) == 0) p = fs::path("/") / p;
#endif
        if (!p.is_absolute()) p = fs::absolute(file.parent_path() / p);
        dirs_[name] = p.lexically_normal();
    }
    pr_mode_ = true;
}

bool Runtime::allowed(const fs::path& p, Access access) const {
    for (const auto& [name, root] : dirs_) {
        (void)name;
        if (inside(root, p)) return true;
    }
    return access == Access::Read && inside(script_dir_, p);
}

fs::path Runtime::resolve(const std::string& spec, Access access) const {
    if (spec.empty()) throw Error("empty path");

    fs::path result;
    if (spec.rfind("dir/", 0) == 0) {
        const auto slash = spec.find('/', 4);
        const std::string name = spec.substr(4, slash == std::string::npos ? std::string::npos : slash - 4);
        const auto it = dirs_.find(name);
        if (it == dirs_.end()) throw Error("unknown directory: dir/" + name);
        result = it->second;
        if (slash != std::string::npos && slash + 1 < spec.size()) result /= spec.substr(slash + 1);
    } else if (fs::path(spec).is_absolute()) {
        result = spec;
    } else if (access == Access::Read) {
        const fs::path beside = script_dir_ / spec;
        std::error_code ec;
        result = fs::exists(beside, ec) ? beside : dirs_.at("default") / spec;
    } else {
        result = dirs_.at("default") / spec;
    }

    result = result.lexically_normal();
    if (!allowed(result, access)) {
        throw SandboxError("blocked by sandbox: " + result.string());
    }
    return result;
}

std::string Runtime::to_spec(const fs::path& p) const {
    std::error_code ec;
    const fs::path full = fs::weakly_canonical(p, ec);
    if (!ec) {
        for (const auto& [name, root] : dirs_) {
            if (!inside(root, full)) continue;
            const fs::path base = fs::weakly_canonical(root, ec);
            if (ec) continue;
            return "dir/" + name + "/" + full.lexically_relative(base).generic_string();
        }
    }
    return p.generic_string();
}

}  // namespace cslash
