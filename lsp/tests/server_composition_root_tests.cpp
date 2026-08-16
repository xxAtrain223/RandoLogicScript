#include <chrono>
#include <filesystem>
#include <optional>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rls/lsp/server_composition_root.h"
#include "rls/lsp/document_uri.h"

namespace fs = std::filesystem;

namespace {

using Json = nlohmann::json;
using rls::lsp::ServerCompositionRoot;

rls::project::FileProject standaloneProject(const std::filesystem::path& path) {
    rls::project::FileProject project;
    project.sourceFiles.push_back(path.lexically_normal());
    project.isStandalone = true;
    return project;
}

TEST(ServerCompositionRootTests, RegistersOnlyImplementedRoutes) {
    ServerCompositionRoot server;

    EXPECT_TRUE(server.router().contains("initialize"));
    EXPECT_TRUE(server.router().contains("textDocument/didOpen"));
    EXPECT_TRUE(server.router().contains("workspace/didChangeWorkspaceFolders"));
    EXPECT_TRUE(server.router().contains("workspace/didChangeWatchedFiles"));
    EXPECT_TRUE(server.router().contains("textDocument/definition"));
    EXPECT_TRUE(server.router().contains("textDocument/references"));
    EXPECT_TRUE(server.router().contains("textDocument/documentHighlight"));
    EXPECT_TRUE(server.router().contains("textDocument/documentSymbol"));
    EXPECT_TRUE(server.router().contains("textDocument/completion"));
    EXPECT_TRUE(server.router().contains("textDocument/signatureHelp"));
    EXPECT_TRUE(server.router().contains("textDocument/hover"));
    EXPECT_TRUE(server.router().contains("textDocument/semanticTokens/full"));
    EXPECT_TRUE(server.router().contains("workspace/symbol"));
    EXPECT_FALSE(server.router().contains("textDocument/publishDiagnostics"));
}

TEST(ServerCompositionRootTests, AdvertisesImplementedTextDocumentFeatures) {
    ServerCompositionRoot server;
    const auto responses = server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");

    ASSERT_EQ(responses.size(), 1);
    const auto result = Json::parse(responses.front())["result"];
    EXPECT_EQ(result["capabilities"]["textDocumentSync"]["change"], 1);
    EXPECT_TRUE(result["capabilities"]["workspace"]["workspaceFolders"]["supported"]);
    EXPECT_EQ(result["capabilities"]["definitionProvider"], true);
    EXPECT_EQ(result["capabilities"]["referencesProvider"], true);
    EXPECT_EQ(result["capabilities"]["documentHighlightProvider"], true);
    EXPECT_EQ(result["capabilities"]["documentSymbolProvider"], true);
    EXPECT_EQ(result["capabilities"]["completionProvider"]["resolveProvider"], false);
    EXPECT_EQ(result["capabilities"]["signatureHelpProvider"]["triggerCharacters"],
        Json::array({"(", ","}));
    EXPECT_EQ(result["capabilities"]["hoverProvider"], true);
    EXPECT_EQ(result["capabilities"]["semanticTokensProvider"]["legend"]["tokenTypes"],
        Json::array({"function", "parameter", "enum", "enumMember", "property", "variable"}));
    EXPECT_EQ(result["capabilities"]["semanticTokensProvider"]["legend"]["tokenModifiers"],
        Json::array({"declaration", "definition", "readonly", "defaultLibrary", "deprecated"}));
    EXPECT_EQ(result["capabilities"]["semanticTokensProvider"]["range"], false);
    EXPECT_EQ(result["capabilities"]["semanticTokensProvider"]["full"], true);
    EXPECT_EQ(result["capabilities"]["workspaceSymbolProvider"], true);
}

TEST(ServerCompositionRootTests, RoutesCompletionWithActiveTokenTextEdit) {
    const fs::path sourcePath = fs::temp_directory_path() / "rls-completion-route.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", uri},
            {"languageId", "rls"},
            {"version", 1},
            {"text", "def\n"},
        }}}},
    }.dump());
    server.scheduler().waitForIdle();

    const auto responses = server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/completion"},
        {"params", {
            {"textDocument", {{"uri", uri}}},
            {"position", {{"line", 0}, {"character", 3}}},
        }},
    }.dump());

    ASSERT_EQ(responses.size(), 1u);
    const auto result = Json::parse(responses.front())["result"];
    ASSERT_FALSE(result.empty());
    EXPECT_EQ(result[0]["label"], "define");
    EXPECT_EQ(result[0]["kind"], 14);
    EXPECT_EQ(result[0]["textEdit"]["newText"], "define");
    EXPECT_EQ(result[0]["insertTextFormat"], 1);
    EXPECT_EQ(result[0]["textEdit"]["range"]["start"]["character"], 0);
    EXPECT_EQ(result[0]["textEdit"]["range"]["end"]["character"], 3);
}

