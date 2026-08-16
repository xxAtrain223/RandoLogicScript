#include <algorithm>
#include <chrono>
#include <filesystem>

#include <gtest/gtest.h>

#include "rls/lsp/completion_service.h"
#include "rls/lsp/document_store.h"
#include "rls/lsp/document_uri.h"

namespace fs = std::filesystem;

namespace {

using rls::lsp::AnalysisScheduler;
using rls::lsp::CompletionItem;
using rls::lsp::CompletionService;
using rls::lsp::DocumentStore;
using rls::lsp::PresentationPosition;
using rls::lsp::ProjectManager;

struct CompletionFixture {
    fs::path path = fs::temp_directory_path() / "rls-completion-service.rls";
    std::string uri = *rls::lsp::PathToFileUri(path);
    std::string content;
    DocumentStore documents;
    ProjectManager projects;
    AnalysisScheduler scheduler;

    explicit CompletionFixture(std::string source)
        : content(std::move(source)),
          projects(documents, [&](const fs::path&) {
              rls::project::FileProject project;
              project.sourceFiles = {path};
              project.isStandalone = true;
              return project;
          }),
          scheduler({
              .debounce = std::chrono::milliseconds(0),
              .maximumConcurrency = 1,
          }) {
        EXPECT_EQ(documents.open(uri, "rls", 1, content),
            rls::lsp::DocumentUpdateResult::Applied);
        EXPECT_EQ(projects.documentOpened(uri),
            rls::lsp::ProjectAssignmentResult::Assigned);
        const auto* project = projects.projectForDocument(uri);
        EXPECT_NE(project, nullptr);
        if (!project) return;
        EXPECT_TRUE(scheduler.schedule({
            project->id,
            project->generation,
            {{path, content}},
            project->documentGeneration,
            project->manifestGeneration,
        }));
        scheduler.waitForIdle();
    }
};

const CompletionItem* findItem(
    const std::vector<CompletionItem>& items, std::string_view label) {
    const auto found = std::find_if(items.begin(), items.end(), [&](const auto& item) {
        return item.label == label;
    });
    return found == items.end() ? nullptr : &*found;
}

struct CrossFileCompletionFixture {
    fs::path root = fs::temp_directory_path() / "rls-cross-file-completion";
    fs::path declarationPath = root / "declaration.rls";
    fs::path usagePath = root / "usage.rls";
    std::string usageUri = *rls::lsp::PathToFileUri(usagePath);
    DocumentStore documents;
    ProjectManager projects;
    AnalysisScheduler scheduler;

    CrossFileCompletionFixture(std::string declarations, std::string usage)
        : projects(documents, [&](const fs::path&) {
              rls::project::FileProject project;
              project.sourceFiles = {declarationPath, usagePath};
              return project;
          }),
          scheduler({
              .debounce = std::chrono::milliseconds(0),
              .maximumConcurrency = 1,
          }) {
        EXPECT_EQ(documents.open(usageUri, "rls", 1, usage),
            rls::lsp::DocumentUpdateResult::Applied);
        EXPECT_EQ(projects.documentOpened(usageUri),
            rls::lsp::ProjectAssignmentResult::Assigned);
        const auto* project = projects.projectForDocument(usageUri);
        EXPECT_NE(project, nullptr);
        if (!project) return;
        EXPECT_TRUE(scheduler.schedule({
            project->id,
            project->generation,
            {
                {declarationPath, std::move(declarations)},
                {usagePath, std::move(usage)},
            },
            project->documentGeneration,
            project->manifestGeneration,
        }));
        scheduler.waitForIdle();
    }

    std::vector<CompletionItem> completeAtEnd(std::string_view usage) {
        return CompletionService(projects, scheduler).complete(
            usageUri, {0, static_cast<uint32_t>(usage.size())});
    }

    std::vector<CompletionItem> complete(PresentationPosition position) {
        return CompletionService(projects, scheduler).complete(usageUri, position);
    }
};

TEST(CompletionServiceTests, OffersOnlyDeclarationKeywordsAtTopLevel) {
    CompletionFixture fixture("def\n");

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {0, 3});

    ASSERT_NE(findItem(items, "define"), nullptr);
    ASSERT_NE(findItem(items, "extern define"), nullptr);
    EXPECT_EQ(findItem(items, "true"), nullptr);
    EXPECT_EQ(items.front().label, "define");
    EXPECT_EQ(items.front().replacementRange.start.character, 0u);
    EXPECT_EQ(items.front().replacementRange.end.character, 3u);
}

