#pragma once

#include <filesystem>
#include <map>
#include <string>

#include "cslash/ast.hpp"

namespace cslash {

namespace fs = std::filesystem;

// Raised when a path would leave the sandbox.
struct SandboxError : Error {
    using Error::Error;
};

enum class Access { Read, Write };

// Parses the text of a pr.cache file:
//   {
//   dir/default = /home/C-Slash/Downloads
//   }
// Returns directory name (without the "dir/" prefix) -> path text.
std::map<std::string, std::string> parse_cache(const std::string& text);

// The platform's Downloads folder (the preset value of dir/default).
fs::path default_downloads();

// Directory table and sandbox for one run.
//
// Without a pr.cache, dir/default is the platform Downloads folder. A loaded
// pr.cache overrides entries. All file access is confined to the configured
// directories (read and write) and the script's own folder (read only).
class Runtime {
public:
    explicit Runtime(fs::path script_dir);

    // Loads a pr.cache file. Throws Error if it cannot be read or parsed.
    void load_cache(const fs::path& file);
    bool pr_mode() const { return pr_mode_; }

    const fs::path& script_dir() const { return script_dir_; }
    const std::map<std::string, fs::path>& dirs() const { return dirs_; }

    // Resolves a path written in C/ source:
    //   dir/NAME[/rest]  -> the configured directory plus rest
    //   absolute path    -> itself
    //   bare/relative    -> read: script folder, then dir/default
    //                       write: dir/default
    // Throws SandboxError when the result is outside the allowed roots.
    fs::path resolve(const std::string& spec, Access access) const;

private:
    fs::path script_dir_;
    std::map<std::string, fs::path> dirs_;
    bool pr_mode_ = false;

    bool allowed(const fs::path& p, Access access) const;
};

}  // namespace cslash