TEST(ServerCompositionRootTests, RoutesCompletionImmediatelyAfterDocumentChange) {
    const fs::path sourcePath = fs::temp_directory_path() /
        "rls-immediate-completion-route.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", uri},
            {"languageId", "rls"},
            {"version", 1},
            {"text", "def\n"},
        }}}},
    }.dump());
    server.scheduler().waitForIdle();

    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didChange"},
        {"params", {
            {"textDocument", {{"uri", uri}, {"version", 2}}},
            {"contentChanges", Json::array({{{"text", "reg\n"}}})},
        }},
    }.dump());
    const auto responses = server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/completion"},
        {"params", {
            {"textDocument", {{"uri", uri}}},
            {"position", {{"line", 0}, {"character", 3}}},
        }},
    }.dump());

    ASSERT_EQ(responses.size(), 1u);
    const auto result = Json::parse(responses.front())["result"];
    ASSERT_FALSE(result.empty());
    EXPECT_EQ(result[0]["label"], "region");
}

TEST(ServerCompositionRootTests, RoutesSignatureHelpWithActiveNamedParameter) {
    const fs::path sourcePath = fs::temp_directory_path() / "rls-signature-route.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    const std::string usage = "define use(): paint(enabled: false, true)\n";
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", uri},
            {"languageId", "rls"},
            {"version", 1},
            {"text",
                "extern define paint(color: Bool, enabled: Bool = true) -> Bool\n"
                + usage},
        }}}},
    }.dump());
    server.scheduler().waitForIdle();

    const auto responses = server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/signatureHelp"},
        {"params", {
            {"textDocument", {{"uri", uri}}},
            {"position", {
                {"line", 1},
                {"character", usage.find("false") + 2},
            }},
        }},
    }.dump());

    ASSERT_EQ(responses.size(), 1u);
    const auto result = Json::parse(responses.front())["result"];
    ASSERT_EQ(result["signatures"].size(), 1u);
    EXPECT_EQ(result["activeSignature"], 0);
    EXPECT_EQ(result["activeParameter"], 1);
    EXPECT_EQ(result["signatures"][0]["activeParameter"], 1);
    EXPECT_EQ(result["signatures"][0]["label"],
        "extern paint(color: Bool, enabled: Bool = true) -> Bool");
    EXPECT_EQ(result["signatures"][0]["parameters"][1]["label"],
        "enabled: Bool = true");
    EXPECT_EQ(result["signatures"][0]["documentation"]["kind"], "markdown");
}

