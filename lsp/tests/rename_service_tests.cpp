#include <chrono>
#include <filesystem>

#include <gtest/gtest.h>

#include "rls/lsp/document_store.h"
#include "rls/lsp/document_uri.h"
#include "rls/lsp/project_manager.h"
#include "rls/lsp/rename_service.h"

namespace fs = std::filesystem;

namespace {

using rls::lsp::AnalysisScheduler;
using rls::lsp::DocumentStore;
using rls::lsp::ProjectManager;
using rls::lsp::RenameError;
using rls::lsp::RenameService;

struct RenameFixture {
    fs::path root = fs::temp_directory_path() / "rls-rename-service";
    fs::path declarationPath = root / "declaration.rls";
    fs::path usagePath = root / "usage.rls";
    std::string declarationUri = *rls::lsp::PathToFileUri(declarationPath);
    std::string usageUri = *rls::lsp::PathToFileUri(usagePath);
    DocumentStore documents;
    ProjectManager projects;
    AnalysisScheduler scheduler;

    RenameFixture(std::string declaration, std::string usage)
        : projects(documents, [&](const fs::path&) {
              rls::project::FileProject project;
              project.sourceFiles = {declarationPath, usagePath};
              return project;
          }),
          scheduler({
              .debounce = std::chrono::milliseconds(0),
              .maximumConcurrency = 1,
          }) {
        EXPECT_EQ(documents.open(declarationUri, "rls", 7, declaration),
            rls::lsp::DocumentUpdateResult::Applied);
        EXPECT_EQ(documents.open(usageUri, "rls", 11, usage),
            rls::lsp::DocumentUpdateResult::Applied);
        EXPECT_EQ(projects.documentOpened(declarationUri),
            rls::lsp::ProjectAssignmentResult::Assigned);
        EXPECT_EQ(projects.documentOpened(usageUri),
            rls::lsp::ProjectAssignmentResult::Assigned);
        const auto* project = projects.projectForDocument(usageUri);
        EXPECT_NE(project, nullptr);
        if (!project) return;
        EXPECT_TRUE(scheduler.schedule({
            project->id,
            project->generation,
            {
                {declarationPath.generic_string(), std::move(declaration)},
                {usagePath.generic_string(), std::move(usage)},
            },
            project->documentGeneration,
            project->manifestGeneration,
        }));
        scheduler.waitForIdle();
    }
};

TEST(RenameServiceTests, ProducesVersionedCrossFileEdits) {
    RenameFixture fixture(
        "define target(): true\n",
        "define caller(): target() and target()\n");
    RenameService rename(fixture.documents, fixture.projects, fixture.scheduler);

    const auto prepared = rename.prepare(fixture.usageUri, {0, 18});
    ASSERT_TRUE(prepared.value);
    EXPECT_EQ(prepared.value->start.character, 17u);
    EXPECT_EQ(prepared.value->end.character, 23u);
    EXPECT_TRUE(rename.prepare(fixture.usageUri, {0, 23}).value);

    const auto result = rename.rename(fixture.usageUri, {0, 23}, "replacement", true);
    ASSERT_TRUE(result.value);
    ASSERT_EQ(result.value->documents.size(), 2u);
    EXPECT_EQ(result.value->documents[0].uri, fixture.declarationUri);
    EXPECT_EQ(result.value->documents[0].version, 7);
    ASSERT_EQ(result.value->documents[0].edits.size(), 1u);
    EXPECT_EQ(result.value->documents[1].uri, fixture.usageUri);
    EXPECT_EQ(result.value->documents[1].version, 11);
    ASSERT_EQ(result.value->documents[1].edits.size(), 2u);
    EXPECT_EQ(result.value->documents[1].edits[0].newText, "replacement");
}

TEST(RenameServiceTests, RenamesExternDefineAcrossCallsAndFunctionReferences) {
    RenameFixture fixture(
        "extern define host() -> Bool\n",
        "define caller(): host()\n"
        "define reference(): host\n"
        "define occupied(): true\n");
    RenameService rename(fixture.documents, fixture.projects, fixture.scheduler);

    ASSERT_TRUE(rename.prepare(fixture.declarationUri, {0, 18}).value);
    ASSERT_TRUE(rename.prepare(fixture.usageUri, {0, 21}).value);
    const auto result = rename.rename(
        fixture.usageUri, {0, 21}, "platform_host", true);
    ASSERT_TRUE(result.value);
    ASSERT_EQ(result.value->documents.size(), 2u);
    ASSERT_EQ(result.value->documents[0].edits.size(), 1u);
    EXPECT_EQ(result.value->documents[0].edits[0].range.start.line, 0u);
    EXPECT_EQ(result.value->documents[0].edits[0].range.start.character, 14u);
    ASSERT_EQ(result.value->documents[1].edits.size(), 2u);
    EXPECT_EQ(result.value->documents[1].edits[0].range.start.line, 0u);
    EXPECT_EQ(result.value->documents[1].edits[1].range.start.line, 1u);
    EXPECT_EQ(rename.rename(
        fixture.usageUri, {0, 21}, "occupied", true).error,
        RenameError::Collision);
}

TEST(RenameServiceTests, RenamesExternEnumTypeAndExplicitMembers) {
    RenameFixture fixture(
        "extern enum Color { RED, BLUE, COLOR_* }\n"
        "extern define choose(color: Color) -> Bool\n"
        "enum Other { VALUE }\n",
        "define use(): choose(Color.RED) and choose(RED)\n");
    RenameService rename(fixture.documents, fixture.projects, fixture.scheduler);

    ASSERT_TRUE(rename.prepare(fixture.declarationUri, {0, 14}).value);
    ASSERT_TRUE(rename.prepare(fixture.usageUri, {0, 23}).value);
    const auto typeResult = rename.rename(
        fixture.usageUri, {0, 23}, "Palette", true);
    ASSERT_TRUE(typeResult.value);
    ASSERT_EQ(typeResult.value->documents.size(), 2u);
    ASSERT_EQ(typeResult.value->documents[0].edits.size(), 2u);
    ASSERT_EQ(typeResult.value->documents[1].edits.size(), 1u);

    ASSERT_TRUE(rename.prepare(fixture.declarationUri, {0, 21}).value);
    ASSERT_TRUE(rename.prepare(fixture.usageUri, {0, 28}).value);
    const auto memberResult = rename.rename(
        fixture.usageUri, {0, 28}, "CRIMSON", true);
    ASSERT_TRUE(memberResult.value);
    ASSERT_EQ(memberResult.value->documents.size(), 2u);
    ASSERT_EQ(memberResult.value->documents[0].edits.size(), 1u);
    ASSERT_EQ(memberResult.value->documents[1].edits.size(), 2u);

    EXPECT_EQ(rename.rename(
        fixture.usageUri, {0, 28}, "BLUE", true).error,
        RenameError::Collision);
    EXPECT_EQ(rename.rename(
        fixture.usageUri, {0, 28}, "COLOR_NEW", true).error,
        RenameError::Collision);
    EXPECT_EQ(rename.rename(
        fixture.usageUri, {0, 23}, "Other", true).error,
        RenameError::Collision);
    EXPECT_EQ(rename.prepare(fixture.declarationUri, {0, 34}).error,
        RenameError::NotRenameable);
}

TEST(RenameServiceTests, RenamesRegionExitKeysAcrossBaseAndExtension) {
    RenameFixture fixture(
        "region RR_TARGET { name: \"Target\" }\n"
        "region RR_SOURCE { name: \"Source\" exits { RR_TARGET: true } }\n",
        "extend region RR_SOURCE { exits { RR_TARGET: true } }\n");
    RenameService rename(fixture.documents, fixture.projects, fixture.scheduler);

    const auto result = rename.rename(
        fixture.declarationUri, {0, 8}, "RR_RENAMED", true);
    ASSERT_TRUE(result.value);
    ASSERT_EQ(result.value->documents.size(), 2u);
    ASSERT_EQ(result.value->documents[0].edits.size(), 2u);
    EXPECT_EQ(result.value->documents[0].edits[0].range.start.line, 0u);
    EXPECT_EQ(result.value->documents[0].edits[1].range.start.line, 1u);
    ASSERT_EQ(result.value->documents[1].edits.size(), 1u);
    EXPECT_EQ(result.value->documents[1].edits[0].range.start.line, 0u);
}

TEST(RenameServiceTests, RenamesOnlyExactPatternBackedExitValue) {
    RenameFixture fixture(
        "extern enum Region { RR_* }\n"
        "region RR_LOCAL { exits { RR_EXTERNAL: true RR_OTHER: true } }\n",
        "region RR_SECOND { exits { RR_EXTERNAL: true } }\n"
        "define region_value(): RR_EXTERNAL\n");
    RenameService rename(fixture.documents, fixture.projects, fixture.scheduler);

    ASSERT_TRUE(rename.prepare(fixture.declarationUri, {1, 28}).value);
    ASSERT_TRUE(rename.prepare(fixture.declarationUri, {1, 37}).value);
    const auto result = rename.rename(
        fixture.declarationUri, {1, 37}, "RR_RENAMED", true);
    ASSERT_TRUE(result.value);
    ASSERT_EQ(result.value->documents.size(), 2u);
    ASSERT_EQ(result.value->documents[0].edits.size(), 1u);
    EXPECT_EQ(result.value->documents[0].edits[0].range.start.line, 1u);
    ASSERT_EQ(result.value->documents[1].edits.size(), 2u);
    EXPECT_EQ(result.value->documents[1].edits[0].range.start.line, 0u);
    EXPECT_EQ(result.value->documents[1].edits[1].range.start.line, 1u);

    EXPECT_EQ(rename.rename(
        fixture.declarationUri, {1, 28}, "EXTERNAL", true).error,
        RenameError::InvalidName);
    EXPECT_EQ(rename.rename(
        fixture.declarationUri, {1, 28}, "RR_OTHER", true).error,
        RenameError::Collision);
}

TEST(RenameServiceTests, RenamesEventDeclarationsAcrossRegionsAndExpressions) {
    RenameFixture fixture(
        "region RR_FIRST { events { EVENT_SHARED: true } }\n"
        "region RR_SECOND { events { EVENT_SHARED: true } }\n",
        "region RR_THIRD { events { EVENT_SHARED: true } }\n"
        "define event_value(): EVENT_SHARED\n");
    RenameService rename(fixture.documents, fixture.projects, fixture.scheduler);

    const auto result = rename.rename(
        fixture.declarationUri, {0, 29}, "EVENT_RENAMED", true);
    ASSERT_TRUE(result.value);
    ASSERT_EQ(result.value->documents.size(), 2u);
    ASSERT_EQ(result.value->documents[0].edits.size(), 2u);
    EXPECT_EQ(result.value->documents[0].edits[0].range.start.line, 0u);
    EXPECT_EQ(result.value->documents[0].edits[1].range.start.line, 1u);
    ASSERT_EQ(result.value->documents[1].edits.size(), 2u);
    EXPECT_EQ(result.value->documents[1].edits[0].range.start.line, 0u);
    EXPECT_EQ(result.value->documents[1].edits[1].range.start.line, 1u);
}

TEST(RenameServiceTests, RenamesRegionDataKeysAcrossAllRegions) {
    RenameFixture fixture(
        "region RR_FIRST { worldNode: \"First\" scene: SCENE_FIRST }\n"
        "region RR_SECOND { worldNode: \"Second\" scene: SCENE_SECOND }\n",
        "region RR_THIRD { worldNode: \"Third\" scene: SCENE_THIRD }\n");
    RenameService rename(fixture.documents, fixture.projects, fixture.scheduler);

    ASSERT_TRUE(rename.prepare(fixture.declarationUri, {0, 20}).value);
    ASSERT_TRUE(rename.prepare(fixture.declarationUri, {0, 27}).value);
    const auto result = rename.rename(
        fixture.declarationUri, {0, 27}, "graphNode", true);
    ASSERT_TRUE(result.value);
    ASSERT_EQ(result.value->documents.size(), 2u);
    ASSERT_EQ(result.value->documents[0].edits.size(), 2u);
    EXPECT_EQ(result.value->documents[0].edits[0].range.start.line, 0u);
    EXPECT_EQ(result.value->documents[0].edits[1].range.start.line, 1u);
    ASSERT_EQ(result.value->documents[1].edits.size(), 1u);
    EXPECT_EQ(result.value->documents[1].edits[0].range.start.line, 0u);

    EXPECT_EQ(rename.rename(
        fixture.declarationUri, {0, 20}, "scene", true).error,
        RenameError::Collision);
}

TEST(RenameServiceTests, KeepsSameNamedParametersInTheirDefineScope) {
    RenameFixture fixture(
        "define first(value: Bool): value\n",
        "define second(value: Bool): value\n"
        "define caller(): first(value: true)\n");
    RenameService rename(fixture.documents, fixture.projects, fixture.scheduler);

    const auto result = rename.rename(fixture.declarationUri, {0, 14}, "item", true);
    ASSERT_TRUE(result.value);
    ASSERT_EQ(result.value->documents.size(), 2u);
    ASSERT_EQ(result.value->documents[0].edits.size(), 2u);
    EXPECT_EQ(result.value->documents[0].uri, fixture.declarationUri);
    ASSERT_EQ(result.value->documents[1].edits.size(), 1u);
    EXPECT_EQ(result.value->documents[1].uri, fixture.usageUri);
    EXPECT_EQ(result.value->documents[1].edits[0].range.start.line, 1u);
    EXPECT_EQ(result.value->documents[1].edits[0].range.start.character, 23u);
}

TEST(RenameServiceTests, RejectsReservedCollisionAndUnsupportedClient) {
    RenameFixture fixture(
        "extern define host() -> Bool\n",
        "define existing(): true\ndefine caller(): host()\n");
    RenameService rename(fixture.documents, fixture.projects, fixture.scheduler);

    EXPECT_TRUE(rename.prepare(fixture.usageUri, {1, 18}).value);
    EXPECT_EQ(rename.rename(fixture.usageUri, {0, 8}, "region", true).error,
        RenameError::InvalidName);
    EXPECT_EQ(rename.rename(fixture.usageUri, {0, 8}, "caller", true).error,
        RenameError::Collision);
    EXPECT_EQ(rename.rename(fixture.usageUri, {0, 8}, "renamed", false).error,
        RenameError::UnsupportedClient);
}

TEST(RenameServiceTests, RejectsStaleSnapshot) {
    RenameFixture fixture(
        "define target(): true\n",
        "define caller(): target()\n");
    RenameService rename(fixture.documents, fixture.projects, fixture.scheduler);
    ASSERT_EQ(fixture.projects.documentChanged(fixture.usageUri),
        rls::lsp::ProjectAssignmentResult::Assigned);

    EXPECT_EQ(rename.prepare(fixture.usageUri, {0, 18}).error, RenameError::StaleSnapshot);
    EXPECT_EQ(rename.rename(fixture.usageUri, {0, 18}, "other", true).error,
        RenameError::StaleSnapshot);
}

} // namespace