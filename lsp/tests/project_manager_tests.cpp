#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "rls/lsp/document_store.h"
#include "rls/lsp/project_manager.h"

namespace fs = std::filesystem;

namespace {

using rls::lsp::DocumentStore;
using rls::lsp::DocumentUpdateResult;
using rls::lsp::ProjectAssignmentResult;
using rls::lsp::ProjectManager;

class TemporaryDirectory {
public:
    TemporaryDirectory() : path_(fs::temp_directory_path() /
        ("rls-lsp-project-manager-" + std::to_string(
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
    std::ofstream(path, std::ios::binary) << content;
}

std::string fileUri(const fs::path& path) {
    const std::string generic = fs::weakly_canonical(path).generic_string();
#ifdef _WIN32
    return "file:///" + generic;
#else
    return "file://" + generic;
#endif
}

TEST(ProjectManagerTests, AssignsDocumentsToTheirNearestManifest) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({"version":1,"sources":["src"]})");
    writeFile(directory.path() / "src" / "first.rls", "define first(): true\n");
    writeFile(directory.path() / "src" / "second.rls", "define second(): true\n");

    DocumentStore documents;
    ProjectManager projects(documents);
    const std::string firstUri = fileUri(directory.path() / "src" / "first.rls");
    const std::string secondUri = fileUri(directory.path() / "src" / "second.rls");
    ASSERT_EQ(documents.open(firstUri, "rls", 1, "first overlay"),
        DocumentUpdateResult::Applied);
    ASSERT_EQ(documents.open(secondUri, "rls", 1, "second overlay"),
        DocumentUpdateResult::Applied);

    EXPECT_EQ(projects.documentOpened(firstUri), ProjectAssignmentResult::Assigned);
    EXPECT_EQ(projects.documentOpened(secondUri), ProjectAssignmentResult::Assigned);

    const auto* firstProject = projects.projectForDocument(firstUri);
    const auto* secondProject = projects.projectForDocument(secondUri);
    ASSERT_NE(firstProject, nullptr);
    ASSERT_NE(secondProject, nullptr);
    EXPECT_EQ(firstProject->id, secondProject->id);
    EXPECT_FALSE(firstProject->isStandalone);
    EXPECT_EQ(firstProject->sourceFiles.size(), 2);
}

TEST(ProjectManagerTests, OpenOverlayWinsThenCloseRestoresDiskContent) {
    TemporaryDirectory directory;
    const fs::path sourcePath = directory.path() / "standalone.rls";
    writeFile(sourcePath, "disk content\n");
    const std::string uri = fileUri(sourcePath);

    DocumentStore documents;
    ProjectManager projects(documents);
    ASSERT_EQ(documents.open(uri, "rls", 1, "overlay content\n"),
        DocumentUpdateResult::Applied);
    ASSERT_EQ(projects.documentOpened(uri), ProjectAssignmentResult::Assigned);

    auto sourceSet = projects.sourceSetForDocument(uri);
    ASSERT_TRUE(sourceSet.error.empty()) << sourceSet.error;
    ASSERT_EQ(sourceSet.sources.size(), 1);
    EXPECT_EQ(sourceSet.sources.front().content, "overlay content\n");
    const uint64_t openGeneration = sourceSet.generation;

    ASSERT_TRUE(documents.close(uri));
    ASSERT_EQ(projects.documentClosed(uri), ProjectAssignmentResult::Assigned);
    sourceSet = projects.sourceSetForDocument(uri);
    ASSERT_TRUE(sourceSet.error.empty()) << sourceSet.error;
    ASSERT_EQ(sourceSet.sources.size(), 1);
    EXPECT_EQ(sourceSet.sources.front().content, "disk content\n");
    EXPECT_GT(sourceSet.generation, openGeneration);
}

TEST(ProjectManagerTests, ChangesAdvanceGenerationAndPreserveOverlay) {
    TemporaryDirectory directory;
    const fs::path sourcePath = directory.path() / "standalone.rls";
    writeFile(sourcePath, "disk\n");
    const std::string uri = fileUri(sourcePath);

    DocumentStore documents;
    ProjectManager projects(documents);
    ASSERT_EQ(documents.open(uri, "rls", 1, "one\n"), DocumentUpdateResult::Applied);
    ASSERT_EQ(projects.documentOpened(uri), ProjectAssignmentResult::Assigned);
    const uint64_t before = projects.projectForDocument(uri)->generation;

    ASSERT_EQ(documents.applyFullChange(uri, 2, "two\n"), DocumentUpdateResult::Applied);
    ASSERT_EQ(projects.documentChanged(uri), ProjectAssignmentResult::Assigned);

    const auto sourceSet = projects.sourceSetForDocument(uri);
    EXPECT_GT(sourceSet.generation, before);
    ASSERT_EQ(sourceSet.sources.size(), 1);
    EXPECT_EQ(sourceSet.sources.front().content, "two\n");
}

TEST(ProjectManagerTests, RequiresAnOpenDocumentBeforeAssignment) {
    TemporaryDirectory directory;
    const fs::path sourcePath = directory.path() / "standalone.rls";
    writeFile(sourcePath, "disk\n");
    const std::string uri = fileUri(sourcePath);

    DocumentStore documents;
    ProjectManager projects(documents);

    EXPECT_EQ(projects.documentOpened(uri), ProjectAssignmentResult::NotAssigned);
    EXPECT_EQ(projects.projectForDocument(uri), nullptr);
}

TEST(ProjectManagerTests, RefreshReassignsOpenDocumentAcrossNestedManifestChanges) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({"version":1,"sources":["nested"]})");
    const fs::path sourcePath = directory.path() / "nested" / "logic.rls";
    writeFile(sourcePath, "define disk(): true\n");
    const std::string uri = fileUri(sourcePath);

    DocumentStore documents;
    ProjectManager projects(documents);
    ASSERT_EQ(documents.open(uri, "rls", 1, "define overlay(): true\n"),
        DocumentUpdateResult::Applied);
    ASSERT_EQ(projects.documentOpened(uri), ProjectAssignmentResult::Assigned);
    const std::string outerProjectId = projects.projectForDocument(uri)->id;
    const uint64_t outerGeneration = projects.projectForDocument(uri)->generation;

    writeFile(directory.path() / "nested" / "rls.json",
        R"({"version":1,"sources":["logic.rls"]})");
    auto refresh = projects.refreshOpenDocuments();
    ASSERT_TRUE(refresh.errors.empty());
    ASSERT_EQ(refresh.changedProjectIds.size(), 1);
    ASSERT_EQ(refresh.removedProjectIds.size(), 1);
    EXPECT_EQ(refresh.removedProjectIds.front(), outerProjectId);
    const auto* nestedProject = projects.projectForDocument(uri);
    ASSERT_NE(nestedProject, nullptr);
    EXPECT_NE(nestedProject->id, outerProjectId);
    EXPECT_GT(nestedProject->generation, outerGeneration);
    auto sourceSet = projects.sourceSetForProject(nestedProject->id);
    ASSERT_EQ(sourceSet.sources.size(), 1);
    EXPECT_EQ(sourceSet.sources.front().content, "define overlay(): true\n");
    const std::string nestedProjectId = nestedProject->id;

    fs::remove(directory.path() / "nested" / "rls.json");
    refresh = projects.refreshOpenDocuments();
    ASSERT_TRUE(refresh.errors.empty());
    ASSERT_EQ(refresh.removedProjectIds.size(), 1);
    EXPECT_EQ(refresh.removedProjectIds.front(), nestedProjectId);
    ASSERT_NE(projects.projectForDocument(uri), nullptr);
    EXPECT_EQ(projects.projectForDocument(uri)->id, outerProjectId);
}

} // namespace