# C-Slash

Private SDK and runtime for **C/** (C-slash). The public language spec and examples live in the `cslash` repository.

Proprietary. All rights reserved. See `LICENSE`.

## Status

v1.0.0 core interpreter. Implements, per the C/ spec:

| Directive | Behavior |
|-----------|----------|
| `#import:` | Loads a file once (idempotent). `.cslash` files run; other files such as `pr.txt` are recorded. |
| `#include:` | Raw paste: runs the file's statements again on every include. |
| `#create:` | Creates an empty file at `to:` (default `dir/default`). |
| `#convert:` | `to: cslash` only. Writes a `.cslash` wrapper that records the source language and imports the original file. |
| `#if` / `then:` | Checks a file exists in a directory, or is `valid`. Runs the action only when true. |
| `#run:` | Runs a validated `.cslash` file, then the `then:` action. |
| `#byte_generate:` | Writes a binary file (real PNG for `.png`, empty otherwise). |
| `#print:` | Prints its text arguments, separated by spaces. Use single quotes around text that contains double quotes. |

Local file checks work on both the PR and the normal Runtime.

## Build

Requires CMake 3.16+ and a C++17 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Targets: Linux, Windows, macOS and Android (arm64, via the NDK toolchain; tests run on the other three).

## Use

```bash
cslash run hello.cslash [--cache pr.cache] [--strict]
cslash check hello.cslash
cslash version
```

- Without `pr.cache`, `dir/default` is the platform Downloads folder.
- `from: "pr.cache"` (or `--cache`) switches to PR mode and overrides directories.
- `--strict` turns a missing `#import` file or `pr.cache` into an error instead of a warning.

## Sandbox

All file access is confined to the configured `dir/NAME` directories (read and write) and the script's own folder (read only). Paths that leave those roots, including `..` escapes, are rejected. The interpreter has no network access.

## Layout

```text
.
├── CMakeLists.txt
├── include/cslash/   # public headers
├── src/              # lexer, parser, runtime, interpreter, png, cli
├── tests/            # test runner
└── .github/workflows/ci.yml
```

## Known gaps

- `#convert:` does not translate foreign code into C/ itself yet; it writes a wrapper that imports the original file.
- Grammar extensions from `pr.txt` are not applied; both quote styles are always accepted.
- `#include:` runs statements rather than splicing raw text before lexing.