TEST(ServerCompositionRootTests, AdvancesSignatureImmediatelyAfterCommaEdit) {
    const fs::path sourcePath = fs::temp_directory_path() /
        "rls-signature-comma-route.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    const std::string before =
        "extern define target(first: Bool, second: Bool) -> Bool\n"
        "define use(): target(true)\n";
    const std::string after =
        "extern define target(first: Bool, second: Bool) -> Bool\n"
        "define use(): target(true,)\n";
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", uri},
            {"languageId", "rls"},
            {"version", 1},
            {"text", before},
        }}}},
    }.dump());
    server.scheduler().waitForIdle();

    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didChange"},
        {"params", {
            {"textDocument", {{"uri", uri}, {"version", 2}}},
            {"contentChanges", Json::array({{{"text", after}}})},
        }},
    }.dump());
    const auto responses = server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/signatureHelp"},
        {"params", {
            {"textDocument", {{"uri", uri}}},
            {"position", {{"line", 1}, {"character", 26}}},
            {"context", {
                {"triggerKind", 2},
                {"triggerCharacter", ","},
                {"isRetrigger", true},
            }},
        }},
    }.dump());

    ASSERT_EQ(responses.size(), 1u);
    const auto result = Json::parse(responses.front())["result"];
    ASSERT_FALSE(result.is_null());
    EXPECT_EQ(result["activeParameter"], 1);
    EXPECT_EQ(result["signatures"][0]["activeParameter"], 1);
}

TEST(ServerCompositionRootTests, ReturnsNullSignatureHelpForUnresolvedCall) {
    const fs::path sourcePath = fs::temp_directory_path() /
        "rls-unresolved-signature-route.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    const std::string source = "define use(): missing(R";
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", uri},
            {"languageId", "rls"},
            {"version", 1},
            {"text", source},
        }}}},
    }.dump());
    server.scheduler().waitForIdle();

    const auto responses = server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/signatureHelp"},
        {"params", {
            {"textDocument", {{"uri", uri}}},
            {"position", {{"line", 0}, {"character", source.size()}}},
        }},
    }.dump());

    ASSERT_EQ(responses.size(), 1u);
    EXPECT_TRUE(Json::parse(responses.front())["result"].is_null());
}

TEST(ServerCompositionRootTests, RoutesHoverWithMarkdownAndRange) {
    const fs::path sourcePath = fs::temp_directory_path() / "rls-hover-route.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    const std::string source =
        "extern define target(value: Bool = true) -> Bool\n"
        "define use(): target(false)\n";
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", uri},
            {"languageId", "rls"},
            {"version", 1},
            {"text", source},
        }}}},
    }.dump());
    server.scheduler().waitForIdle();

    const auto responses = server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/hover"},
        {"params", {
            {"textDocument", {{"uri", uri}}},
            {"position", {{"line", 1}, {"character", 16}}},
        }},
    }.dump());

    ASSERT_EQ(responses.size(), 1u);
    const auto result = Json::parse(responses.front())["result"];
    EXPECT_EQ(result["contents"]["kind"], "markdown");
    EXPECT_NE(result["contents"]["value"].get<std::string>().find(
        "extern target(value: Bool = true) -> Bool"), std::string::npos);
    EXPECT_EQ(result["range"]["start"]["line"], 1);
    EXPECT_EQ(result["range"]["start"]["character"], 14);
    EXPECT_EQ(result["range"]["end"]["character"], 20);
}

TEST(ServerCompositionRootTests, ReturnsNullHoverForUnresolvedName) {
    const fs::path sourcePath = fs::temp_directory_path() /
        "rls-unresolved-hover-route.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", uri},
            {"languageId", "rls"},
            {"version", 1},
            {"text", "define use(): missing\n"},
        }}}},
    }.dump());
    server.scheduler().waitForIdle();

    const auto responses = server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/hover"},
        {"params", {
            {"textDocument", {{"uri", uri}}},
            {"position", {{"line", 0}, {"character", 16}}},
        }},
    }.dump());

    ASSERT_EQ(responses.size(), 1u);
    EXPECT_TRUE(Json::parse(responses.front())["result"].is_null());
}

TEST(ServerCompositionRootTests, RoutesFullSemanticTokensAsDeltaEncodedData) {
    const fs::path sourcePath = fs::temp_directory_path() /
        "rls-semantic-token-route.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", uri},
            {"languageId", "rls"},
            {"version", 1},
            {"text", "define check(flag: Bool): flag\n"},
        }}}},
    }.dump());
    server.scheduler().waitForIdle();

    const auto responses = server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/semanticTokens/full"},
        {"params", {{"textDocument", {{"uri", uri}}}}},
    }.dump());

    ASSERT_EQ(responses.size(), 1u);
    EXPECT_EQ(Json::parse(responses.front())["result"]["data"], Json::array({
        0, 7, 5, 0, 2,
        0, 6, 4, 1, 1,
        0, 13, 4, 1, 0,
    }));
}

