#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() : path_(fs::temp_directory_path() /
        ("rls-console-tests-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()))) {
        fs::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

void writeFile(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

int runConsole(const std::string& arguments, const fs::path& output) {
#ifdef _WIN32
    const auto command = "cmd.exe /D /S /C \"\"" + std::string(RLS_CONSOLE_PATH) + "\" " +
        arguments + " > \"" + output.string() + "\" 2>&1\"";
#else
    const auto command = "\"" + std::string(RLS_CONSOLE_PATH) + "\" " + arguments +
        " > \"" + output.string() + "\" 2>&1";
#endif
    return std::system(command.c_str());
}

int runConsoleFrom(const fs::path& directory, const std::string& arguments, const fs::path& output) {
    const auto previousDirectory = fs::current_path();
    fs::current_path(directory);
    const auto exitCode = runConsole(arguments, output);
    fs::current_path(previousDirectory);
    return exitCode;
}

std::string readFile(const fs::path& path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("could not read " + path.string());
    return std::string(std::istreambuf_iterator<char>{input}, {});
}

constexpr auto trackerSource =
    "extern enum Scene { SCENE_* }\n"
    "region RR_TEST {\n"
    "    name: \"Test\"\n"
    "    scene: SCENE_TEST\n"
    "    locations { RC_A: true }\n"
    "}\n";

TEST(ConsoleProject, ListsTrackerInHelp) {
    TemporaryDirectory directory;
    const auto log = directory.path() / "help.log";
    EXPECT_EQ(runConsole("--help", log), 0);
    EXPECT_NE(readFile(log).find("soh_tracker"), std::string::npos);
}

TEST(ConsoleProject, TrackerUsesExplicitFileParentAsSourceRoot) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "src" / "logic.rls", trackerSource);
    EXPECT_EQ(runConsoleFrom(directory.path(), "-t soh_tracker -o generated src/logic.rls",
        directory.path() / "explicit-tracker.log"), 0);
    const auto output = directory.path() / "generated";
    EXPECT_TRUE(fs::exists(output / "logic_tracker.gen.h"));
    EXPECT_TRUE(fs::exists(output / "logic_tracker.gen.cpp"));
    const auto listing = readFile(output / "logic_tracker.gen.txt");
    EXPECT_NE(listing.find("logic.rls"), std::string::npos);
    EXPECT_EQ(listing.find("src/logic.rls"), std::string::npos);
    EXPECT_EQ(listing.find(directory.path().string()), std::string::npos);
}

TEST(ConsoleProject, TrackerUsesCommonSourceParentForExplicitInputs) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "src" / "logic.rls", trackerSource);
    writeFile(directory.path() / "shared" / "helper.rls",
        "region RR_OTHER { locations { RC_B: false } }\n");
    EXPECT_EQ(runConsoleFrom(directory.path(),
        "-t soh_tracker -o generated src shared/helper.rls",
        directory.path() / "multiple-tracker.log"), 0);
    const auto listing = readFile(directory.path() / "generated" / "logic_tracker.gen.txt");
    EXPECT_NE(listing.find("src/logic.rls"), std::string::npos);
    EXPECT_NE(listing.find("shared/helper.rls"), std::string::npos);
}

TEST(ConsoleProject, TrackerRunsOnlyWhenConfiguredOrSelected) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1, "sources": ["src"],
        "transpilers": { "soh": { "output": "generated/soh" } }
    })");
    writeFile(directory.path() / "src" / "logic.rls", trackerSource);
    ASSERT_EQ(runConsoleFrom(directory.path(), "", directory.path() / "without-tracker.log"), 0)
        << readFile(directory.path() / "without-tracker.log");
    EXPECT_FALSE(fs::exists(directory.path() / "generated" / "soh" / "logic_tracker.gen.h"));

    writeFile(directory.path() / "rls.json", R"({
        "version": 1, "sources": ["src"],
        "transpilers": {
            "soh": { "output": "generated/soh" },
            "soh_tracker": { "output": "generated/tracker" }
        }
    })");
    ASSERT_EQ(runConsoleFrom(directory.path() / "src", "",
        directory.path() / "default-tracker.log"), 0)
        << readFile(directory.path() / "default-tracker.log");
    const auto listing = readFile(
        directory.path() / "generated" / "tracker" / "logic_tracker.gen.txt");
    EXPECT_NE(listing.find("src/logic.rls"), std::string::npos);
}

