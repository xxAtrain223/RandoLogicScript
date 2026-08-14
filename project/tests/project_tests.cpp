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

TEST(ProjectManifest, ResolvesRelativeManifestPathsIndependentlyOfCurrentDirectory) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "nested" / "rls.json", R"({ "version": 1, "sources": ["src"] })");
    writeFile(directory.path() / "nested" / "src" / "logic.rls");

    const auto result = rls::project::LoadManifest(directory.path() / "nested" / "." / "rls.json");

    ASSERT_TRUE(result.config.has_value()) << result.error;
    const auto sources = rls::project::CollectManifestSources(*result.config);
    ASSERT_TRUE(sources.error.empty());
    ASSERT_EQ(sources.sourceFiles.size(), 1);
    EXPECT_EQ(sources.sourceFiles[0],
        fs::weakly_canonical(directory.path() / "nested" / "src" / "logic.rls"));
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

TEST(ProjectManifest, ReturnsStructuredConfigurationDiagnostics) {
    TemporaryDirectory directory;
    const fs::path manifestPath = directory.path() / "rls.json";
    writeFile(manifestPath, "{\"name\":\"\xF0\x9F\x98\x80\", invalid}");

    const auto invalidJson = rls::project::LoadManifest(manifestPath);
    ASSERT_FALSE(invalidJson.config.has_value());
    ASSERT_EQ(invalidJson.diagnostics.size(), 1);
    EXPECT_EQ(invalidJson.diagnostics[0].path, fs::weakly_canonical(manifestPath));
    EXPECT_EQ(invalidJson.diagnostics[0].code, "RLS-C002");
    EXPECT_GT(invalidJson.diagnostics[0].startByte, 0);

    writeFile(manifestPath, R"({"version":1,"sources":["missing"]})");
    writeFile(directory.path() / "logic.rls");
    const auto resolved = rls::project::ResolveFileProject(directory.path() / "logic.rls");
    ASSERT_FALSE(resolved.error.empty());
    ASSERT_EQ(resolved.diagnostics.size(), 1);
    EXPECT_EQ(resolved.diagnostics[0].code, "RLS-C004");
    EXPECT_EQ(resolved.diagnostics[0].path, fs::weakly_canonical(manifestPath));
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

TEST(ProjectManifest, CollectsDeterministicSourcesWithConfiguredAndDefaultExclusions) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1,
        "sources": ["."],
        "exclude": ["ignored/**"],
        "transpilers": { "soh": { "output": "generated/soh" } }
    })");
    writeFile(directory.path() / "src" / "z.rls");
    writeFile(directory.path() / "src" / "a.rls");
    writeFile(directory.path() / "ignored" / "skip.rls");
    writeFile(directory.path() / "generated" / "soh" / "skip.rls");
    writeFile(directory.path() / "build" / "skip.rls");
    writeFile(directory.path() / ".git" / "skip.rls");

    const auto manifest = rls::project::LoadManifest(directory.path() / "rls.json");
    ASSERT_TRUE(manifest.config.has_value()) << manifest.error;
    const auto result = rls::project::CollectManifestSources(*manifest.config);

    ASSERT_TRUE(result.error.empty());
    ASSERT_EQ(result.sourceFiles.size(), 2);
    EXPECT_EQ(result.sourceFiles[0], fs::weakly_canonical(directory.path() / "src" / "a.rls"));
    EXPECT_EQ(result.sourceFiles[1], fs::weakly_canonical(directory.path() / "src" / "z.rls"));
}

TEST(ProjectManifest, ReportsMissingAndEmptySourceSets) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({ "version": 1, "sources": ["missing"] })");

    auto manifest = rls::project::LoadManifest(directory.path() / "rls.json");
    ASSERT_TRUE(manifest.config.has_value()) << manifest.error;
    EXPECT_EQ(rls::project::CollectManifestSources(*manifest.config).error,
        "manifest source does not exist: " + (directory.path() / "missing").string());

    writeFile(directory.path() / "rls.json", R"({ "version": 1, "sources": ["empty"] })");
    fs::create_directories(directory.path() / "empty");
    manifest = rls::project::LoadManifest(directory.path() / "rls.json");
    ASSERT_TRUE(manifest.config.has_value()) << manifest.error;
    EXPECT_EQ(rls::project::CollectManifestSources(*manifest.config).error,
        "manifest does not resolve to any .rls source files");
}

TEST(ProjectManifest, IncludesOutputOnlyWhenExplicitlyListedAsASource) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({
        "version": 1,
        "sources": ["generated/soh"],
        "transpilers": { "soh": { "output": "generated/soh" } }
    })");
    writeFile(directory.path() / "generated" / "soh" / "included.rls");

    const auto manifest = rls::project::LoadManifest(directory.path() / "rls.json");
    ASSERT_TRUE(manifest.config.has_value()) << manifest.error;
    const auto result = rls::project::CollectManifestSources(*manifest.config);

    ASSERT_TRUE(result.error.empty());
    ASSERT_EQ(result.sourceFiles.size(), 1);
    EXPECT_EQ(result.sourceFiles[0],
        fs::weakly_canonical(directory.path() / "generated" / "soh" / "included.rls"));
}

TEST(ProjectResolution, UsesNearestManifestOrStandaloneFile) {
    TemporaryDirectory directory;
    TemporaryDirectory standaloneDirectory;
    writeFile(directory.path() / "rls.json", R"({ "version": 1, "sources": ["src"] })");
    writeFile(directory.path() / "src" / "outer.rls");
    writeFile(directory.path() / "nested" / "rls.json", R"({ "version": 1, "sources": ["logic.rls"] })");
    writeFile(directory.path() / "nested" / "logic.rls");
    writeFile(standaloneDirectory.path() / "standalone.rls");

    const auto nested = rls::project::ResolveFileProject(directory.path() / "nested" / "logic.rls");
    ASSERT_TRUE(nested.error.empty());
    ASSERT_TRUE(nested.manifest.has_value());
    EXPECT_FALSE(nested.isStandalone);
    EXPECT_EQ(nested.manifest->manifestPath,
        fs::weakly_canonical(directory.path() / "nested" / "rls.json"));

    const auto standalone = rls::project::ResolveFileProject(standaloneDirectory.path() / "standalone.rls");
    ASSERT_TRUE(standalone.error.empty());
    EXPECT_FALSE(standalone.manifest.has_value());
    ASSERT_TRUE(standalone.isStandalone);
    ASSERT_EQ(standalone.sourceFiles.size(), 1);
    EXPECT_EQ(standalone.sourceFiles[0],
        fs::weakly_canonical(standaloneDirectory.path() / "standalone.rls"));
}

} // namespace