TEST(ServerCompositionRootTests, NegotiatesCompletionSnippetsWithPlainFallback) {
    const fs::path sourcePath = fs::temp_directory_path() / "rls-snippet-route.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    const auto complete = [&](bool snippetSupport,
                              std::optional<std::string> indentationMode,
                              std::string text,
                              uint32_t line, uint32_t character) {
        ServerCompositionRoot server(standaloneProject);
        Json initializeParams = Json::object();
        if (snippetSupport) {
            initializeParams = {
                {"capabilities", {{"textDocument", {{"completion", {
                    {"completionItem", {{"snippetSupport", true}}},
                }}}}}},
            };
        }
        if (indentationMode) {
            initializeParams["initializationOptions"] = {
                {"completion", {
                    {"sectionSnippetIndentation", *indentationMode},
                }},
            };
        }
        server.handlePayload(Json{
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "initialize"},
            {"params", std::move(initializeParams)},
        }.dump());
        server.handlePayload(
            R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
        server.handlePayload(Json{
            {"jsonrpc", "2.0"},
            {"method", "textDocument/didOpen"},
            {"params", {{"textDocument", {
                {"uri", uri},
                {"languageId", "rls"},
                {"version", 1},
                {"text", std::move(text)},
            }}}},
        }.dump());
        server.scheduler().waitForIdle();
        const auto responses = server.handlePayload(Json{
            {"jsonrpc", "2.0"},
            {"id", 2},
            {"method", "textDocument/completion"},
            {"params", {
                {"textDocument", {{"uri", uri}}},
                {"position", {{"line", line}, {"character", character}}},
            }},
        }.dump());
        EXPECT_EQ(responses.size(), 1u);
        return Json::parse(responses.front())["result"];
    };
    const auto find = [](const Json& items, std::string_view label) -> Json {
        for (const auto& item : items) {
            if (item.at("label").get<std::string>() == label) return item;
        }
        return Json(nullptr);
    };

    const std::string regionText =
        "region RR_TEMPLATE { customField: true }\n"
        "region RR_TEST {\n"
        "  \n"
        "}\n";
    const auto plainField = find(
        complete(false, std::nullopt, regionText, 2, 2), "customField");
    ASSERT_FALSE(plainField.is_null());
    EXPECT_EQ(plainField["insertTextFormat"], 1);
    EXPECT_EQ(plainField["textEdit"]["newText"], "customField");

    const auto serverItems = complete(true, std::nullopt, regionText, 2, 2);
    const auto serverEvents = find(serverItems, "events");
    ASSERT_FALSE(serverEvents.is_null());
    EXPECT_EQ(serverEvents["insertTextMode"], 1);
    EXPECT_EQ(serverEvents["textEdit"]["newText"], "events {\n      $0\n  }");

    const auto snippetItems = complete(true, "client", regionText, 2, 2);
    const auto snippetField = find(snippetItems, "customField");
    ASSERT_FALSE(snippetField.is_null());
    EXPECT_EQ(snippetField["insertTextFormat"], 2);
    EXPECT_EQ(snippetField["textEdit"]["newText"], "customField: ${1}");
    const auto snippetEvents = find(snippetItems, "events");
    ASSERT_FALSE(snippetEvents.is_null());
    EXPECT_EQ(snippetEvents["insertTextFormat"], 2);
    EXPECT_EQ(snippetEvents["insertTextMode"], 2);
    EXPECT_EQ(snippetEvents["textEdit"]["newText"], "events {\n    $0\n}");

    const auto plainKeyword = find(
        complete(true, "client", "def\n", 0, 3), "define");
    ASSERT_FALSE(plainKeyword.is_null());
    EXPECT_EQ(plainKeyword["insertTextFormat"], 1);
    EXPECT_EQ(plainKeyword["textEdit"]["newText"], "define");
}

