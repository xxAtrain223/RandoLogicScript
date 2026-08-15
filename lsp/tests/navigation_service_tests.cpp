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

    const auto references = navigation.references(usageUri, {0, 18}, true);
    ASSERT_EQ(references.size(), 2u);
    EXPECT_EQ(references[0].uri, *rls::lsp::PathToFileUri(declarationPath));
    EXPECT_EQ(references[1].uri, usageUri);
    const auto referencesWithoutDeclaration = navigation.references(
        usageUri, {0, 18}, false);
    ASSERT_EQ(referencesWithoutDeclaration.size(), 1u);
    EXPECT_EQ(referencesWithoutDeclaration[0].uri, usageUri);
    const auto highlights = navigation.documentHighlights(usageUri, {0, 18});
    ASSERT_EQ(highlights.size(), 1u);
    EXPECT_EQ(highlights[0].start.character, 17u);

    ASSERT_EQ(projects.documentChanged(usageUri),
        rls::lsp::ProjectAssignmentResult::Assigned);
    EXPECT_FALSE(navigation.definition(usageUri, {0, 18}));
    EXPECT_TRUE(navigation.references(usageUri, {0, 18}, true).empty());
    EXPECT_TRUE(navigation.documentHighlights(usageUri, {0, 18}).empty());
}

TEST(NavigationServiceTests, KeepsSameNameParametersInSeparateScopes) {
    const fs::path sourcePath = fs::temp_directory_path() / "rls-navigation-parameters.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    const std::string content =
        "define first(value: Bool): value\n"
        "define second(value: Bool): value\n";
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
    const auto firstReferences = navigation.references(uri, {0, 28}, true);
    ASSERT_EQ(firstReferences.size(), 2u);
    EXPECT_EQ(firstReferences[0].range.start.line, 0u);
    EXPECT_EQ(firstReferences[0].range.start.character, 13u);
    EXPECT_EQ(firstReferences[1].range.start.line, 0u);
    EXPECT_EQ(firstReferences[1].range.start.character, 27u);
    const auto firstHighlights = navigation.documentHighlights(uri, {0, 28});
    ASSERT_EQ(firstHighlights.size(), 2u);
    EXPECT_EQ(firstHighlights[0].start.line, 0u);
    EXPECT_EQ(firstHighlights[1].start.line, 0u);
}

TEST(NavigationServiceTests, BuildsStableSourceOrderedDocumentSymbolHierarchy) {
    const fs::path sourcePath = fs::temp_directory_path() / "rls-document-symbols.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    const std::string content =
        "region RR_BASE { name: \"Base\" events { EVENT_BASE: true } }\n"
        "extend region RR_BASE { events { EVENT_EXT: true } }\n"
        "define check(value: Bool): value\n"
        "extern define host(item: Item) -> Bool\n"
        "enum Color { RED, BLUE }\n"
        "extern enum Item { RG_HOOKSHOT, RG_* }\n";
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
    const auto symbols = navigation.documentSymbols(uri);
    ASSERT_EQ(symbols.size(), 6u);
    EXPECT_EQ(symbols[0].name, "RR_BASE");
    EXPECT_EQ(symbols[1].name, "RR_BASE");
    EXPECT_EQ(symbols[2].name, "check");
    EXPECT_EQ(symbols[3].name, "host");
    EXPECT_EQ(symbols[4].name, "Color");
    EXPECT_EQ(symbols[5].name, "Item");

    ASSERT_EQ(symbols[0].children.size(), 2u);
    EXPECT_EQ(symbols[0].children[0].name, "name");
    EXPECT_EQ(symbols[0].children[1].name, "EVENT_BASE");
    ASSERT_EQ(symbols[1].children.size(), 1u);
    EXPECT_EQ(symbols[1].children[0].name, "EVENT_EXT");
    ASSERT_EQ(symbols[2].children.size(), 1u);
    EXPECT_EQ(symbols[2].children[0].name, "value");
    EXPECT_EQ(symbols[2].children[0].range.start.character, 13u);
    EXPECT_EQ(symbols[2].children[0].range.end.character, 24u);
    EXPECT_EQ(symbols[2].children[0].selectionRange.start.character, 13u);
    EXPECT_EQ(symbols[2].children[0].selectionRange.end.character, 18u);
    ASSERT_EQ(symbols[4].children.size(), 2u);
    EXPECT_EQ(symbols[4].children[0].name, "RED");
    EXPECT_EQ(symbols[4].children[1].name, "BLUE");
    ASSERT_EQ(symbols[5].children.size(), 2u);
    EXPECT_EQ(symbols[5].children[1].name, "RG_*");

    ASSERT_EQ(projects.documentChanged(uri),
        rls::lsp::ProjectAssignmentResult::Assigned);
    EXPECT_TRUE(navigation.documentSymbols(uri).empty());
}