TEST(ConsoleProject, SelectsTrackerWithoutRunningOtherManifestTargets) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1, "sources": ["src"],
        "transpilers": {
            "soh_ap": { "output": "generated/ap" },
            "soh_tracker": { "output": "generated/tracker" }
        }
    })");
    writeFile(directory.path() / "src" / "logic.rls", trackerSource);
    EXPECT_EQ(runConsole("--project \"" + directory.path().string() + "\" -t soh_tracker",
        directory.path() / "select-tracker.log"), 0);
    EXPECT_TRUE(fs::exists(directory.path() / "generated" / "tracker" / "logic_tracker.gen.h"));
    EXPECT_FALSE(fs::exists(directory.path() / "generated" / "ap"));
}

TEST(ConsoleProject, OverridesTrackerOutputAndKeepsOtherManifestTargets) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1, "sources": ["src"],
        "transpilers": {
            "soh": { "output": "generated/soh" },
            "soh_tracker": { "output": "generated/tracker" }
        }
    })");
    writeFile(directory.path() / "src" / "logic.rls", trackerSource);
    ASSERT_EQ(runConsoleFrom(directory.path(),
        "--project . -t soh_tracker -o override", directory.path() / "override-tracker.log"), 0)
        << readFile(directory.path() / "override-tracker.log");
    EXPECT_TRUE(fs::exists(directory.path() / "override" / "logic_tracker.gen.cpp"));
    EXPECT_TRUE(fs::exists(directory.path() / "generated" / "soh" / "functions.gen.h"));
    EXPECT_FALSE(fs::exists(directory.path() / "generated" / "tracker"));
}

TEST(ConsoleProject, RejectsUnconfiguredTrackerSelectionAndUnknownConfiguredTarget) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1, "sources": ["src"],
        "transpilers": { "custom-target": { "output": "generated/custom" } }
    })");
    writeFile(directory.path() / "src" / "logic.rls", trackerSource);
    EXPECT_NE(runConsoleFrom(directory.path(), "-t soh_tracker",
        directory.path() / "unconfigured-tracker.log"), 0);
    EXPECT_NE(readFile(directory.path() / "unconfigured-tracker.log").find(
        "manifest does not configure transpiler 'soh_tracker'"), std::string::npos);
    EXPECT_NE(runConsoleFrom(directory.path(), "",
        directory.path() / "unknown-target.log"), 0);
    EXPECT_NE(readFile(directory.path() / "unknown-target.log").find(
        "unknown transpiler 'custom-target'"), std::string::npos);
    EXPECT_FALSE(fs::exists(directory.path() / "generated"));
}

TEST(ConsoleProject, RejectsInvalidTrackerManifestConfiguration) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "src" / "logic.rls", trackerSource);
    for (const auto& settings : {R"({})", R"({"output": 42})",
            R"({"output": "../outside"})"}) {
        SCOPED_TRACE(settings);
        writeFile(directory.path() / "rls.json",
            std::string(R"({"version":1,"sources":["src"],"transpilers":{"soh_tracker":)") +
            settings + "}}");
        const auto log = directory.path() / "invalid-tracker.log";
        EXPECT_NE(runConsoleFrom(directory.path(), "", log), 0);
        EXPECT_NE(readFile(log).find("error:"), std::string::npos);
        EXPECT_FALSE(fs::exists(directory.path() / "generated"));
    }
}