TEST(ServerCompositionRootTests, RoutesDefinitionFromAcceptedSnapshot) {
    const fs::path sourcePath = fs::temp_directory_path() / "rls-definition-route.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(R"({
        "jsonrpc":"2.0","id":1,"method":"initialize","params":{
            "capabilities":{"textDocument":{"definition":{"linkSupport":true}}}
        }
    })");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", uri},
            {"languageId", "rls"},
            {"version", 1},
            {"text", "define target(): true\ndefine caller(): target()\n"},
        }}}},
    }.dump());
    server.scheduler().waitForIdle();

    const auto responses = server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/definition"},
        {"params", {
            {"textDocument", {{"uri", uri}}},
            {"position", {{"line", 1}, {"character", 18}}},
        }},
    }.dump());

    ASSERT_EQ(responses.size(), 1);
    const auto result = Json::parse(responses.front())["result"];
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]["targetUri"], uri);
    EXPECT_EQ(result[0]["originSelectionRange"]["start"]["character"], 17);
    EXPECT_EQ(result[0]["targetSelectionRange"]["start"]["character"], 7);
}

TEST(ServerCompositionRootTests, FallsBackToLocationWithoutDefinitionLinkSupport) {
    const fs::path sourcePath = fs::temp_directory_path() / "rls-definition-location.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", uri},
            {"languageId", "rls"},
            {"version", 1},
            {"text", "define target(): true\ndefine caller(): target()\n"},
        }}}},
    }.dump());
    server.scheduler().waitForIdle();

    const auto responses = server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/definition"},
        {"params", {
            {"textDocument", {{"uri", uri}}},
            {"position", {{"line", 1}, {"character", 18}}},
        }},
    }.dump());

    ASSERT_EQ(responses.size(), 1);
    const auto result = Json::parse(responses.front())["result"];
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]["uri"], uri);
    EXPECT_EQ(result[0]["range"]["start"]["character"], 7);
    EXPECT_FALSE(result[0].contains("targetUri"));
}

TEST(ServerCompositionRootTests, RoutesReferencesAndDocumentHighlights) {
    const fs::path sourcePath = fs::temp_directory_path() / "rls-references-route.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", uri},
            {"languageId", "rls"},
            {"version", 1},
            {"text", "define target(): true\ndefine caller(): target() and target()\n"},
        }}}},
    }.dump());
    server.scheduler().waitForIdle();

    const auto request = [&](std::string method, Json extra) {
        Json params = {
            {"textDocument", {{"uri", uri}}},
            {"position", {{"line", 1}, {"character", 18}}},
        };
        if (!extra.is_null()) {
            params.update(std::move(extra));
        }
        return server.handlePayload(Json{
            {"jsonrpc", "2.0"},
            {"id", 2},
            {"method", std::move(method)},
            {"params", std::move(params)},
        }.dump());
    };

    const auto referencesResponse = request(
        "textDocument/references", {{"context", {{"includeDeclaration", false}}}});
    ASSERT_EQ(referencesResponse.size(), 1u);
    const auto references = Json::parse(referencesResponse.front())["result"];
    ASSERT_EQ(references.size(), 2u);
    EXPECT_EQ(references[0]["range"]["start"]["character"], 17);
    EXPECT_EQ(references[1]["range"]["start"]["character"], 30);

    const auto highlightsResponse = request("textDocument/documentHighlight", {});
    ASSERT_EQ(highlightsResponse.size(), 1u);
    const auto highlights = Json::parse(highlightsResponse.front())["result"];
    ASSERT_EQ(highlights.size(), 3u);
    EXPECT_EQ(highlights[0]["kind"], 1);
    EXPECT_EQ(highlights[0]["range"]["start"]["line"], 0);
}

