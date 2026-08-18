#include <chrono>
#include <filesystem>

#include <gtest/gtest.h>

#include "rls/lsp/document_store.h"
#include "rls/lsp/document_uri.h"
#include "rls/lsp/signature_help_service.h"

namespace fs = std::filesystem;

namespace {

using rls::lsp::AnalysisScheduler;
using rls::lsp::DocumentStore;
using rls::lsp::PresentationPosition;
using rls::lsp::ProjectManager;
using rls::lsp::SignatureHelpService;

struct SignatureFixture {
    fs::path root = fs::temp_directory_path() / "rls-signature-help";
    fs::path declarationPath = root / "declarations.rls";
    fs::path usagePath = root / "usage.rls";
    std::string usageUri = *rls::lsp::PathToFileUri(usagePath);
    DocumentStore documents;
    ProjectManager projects;
    AnalysisScheduler scheduler;

    SignatureFixture(std::string declarations, std::string usage)
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
                {declarationPath.generic_string(), std::move(declarations)},
                {usagePath.generic_string(), std::move(usage)},
            },
            project->documentGeneration,
            project->manifestGeneration,
        }));
        scheduler.waitForIdle();
    }

    std::optional<rls::lsp::SignatureHelpResult> at(PresentationPosition position) {
        return SignatureHelpService(projects, scheduler).signatureHelp(usageUri, position);
    }
};

TEST(SignatureHelpServiceTests, RendersExternSignatureAndNamedActiveParameter) {
    const std::string declarations =
        "enum Color { RED }\n"
        "extern define paint(color: Color, enabled: Bool = true) -> Bool\n";
    const std::string usage = "define use(): paint(enabled: false, RED)\n";
    SignatureFixture fixture(declarations, usage);

    const auto enabled = fixture.at({0, static_cast<uint32_t>(usage.find("false") + 2)});
    ASSERT_TRUE(enabled);
    EXPECT_EQ(enabled->label,
        "extern paint(color: Color, enabled: Bool = true) -> Bool");
    EXPECT_EQ(enabled->documentation, "*External declaration.*");
    EXPECT_EQ(enabled->parameterLabels,
        (std::vector<std::string>{"color: Color", "enabled: Bool = true"}));
    EXPECT_EQ(enabled->activeParameter, 1u);

    const auto color = fixture.at({0, static_cast<uint32_t>(usage.find("RED") + 1)});
    ASSERT_TRUE(color);
    EXPECT_EQ(color->activeParameter, 0u);
}

TEST(SignatureHelpServiceTests, SupportsKnownRecoveredIncompleteCall) {
    const std::string declarations =
        "enum Color { RED }\n"
        "extern define paint(color: Color) -> Bool\n";
    const std::string usage = "define use(): paint(R";
    SignatureFixture fixture(declarations, usage);

    const auto result = fixture.at({0, static_cast<uint32_t>(usage.size())});

    ASSERT_TRUE(result);
    EXPECT_EQ(result->label, "extern paint(color: Color) -> Bool");
    EXPECT_EQ(result->activeParameter, 0u);
}

TEST(SignatureHelpServiceTests, AdvancesAfterTrailingComma) {
    const std::string declarations =
        "extern define target(first: Bool, second: Bool) -> Bool\n";
    const std::string usage = "define use(): target(true,)";
    SignatureFixture fixture(declarations, usage);

    const auto result = fixture.at({
        0, static_cast<uint32_t>(usage.find(',') + 1)});

    ASSERT_TRUE(result);
    EXPECT_EQ(result->activeParameter, 1u);
}

TEST(SignatureHelpServiceTests, RendersUserDefineInferredReturnType) {
    const std::string declarations =
        "enum Color { RED }\n"
        "define choose(color: Color = RED): color\n";
    const std::string usage = "define use(): choose(R";
    SignatureFixture fixture(declarations, usage);

    const auto result = fixture.at({0, static_cast<uint32_t>(usage.size())});

    ASSERT_TRUE(result);
    EXPECT_EQ(result->label, "choose(color: Color = RED) -> Color");
    EXPECT_TRUE(result->documentation.empty());
    EXPECT_EQ(result->activeParameter, 0u);
}

TEST(SignatureHelpServiceTests, SuppressesUnresolvedAndAmbiguousCalls) {
    const std::string unresolvedUsage = "define use(): missing(R";
    SignatureFixture unresolved("enum Color { RED }\n", unresolvedUsage);
    EXPECT_FALSE(unresolved.at({0, static_cast<uint32_t>(unresolvedUsage.size())}));

    const std::string declarations =
        "extern define paint(value: Bool) -> Bool\n"
        "extern define paint(value: Int) -> Bool\n";
    const std::string ambiguousUsage = "define use(): paint(R";
    SignatureFixture ambiguous(declarations, ambiguousUsage);
    EXPECT_FALSE(ambiguous.at({0, static_cast<uint32_t>(ambiguousUsage.size())}));
}

TEST(SignatureHelpServiceTests, RejectsStaleAcceptedSnapshot) {
    const std::string usage = "define use(): target(t";
    SignatureFixture fixture(
        "extern define target(value: Bool) -> Bool\n", usage);
    ASSERT_EQ(fixture.projects.documentChanged(fixture.usageUri),
        rls::lsp::ProjectAssignmentResult::Assigned);

    EXPECT_FALSE(fixture.at({0, static_cast<uint32_t>(usage.size())}));
}

} // namespace