TEST(ConsoleProject, TrackerReportsBackendDiagnosticsWithoutTouchingArtifacts) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "logic.rls",
        "extern enum Custom { CUSTOM_* }\n"
        "extern define host(value: Custom) -> Bool\n"
        "region RR_TEST { locations { RC_A: host(CUSTOM_ONE) } }\n");
    writeFile(directory.path() / "generated" / "logic_tracker.gen.h", "previous output\n");
    EXPECT_NE(runConsoleFrom(directory.path(), "-t soh_tracker -o generated logic.rls",
        directory.path() / "backend-error.log"), 0);
    EXPECT_EQ(readFile(directory.path() / "generated" / "logic_tracker.gen.h"), "previous output\n");
    EXPECT_FALSE(fs::exists(directory.path() / "generated" / "logic_tracker.gen.cpp"));
    const auto log = readFile(directory.path() / "backend-error.log");
    EXPECT_NE(log.find("logic.rls:"), std::string::npos);
    EXPECT_NE(log.find("error:"), std::string::npos);
    EXPECT_NE(log.find("aborting due to soh_tracker transpiler errors"), std::string::npos);
}

TEST(ConsoleProject, TrackerRejectsParseAndSemanticErrorsBeforeCreatingOutputs) {
    TemporaryDirectory directory;
    for (const auto& source : {"define broken(:\n",
            "region RR_TEST { locations { RC_A: missing() } }\n"}) {
        SCOPED_TRACE(source);
        writeFile(directory.path() / "logic.rls", source);
        EXPECT_NE(runConsoleFrom(directory.path(), "-t soh_tracker -o generated logic.rls",
            directory.path() / "source-error.log"), 0);
        EXPECT_FALSE(fs::exists(directory.path() / "generated"));
        EXPECT_NE(readFile(directory.path() / "source-error.log").find("logic.rls:"),
            std::string::npos);
    }
}

TEST(ConsoleProject, TrackerReportsOutputDirectoryAndArtifactOpenFailures) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "logic.rls", trackerSource);
    writeFile(directory.path() / "blocked", "not a directory");
    for (const auto& output : {"blocked", "generated"}) {
        SCOPED_TRACE(output);
        fs::create_directories(directory.path() / "generated" / "logic_tracker.gen.cpp");
        const auto log = directory.path() / (std::string(output) + ".log");
        EXPECT_NE(runConsoleFrom(directory.path(),
            "-t soh_tracker -o " + std::string(output) + " logic.rls", log), 0);
        const auto diagnostics = readFile(log);
        EXPECT_NE(diagnostics.find("error: transpiler 'soh_tracker' output '"),
            std::string::npos);
        EXPECT_FALSE(fs::exists(directory.path() / output / "logic_tracker.gen.txt"));
    }
}

TEST(ConsoleProject, LoadsManifestAndUsesConfiguredOutput) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1,
        "sources": ["src"],
        "transpilers": { "soh_ap": { "output": "generated/soh_ap" } }
    })");
    writeFile(directory.path() / "src" / "logic.rls", "define smoke(): true\n");

    EXPECT_EQ(runConsole("--project \"" + directory.path().string() + "\"",
        directory.path() / "console.log"), 0);
    EXPECT_TRUE(fs::exists(directory.path() / "generated" / "soh_ap" / "functions_gen.py"));
}

TEST(ConsoleProject, SelectsOnlyOneConfiguredManifestTranspiler) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1,
        "sources": ["src"],
        "transpilers": {
            "soh_ap": { "output": "generated/soh_ap" },
            "soh": { "output": "generated/soh" }
        }
    })");
    writeFile(directory.path() / "src" / "logic.rls", "define smoke(): true\n");

    EXPECT_EQ(runConsole("--project \"" + directory.path().string() + "\" -t soh_ap",
        directory.path() / "selection.log"), 0);
    EXPECT_TRUE(fs::exists(directory.path() / "generated" / "soh_ap" / "functions_gen.py"));
    EXPECT_FALSE(fs::exists(directory.path() / "generated" / "soh"));
}