TEST(CompletionServiceTests, OffersBuiltInAndDeclaredTypesInTypePosition) {
    CompletionFixture fixture(
        "enum Color { RED }\n"
        "define choose(value: Color): value\n");

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {1, 26});

    ASSERT_NE(findItem(items, "Color"), nullptr);
    ASSERT_NE(findItem(items, "Bool"), nullptr);
    EXPECT_EQ(findItem(items, "RED"), nullptr);
    EXPECT_EQ(findItem(items, "choose"), nullptr);
}

TEST(CompletionServiceTests, CompletesRecoveredParameterAndReturnTypes) {
    const std::string parameterSource =
        "enum Color { RED }\ndefine choose(value: Col";
    CompletionFixture parameterFixture(parameterSource);
    const auto parameters = CompletionService(
        parameterFixture.projects, parameterFixture.scheduler)
        .complete(parameterFixture.uri, {1, 24});

    EXPECT_NE(findItem(parameters, "Color"), nullptr);
    EXPECT_NE(findItem(parameters, "Condition"), nullptr);
    EXPECT_NE(findItem(parameters, "Event"), nullptr);
    EXPECT_NE(findItem(parameters, "Location"), nullptr);
    EXPECT_NE(findItem(parameters, "Region"), nullptr);
    EXPECT_EQ(findItem(parameters, "RED"), nullptr);
    EXPECT_EQ(parameters.front().label, "Color");

    const std::string returnSource =
        "enum Color { RED }\nextern define choose() -> Col";
    CompletionFixture returnFixture(returnSource);
    const auto returns = CompletionService(returnFixture.projects, returnFixture.scheduler)
        .complete(returnFixture.uri, {1, 29});

    EXPECT_NE(findItem(returns, "Color"), nullptr);
    EXPECT_NE(findItem(returns, "Bool"), nullptr);
    EXPECT_EQ(findItem(returns, "RED"), nullptr);
    EXPECT_EQ(returns.front().label, "Color");
}

TEST(CompletionServiceTests, UsesScopeAndExpectedEnumForExpressionCandidates) {
    CompletionFixture fixture(
        "enum Color { RED, BLUE }\n"
        "enum Size { SMALL }\n"
        "define choose(value: Color): value\n"
        "define other(hidden: Bool): hidden\n"
        "define use(input: Color): choose(R)\n");

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {4, 34});

    ASSERT_NE(findItem(items, "RED"), nullptr);
    ASSERT_NE(findItem(items, "BLUE"), nullptr);
    ASSERT_NE(findItem(items, "input"), nullptr);
    ASSERT_NE(findItem(items, "choose"), nullptr);
    EXPECT_EQ(findItem(items, "SMALL"), nullptr);
    EXPECT_EQ(findItem(items, "hidden"), nullptr);
    EXPECT_EQ(findItem(items, "true"), nullptr);
    EXPECT_EQ(items.front().label, "RED");
    EXPECT_EQ(items.front().replacementRange.start.character, 33u);
    EXPECT_EQ(items.front().replacementRange.end.character, 34u);
    EXPECT_EQ(findItem(items, "choose")->detail, "choose(value: Color)");
}

TEST(CompletionServiceTests, RejectsAStaleAcceptedSnapshot) {
    CompletionFixture fixture("define check(flag: Bool): flag\n");
    ASSERT_EQ(fixture.projects.documentChanged(fixture.uri),
        rls::lsp::ProjectAssignmentResult::Assigned);

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {0, 31});

    EXPECT_TRUE(items.empty());
}

