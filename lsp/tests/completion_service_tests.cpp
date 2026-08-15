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

TEST(CompletionServiceTests, CompletesRecoveredRegionBodyWithoutDuplicates) {
    CompletionFixture fixture(
        "region RR_TEST {\n"
        "  name: \"Test\"\n"
        "  events {}\n"
        "  loc\n");

    const auto items = CompletionService(fixture.projects, fixture.scheduler)
        .complete(fixture.uri, {3, 5});

    ASSERT_NE(findItem(items, "locations"), nullptr);
    ASSERT_NE(findItem(items, "scene"), nullptr);
    ASSERT_NE(findItem(items, "areas"), nullptr);
    EXPECT_EQ(findItem(items, "name"), nullptr);
    EXPECT_EQ(findItem(items, "events"), nullptr);
    EXPECT_EQ(findItem(items, "define"), nullptr);
    EXPECT_EQ(items.front().label, "locations");
    EXPECT_EQ(items.front().replacementRange.start.character, 2u);
    EXPECT_EQ(items.front().replacementRange.end.character, 5u);
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

} // namespace