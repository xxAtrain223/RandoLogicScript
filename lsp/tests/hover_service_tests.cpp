#include <chrono>
#include <filesystem>

#include <gtest/gtest.h>

#include "rls/lsp/document_store.h"
#include "rls/lsp/document_uri.h"
#include "rls/lsp/hover_service.h"

namespace fs = std::filesystem;

namespace {

using rls::lsp::AnalysisScheduler;
using rls::lsp::DocumentStore;
using rls::lsp::HoverService;
using rls::lsp::PresentationPosition;
using rls::lsp::ProjectManager;

struct HoverFixture {
    fs::path root = fs::temp_directory_path() / "rls-hover-service";
    fs::path declarationPath = root / "declarations.rls";
    fs::path usagePath = root / "usage.rls";
    std::string usageUri = *rls::lsp::PathToFileUri(usagePath);
    DocumentStore documents;
    ProjectManager projects;
    AnalysisScheduler scheduler;

    HoverFixture(std::string declarations, std::string usage)
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

    std::optional<rls::lsp::HoverResult> at(PresentationPosition position) {
        return HoverService(projects, scheduler).hover(usageUri, position);
    }
};

TEST(HoverServiceTests, RendersCallSignatureDefaultsProvenanceAndLocation) {
    const std::string declarations =
        "enum Color { RED }\n"
        "extern define paint(color: Color = RED) -> Bool\n";
    const std::string usage = "define use(input: Color): paint(input)\n";
    HoverFixture fixture(declarations, usage);

    const auto result = fixture.at({0, static_cast<uint32_t>(usage.find("paint") + 2)});

    ASSERT_TRUE(result);
    EXPECT_NE(result->markdown.find(
        "```rls\nextern paint(color: Color = RED) -> Bool\n```"),
        std::string::npos);
    EXPECT_NE(result->markdown.find("External function declaration."), std::string::npos);
    EXPECT_NE(result->markdown.find("*External declaration.*"), std::string::npos);
    EXPECT_NE(result->markdown.find("[Open declaration](file:"), std::string::npos);
    EXPECT_EQ(result->range.start.line, 0u);
    EXPECT_EQ(result->range.start.character, usage.find("paint"));
    EXPECT_EQ(result->range.end.character, usage.find("paint") + 5);
}

TEST(HoverServiceTests, SupportsParameterUsesAndEnumMemberExpressions) {
    const std::string declarations = "enum Color { RED }\n";
    const std::string usage =
        "define use(input: Color): input == Color.RED\n";
    HoverFixture fixture(declarations, usage);

    const auto parameter = fixture.at({
        0, static_cast<uint32_t>(usage.find("input ==") + 2)});
    ASSERT_TRUE(parameter);
    EXPECT_NE(parameter->markdown.find("input: Color"), std::string::npos);
    EXPECT_NE(parameter->markdown.find("Parameter of `use`."), std::string::npos);

    const auto enumType = fixture.at({
        0, static_cast<uint32_t>(usage.find("Color.RED") + 2)});
    ASSERT_TRUE(enumType);
    EXPECT_NE(enumType->markdown.find("enum Color"), std::string::npos);

    const auto member = fixture.at({
        0, static_cast<uint32_t>(usage.find("Color.RED") + 7)});
    ASSERT_TRUE(member);
    EXPECT_NE(member->markdown.find("RED: Color"), std::string::npos);
    EXPECT_NE(member->markdown.find("Member of enum `Color`."), std::string::npos);
}

TEST(HoverServiceTests, SupportsRegionsSectionEntriesAndTypedExpressions) {
    const std::string usage =
        "region RR_TEST {\n"
        "  name: \"Test\"\n"
        "  events { EVENT_READY: true }\n"
        "  locations { RC_CHEST: true }\n"
        "}\n"
        "define arithmetic(): 1 + 2\n";
    HoverFixture fixture({}, usage);

    const auto region = fixture.at({0, 9});
    ASSERT_TRUE(region);
    EXPECT_NE(region->markdown.find("region RR_TEST"), std::string::npos);
    EXPECT_NE(region->markdown.find("Region value."), std::string::npos);

    const auto event = fixture.at({2, 13});
    ASSERT_TRUE(event);
    EXPECT_NE(event->markdown.find("EVENT_READY: Event"), std::string::npos);
    EXPECT_NE(event->markdown.find("Declared event value."), std::string::npos);

    const auto location = fixture.at({3, 16});
    ASSERT_TRUE(location);
    EXPECT_NE(location->markdown.find("RC_CHEST: Location"), std::string::npos);

    const auto expression = fixture.at({5, 23});
    ASSERT_TRUE(expression);
    EXPECT_NE(expression->markdown.find("expression: Int"), std::string::npos);
    EXPECT_NE(expression->markdown.find("Inferred expression type."), std::string::npos);
}

TEST(HoverServiceTests, SupportsKnownRecoveredCallAndRejectsUnknownOrStaleData) {
    const std::string declarations =
        "extern define target(value: Bool) -> Bool\n";
    const std::string usage = "define use(): target(";
    HoverFixture recovered(declarations, usage);
    const auto known = recovered.at({0, 16});
    ASSERT_TRUE(known);
    EXPECT_NE(known->markdown.find("extern target(value: Bool) -> Bool"),
        std::string::npos);

    HoverFixture unresolved({}, "define use(): missing\n");
    EXPECT_FALSE(unresolved.at({0, 16}));

    HoverFixture stale(declarations, "define use(): target(true)\n");
    ASSERT_EQ(stale.projects.documentChanged(stale.usageUri),
        rls::lsp::ProjectAssignmentResult::Assigned);
    EXPECT_FALSE(stale.at({0, 16}));
}

} // namespace