TEST(CompletionServiceTests, ExpeditesLatestScheduledDocumentGeneration) {
    const fs::path sourcePath = fs::temp_directory_path() /
        "rls-immediate-completion.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    DocumentStore documents;
    ASSERT_EQ(documents.open(uri, "rls", 1, "def\n"),
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
        .debounce = std::chrono::seconds(5),
        .maximumConcurrency = 1,
    });
    ASSERT_TRUE(scheduler.schedule({
        project->id,
        project->generation,
        {{sourcePath, "def\n"}},
        project->documentGeneration,
        project->manifestGeneration,
    }));
    ASSERT_NE(scheduler.awaitSnapshot(
        project->id, project->generation, std::chrono::seconds(1)), nullptr);

    const std::string changed = "reg\n";
    ASSERT_EQ(documents.applyFullChange(uri, 2, changed),
        rls::lsp::DocumentUpdateResult::Applied);
    ASSERT_EQ(projects.documentChanged(uri),
        rls::lsp::ProjectAssignmentResult::Assigned);
    project = projects.projectForDocument(uri);
    ASSERT_NE(project, nullptr);
    ASSERT_TRUE(scheduler.schedule({
        project->id,
        project->generation,
        {{sourcePath, changed}},
        project->documentGeneration,
        project->manifestGeneration,
    }));

    const auto items = CompletionService(projects, scheduler)
        .complete(uri, {0, 3});

    ASSERT_NE(findItem(items, "region"), nullptr);
    EXPECT_EQ(items.front().label, "region");
}

TEST(CompletionServiceTests, CompletesRecoveredRegionBodyWithoutDuplicates) {
    const std::string usage =
        "region RR_CURRENT { displayLabel: \"Current\" wo";
    CrossFileCompletionFixture fixture(
        "region RR_TEMPLATE { displayLabel: \"Template\" worldNode: true }\n",
        usage);

    const auto items = fixture.completeAtEnd(usage);

    ASSERT_NE(findItem(items, "locations"), nullptr);
    ASSERT_NE(findItem(items, "worldNode"), nullptr);
    EXPECT_EQ(findItem(items, "worldNode")->snippetText, "worldNode: ${1}");
    EXPECT_EQ(findItem(items, "worldNode")->detail, "project region data key");
    EXPECT_EQ(findItem(items, "displayLabel"), nullptr);
    EXPECT_EQ(findItem(items, "define"), nullptr);
    EXPECT_EQ(items.front().label, "worldNode");
    EXPECT_EQ(items.front().replacementRange.start.character, usage.size() - 2);
    EXPECT_EQ(items.front().replacementRange.end.character, usage.size());
}

TEST(CompletionServiceTests, FallsBackToSectionsWithoutObservedRegionKeys) {
    CompletionFixture fixture(
        "region RR_TEST {\n"
        "  \n"
        "}\n");

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {1, 2});

    ASSERT_EQ(items.size(), 3u);
    EXPECT_NE(findItem(items, "events"), nullptr);
    EXPECT_NE(findItem(items, "locations"), nullptr);
    EXPECT_NE(findItem(items, "exits"), nullptr);
    EXPECT_EQ(findItem(items, "events")->snippetText, "events {\n    $0\n}");
    EXPECT_EQ(findItem(items, "locations")->snippetText, "locations {\n    $0\n}");
    EXPECT_EQ(findItem(items, "exits")->snippetText, "exits {\n    $0\n}");
    EXPECT_EQ(findItem(items, "events")->serverIndentedSnippetText,
        "events {\n      $0\n  }");
    EXPECT_EQ(findItem(items, "locations")->serverIndentedSnippetText,
        "locations {\n      $0\n  }");
    EXPECT_EQ(findItem(items, "exits")->serverIndentedSnippetText,
        "exits {\n      $0\n  }");
}

TEST(CompletionServiceTests, LimitsExtensionBodiesToMissingSections) {
    CompletionFixture fixture(
        "extend region RR_TEST {\n"
        "  events {}\n"
        "  ex\n");

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {2, 4});

    ASSERT_NE(findItem(items, "exits"), nullptr);
    ASSERT_NE(findItem(items, "locations"), nullptr);
    EXPECT_EQ(findItem(items, "events"), nullptr);
    EXPECT_EQ(findItem(items, "name"), nullptr);
    EXPECT_EQ(items.front().label, "exits");
}

TEST(CompletionServiceTests, OffersHereOnlyInRegionExpressions) {
    CompletionFixture fixture(
        "region RR_TEST { events { EVENT_TEST: tr } }\n"
        "define check(): tr\n");
    CompletionService completion(fixture.projects, fixture.scheduler);

    const auto regionItems = completion.complete(fixture.uri, {0, 40});
    const auto defineItems = completion.complete(fixture.uri, {1, 18});

    ASSERT_NE(findItem(regionItems, "here"), nullptr);
    EXPECT_EQ(findItem(regionItems, "here")->detail, "built-in here: Region");
    EXPECT_EQ(findItem(defineItems, "here"), nullptr);
}