TEST(ServerCompositionRootTests, RoutesHierarchicalDocumentSymbols) {
    const fs::path sourcePath = fs::temp_directory_path() / "rls-document-symbol-route.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(R"({
        "jsonrpc":"2.0","id":1,"method":"initialize","params":{
            "capabilities":{"textDocument":{"documentSymbol":{
                "hierarchicalDocumentSymbolSupport":true
            }}}
        }
    })");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", uri},
            {"languageId", "rls"},
            {"version", 1},
            {"text", "define check(value: Bool): value\nenum Color { RED }\n"},
        }}}},
    }.dump());
    server.scheduler().waitForIdle();

    const auto responses = server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/documentSymbol"},
        {"params", {{"textDocument", {{"uri", uri}}}}},
    }.dump());

    ASSERT_EQ(responses.size(), 1u);
    const auto result = Json::parse(responses.front())["result"];
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0]["name"], "check");
    EXPECT_EQ(result[0]["kind"], 12);
    ASSERT_EQ(result[0]["children"].size(), 1u);
    EXPECT_EQ(result[0]["children"][0]["name"], "value");
    EXPECT_EQ(result[0]["children"][0]["kind"], 13);
    EXPECT_EQ(result[0]["children"][0]["range"]["end"]["character"], 24);
    EXPECT_EQ(result[0]["children"][0]["selectionRange"]["end"]["character"], 18);
    EXPECT_EQ(result[1]["name"], "Color");
    EXPECT_EQ(result[1]["kind"], 10);
    EXPECT_EQ(result[1]["children"][0]["kind"], 22);
}

TEST(ServerCompositionRootTests, FallsBackToFlatDocumentSymbols) {
    const fs::path sourcePath = fs::temp_directory_path() / "rls-flat-symbol-route.rls";
    const std::string uri = *rls::lsp::PathToFileUri(sourcePath);
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", uri},
            {"languageId", "rls"},
            {"version", 1},
            {"text", "enum Color { RED }\n"},
        }}}},
    }.dump());
    server.scheduler().waitForIdle();

    const auto responses = server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/documentSymbol"},
        {"params", {{"textDocument", {{"uri", uri}}}}},
    }.dump());

    ASSERT_EQ(responses.size(), 1u);
    const auto result = Json::parse(responses.front())["result"];
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0]["name"], "Color");
    EXPECT_EQ(result[0]["location"]["uri"], uri);
    EXPECT_FALSE(result[0].contains("children"));
    EXPECT_EQ(result[1]["name"], "RED");
    EXPECT_EQ(result[1]["containerName"], "Color");
}

TEST(ServerCompositionRootTests, RoutesWorkspaceSymbolsWithoutLeakingExternalProject) {
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path workspaceRoot = fs::temp_directory_path() /
        ("rls-workspace-symbol-root-" + suffix);
    const fs::path externalRoot = fs::temp_directory_path() /
        ("rls-workspace-symbol-external-" + suffix);
    fs::create_directories(workspaceRoot);
    fs::create_directories(externalRoot);
    const fs::path insidePath = workspaceRoot / "inside.rls";
    const fs::path outsidePath = externalRoot / "outside.rls";
    const std::string insideUri = *rls::lsp::PathToFileUri(insidePath);
    const std::string outsideUri = *rls::lsp::PathToFileUri(outsidePath);
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {{"workspaceFolders", Json::array({{
            {"uri", *rls::lsp::PathToFileUri(workspaceRoot)},
            {"name", "workspace"},
        }})}}},
    }.dump());
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    const auto open = [&](const std::string& uri, std::string text) {
        server.handlePayload(Json{
            {"jsonrpc", "2.0"},
            {"method", "textDocument/didOpen"},
            {"params", {{"textDocument", {
                {"uri", uri},
                {"languageId", "rls"},
                {"version", 1},
                {"text", std::move(text)},
            }}}},
        }.dump());
    };
    open(insideUri, "define InsideTarget(): true\n");
    open(outsideUri, "define OutsideTarget(): true\n");
    server.scheduler().waitForIdle();

    const auto responses = server.handlePayload(Json{
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "workspace/symbol"},
        {"params", {{"query", "target"}}},
    }.dump());

    ASSERT_EQ(responses.size(), 1u);
    const auto result = Json::parse(responses.front())["result"];
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0]["name"], "InsideTarget");
    EXPECT_EQ(result[0]["kind"], 12);
    EXPECT_EQ(result[0]["location"]["uri"], insideUri);

    std::error_code error;
    fs::remove_all(workspaceRoot, error);
    fs::remove_all(externalRoot, error);
}

