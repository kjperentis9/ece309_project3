// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

TEST(reg_null_rejected) {
    ToolRegistry registry;

    CHECK(registry.size() == 0);
    CHECK(!registry.add(nullptr));
    CHECK(registry.size() == 0);
    CHECK(registry.find("missing") == nullptr);
    CHECK(registry.size() == 0);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry registry;
    REQUIRE(registry.add(std::make_unique<NamedTool>("alpha")));
    REQUIRE(registry.add(std::make_unique<NamedTool>("beta")));
    REQUIRE(registry.add(std::make_unique<NamedTool>("gamma")));

    int alpha_count = 0;
    int beta_count = 0;
    int gamma_count = 0;
    int total = 0;

    registry.for_each([&](const Tool& tool) {
        std::string name = tool.name();

        if (name == "alpha") {
            alpha_count++;
        } else if (name == "beta") {
            beta_count++;
        } else if (name == "gamma") {
            gamma_count++;
        }

        total++;
    });

    CHECK(alpha_count == 1);
    CHECK(beta_count == 1);
    CHECK(gamma_count == 1);
    CHECK(total == 3);
}

TEST(reg_10000_tools) {
    ToolRegistry registry;
    Tool* first_tool = nullptr;

    for (int i = 0; i < 10000; i++) {
        std::string name = "tool_" + std::to_string(i);
        REQUIRE(registry.add(std::make_unique<NamedTool>(name)));

        if (i == 0) {
            first_tool = registry.find(name);
            REQUIRE(first_tool != nullptr);
        }

        CHECK(registry.size() == static_cast<std::size_t>(i + 1));
        CHECK(registry.load_factor() <= 1.0);
    }

    for (int i = 0; i < 10000; i++) {
        std::string name = "tool_" + std::to_string(i);
        Tool* tool = registry.find(name);

        REQUIRE(tool != nullptr);
        CHECK(tool->name() == name);
    }

    CHECK(registry.find("tool_0") == first_tool);
    CHECK(registry.max_probe_length() <= 64);

    std::printf("Real hash: buckets=%zu, load=%.6f, longest chain=%zu\n",
                registry.bucket_count(),
                registry.load_factor(),
                registry.max_probe_length());
}

TEST(reg_alternative_hash) {
    ToolRegistry registry;

    for (int i = 0; i < 10000; i++) {
        std::string name = "tool_" + std::to_string(i);
        REQUIRE(registry.add(std::make_unique<NamedTool>(name)));
    }

    std::vector<int> bucket_counts(registry.bucket_count(), 0);

    // Compare the weak hash using the same final bucket count.
    for (int i = 0; i < 10000; i++) {
        std::string name = "tool_" + std::to_string(i);
        std::size_t weak_hash =
            static_cast<unsigned char>(name[0]) + name.size();
        std::size_t index = weak_hash % bucket_counts.size();

        bucket_counts[index]++;
    }

    int longest_chain = 0;

    for (int count : bucket_counts) {
        if (count > longest_chain) {
            longest_chain = count;
        }
    }

    CHECK(longest_chain == 9000);

    std::printf("Comparison: buckets=%zu, load=%.6f, "
                "real longest=%zu, weak longest=%d\n",
                registry.bucket_count(),
                registry.load_factor(),
                registry.max_probe_length(),
                longest_chain);
}

TEST(ws_accepts_relative_paths) {
    TempDir directory;
    Workspace workspace(directory.path());
    fs::path result;
    std::string reason;

    std::vector<std::string> paths = {
        "notes/todo.txt",
        "./notes//todo.txt",
        ".",
        "a/../b",
        "a/..",
        "new/dir/file.txt",
        "my..notes.txt"
    };

    std::vector<fs::path> expected = {
        workspace.root() / "notes" / "todo.txt",
        workspace.root() / "notes" / "todo.txt",
        workspace.root(),
        workspace.root() / "b",
        workspace.root(),
        workspace.root() / "new" / "dir" / "file.txt",
        workspace.root() / "my..notes.txt"
    };

    for (std::size_t i = 0; i < paths.size(); i++) {
        REQUIRE(workspace.resolve(paths[i], &result, &reason));
        CHECK(result == expected[i]);
    }

    // Resolving a path should not create anything.
    CHECK(!fs::exists(workspace.root() / "new"));
}

TEST(ws_rejects_invalid_paths) {
    TempDir directory;
    Workspace workspace(directory.path());
    fs::path result;
    std::string reason;

    std::vector<std::string> paths = {
        "",
        "/etc/passwd",
        "..",
        "../etc/passwd",
        "a/../../b",
        "notes/../../ws/notes/todo.txt"
    };

    for (const std::string& path : paths) {
        reason.clear();

        CHECK(!workspace.resolve(path, &result, &reason));
        CHECK(!reason.empty());
    }
}

TEST(ws_rejects_null_character) {
    TempDir directory;
    Workspace workspace(directory.path());
    fs::path result;
    std::string reason;

    std::string path = std::string("notes") + '\0' + "/x";

    CHECK(!workspace.resolve(path, &result, &reason));
    CHECK(!reason.empty());
}

TEST(calc_left_to_right) {
    double value = 0.0;
    std::string error;

    REQUIRE(eval("10 - 4 - 3", &value, &error));
    CHECK(value == 3);

    REQUIRE(eval("100 / 10 / 5", &value, &error));
    CHECK(value == 2);
}

TEST(calc_parentheses_and_zero) {
    double value = 0.0;
    std::string error;

    REQUIRE(eval("(2 + 3) * 4", &value, &error));
    CHECK(value == 20);

    REQUIRE(eval("2 * (3 + 4) - 5", &value, &error));
    CHECK(value == 9);

    CHECK(!eval("1 / (2 - 2)", &value, &error));
    CHECK(error == "division by zero");
}

TEST(calc_negatives_decimals_and_extra_expressions) {
    double value = 0.0;
    std::string error;

    REQUIRE(eval("-3 * -3", &value, &error));
    CHECK(value == 9);

    REQUIRE(eval("7 / 2", &value, &error));
    CHECK(value == 3.5);

    // Two additional expressions beyond the spec's examples.
    REQUIRE(eval("1.5 * (8 - 2) + 0.5", &value, &error));
    CHECK(value == 9.5);

    REQUIRE(eval("18 / (3 * 2) - 7", &value, &error));
    CHECK(value == -4);
}

// ------------------------------------------------------------- your tests
//
// Still to write (spec §6 has the full list). Suggested names:
//   reg_null_rejected, reg_for_each_visits_each_once
//   reg_10000_tools               REQUIRED: print bucket_count / load_factor /
//                                 max_probe_length, check the bound
//   reg_alternative_hash          REQUIRED: longest chain of a weak hash
//   ws_<case>                     REQUIRED: every row of the table in spec §4.4
//   calc_<case>                   left-to-right, parentheses, negatives

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
