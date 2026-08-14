#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

TEST(ConsoleProject, LoadsManifestAndUsesConfiguredOutput) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1,
        "sources": ["src"],
        "transpilers": { "ap": { "output": "generated/ap" } }
    })");
    writeFile(directory.path() / "src" / "logic.rls", "define smoke(): true\n");

    EXPECT_EQ(runConsole("--project \"" + directory.path().string() + "\"",
        directory.path() / "console.log"), 0);
    EXPECT_TRUE(fs::exists(directory.path() / "generated" / "ap" / "ap.py"));
}

TEST(ConsoleProject, SelectsOnlyOneConfiguredManifestTranspiler) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1,
        "sources": ["src"],
        "transpilers": {
            "ap": { "output": "generated/ap" },
            "soh": { "output": "generated/soh" }
        }
    })");
    writeFile(directory.path() / "src" / "logic.rls", "define smoke(): true\n");

    EXPECT_EQ(runConsole("--project \"" + directory.path().string() + "\" -t ap",
        directory.path() / "selection.log"), 0);
    EXPECT_TRUE(fs::exists(directory.path() / "generated" / "ap" / "ap.py"));
    EXPECT_FALSE(fs::exists(directory.path() / "generated" / "soh"));
}

TEST(ConsoleProject, RejectsUnconfiguredManifestTranspilerSelection) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1,
        "sources": ["src"],
        "transpilers": { "ap": { "output": "generated/ap" } }
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
        "transpilers": { "ap": { "output": "generated/ap" } }
    })");
    writeFile(directory.path() / "src" / "logic.rls", "define smoke(): true\n");

    EXPECT_EQ(runConsoleFrom(directory.path() / "src", "", directory.path() / "discovery.log"), 0);
    EXPECT_TRUE(fs::exists(directory.path() / "generated" / "ap" / "ap.py"));
}

TEST(ConsoleProject, SupportsExplicitInputsAndRejectsUnknownTranspilers) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "logic.rls", "define smoke(): true\n");

    EXPECT_EQ(runConsole("-t ap -o \"" + (directory.path() / "ap").string() + "\" \"" +
        (directory.path() / "logic.rls").string() + "\"", directory.path() / "explicit.log"), 0);
    EXPECT_TRUE(fs::exists(directory.path() / "ap" / "ap.py"));

    EXPECT_NE(runConsole("-t unknown -o \"" + (directory.path() / "unknown").string() + "\" \"" +
        (directory.path() / "logic.rls").string() + "\"", directory.path() / "unknown.log"), 0);
}

} // namespace