TEST(CompletionServiceTests, CompletesPreviouslyDeclaredSectionEntriesByKind) {
    const std::string declarations =
        "region RR_TEMPLATE {\n"
        "  events {\n"
        "    EVENT_EXISTING: true\n"
        "    EVENT_OTHER: true\n"
        "  }\n"
        "  locations {\n"
        "    RC_EXISTING: true\n"
        "    RC_OTHER: true\n"
        "  }\n"
        "}\n";
    const std::string eventUsage =
        "region RR_CURRENT {\n"
        "  events {\n"
        "    EVENT_EXISTING: true\n"
        "    EVENT_\n"
        "  }\n"
        "}\n";
    CrossFileCompletionFixture eventFixture(declarations, eventUsage);

    const auto events = eventFixture.complete({3, 10});

    const auto* event = findItem(events, "EVENT_OTHER");
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->insertText, "EVENT_OTHER: ");
    EXPECT_EQ(event->snippetText, "EVENT_OTHER: ${1}");
    EXPECT_EQ(event->detail, "EVENT_OTHER: Event");
    EXPECT_EQ(findItem(events, "EVENT_EXISTING"), nullptr);
    EXPECT_EQ(findItem(events, "RC_OTHER"), nullptr);

    const std::string locationUsage =
        "region RR_CURRENT {\n"
        "  locations {\n"
        "    RC_EXISTING: true\n"
        "    \n"
        "  }\n"
        "}\n";
    CrossFileCompletionFixture locationFixture(declarations, locationUsage);
    const auto locations = locationFixture.complete({3, 4});

    const auto* location = findItem(locations, "RC_OTHER");
    ASSERT_NE(location, nullptr);
    EXPECT_EQ(location->snippetText, "RC_OTHER: ${1}");
    EXPECT_EQ(location->detail, "RC_OTHER: Location");
    EXPECT_EQ(findItem(locations, "RC_EXISTING"), nullptr);
    EXPECT_EQ(findItem(locations, "EVENT_OTHER"), nullptr);
}

TEST(CompletionServiceTests, CompletesExitLabelsFromDeclaredRegions) {
    const std::string declarations =
        "region RR_FIRST {}\n"
        "region RR_SECOND {}\n"
        "region RR_THIRD {}\n";
    const std::string usage =
        "region RR_FIRST {\n"
        "  exits {\n"
        "    RR_SECOND: true\n"
        "    RR_\n"
        "  }\n"
        "}\n";
    CrossFileCompletionFixture fixture(declarations, usage);

    const auto items = fixture.complete({3, 7});

    const auto* third = findItem(items, "RR_THIRD");
    ASSERT_NE(third, nullptr);
    EXPECT_EQ(third->insertText, "RR_THIRD: ");
    EXPECT_EQ(third->snippetText, "RR_THIRD: ${1}");
    EXPECT_EQ(third->detail, "region RR_THIRD");
    EXPECT_EQ(findItem(items, "RR_FIRST"), nullptr);
    EXPECT_EQ(findItem(items, "RR_SECOND"), nullptr);
}

TEST(CompletionServiceTests, SuppressesEntriesFromOtherContributionsToActiveRegion) {
    const std::string declarations =
        "region RR_CURRENT { events { EVENT_EXISTING: true } }\n"
        "region RR_OTHER { events { EVENT_OTHER: true } }\n";
    const std::string usage =
        "extend region RR_CURRENT {\n"
        "  events {\n"
        "    EVENT_\n"
        "  }\n"
        "}\n";
    CrossFileCompletionFixture fixture(declarations, usage);

    const auto items = fixture.complete({2, 10});

    EXPECT_EQ(findItem(items, "EVENT_EXISTING"), nullptr);
    EXPECT_NE(findItem(items, "EVENT_OTHER"), nullptr);
}

