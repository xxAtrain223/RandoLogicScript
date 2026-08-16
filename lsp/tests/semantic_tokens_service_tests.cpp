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

    std::vector<DecodedToken> tokens() {
        return decode(SemanticTokensService(projects, scheduler).full(uri));
    }
};

TEST(SemanticTokensServiceTests, EncodesResolvedCategoriesAndModifiers) {
    SemanticTokensFixture fixture(
        "extern enum Color { RED }\n"
        "extern define paint(color: Color) -> Bool\n"
        "define use(input: Color): paint(input == RED)\n"
        "region RR_TEST { name: \"Test\" events { EVENT_TEST: true } exits { RR_EXIT: true } }\n"
        "extend region RR_TEST {}\n"
        "define event_value(): EVENT_TEST\n"
        "define region_value(): RR_TEST\n");

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
    const auto* exit = tokenAt(tokens, 3, 66);
    const auto* extensionTarget = tokenAt(tokens, 4, 14);
    const auto* entryUse = tokenAt(tokens, 5, 22);
    const auto* regionUse = tokenAt(tokens, 6, 23);

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
    EXPECT_EQ(region, nullptr);
    EXPECT_EQ(property, nullptr);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->type, 4u);
    EXPECT_EQ(entry->modifiers, 1u);
    ASSERT_NE(exit, nullptr);
    EXPECT_EQ(exit->type, 4u);
    EXPECT_EQ(exit->modifiers, 1u);
    ASSERT_NE(entryUse, nullptr);
    EXPECT_EQ(entryUse->type, 3u);
    EXPECT_EQ(entryUse->modifiers, 0u);
    ASSERT_NE(regionUse, nullptr);
    EXPECT_EQ(regionUse->type, 3u);
    EXPECT_EQ(regionUse->modifiers, 4u);
    ASSERT_NE(extensionTarget, nullptr);
    EXPECT_EQ(extensionTarget->type, 3u);
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

TEST(SemanticTokensServiceTests, EmitsLogicalOperatorsWithTheSameType) {
    const std::string source = "define check(): true or (true and true)\n";
    SemanticTokensFixture fixture(source);

    const auto tokens = fixture.tokens();
    const auto orPosition = static_cast<uint32_t>(source.find("or"));
    const auto andPosition = static_cast<uint32_t>(source.find("and"));

    const auto* orToken = tokenAt(tokens, 0, orPosition);
    const auto* andToken = tokenAt(tokens, 0, andPosition);

    ASSERT_NE(orToken, nullptr);
    ASSERT_NE(andToken, nullptr);
    EXPECT_EQ(orToken->type, andToken->type);
    EXPECT_EQ(orToken->type, 6u);
    EXPECT_EQ(orToken->length, 2u);
    EXPECT_EQ(andToken->length, 3u);
}

TEST(SemanticTokensServiceTests, HighlightsConcreteValuesResolvedThroughUniquePatterns) {
    SemanticTokensFixture fixture(
        "extern enum Item { RG_* }\n"
        "define use(): RG_HOOKSHOT == Item.RG_BOW\n");

    const auto tokens = fixture.tokens();
    const auto* pattern = tokenAt(tokens, 0, 19);
    const auto* bare = tokenAt(tokens, 1, 14);
    const auto* qualified = tokenAt(tokens, 1, 34);

    EXPECT_EQ(pattern, nullptr);
    ASSERT_NE(bare, nullptr);
    EXPECT_EQ(bare->type, 3u);
    EXPECT_EQ(bare->modifiers, 12u);
    ASSERT_NE(qualified, nullptr);
    EXPECT_EQ(qualified->type, 3u);
    EXPECT_EQ(qualified->modifiers, 12u);
}

TEST(SemanticTokensServiceTests, HighlightsWildcardValuesInExternParameterDefaults) {
    SemanticTokensFixture fixture(
        "extern enum Region { RR_* }\n"
        "extern define spirit_shared(value: Region = RR_NONE) -> Bool\n");

    const auto tokens = fixture.tokens();
    const auto* regionDefault = tokenAt(tokens, 1, 44);

    ASSERT_NE(regionDefault, nullptr);
    EXPECT_EQ(regionDefault->type, 3u);
    EXPECT_EQ(regionDefault->modifiers, 12u);
}

TEST(SemanticTokensServiceTests, HighlightsBuiltinExternReturnTypes) {
    SemanticTokensFixture fixture(
        "extern define test1(item: Item) -> Item\n"
        "extern define test2(bool: Bool) -> Bool\n");

    const auto tokens = fixture.tokens();
    const auto* item = tokenAt(tokens, 0, 35);
    const auto* boolean = tokenAt(tokens, 1, 35);

    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->type, 2u);
    EXPECT_EQ(item->modifiers, 8u);
    ASSERT_NE(boolean, nullptr);
    EXPECT_EQ(boolean->type, 2u);
    EXPECT_EQ(boolean->modifiers, 8u);
}

TEST(SemanticTokensServiceTests, ReturnsEmptyForMalformedOrStaleDocument) {
    SemanticTokensFixture malformed("define broken(");
    EXPECT_TRUE(malformed.tokens().empty());

    SemanticTokensFixture stale("define check(): true\n");
    ASSERT_EQ(stale.projects.documentChanged(stale.uri),
        rls::lsp::ProjectAssignmentResult::Assigned);
    EXPECT_TRUE(stale.tokens().empty());
}

TEST(SemanticTokensServiceTests, ExpeditesLatestScheduledGeneration) {
    const fs::path path = fs::temp_directory_path() /
        "rls-immediate-semantic-tokens.rls";
    const std::string uri = *rls::lsp::PathToFileUri(path);
    const std::string initial = "define check(flag: Bool): flag\n";
    DocumentStore documents;
    ASSERT_EQ(documents.open(uri, "rls", 1, initial),
        rls::lsp::DocumentUpdateResult::Applied);
    ProjectManager projects(documents, [&](const fs::path&) {
        rls::project::FileProject project;
        project.sourceFiles = {path};
        project.isStandalone = true;
        return project;
    });
    ASSERT_EQ(projects.documentOpened(uri),
        rls::lsp::ProjectAssignmentResult::Assigned);
    auto* project = projects.projectForDocument(uri);
    ASSERT_NE(project, nullptr);
    AnalysisScheduler scheduler({
        .debounce = std::chrono::seconds(5),
        .maximumConcurrency = 1,
    });
    ASSERT_TRUE(scheduler.schedule({
        project->id,
        project->generation,
        {{path, initial}},
        project->documentGeneration,
        project->manifestGeneration,
    }));
    ASSERT_NE(scheduler.awaitSnapshot(
        project->id, project->generation, std::chrono::seconds(1)), nullptr);

    const std::string changed = "define renamed(flag: Bool): flag\n";
    ASSERT_EQ(documents.applyFullChange(uri, 2, changed),
        rls::lsp::DocumentUpdateResult::Applied);
    ASSERT_EQ(projects.documentChanged(uri),
        rls::lsp::ProjectAssignmentResult::Assigned);
    project = projects.projectForDocument(uri);
    ASSERT_NE(project, nullptr);
    ASSERT_TRUE(scheduler.schedule({
        project->id,
        project->generation,
        {{path, changed}},
        project->documentGeneration,
        project->manifestGeneration,
    }));

    const auto tokens = decode(SemanticTokensService(projects, scheduler).full(uri));

    const auto* renamed = tokenAt(tokens, 0, 7);
    ASSERT_NE(renamed, nullptr);
    EXPECT_EQ(renamed->length, 7u);
    EXPECT_EQ(renamed->type, 0u);
}

} // namespace