TEST(NavigationServiceTests, FiltersAndOrdersWorkspaceProjectDeclarations) {
    const fs::path firstPath = fs::temp_directory_path() / "rls-workspace-symbol-first.rls";
    const fs::path secondPath = fs::temp_directory_path() / "rls-workspace-symbol-second.rls";
    const std::string firstUri = *rls::lsp::PathToFileUri(firstPath);
    const std::string secondUri = *rls::lsp::PathToFileUri(secondPath);
    DocumentStore documents;
    ASSERT_EQ(documents.open(firstUri, "rls", 1,
        "enum Holder { ALPHA_MEMBER }\n"
        "extern define alpha_host() -> Bool\n"
        "define alpha_define(): true\n"
        "region ALPHA_REGION { name: \"Alpha\" }\n"),
        rls::lsp::DocumentUpdateResult::Applied);
    ASSERT_EQ(documents.open(secondUri, "rls", 1, "define alpha_other(): true\n"),
        rls::lsp::DocumentUpdateResult::Applied);
    ProjectManager projects(documents, [](const fs::path& path) {
        rls::project::FileProject project;
        project.sourceFiles = {path};
        project.isStandalone = true;
        return project;
    });
    ASSERT_EQ(projects.documentOpened(firstUri),
        rls::lsp::ProjectAssignmentResult::Assigned);
    ASSERT_EQ(projects.documentOpened(secondUri),
        rls::lsp::ProjectAssignmentResult::Assigned);
    const auto* firstProject = projects.projectForDocument(firstUri);
    const auto* secondProject = projects.projectForDocument(secondUri);
    ASSERT_NE(firstProject, nullptr);
    ASSERT_NE(secondProject, nullptr);
    const std::string firstProjectId = firstProject->id;
    const std::string secondProjectId = secondProject->id;

    AnalysisScheduler scheduler({
        .debounce = std::chrono::milliseconds(0),
        .maximumConcurrency = 1,
    });
    ASSERT_TRUE(scheduler.schedule({
        firstProjectId,
        firstProject->generation,
        {{firstPath,
            "enum Holder { ALPHA_MEMBER }\n"
            "extern define alpha_host() -> Bool\n"
            "define alpha_define(): true\n"
            "region ALPHA_REGION { name: \"Alpha\" }\n"}},
        firstProject->documentGeneration,
        firstProject->manifestGeneration,
    }));
    ASSERT_TRUE(scheduler.schedule({
        secondProjectId,
        secondProject->generation,
        {{secondPath, "define alpha_other(): true\n"}},
        secondProject->documentGeneration,
        secondProject->manifestGeneration,
    }));
    scheduler.waitForIdle();

    NavigationService navigation(projects, scheduler);
    const auto symbols = navigation.workspaceSymbols("AlPhA", {firstProjectId});
    ASSERT_EQ(symbols.size(), 4u);
    EXPECT_EQ(symbols[0].name, "ALPHA_REGION");
    EXPECT_EQ(symbols[1].name, "alpha_define");
    EXPECT_EQ(symbols[2].name, "alpha_host");
    EXPECT_EQ(symbols[3].name, "ALPHA_MEMBER");
    EXPECT_EQ(symbols[3].containerName, "Holder");
    EXPECT_EQ(symbols[0].location.uri, firstUri);

    const auto bothProjects = navigation.workspaceSymbols(
        "alpha", {firstProjectId, secondProjectId});
    ASSERT_EQ(bothProjects.size(), 5u);
    EXPECT_EQ(bothProjects[2].name, "alpha_other");
}

TEST(NavigationServiceTests, ResolvesCanonicalRegionAndRejectsNamesWithoutConcreteTargets) {
    const fs::path sourcePath = fs::temp_directory_path() / "rls-navigation-targets.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    const std::string content =
        "region RR_BASE { name: \"Base\" }\n"
        "extend region RR_BASE { events { EVENT_BASE: true } }\n"
        "extend region RR_MISSING { events { EVENT_MISSING: true } }\n"
        "extern enum Item { RG_* }\n"
        "define item(): RG_SWORD\n"
        "enum Alpha { SHARED }\n"
        "enum Beta { SHARED }\n"
        "define ambiguous(): SHARED\n";
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
    EXPECT_TRUE(navigation.references(uri, {2, 15}, true).empty());
    EXPECT_TRUE(navigation.documentHighlights(uri, {2, 15}).empty());
    EXPECT_TRUE(navigation.references(uri, {4, 16}, true).empty());
    EXPECT_TRUE(navigation.documentHighlights(uri, {4, 16}).empty());
    EXPECT_FALSE(navigation.definition(uri, {7, 20}));
    EXPECT_TRUE(navigation.references(uri, {7, 20}, true).empty());
    EXPECT_TRUE(navigation.documentHighlights(uri, {7, 20}).empty());
}

} // namespace