TEST(CompletionServiceTests, RecoversSameFileEventsWhileRecreatingCommentedRegion) {
    CompletionFixture fixture(
        "region RR_KOKIRI_FOREST {\n"
        "  events {\n"
        "    LOGIC_FAIRY_ACCESS: always\n"
        "    LOGIC_OTHER: true\n"
        "  }\n"
        "}\n"
        "# region RR_KF_STORMS_GROTTO {\n"
        "#   events {\n"
        "#     LOGIC_FAIRY_ACCESS: true\n"
        "#   }\n"
        "# }\n"
        "region RR_KF_STORMS_GROTTO {\n"
        "  events {\n"
        "    LO\n");

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {13, 6});

    ASSERT_NE(findItem(items, "LOGIC_FAIRY_ACCESS"), nullptr);
    ASSERT_NE(findItem(items, "LOGIC_OTHER"), nullptr);
    EXPECT_EQ(items.front().label, "LOGIC_FAIRY_ACCESS");
}

TEST(CompletionServiceTests, RecoversSameFileRegionsForExitCompletion) {
    CompletionFixture fixture(
        "region RR_FIRST {}\n"
        "region RR_SECOND {}\n"
        "# region RR_COMMENTED {}\n"
        "region RR_CURRENT {\n"
        "  exits {\n"
        "    RR_\n");

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {5, 7});

    EXPECT_NE(findItem(items, "RR_FIRST"), nullptr);
    EXPECT_NE(findItem(items, "RR_SECOND"), nullptr);
    EXPECT_EQ(findItem(items, "RR_CURRENT"), nullptr);
    EXPECT_EQ(findItem(items, "RR_COMMENTED"), nullptr);
}

TEST(CompletionServiceTests, CompletesOnlyMembersOfQualifiedEnum) {
    CompletionFixture fixture(
        "enum Alpha { SHARED, ALPHA_ONLY }\n"
        "enum Beta { SHARED, BETA_ONLY }\n"
        "define check(): Alpha.S\n");

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {2, 23});

    ASSERT_NE(findItem(items, "SHARED"), nullptr);
    ASSERT_NE(findItem(items, "ALPHA_ONLY"), nullptr);
    EXPECT_EQ(findItem(items, "BETA_ONLY"), nullptr);
    EXPECT_EQ(findItem(items, "Alpha"), nullptr);
    EXPECT_EQ(items.front().label, "SHARED");
    EXPECT_EQ(items.front().replacementRange.start.character, 22u);
    EXPECT_EQ(items.front().replacementRange.end.character, 23u);
}

TEST(CompletionServiceTests, ExcludesPatternsAndUnknownEnumFallbacks) {
    CompletionFixture fixture(
        "extern enum Status { READY, ST_* }\n"
        "define known(): Status.R\n"
        "define unknown(): Missing.R\n");
    CompletionService completion(fixture.projects, fixture.scheduler);

    const auto known = completion.complete(fixture.uri, {1, 24});
    const auto unknown = completion.complete(fixture.uri, {2, 27});

    ASSERT_NE(findItem(known, "READY"), nullptr);
    EXPECT_EQ(findItem(known, "ST_*"), nullptr);
    EXPECT_TRUE(unknown.empty());
}

TEST(CompletionServiceTests, CompletesPreviouslyObservedExternPatternValues) {
    const std::string declarations =
        "extern enum Item { RG_EXPLICIT, RG_* }\n"
        "extern enum Status { ST_* }\n"
        "extern define has(item: Item) -> Bool\n"
        "define seen(): has(RG_HOOKSHOT)\n"
        "define seen_qualified(): Item.RG_BOW\n"
        "define other(): Status.ST_READY\n";
    const std::string expectedUsage = "define use(): has(RG_";
    CrossFileCompletionFixture expectedFixture(declarations, expectedUsage);

    const auto expectedItems = expectedFixture.completeAtEnd(expectedUsage);

    EXPECT_NE(findItem(expectedItems, "RG_EXPLICIT"), nullptr);
    EXPECT_NE(findItem(expectedItems, "RG_HOOKSHOT"), nullptr);
    EXPECT_NE(findItem(expectedItems, "RG_BOW"), nullptr);
    EXPECT_EQ(findItem(expectedItems, "RG_*"), nullptr);
    EXPECT_EQ(findItem(expectedItems, "ST_READY"), nullptr);

    const std::string qualifiedUsage = "define use(): Item.RG_";
    CrossFileCompletionFixture qualifiedFixture(declarations, qualifiedUsage);
    const auto qualifiedItems = qualifiedFixture.completeAtEnd(qualifiedUsage);

    EXPECT_NE(findItem(qualifiedItems, "RG_EXPLICIT"), nullptr);
    EXPECT_NE(findItem(qualifiedItems, "RG_HOOKSHOT"), nullptr);
    EXPECT_NE(findItem(qualifiedItems, "RG_BOW"), nullptr);
    EXPECT_EQ(findItem(qualifiedItems, "RG_*"), nullptr);
    EXPECT_EQ(findItem(qualifiedItems, "ST_READY"), nullptr);
}

