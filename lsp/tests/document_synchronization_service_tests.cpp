#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "rls/lsp/document_store.h"
#include "rls/lsp/document_synchronization_service.h"
#include "rls/lsp/lifecycle_service.h"
#include "rls/lsp/project_manager.h"

namespace fs = std::filesystem;

namespace {

using rls::lsp::DocumentStore;
using rls::lsp::DocumentSynchronizationResult;
using rls::lsp::DocumentSynchronizationService;
using rls::lsp::LifecycleService;
using rls::lsp::ProjectManager;

class TemporaryDirectory {
public:
    TemporaryDirectory() : path_(fs::temp_directory_path() /
        ("rls-lsp-document-sync-" + std::to_string(
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

struct Services {
    DocumentStore documents;
    ProjectManager projects{documents};
    LifecycleService lifecycle;
    DocumentSynchronizationService synchronization{lifecycle, documents, projects};

    void start() {
        lifecycle.initialize();
        lifecycle.initialized();
    }
};

TEST(DocumentSynchronizationServiceTests, RejectsUpdatesUntilInitialized) {
    Services services;

    EXPECT_EQ(services.synchronization.open(
        "file:///early.rls", "rls", 1, "early"),
        DocumentSynchronizationResult::NotReady);
    EXPECT_EQ(services.documents.size(), 0);
}

TEST(DocumentSynchronizationServiceTests, RejectsStaleChangesWithoutAdvancingProject) {
    TemporaryDirectory directory;
    const fs::path sourcePath = directory.path() / "main.rls";
    writeFile(sourcePath, "disk\n");
    const std::string uri = fileUri(sourcePath);
    Services services;
    services.start();
    ASSERT_EQ(services.synchronization.open(uri, "rls", 3, "current\n"),
        DocumentSynchronizationResult::Applied);
    const uint64_t generation = services.projects.projectForDocument(uri)->generation;

    EXPECT_EQ(services.synchronization.change(uri, 3, "stale\n"),
        DocumentSynchronizationResult::StaleVersion);
    EXPECT_EQ(services.documents.find(uri)->text, "current\n");
    EXPECT_EQ(services.projects.projectForDocument(uri)->generation, generation);
}

TEST(DocumentSynchronizationServiceTests, FailedProjectResolutionRollsBackOverlay) {
    TemporaryDirectory directory;
    const fs::path missingPath = directory.path() / "missing.rls";
    const std::string uri = fileUri(missingPath);
    Services services;
    services.start();

    EXPECT_EQ(services.synchronization.open(uri, "rls", 1, "overlay\n"),
        DocumentSynchronizationResult::ProjectResolutionFailed);
    EXPECT_EQ(services.documents.find(uri), nullptr);
}

TEST(DocumentSynchronizationServiceTests, ClosingOverlayRestoresDiskSource) {
    TemporaryDirectory directory;
    const fs::path sourcePath = directory.path() / "main.rls";
    writeFile(sourcePath, "disk\n");
    const std::string uri = fileUri(sourcePath);
    Services services;
    services.start();
    ASSERT_EQ(services.synchronization.open(uri, "rls", 1, "overlay\n"),
        DocumentSynchronizationResult::Applied);

    ASSERT_EQ(services.synchronization.close(uri), DocumentSynchronizationResult::Applied);
    const auto sourceSet = services.projects.sourceSetForDocument(uri);
    ASSERT_TRUE(sourceSet.error.empty()) << sourceSet.error;
    ASSERT_EQ(sourceSet.sources.size(), 1);
    EXPECT_EQ(sourceSet.sources.front().content, "disk\n");
}

} // namespace