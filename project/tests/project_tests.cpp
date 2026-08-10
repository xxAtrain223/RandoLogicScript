#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>

#include "project.h"

namespace fs = std::filesystem;

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() : path_(fs::temp_directory_path() /
        ("rls-project-tests-" + std::to_string(
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

void writeFile(const fs::path& path, const std::string& content = "define test(): true\n") {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

TEST(ProjectSources, CanonicalizesDeduplicatesAndSortsFiles) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "z.rls");
    writeFile(directory.path() / "nested" / "a.rls");

    const auto result = rls::project::CollectExplicitSources({
        directory.path(),
        directory.path() / "nested" / ".." / "z.rls",
    });

    ASSERT_TRUE(result.error.empty());
    ASSERT_EQ(result.sourceFiles.size(), 2);
    EXPECT_LT(result.sourceFiles[0], result.sourceFiles[1]);
    EXPECT_EQ(result.sourceFiles[1], fs::weakly_canonical(directory.path() / "z.rls"));
}

TEST(ProjectManifest, FindsNearestManifest) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json");
    writeFile(directory.path() / "nested" / "rls.json");
    writeFile(directory.path() / "nested" / "src" / "logic.rls");

    const auto manifest = rls::project::FindManifest(directory.path() / "nested" / "src" / "logic.rls");

    ASSERT_TRUE(manifest.has_value());
    EXPECT_EQ(*manifest, fs::weakly_canonical(directory.path() / "nested" / "rls.json"));
}

TEST(ProjectManifest, LoadsAndResolvesPathsFromManifestDirectory) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1,
        "sources": ["src", "stdlib/host.rls"],
        "exclude": ["generated"],
        "transpilers": { "soh": { "output": "generated/soh" } }
    })");

    const auto result = rls::project::LoadManifest(directory.path() / "rls.json");

    ASSERT_TRUE(result.error.empty());
    ASSERT_TRUE(result.config.has_value());
    EXPECT_EQ(result.config->root, fs::weakly_canonical(directory.path()));
    EXPECT_EQ(result.config->sources[0], fs::weakly_canonical(directory.path() / "src"));
    EXPECT_EQ(result.config->transpilerOutputs[0].second,
        fs::weakly_canonical(directory.path() / "generated" / "soh"));
}

TEST(ProjectManifest, RejectsUnknownFieldsAndEscapingOutputPaths) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({ "version": 1, "sources": ["src"], "extra": true })");
    EXPECT_EQ(rls::project::LoadManifest(directory.path() / "rls.json").error,
        "unknown manifest field: extra");

    writeFile(directory.path() / "rls.json", R"({
        "version": 1,
        "sources": ["src"],
        "transpilers": { "ap": { "output": "../outside" } }
    })");
    EXPECT_EQ(rls::project::LoadManifest(directory.path() / "rls.json").error,
        "manifest path escapes the project root: ../outside");
}

TEST(ProjectManifest, AcceptsTranspilerNamesWithoutKnowingImplementations) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1,
        "sources": ["src"],
        "transpilers": { "custom-target": { "output": "generated/custom" } }
    })");

    const auto result = rls::project::LoadManifest(directory.path() / "rls.json");

    ASSERT_TRUE(result.error.empty());
    ASSERT_TRUE(result.config.has_value());
    ASSERT_EQ(result.config->transpilerOutputs.size(), 1);
    EXPECT_EQ(result.config->transpilerOutputs[0].first, "custom-target");
}

} // namespace