TEST(CompletionServiceTests, RecoversEmptyMemberAcrossFiles) {
    const fs::path root = fs::temp_directory_path() / "rls-member-completion";
    const fs::path declarationPath = root / "declaration.rls";
    const fs::path usagePath = root / "usage.rls";
    const std::string usageUri = *rls::lsp::PathToFileUri(usagePath);
    const std::string usage = "define choose(): Color.\n";
    DocumentStore documents;
    ASSERT_EQ(documents.open(usageUri, "rls", 1, usage),
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
            {declarationPath, "enum Color { RED, BLUE }\n"},
            {usagePath, usage},
        },
        project->documentGeneration,
        project->manifestGeneration,
    }));
    scheduler.waitForIdle();

    const auto items = CompletionService(projects, scheduler)
        .complete(usageUri, {0, 23});

    ASSERT_NE(findItem(items, "RED"), nullptr);
    ASSERT_NE(findItem(items, "BLUE"), nullptr);
    EXPECT_EQ(items.front().replacementRange.start.character, 23u);
    EXPECT_EQ(items.front().replacementRange.end.character, 23u);
}

TEST(CompletionServiceTests, CompletesOnlyUnboundNamedArgumentsAcrossFiles) {
    const fs::path root = fs::temp_directory_path() / "rls-named-argument-completion";
    const fs::path declarationPath = root / "declaration.rls";
    const fs::path usagePath = root / "usage.rls";
    const std::string usageUri = *rls::lsp::PathToFileUri(usagePath);
    const std::string usage =
        "define use(): target(true, third: false, se";
    DocumentStore documents;
    ASSERT_EQ(documents.open(usageUri, "rls", 1, usage),
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
            {declarationPath,
                "extern define target(first: Bool, second: Bool, third: Bool) -> Bool\n"},
            {usagePath, usage},
        },
        project->documentGeneration,
        project->manifestGeneration,
    }));
    scheduler.waitForIdle();

    const auto items = CompletionService(projects, scheduler)
        .complete(usageUri, {0, static_cast<uint32_t>(usage.size())});

    const auto* second = findItem(items, "second");
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->insertText, "second: ");
    EXPECT_EQ(second->snippetText, "second: ${1}");
    EXPECT_EQ(second->detail, "second: Bool");
    EXPECT_EQ(findItem(items, "first"), nullptr);
    EXPECT_EQ(findItem(items, "third"), nullptr);
    EXPECT_EQ(items.front().label, "second");
    EXPECT_EQ(second->replacementRange.start.character, usage.size() - 2);
    EXPECT_EQ(second->replacementRange.end.character, usage.size());
}

TEST(CompletionServiceTests, KeepsNestedCallsOutOfOuterArgumentBinding) {
    const fs::path root = fs::temp_directory_path() / "rls-nested-label-completion";
    const fs::path declarationPath = root / "declaration.rls";
    const fs::path usagePath = root / "usage.rls";
    const std::string usageUri = *rls::lsp::PathToFileUri(usagePath);
    const std::string usage = "define use(): outer(nested(true), se";
    DocumentStore documents;
    ASSERT_EQ(documents.open(usageUri, "rls", 1, usage),
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
            {declarationPath,
                "extern define nested(value: Bool) -> Bool\n"
                "extern define outer(first: Bool, second: Bool) -> Bool\n"},
            {usagePath, usage},
        },
        project->documentGeneration,
        project->manifestGeneration,
    }));
    scheduler.waitForIdle();

    const auto items = CompletionService(projects, scheduler)
        .complete(usageUri, {0, static_cast<uint32_t>(usage.size())});

    ASSERT_NE(findItem(items, "second"), nullptr);
    EXPECT_EQ(findItem(items, "first"), nullptr);
}

TEST(CompletionServiceTests, DoesNotFabricateLabelsForUnknownCallee) {
    CompletionFixture fixture("define use(): missing(arg\n");

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {0, 25});

    EXPECT_EQ(findItem(items, "arg"), nullptr);
}