TEST(ServerCompositionRootTests, SynchronizesOpenChangeAndClose) {
    ServerCompositionRoot server(standaloneProject);
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    server.handlePayload(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.handlePayload(R"({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{"textDocument":{
            "uri":"file:///main.rls","languageId":"rls","version":1,"text":"old"
        }}
    })");
    ASSERT_NE(server.documents().find("file:///main.rls"), nullptr);

    server.handlePayload(R"({
        "jsonrpc":"2.0",
        "method":"textDocument/didChange",
        "params":{
            "textDocument":{"uri":"file:///main.rls","version":2},
            "contentChanges":[{"text":"new"}]
        }
    })");
    EXPECT_EQ(server.documents().find("file:///main.rls")->text, "new");

    server.handlePayload(R"({
        "jsonrpc":"2.0",
        "method":"textDocument/didChange",
        "params":{
            "textDocument":{"uri":"file:///main.rls","version":2},
            "contentChanges":[{"text":"stale"}]
        }
    })");
    EXPECT_EQ(server.documents().find("file:///main.rls")->text, "new");

    server.handlePayload(R"({
        "jsonrpc":"2.0",
        "method":"textDocument/didClose",
        "params":{"textDocument":{"uri":"file:///main.rls"}}
    })");
    EXPECT_EQ(server.documents().find("file:///main.rls"), nullptr);
}

TEST(ServerCompositionRootTests, TracksCleanShutdownAndExit) {
    ServerCompositionRoot server;
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    auto responses = server.handlePayload(
        R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})");
    ASSERT_EQ(responses.size(), 1);
    EXPECT_TRUE(Json::parse(responses.front())["result"].is_null());

    EXPECT_FALSE(server.shouldExit());
    server.handlePayload(R"({"jsonrpc":"2.0","method":"exit"})");
    EXPECT_TRUE(server.shouldExit());
    EXPECT_EQ(server.exitCode(), 0);
}

TEST(ServerCompositionRootTests, IgnoresDocumentNotificationsBeforeInitialize) {
    ServerCompositionRoot server;
    server.handlePayload(R"({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{"textDocument":{
            "uri":"file:///early.rls","languageId":"rls","version":1,"text":"early"
        }}
    })");

    EXPECT_EQ(server.documents().find("file:///early.rls"), nullptr);
}

TEST(ServerCompositionRootTests, RoutesWorkspaceFolderChanges) {
    const fs::path first = fs::temp_directory_path() /
        ("rls-lsp-workspace-route-first-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const fs::path second = fs::temp_directory_path() /
        ("rls-lsp-workspace-route-second-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(first);
    fs::create_directories(second);
    const std::string firstUri = *rls::lsp::PathToFileUri(first);
    const std::string secondUri = *rls::lsp::PathToFileUri(second);
    ServerCompositionRoot server;

    server.handlePayload(Json{
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"workspaceFolders", Json::array({
            {{"uri", firstUri}, {"name", "first"}},
        })}}},
    }.dump());
    server.handlePayload(R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    ASSERT_EQ(server.workspace().folderCount(), 1);

    server.handlePayload(Json{
        {"jsonrpc", "2.0"}, {"method", "workspace/didChangeWorkspaceFolders"},
        {"params", {{"event", {
            {"added", Json::array({{{"uri", secondUri}, {"name", "second"}}})},
            {"removed", Json::array({{{"uri", firstUri}, {"name", "first"}}})},
        }}}},
    }.dump());
    EXPECT_EQ(server.workspace().folderCount(), 1);

    std::error_code error;
    fs::remove_all(first, error);
    fs::remove_all(second, error);
}

} // namespace