TEST(ConsoleProject, RejectsUnconfiguredManifestTranspilerSelection) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1,
        "sources": ["src"],
        "transpilers": { "soh_ap": { "output": "generated/soh_ap" } }
    })");
    writeFile(directory.path() / "src" / "logic.rls", "define smoke(): true\n");

    EXPECT_NE(runConsole("--project \"" + directory.path().string() + "\" -t soh",
        directory.path() / "selection-error.log"), 0);
}

TEST(ConsoleProject, DiscoversManifestFromCurrentDirectory) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1,
        "sources": ["src"],
        "transpilers": { "soh_ap": { "output": "generated/soh_ap" } }
    })");
    writeFile(directory.path() / "src" / "logic.rls", "define smoke(): true\n");

    EXPECT_EQ(runConsoleFrom(directory.path() / "src", "", directory.path() / "discovery.log"), 0);
    EXPECT_TRUE(fs::exists(directory.path() / "generated" / "soh_ap" / "functions_gen.py"));
}

TEST(ConsoleProject, SupportsExplicitInputsAndRejectsUnknownTranspilers) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "logic.rls", "define smoke(): true\n");

    EXPECT_EQ(runConsole("-t soh_ap -o \"" + (directory.path() / "soh_ap").string() + "\" \"" +
        (directory.path() / "logic.rls").string() + "\"", directory.path() / "explicit.log"), 0);
    EXPECT_TRUE(fs::exists(directory.path() / "soh_ap" / "functions_gen.py"));

    EXPECT_NE(runConsole("-t unknown -o \"" + (directory.path() / "unknown").string() + "\" \"" +
        (directory.path() / "logic.rls").string() + "\"", directory.path() / "unknown.log"), 0);
}

TEST(ConsoleProject, RejectsEnumOverflowBeforeTouchingExistingArtifacts) {
    TemporaryDirectory directory;
    const auto source = directory.path() / "limits.rls";
    const auto output = directory.path() / "generated";
    const auto log = directory.path() / "overflow.log";
    writeFile(source, "enum Limit { Highest = 2147483647, Overflow }\n");
    writeFile(output / "functions.gen.h", "previous successful output\n");

    EXPECT_NE(runConsole("-t soh -o \"" + output.string() + "\" \"" +
        source.string() + "\"", log), 0);
    std::ifstream artifact(output / "functions.gen.h");
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>{artifact}, {}),
        "previous successful output\n");
    EXPECT_FALSE(fs::exists(output / "regions.gen.cpp"));

    std::ifstream diagnostics(log);
    const std::string text(std::istreambuf_iterator<char>{diagnostics}, {});
    EXPECT_NE(text.find("exceeds the Int value range"), std::string::npos);
    EXPECT_NE(text.find("Overflow"), std::string::npos);
}

TEST(ConsoleProject, RejectsOutOfRangeNumericLiteralsBeforeCreatingArtifacts) {
    TemporaryDirectory directory;
    const std::string sources[] = {
        "define too_large(): 2147483648\n",
        "define too_small(): -2147483649\n",
        "enum Limit { TooLarge = 2147483648 }\n",
        "enum Limit { TooSmall = -2147483649 }\n"
    };
    for (size_t i = 0; i < std::size(sources); ++i) {
        SCOPED_TRACE(sources[i]);
        const auto source = directory.path() / ("limits" + std::to_string(i) + ".rls");
        const auto output = directory.path() / ("generated" + std::to_string(i));
        const auto log = directory.path() / ("limits" + std::to_string(i) + ".log");
        writeFile(source, sources[i]);
        EXPECT_NE(runConsole("-t soh -o \"" + output.string() + "\" \"" +
            source.string() + "\"", log), 0);
        EXPECT_FALSE(fs::exists(output));
        std::ifstream diagnostics(log);
        const std::string text(std::istreambuf_iterator<char>{diagnostics}, {});
        EXPECT_NE(text.find("invalid integer literal"), std::string::npos);
    }
}

} // namespace