TEST(CompletionServiceTests, DoesNotTreatDeclarationParametersAsArguments) {
    CompletionFixture fixture(
        "extern define target(first: Bool, second: Bool) -> Bool\n");

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {0, 26});

    EXPECT_TRUE(items.empty());
}

TEST(CompletionServiceTests, CompletesLabelsInParsedCalls) {
    CompletionFixture fixture(
        "extern define target(first: Bool, second: Bool) -> Bool\n"
        "define use(): target(first: true, se: false)\n");

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {1, 36});

    ASSERT_NE(findItem(items, "second"), nullptr);
    EXPECT_EQ(findItem(items, "first"), nullptr);
    EXPECT_EQ(findItem(items, "second")->insertText, "second: ");
}

TEST(CompletionServiceTests, KeepsCandidatesBeforeLaterPositionalArguments) {
    CompletionFixture fixture(
        "extern define target(first: Bool, second: Bool) -> Bool\n"
        "define use(): target(fi, true)\n");

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {1, 23});

    ASSERT_NE(findItem(items, "first"), nullptr);
    ASSERT_NE(findItem(items, "second"), nullptr);
    EXPECT_EQ(items.front().label, "first");
}

TEST(CompletionServiceTests, FiltersRecoveredPositionalAndNamedEnumValues) {
    const std::string declarations =
        "enum Color { RED, BLUE }\n"
        "enum Size { SMALL }\n"
        "extern define paint(color: Color) -> Bool\n";
    const std::string positionalUsage = "define use(): paint(R";
    CrossFileCompletionFixture positional(declarations, positionalUsage);

    const auto positionalItems = positional.completeAtEnd(positionalUsage);

    ASSERT_NE(findItem(positionalItems, "RED"), nullptr);
    ASSERT_NE(findItem(positionalItems, "BLUE"), nullptr);
    EXPECT_EQ(findItem(positionalItems, "SMALL"), nullptr);
    EXPECT_EQ(findItem(positionalItems, "true"), nullptr);
    EXPECT_EQ(positionalItems.front().label, "RED");
    EXPECT_EQ(positionalItems.front().replacementRange.start.character,
        positionalUsage.size() - 1);

    const std::string emptyUsage = "define use(): paint(";
    CrossFileCompletionFixture empty(declarations, emptyUsage);
    const auto emptyItems = empty.completeAtEnd(emptyUsage);

    ASSERT_NE(findItem(emptyItems, "RED"), nullptr);
    ASSERT_NE(findItem(emptyItems, "BLUE"), nullptr);
    EXPECT_EQ(findItem(emptyItems, "SMALL"), nullptr);
    EXPECT_EQ(findItem(emptyItems, "RED")->replacementRange.start.character,
        emptyUsage.size());
    EXPECT_EQ(findItem(emptyItems, "RED")->replacementRange.end.character,
        emptyUsage.size());

    const std::string namedUsage = "define use(): paint(color: B";
    CrossFileCompletionFixture named(declarations, namedUsage);
    const auto namedItems = named.completeAtEnd(namedUsage);

    ASSERT_NE(findItem(namedItems, "BLUE"), nullptr);
    ASSERT_NE(findItem(namedItems, "RED"), nullptr);
    EXPECT_EQ(findItem(namedItems, "SMALL"), nullptr);
    EXPECT_EQ(namedItems.front().label, "BLUE");
}

TEST(CompletionServiceTests, ReplaysBindingsForRecoveredBooleanValues) {
    const std::string declarations =
        "enum Color { RED }\n"
        "extern define target(first: Color, second: Bool, third: Bool) -> Bool\n";
    const std::string usage = "define use(): target(RED, third: false, tr";
    CrossFileCompletionFixture fixture(declarations, usage);

    const auto items = fixture.completeAtEnd(usage);

    ASSERT_NE(findItem(items, "true"), nullptr);
    ASSERT_NE(findItem(items, "false"), nullptr);
    EXPECT_EQ(findItem(items, "RED"), nullptr);
    EXPECT_EQ(findItem(items, "third"), nullptr);
    EXPECT_EQ(items.front().label, "true");
}

