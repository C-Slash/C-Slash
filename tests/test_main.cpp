// Minimal dependency-free test runner.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "cslash/interpreter.hpp"
#include "cslash/lexer.hpp"
#include "cslash/parser.hpp"
#include "cslash/runtime.hpp"

namespace fs = std::filesystem;

static int g_failed = 0;
static int g_total = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_total;                                                               \
        if (!(cond)) {                                                           \
            ++g_failed;                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                        \
    } while (0)

#define CHECK_THROWS(expr)                                                       \
    do {                                                                         \
        ++g_total;                                                               \
        bool threw = false;                                                      \
        try { expr; } catch (const cslash::Error&) { threw = true; }             \
        if (!threw) {                                                            \
            ++g_failed;                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  expected throw: " #expr "\n"; \
        }                                                                        \
    } while (0)

static fs::path make_temp_dir(const std::string& tag) {
    const fs::path base = fs::temp_directory_path() / ("cslash_test_" + tag);
    fs::remove_all(base);
    fs::create_directories(base);
    return fs::weakly_canonical(base);
}

static void write_file(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

static std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void test_lexer() {
    using cslash::Tok;
    auto toks = cslash::lex("* // comment\n#import: 'pr.txt';");
    CHECK(toks.size() == 4);  // directive, string, semi, end
    CHECK(toks[0].kind == Tok::Directive && toks[0].text == "import");
    CHECK(toks[1].kind == Tok::String && toks[1].text == "pr.txt");

    // Both quote styles, and nested quotes without escapes.
    toks = cslash::lex("#a: \"it's\", 'say \"hi\"';");
    CHECK(toks[1].text == "it's");
    CHECK(toks[3].text == "say \"hi\"");

    CHECK_THROWS(cslash::lex("#a: \"open"));
    CHECK_THROWS(cslash::lex("* not a comment"));
}

static void test_parser_spec_examples() {
    // README / spec quick look.
    const char* quick =
        "#import: \"pr.txt\";\n"
        "* // This is a native comment line!\n"
        "#create: \"byte_target.txt\", to: dir/default;\n"
        "from: \"pr.cache\";\n"
        "\n"
        "#if: \"byte_target.txt\" = dir/default/byte_target.txt,\n"
        "#convert: dir/default/byte_target.txt, to: cslash;\n"
        "\n"
        "#if dir/default/byte_target.cslash = valid,\n"
        "#run: \"byte_target.cslash\", then:\n"
        "#byte_generate: \"logo_test.png\"\n";
    auto prog = cslash::parse(quick);
    CHECK(prog.statements.size() == 4);

    const auto& create = *prog.statements[1];
    CHECK(create.name == "create");
    CHECK(create.args.size() == 1 && create.args[0] == "byte_target.txt");
    CHECK(create.params.at("to") == "dir/default");
    CHECK(create.params.at("from") == "pr.cache");  // continuation line

    const auto& if1 = *prog.statements[2];
    CHECK(if1.has_condition && if1.condition.lhs == "byte_target.txt");
    CHECK(if1.action && if1.action->name == "convert");
    CHECK(if1.action->params.at("to") == "cslash");

    const auto& if2 = *prog.statements[3];
    CHECK(if2.condition.rhs == "valid");
    CHECK(if2.action && if2.action->name == "run");
    CHECK(if2.action->action && if2.action->action->name == "byte_generate");
    CHECK(if2.action->action->args[0] == "logo_test.png");

    // The #if / from: / then: example (no semicolons on #import / then).
    const char* cond =
        "#import: \"pr.txt\"\n"
        "#if: \"2.2081.lol\" = dir/default,\n"
        "from: \"pr.cache\";\n"
        "    then: create \".gitattributes\"\n";
    prog = cslash::parse(cond);
    CHECK(prog.statements.size() == 2);
    const auto& iff = *prog.statements[1];
    CHECK(iff.params.at("from") == "pr.cache");
    CHECK(iff.action && iff.action->name == "create");
    CHECK(iff.action->args[0] == ".gitattributes");

    CHECK_THROWS(cslash::parse("from: \"x\";"));
    CHECK_THROWS(cslash::parse("#create: \"a\", to:;"));
}

static void test_cache() {
    const auto m = cslash::parse_cache("{\ndir/default = /home/C-Slash/Downloads\n}\n");
    CHECK(m.size() == 1 && m.at("default") == "/home/C-Slash/Downloads");

    const auto android = cslash::parse_cache("{\ndir/default = storage/emulated/0/Download\n}");
    CHECK(android.at("default") == "storage/emulated/0/Download");

    CHECK_THROWS(cslash::parse_cache("{\nnot a mapping\n}"));
    CHECK_THROWS(cslash::parse_cache("{\ndefault = /x\n}"));
}

static void test_pipeline() {
    // The spec's staged pipeline, end to end.
    const fs::path root = make_temp_dir("pipeline");
    const fs::path out_dir = root / "out";
    write_file(root / "pr.cache", "{\ndir/default = " + out_dir.generic_string() + "\n}\n");

    cslash::Runtime rt(root);
    std::ostringstream out, err;
    cslash::Options opt;
    opt.out = &out;
    opt.err = &err;
    cslash::Interpreter interp(rt, opt);

    interp.run_source(
        "#import: \"pr.txt\";\n"
        "#create: \"byte_target.txt\", to: dir/default;\n"
        "from: \"pr.cache\";\n"
        "#if: \"byte_target.txt\" = dir/default/byte_target.txt,\n"
        "#convert: dir/default/byte_target.txt, to: cslash;\n"
        "#if dir/default/byte_target.cslash = valid,\n"
        "#run: \"byte_target.cslash\", then:\n"
        "#byte_generate: \"logo_test.png\"\n");

    CHECK(rt.pr_mode());
    CHECK(fs::exists(out_dir / "byte_target.txt"));
    CHECK(fs::exists(out_dir / "byte_target.cslash"));
    CHECK(fs::exists(out_dir / "logo_test.png"));

    const std::string png = slurp(out_dir / "logo_test.png");
    CHECK(png.size() > 8 && png.compare(1, 3, "PNG") == 0);
    CHECK(png.find("IHDR") != std::string::npos);
    CHECK(png.find("IEND") != std::string::npos);

    CHECK(cslash::Interpreter::is_valid(out_dir / "byte_target.cslash"));
    CHECK(err.str().find("import not found") != std::string::npos);  // pr.txt absent
}

static void test_if_semantics() {
    const fs::path root = make_temp_dir("if");
    const fs::path out_dir = root / "out";
    fs::create_directories(out_dir);
    write_file(root / "pr.cache", "{\ndir/default = " + out_dir.generic_string() + "\n}\n");
    write_file(out_dir / "present.lol", "x");

    cslash::Runtime rt(root);
    std::ostringstream out, err;
    cslash::Options opt;
    opt.out = &out;
    opt.err = &err;
    cslash::Interpreter interp(rt, opt);

    // True: file present, so `then:` creates the marker.
    interp.run_source(
        "#if: \"present.lol\" = dir/default,\n"
        "from: \"pr.cache\";\n"
        "    then: create \"yes.txt\"\n");
    CHECK(interp.last_condition());
    CHECK(fs::exists(out_dir / "yes.txt"));

    // False: file missing, so `then:` is skipped.
    interp.run_source(
        "#if: \"absent.lol\" = dir/default;\n"
        "then: create \"no.txt\"\n");
    CHECK(!interp.last_condition());
    CHECK(!fs::exists(out_dir / "no.txt"));
}

static void test_sandbox() {
    const fs::path root = make_temp_dir("sandbox");
    const fs::path out_dir = root / "out";
    fs::create_directories(out_dir);
    write_file(root / "pr.cache", "{\ndir/default = " + out_dir.generic_string() + "\n}\n");

    cslash::Runtime rt(root);
    rt.load_cache(root / "pr.cache");

    CHECK(rt.resolve("dir/default/a.txt", cslash::Access::Write) == out_dir / "a.txt");
    CHECK_THROWS(rt.resolve("dir/default/../../escape.txt", cslash::Access::Write));
    CHECK_THROWS(rt.resolve("dir/missing/a.txt", cslash::Access::Read));
#ifndef _WIN32
    CHECK_THROWS(rt.resolve("/etc/passwd", cslash::Access::Read));
    CHECK_THROWS(rt.resolve("/etc/passwd", cslash::Access::Write));
#endif
    // Script folder is readable but not writable.
    CHECK(rt.resolve((root / "pr.cache").string(), cslash::Access::Read) == root / "pr.cache");
    CHECK_THROWS(rt.resolve((root / "x.txt").string(), cslash::Access::Write));

    cslash::Interpreter interp(rt);
    CHECK_THROWS(interp.run_source("#create: \"x.txt\", to: dir/default/../..;"));
}

static void test_strict_and_limits() {
    const fs::path root = make_temp_dir("strict");
    cslash::Runtime rt(root);
    cslash::Options opt;
    opt.strict = true;
    cslash::Interpreter interp(rt, opt);

    CHECK_THROWS(interp.run_source("#import: \"pr.txt\";"));
    CHECK_THROWS(interp.run_source("#create: \"a\", from: \"nope.cache\";"));
    CHECK_THROWS(interp.run_source("#bogus: \"a\";"));

    // A file that includes itself must stop, not recurse forever.
    const fs::path out_dir = root / "out";
    fs::create_directories(out_dir);
    write_file(root / "pr.cache", "{\ndir/default = " + out_dir.generic_string() + "\n}\n");
    write_file(root / "loop.cslash", "#include: \"loop.cslash\";\n");
    cslash::Runtime rt2(root);
    rt2.load_cache(root / "pr.cache");
    cslash::Interpreter interp2(rt2);
    CHECK_THROWS(interp2.run_file(root / "loop.cslash"));
}

int main() {
    test_lexer();
    test_parser_spec_examples();
    test_cache();
    test_pipeline();
    test_if_semantics();
    test_sandbox();
    test_strict_and_limits();

    std::cout << (g_total - g_failed) << "/" << g_total << " checks passed\n";
    return g_failed == 0 ? 0 : 1;
}
