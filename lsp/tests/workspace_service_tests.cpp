#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rls/lsp/analysis_scheduler.h"
#include "rls/lsp/diagnostic_publisher.h"
#include "rls/lsp/document_store.h"
#include "rls/lsp/document_synchronization_service.h"
#include "rls/lsp/document_uri.h"
#include "rls/lsp/lifecycle_service.h"
#include "rls/lsp/outbound_message_queue.h"
#include "rls/lsp/project_manager.h"
#include "rls/lsp/workspace_service.h"

namespace fs = std::filesystem;

namespace {

using Json = nlohmann::json;
using namespace rls::lsp;

class TemporaryDirectory {
public:
    TemporaryDirectory() : path_(fs::temp_directory_path() /
        ("rls-lsp-workspace-" + std::to_string(
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

struct Services {
    OutboundMessageQueue outbound;
    DocumentStore documents;
    ProjectManager projects{documents};
    LifecycleService lifecycle;
    DiagnosticPublisher diagnostics{outbound};
    AnalysisScheduler scheduler{{
        .debounce = std::chrono::milliseconds(0),
        .maximumConcurrency = 1,
    }};
    WorkspaceService workspace{projects, scheduler, diagnostics};
    DocumentSynchronizationService synchronization{
        lifecycle, documents, projects, scheduler, diagnostics};

    Services() {
        scheduler.setAcceptedHandler(
            [this](std::string projectId, AnalysisScheduler::Snapshot snapshot) {
                diagnostics.acceptedSnapshot(std::move(projectId), std::move(snapshot));
            });
        lifecycle.initialize();
        lifecycle.initialized();
    }
};

void drain(OutboundMessageQueue& outbound) {
    while (outbound.tryPop()) {
    }
}

TEST(WorkspaceServiceTests, TracksInitialAndChangedWorkspaceFolders) {
    TemporaryDirectory first;
    TemporaryDirectory second;
    Services services;
    ASSERT_TRUE(services.workspace.initialize({*PathToFileUri(first.path())}));
    EXPECT_EQ(services.workspace.folderCount(), 1);

    EXPECT_TRUE(services.workspace.changeFolders(
        {*PathToFileUri(second.path())}, {*PathToFileUri(first.path())}));
    EXPECT_EQ(services.workspace.folderCount(), 1);
}

TEST(WorkspaceServiceTests, WatchedManifestReassignsOpenDocumentToNestedProject) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({"version":1,"sources":["nested"]})");
    const fs::path sourcePath = directory.path() / "nested" / "logic.rls";
    writeFile(sourcePath, "define disk(): true\n");
    const std::string sourceUri = *PathToFileUri(sourcePath);
    Services services;
    ASSERT_TRUE(services.workspace.initialize({*PathToFileUri(directory.path())}));
    ASSERT_EQ(services.synchronization.open(
        sourceUri, "rls", 1, "define overlay(): true\n"),
        DocumentSynchronizationResult::Applied);
    services.scheduler.waitForIdle();
    const std::string outerProjectId = services.projects.projectForDocument(sourceUri)->id;
    ASSERT_NE(services.scheduler.acceptedSnapshot(outerProjectId), nullptr);
    drain(services.outbound);

    const fs::path nestedManifest = directory.path() / "nested" / "rls.json";
    writeFile(nestedManifest, R"({"version":1,"sources":["logic.rls"]})");
    ASSERT_TRUE(services.workspace.watchedFilesChanged({*PathToFileUri(nestedManifest)}));
    services.scheduler.waitForIdle();

    const auto* nestedProject = services.projects.projectForDocument(sourceUri);
    ASSERT_NE(nestedProject, nullptr);
    EXPECT_NE(nestedProject->id, outerProjectId);
    EXPECT_EQ(services.scheduler.acceptedSnapshot(outerProjectId), nullptr);
    const auto nestedSnapshot = services.scheduler.acceptedSnapshot(nestedProject->id);
    ASSERT_NE(nestedSnapshot, nullptr);
    EXPECT_EQ(nestedSnapshot->sourceText(
        fs::weakly_canonical(sourcePath).generic_string())->content(),
        "define overlay(): true\n");
}

TEST(WorkspaceServiceTests, WatchedDiskEditReanalyzesAndClearsDiagnostics) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({"version":1,"sources":["src"]})");
    const fs::path openPath = directory.path() / "src" / "open.rls";
    const fs::path diskPath = directory.path() / "src" / "disk.rls";
    writeFile(openPath, "define open(): true\n");
    writeFile(diskPath, "region RR_TEST { events { EVENT_TEST: \"invalid\" } }\n");
    const std::string openUri = *PathToFileUri(openPath);
    const std::string diskUri = *PathToFileUri(diskPath);
    Services services;
    ASSERT_TRUE(services.workspace.initialize({*PathToFileUri(directory.path())}));
    ASSERT_EQ(services.synchronization.open(
        openUri, "rls", 1, "define overlay(): true\n"),
        DocumentSynchronizationResult::Applied);
    services.scheduler.waitForIdle();
    drain(services.outbound);

    writeFile(diskPath, "region RR_TEST { events { EVENT_TEST: true } }\n");
    ASSERT_TRUE(services.workspace.watchedFilesChanged({diskUri}));
    services.scheduler.waitForIdle();

    bool cleared = false;
    while (const auto payload = services.outbound.tryPop()) {
        const Json message = Json::parse(*payload);
        if (message["params"]["uri"] == diskUri
            && message["params"]["diagnostics"].empty()) {
            cleared = true;
        }
    }
    EXPECT_TRUE(cleared);
}

TEST(WorkspaceServiceTests, FolderRemovalMakesOpenDocumentStandaloneUntilReadded) {
    TemporaryDirectory directory;
    writeFile(directory.path() / "rls.json", R"({"version":1,"sources":["src"]})");
    const fs::path sourcePath = directory.path() / "src" / "logic.rls";
    writeFile(sourcePath, "define disk(): true\n");
    const std::string rootUri = *PathToFileUri(directory.path());
    const std::string sourceUri = *PathToFileUri(sourcePath);
    Services services;
    ASSERT_TRUE(services.workspace.initialize({rootUri}));
    ASSERT_EQ(services.synchronization.open(
        sourceUri, "rls", 1, "define overlay(): true\n"),
        DocumentSynchronizationResult::Applied);
    services.scheduler.waitForIdle();
    ASSERT_FALSE(services.projects.projectForDocument(sourceUri)->isStandalone);
    const std::string manifestProjectId = services.projects.projectForDocument(sourceUri)->id;

    ASSERT_TRUE(services.workspace.changeFolders({}, {rootUri}));
    services.scheduler.waitForIdle();
    const auto* standalone = services.projects.projectForDocument(sourceUri);
    ASSERT_NE(standalone, nullptr);
    EXPECT_TRUE(standalone->isStandalone);
    EXPECT_NE(standalone->id, manifestProjectId);
    EXPECT_EQ(services.scheduler.acceptedSnapshot(manifestProjectId), nullptr);

    ASSERT_TRUE(services.workspace.changeFolders({rootUri}, {}));
    services.scheduler.waitForIdle();
    ASSERT_NE(services.projects.projectForDocument(sourceUri), nullptr);
    EXPECT_FALSE(services.projects.projectForDocument(sourceUri)->isStandalone);
    EXPECT_EQ(services.projects.projectForDocument(sourceUri)->id, manifestProjectId);
}

TEST(WorkspaceServiceTests, InvalidManifestKeepsLastGoodProjectAndRecoversWhenFixed) {
    TemporaryDirectory directory;
    const fs::path manifestPath = directory.path() / "rls.json";
    writeFile(manifestPath, R"({"version":1,"sources":["src"]})");
    const fs::path sourcePath = directory.path() / "src" / "logic.rls";
    writeFile(sourcePath, "define disk(): true\n");
    const std::string sourceUri = *PathToFileUri(sourcePath);
    Services services;
    ASSERT_TRUE(services.workspace.initialize({*PathToFileUri(directory.path())}));
    ASSERT_EQ(services.synchronization.open(
        sourceUri, "rls", 1, "define overlay(): true\n"),
        DocumentSynchronizationResult::Applied);
    services.scheduler.waitForIdle();
    const std::string projectId = services.projects.projectForDocument(sourceUri)->id;
    const uint64_t generation = services.projects.projectForDocument(sourceUri)->generation;

    writeFile(manifestPath, "{ invalid");
    EXPECT_FALSE(services.workspace.watchedFilesChanged({*PathToFileUri(manifestPath)}));
    ASSERT_NE(services.projects.projectForDocument(sourceUri), nullptr);
    EXPECT_EQ(services.projects.projectForDocument(sourceUri)->id, projectId);

    writeFile(manifestPath, R"({"version":1,"sources":["src"]})");
    EXPECT_TRUE(services.workspace.watchedFilesChanged({*PathToFileUri(manifestPath)}));
    services.scheduler.waitForIdle();
    ASSERT_NE(services.projects.projectForDocument(sourceUri), nullptr);
    EXPECT_EQ(services.projects.projectForDocument(sourceUri)->id, projectId);
    EXPECT_GT(services.projects.projectForDocument(sourceUri)->generation, generation);
}

TEST(WorkspaceServiceTests, MultipleManifestProjectsKeepSourcesAndSnapshotsIsolated) {
    TemporaryDirectory first;
    TemporaryDirectory second;
    writeFile(first.path() / "rls.json", R"({"version":1,"sources":["src"]})");
    writeFile(second.path() / "rls.json", R"({"version":1,"sources":["src"]})");
    const fs::path firstPath = first.path() / "src" / "logic.rls";
    const fs::path secondPath = second.path() / "src" / "logic.rls";
    writeFile(firstPath, "define first_disk(): true\n");
    writeFile(secondPath, "define second_disk(): true\n");
    const std::string firstUri = *PathToFileUri(firstPath);
    const std::string secondUri = *PathToFileUri(secondPath);
    Services services;
    ASSERT_TRUE(services.workspace.initialize({
        *PathToFileUri(first.path()), *PathToFileUri(second.path()),
    }));
    ASSERT_EQ(services.synchronization.open(
        firstUri, "rls", 1, "define first_overlay(): true\n"),
        DocumentSynchronizationResult::Applied);
    ASSERT_EQ(services.synchronization.open(
        secondUri, "rls", 1, "define second_overlay(): true\n"),
        DocumentSynchronizationResult::Applied);
    services.scheduler.waitForIdle();

    const std::string firstProjectId = services.projects.projectForDocument(firstUri)->id;
    const std::string secondProjectId = services.projects.projectForDocument(secondUri)->id;
    ASSERT_NE(firstProjectId, secondProjectId);
    writeFile(firstPath, "define first_disk_changed(): true\n");
    ASSERT_TRUE(services.workspace.watchedFilesChanged({firstUri}));
    services.scheduler.waitForIdle();

    const auto firstSnapshot = services.scheduler.acceptedSnapshot(firstProjectId);
    const auto secondSnapshot = services.scheduler.acceptedSnapshot(secondProjectId);
    ASSERT_NE(firstSnapshot, nullptr);
    ASSERT_NE(secondSnapshot, nullptr);
    EXPECT_EQ(firstSnapshot->documentCount(), 1);
    EXPECT_EQ(secondSnapshot->documentCount(), 1);
    EXPECT_NE(firstSnapshot->sourceText(fs::weakly_canonical(firstPath).generic_string()), nullptr);
    EXPECT_EQ(firstSnapshot->sourceText(fs::weakly_canonical(secondPath).generic_string()), nullptr);
    EXPECT_NE(secondSnapshot->sourceText(fs::weakly_canonical(secondPath).generic_string()), nullptr);
    EXPECT_EQ(secondSnapshot->sourceText(fs::weakly_canonical(firstPath).generic_string()), nullptr);
}

} // namespace