TEST(CompletionServiceTests, UsesInnermostRecoveredCallExpectedType) {
    const std::string declarations =
        "enum Color { RED, BLUE }\n"
        "extern define nested(value: Color) -> Bool\n"
        "extern define outer(flag: Bool, result: Bool) -> Bool\n";
    const std::string usage = "define use(): outer(true, nested(R";
    CrossFileCompletionFixture fixture(declarations, usage);

    const auto items = fixture.completeAtEnd(usage);

    ASSERT_NE(findItem(items, "RED"), nullptr);
    ASSERT_NE(findItem(items, "BLUE"), nullptr);
    EXPECT_EQ(findItem(items, "true"), nullptr);
}

TEST(CompletionServiceTests, DoesNotInventExpectedTypeForUnknownCall) {
    const std::string declarations = "enum Color { RED }\n";
    const std::string usage = "define use(): missing(R";
    CrossFileCompletionFixture fixture(declarations, usage);

    const auto items = fixture.completeAtEnd(usage);

    EXPECT_EQ(findItem(items, "RED"), nullptr);
}

TEST(CompletionServiceTests, DoesNotResolveAmbiguousRecoveredCall) {
    const std::string declarations =
        "enum Color { RED }\n"
        "enum Size { SMALL }\n"
        "extern define paint(value: Color) -> Bool\n"
        "extern define paint(value: Size) -> Bool\n";
    const std::string usage = "define use(): paint(R";
    CrossFileCompletionFixture fixture(declarations, usage);

    const auto items = fixture.completeAtEnd(usage);

    EXPECT_EQ(findItem(items, "RED"), nullptr);
    EXPECT_EQ(findItem(items, "SMALL"), nullptr);
}

TEST(CompletionServiceTests, DoesNotResolveInvalidRecoveredArgumentBinding) {
    const std::string declarations =
        "enum Color { RED }\n"
        "extern define paint(color: Color) -> Bool\n";
    const std::string usage = "define use(): paint(missing: R";
    CrossFileCompletionFixture fixture(declarations, usage);

    const auto items = fixture.completeAtEnd(usage);

    EXPECT_EQ(findItem(items, "RED"), nullptr);
    EXPECT_EQ(findItem(items, "color"), nullptr);
}

TEST(CompletionServiceTests, FiltersCrossFileDeclaredDomainValuesByExpectedType) {
    const std::string declarations =
        "region RR_FIRST {\n"
        "  events { EVENT_FIRST: true }\n"
        "  locations { RC_FIRST: true }\n"
        "}\n"
        "region RR_SECOND {\n"
        "  events { EVENT_SECOND: true }\n"
        "  locations { RC_SECOND: true }\n"
        "}\n"
        "extern define use_values(reg: Region, evt: Event, loc: Location) -> Bool\n";

    const std::string regionUsage = "define use(): use_values(RR_";
    CrossFileCompletionFixture regionFixture(declarations, regionUsage);
    const auto regions = regionFixture.completeAtEnd(regionUsage);
    EXPECT_NE(findItem(regions, "RR_FIRST"), nullptr);
    EXPECT_NE(findItem(regions, "RR_SECOND"), nullptr);
    EXPECT_EQ(findItem(regions, "EVENT_FIRST"), nullptr);
    EXPECT_EQ(findItem(regions, "RC_FIRST"), nullptr);

    const std::string eventUsage = "define use(): use_values(RR_FIRST, EVENT_";
    CrossFileCompletionFixture eventFixture(declarations, eventUsage);
    const auto events = eventFixture.completeAtEnd(eventUsage);
    EXPECT_NE(findItem(events, "EVENT_FIRST"), nullptr);
    EXPECT_NE(findItem(events, "EVENT_SECOND"), nullptr);
    EXPECT_EQ(findItem(events, "RR_FIRST"), nullptr);
    EXPECT_EQ(findItem(events, "RC_FIRST"), nullptr);

    const std::string locationUsage =
        "define use(): use_values(RR_FIRST, EVENT_FIRST, RC_";
    CrossFileCompletionFixture locationFixture(declarations, locationUsage);
    const auto locations = locationFixture.completeAtEnd(locationUsage);
    EXPECT_NE(findItem(locations, "RC_FIRST"), nullptr);
    EXPECT_NE(findItem(locations, "RC_SECOND"), nullptr);
    EXPECT_EQ(findItem(locations, "RR_FIRST"), nullptr);
    EXPECT_EQ(findItem(locations, "EVENT_FIRST"), nullptr);
}

} // namespace