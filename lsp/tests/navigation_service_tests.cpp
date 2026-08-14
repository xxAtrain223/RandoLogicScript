#include <chrono>
#include <filesystem>

#include <gtest/gtest.h>

#include "rls/lsp/document_store.h"
#include "rls/lsp/document_uri.h"
#include "rls/lsp/navigation_service.h"

namespace fs = std::filesystem;

namespace {

using rls::lsp::AnalysisScheduler;
using rls::lsp::DocumentStore;
using rls::lsp::NavigationService;
using rls::lsp::ProjectManager;

TEST(NavigationServiceTests, FindsCrossFileDefinitionInCurrentSnapshot) {
    const fs::path root = fs::temp_directory_path() / "rls-navigation-service";
    const fs::path declarationPath = root / "declaration.rls";
    const fs::path usagePath = root / "usage.rls";
    const std::string usageUri = *rls::lsp::PathToFileUri(usagePath);
    DocumentStore documents;
    ASSERT_EQ(documents.open(
        usageUri, "rls", 1, "define caller(): target()\n"),
        rls::lsp::DocumentUpdateResult::Applied);
    ProjectManager projects(documents, [&](const fs::path&) {
        rls::project::FileProject project;
        project.sourceFiles = {declarationPath, usagePath};
        return project;
    });
    ASSERT_EQ(projects.documentOpened(usageUri),
        rls::lsp::ProjectAssignmentResult::Assigned);
    const auto* project = projects.projectForDocument(usageUri);
    ASSERT_NE(project, nullptr);

    AnalysisScheduler scheduler({
        .debounce = std::chrono::milliseconds(0),
        .maximumConcurrency = 1,
    });
    ASSERT_TRUE(scheduler.schedule({
        project->id,
        project->generation,
        {
            {declarationPath, "extern define target() -> Bool\n"},
            {usagePath, "define caller(): target()\n"},
        },
        project->documentGeneration,
        project->manifestGeneration,
    }));
    scheduler.waitForIdle();

    NavigationService navigation(projects, scheduler);
    const auto definition = navigation.definition(usageUri, {0, 18});
    ASSERT_TRUE(definition);
    EXPECT_EQ(definition->targetUri, *rls::lsp::PathToFileUri(declarationPath));
    EXPECT_EQ(definition->originSelectionRange.start.line, 0u);
    EXPECT_EQ(definition->originSelectionRange.start.character, 17u);
    EXPECT_EQ(definition->originSelectionRange.end.character, 23u);
    EXPECT_EQ(definition->targetSelectionRange.start.character, 14u);
    EXPECT_EQ(definition->targetSelectionRange.end.character, 20u);

    ASSERT_EQ(projects.documentChanged(usageUri),
        rls::lsp::ProjectAssignmentResult::Assigned);
    EXPECT_FALSE(navigation.definition(usageUri, {0, 18}));
}

TEST(NavigationServiceTests, ResolvesCanonicalRegionAndRejectsNamesWithoutConcreteTargets) {
    const fs::path sourcePath = fs::temp_directory_path() / "rls-navigation-targets.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    const std::string content =
        "region RR_BASE { name: \"Base\" }\n"
        "extend region RR_BASE { events { EVENT_BASE: true } }\n"
        "extend region RR_MISSING { events { EVENT_MISSING: true } }\n"
        "extern enum Item { RG_* }\n"
        "define item(): RG_SWORD\n";
    DocumentStore documents;
    ASSERT_EQ(documents.open(uri, "rls", 1, content),
        rls::lsp::DocumentUpdateResult::Applied);
    ProjectManager projects(documents, [&](const fs::path&) {
        rls::project::FileProject project;
        project.sourceFiles = {sourcePath};
        project.isStandalone = true;
        return project;
    });
    ASSERT_EQ(projects.documentOpened(uri),
        rls::lsp::ProjectAssignmentResult::Assigned);
    const auto* project = projects.projectForDocument(uri);
    ASSERT_NE(project, nullptr);

    AnalysisScheduler scheduler({
        .debounce = std::chrono::milliseconds(0),
        .maximumConcurrency = 1,
    });
    ASSERT_TRUE(scheduler.schedule({
        project->id,
        project->generation,
        {{sourcePath, content}},
        project->documentGeneration,
        project->manifestGeneration,
    }));
    scheduler.waitForIdle();

    NavigationService navigation(projects, scheduler);
    const auto region = navigation.definition(uri, {1, 15});
    ASSERT_TRUE(region);
    EXPECT_EQ(region->targetSelectionRange.start.line, 0u);
    EXPECT_EQ(region->targetSelectionRange.start.character, 7u);
    EXPECT_EQ(region->targetSelectionRange.end.character, 14u);
    EXPECT_FALSE(navigation.definition(uri, {2, 15}));
    EXPECT_FALSE(navigation.definition(uri, {4, 16}));
}

} // namespace