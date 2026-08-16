#include <algorithm>
#include <chrono>
#include <filesystem>

#include <gtest/gtest.h>

#include "rls/lsp/document_store.h"
#include "rls/lsp/document_uri.h"
#include "rls/lsp/semantic_tokens_service.h"

namespace fs = std::filesystem;

namespace {

using rls::lsp::AnalysisScheduler;
using rls::lsp::DocumentStore;
using rls::lsp::ProjectManager;
using rls::lsp::SemanticTokensService;

struct DecodedToken {
    uint32_t line;
    uint32_t character;
    uint32_t length;
    uint32_t type;
    uint32_t modifiers;
};

std::vector<DecodedToken> decode(const std::vector<uint32_t>& data) {
    EXPECT_EQ(data.size() % 5, 0u);
    std::vector<DecodedToken> result;
    uint32_t line = 0;
    uint32_t character = 0;
    for (size_t index = 0; index + 4 < data.size(); index += 5) {
        line += data[index];
        character = data[index] == 0 ? character + data[index + 1] : data[index + 1];
        result.push_back({line, character, data[index + 2], data[index + 3], data[index + 4]});
    }
    return result;
}

const DecodedToken* tokenAt(
    const std::vector<DecodedToken>& tokens, uint32_t line, uint32_t character) {
    const auto found = std::find_if(tokens.begin(), tokens.end(), [&](const auto& token) {
        return token.line == line && token.character == character;
    });
    return found == tokens.end() ? nullptr : &*found;
}

struct SemanticTokensFixture {
    fs::path path = fs::temp_directory_path() / "rls-semantic-tokens.rls";
    std::string uri = *rls::lsp::PathToFileUri(path);
    DocumentStore documents;
    ProjectManager projects;
    AnalysisScheduler scheduler;

    explicit SemanticTokensFixture(std::string source)
        : projects(documents, [&](const fs::path&) {
              rls::project::FileProject project;
              project.sourceFiles = {path};
              project.isStandalone = true;
              return project;
          }),
          scheduler({
              .debounce = std::chrono::milliseconds(0),
              .maximumConcurrency = 1,
          }) {
        EXPECT_EQ(documents.open(uri, "rls", 1, source),
            rls::lsp::DocumentUpdateResult::Applied);
        EXPECT_EQ(projects.documentOpened(uri),
            rls::lsp::ProjectAssignmentResult::Assigned);
        const auto* project = projects.projectForDocument(uri);
        EXPECT_NE(project, nullptr);
        if (!project) return;
        EXPECT_TRUE(scheduler.schedule({
            project->id,
            project->generation,
            {{path, std::move(source)}},
            project->documentGeneration,
            project->manifestGeneration,
        }));
        scheduler.waitForIdle();
    }

    std::vector<DecodedToken> tokens() const {
        return decode(SemanticTokensService(projects, scheduler).full(uri));
    }
};

TEST(SemanticTokensServiceTests, EncodesResolvedCategoriesAndModifiers) {
    SemanticTokensFixture fixture(
        "extern enum Color { RED }\n"
        "extern define paint(color: Color) -> Bool\n"
        "define use(input: Color): paint(input == RED)\n"
        "region RR_TEST { name: \"Test\" events { EVENT_TEST: true } }\n"
        "extend region RR_TEST {}\n");

    const auto tokens = fixture.tokens();
    const auto* externEnum = tokenAt(tokens, 0, 12);
    const auto* enumMember = tokenAt(tokens, 0, 20);
    const auto* externFunction = tokenAt(tokens, 1, 14);
    const auto* define = tokenAt(tokens, 2, 7);
    const auto* parameter = tokenAt(tokens, 2, 11);
    const auto* call = tokenAt(tokens, 2, 26);
    const auto* parameterUse = tokenAt(tokens, 2, 32);
    const auto* memberUse = tokenAt(tokens, 2, 41);
    const auto* region = tokenAt(tokens, 3, 7);
    const auto* property = tokenAt(tokens, 3, 17);
    const auto* entry = tokenAt(tokens, 3, 39);
    const auto* extensionTarget = tokenAt(tokens, 4, 14);

    ASSERT_NE(externEnum, nullptr);
    EXPECT_EQ(externEnum->type, 2u);
    EXPECT_EQ(externEnum->modifiers, 9u);
    ASSERT_NE(enumMember, nullptr);
    EXPECT_EQ(enumMember->type, 3u);
    EXPECT_EQ(enumMember->modifiers, 13u);
    ASSERT_NE(externFunction, nullptr);
    EXPECT_EQ(externFunction->type, 0u);
    EXPECT_EQ(externFunction->modifiers, 9u);
    ASSERT_NE(define, nullptr);
    EXPECT_EQ(define->type, 0u);
    EXPECT_EQ(define->modifiers, 2u);
    ASSERT_NE(parameter, nullptr);
    EXPECT_EQ(parameter->type, 1u);
    EXPECT_EQ(parameter->modifiers, 1u);
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->type, 0u);
    EXPECT_EQ(call->modifiers, 8u);
    ASSERT_NE(parameterUse, nullptr);
    EXPECT_EQ(parameterUse->type, 1u);
    EXPECT_EQ(parameterUse->modifiers, 0u);
    ASSERT_NE(memberUse, nullptr);
    EXPECT_EQ(memberUse->type, 3u);
    EXPECT_EQ(memberUse->modifiers, 12u);
    ASSERT_NE(region, nullptr);
    EXPECT_EQ(region->type, 5u);
    EXPECT_EQ(region->modifiers, 6u);
    ASSERT_NE(property, nullptr);
    EXPECT_EQ(property->type, 4u);
    EXPECT_EQ(property->modifiers, 1u);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->type, 5u);
    EXPECT_EQ(entry->modifiers, 5u);
    ASSERT_NE(extensionTarget, nullptr);
    EXPECT_EQ(extensionTarget->type, 5u);
    EXPECT_EQ(extensionTarget->modifiers, 4u);
}

TEST(SemanticTokensServiceTests, UsesUtf16ColumnsAndOmitsUnresolvedNames) {
    SemanticTokensFixture utf16(
        "define check(value: Bool): \"😀\" == value\n");
    const auto utf16Tokens = utf16.tokens();
    const auto* parameterUse = tokenAt(utf16Tokens, 0, 35);
    ASSERT_NE(parameterUse, nullptr);
    EXPECT_EQ(parameterUse->type, 1u);
    EXPECT_EQ(parameterUse->length, 5u);

    SemanticTokensFixture ambiguous(
        "enum Alpha { SHARED }\n"
        "enum Beta { SHARED }\n"
        "define use(): SHARED\n");
    const auto ambiguousTokens = ambiguous.tokens();
    EXPECT_EQ(tokenAt(ambiguousTokens, 2, 14), nullptr);
}

TEST(SemanticTokensServiceTests, ReturnsEmptyForMalformedOrStaleDocument) {
    SemanticTokensFixture malformed("define broken(");
    EXPECT_TRUE(malformed.tokens().empty());

    SemanticTokensFixture stale("define check(): true\n");
    ASSERT_EQ(stale.projects.documentChanged(stale.uri),
        rls::lsp::ProjectAssignmentResult::Assigned);
    EXPECT_TRUE(stale.tokens().empty());
}

} // namespace
