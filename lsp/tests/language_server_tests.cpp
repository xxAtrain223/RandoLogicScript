#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "language_server.h"

namespace {

using rls::lsp::LanguageServer;
using rls::lsp::TextDocumentSyncKind;
using json = nlohmann::json;

TEST(LanguageServerTests, DefaultCapabilitiesMatchPhaseOneScope) {
    const LanguageServer server;
    const auto& caps = server.capabilities();

    EXPECT_EQ(caps.textDocumentSync, TextDocumentSyncKind::Full);
    EXPECT_TRUE(caps.definitionProvider);
    EXPECT_TRUE(caps.referencesProvider);
    EXPECT_TRUE(caps.hoverProvider);
    EXPECT_TRUE(caps.completionProvider);
    EXPECT_TRUE(caps.documentSymbolProvider);
    EXPECT_TRUE(caps.workspaceSymbolProvider);
}

TEST(LanguageServerTests, ExposesDocumentStore) {
    LanguageServer server;
    server.documents().open("file:///x.rls", "rls", 1, "define x(): true");

    const auto* doc = server.documents().find("file:///x.rls");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->version, 1);
}

TEST(LanguageServerTests, InitializeReturnsCapabilities) {
    LanguageServer server;

    const auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "id":1,
        "method":"initialize",
        "params":{}
    })json");

    ASSERT_EQ(outbound.size(), 1u);
    const auto response = json::parse(outbound[0]);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 1);
    ASSERT_TRUE(response.contains("result"));
    ASSERT_TRUE(response["result"].contains("capabilities"));
    EXPECT_EQ(response["result"]["capabilities"]["textDocumentSync"], 1);
}

TEST(LanguageServerTests, DidOpenPublishesDiagnostics) {
    LanguageServer server;

    const auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{
            "textDocument":{
                "uri":"file:///bad.rls",
                "languageId":"rls",
                "version":1,
                "text":"define foo() true"
            }
        }
    })json");

    ASSERT_EQ(outbound.size(), 1u);
    const auto notification = json::parse(outbound[0]);
    EXPECT_EQ(notification["method"], "textDocument/publishDiagnostics");
    EXPECT_EQ(notification["params"]["uri"], "file:///bad.rls");
    ASSERT_TRUE(notification["params"].contains("diagnostics"));
    EXPECT_FALSE(notification["params"]["diagnostics"].empty());
}

TEST(LanguageServerTests, DidChangeWithStaleVersionIsIgnored) {
    LanguageServer server;
    (void)server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{
            "textDocument":{
                "uri":"file:///doc.rls",
                "languageId":"rls",
                "version":2,
                "text":"define foo(): true"
            }
        }
    })json");

    const auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didChange",
        "params":{
            "textDocument":{
                "uri":"file:///doc.rls",
                "version":1
            },
            "contentChanges":[{"text":"define foo() true"}]
        }
    })json");

    EXPECT_TRUE(outbound.empty());
    const auto* doc = server.documents().find("file:///doc.rls");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->version, 2);
    EXPECT_EQ(doc->text, "define foo(): true");
}

TEST(LanguageServerTests, DidCloseClearsDiagnostics) {
    LanguageServer server;
    (void)server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{
            "textDocument":{
                "uri":"file:///doc.rls",
                "languageId":"rls",
                "version":1,
                "text":"define foo(): true"
            }
        }
    })json");

    const auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didClose",
        "params":{
            "textDocument":{
                "uri":"file:///doc.rls"
            }
        }
    })json");

    ASSERT_EQ(outbound.size(), 1u);
    const auto notification = json::parse(outbound[0]);
    EXPECT_EQ(notification["method"], "textDocument/publishDiagnostics");
    EXPECT_EQ(notification["params"]["uri"], "file:///doc.rls");
    ASSERT_TRUE(notification["params"].contains("diagnostics"));
    EXPECT_TRUE(notification["params"]["diagnostics"].empty());
}

TEST(LanguageServerTests, DefinitionReturnsDeclarationLocation) {
    LanguageServer server;
    (void)server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{
            "textDocument":{
                "uri":"file:///defs.rls",
                "languageId":"rls",
                "version":1,
                "text":"define foo(): true\ndefine bar(): foo()"
            }
        }
    })json");

    const auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "id":21,
        "method":"textDocument/definition",
        "params":{
            "textDocument":{"uri":"file:///defs.rls"},
            "position":{"line":1,"character":15}
        }
    })json");

    ASSERT_EQ(outbound.size(), 1u);
    const auto response = json::parse(outbound[0]);
    ASSERT_TRUE(response["result"].is_array());
    ASSERT_FALSE(response["result"].empty());
    EXPECT_EQ(response["result"][0]["uri"], "file:///defs.rls");
}

TEST(LanguageServerTests, ReferencesFindsAllMatches) {
    LanguageServer server;
    (void)server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{
            "textDocument":{
                "uri":"file:///refs.rls",
                "languageId":"rls",
                "version":1,
                "text":"define foo(): true\ndefine bar(): foo()\ndefine baz(): foo()"
            }
        }
    })json");

    const auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "id":22,
        "method":"textDocument/references",
        "params":{
            "textDocument":{"uri":"file:///refs.rls"},
            "position":{"line":1,"character":15},
            "context":{"includeDeclaration":true}
        }
    })json");

    ASSERT_EQ(outbound.size(), 1u);
    const auto response = json::parse(outbound[0]);
    ASSERT_TRUE(response["result"].is_array());
    EXPECT_GE(response["result"].size(), 3u);
}

TEST(LanguageServerTests, HoverReturnsSymbolDetails) {
    LanguageServer server;
    (void)server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{
            "textDocument":{
                "uri":"file:///hover.rls",
                "languageId":"rls",
                "version":1,
                "text":"define foo(): true\ndefine bar(): foo()"
            }
        }
    })json");

    const auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "id":23,
        "method":"textDocument/hover",
        "params":{
            "textDocument":{"uri":"file:///hover.rls"},
            "position":{"line":1,"character":15}
        }
    })json");

    ASSERT_EQ(outbound.size(), 1u);
    const auto response = json::parse(outbound[0]);
    ASSERT_TRUE(response.contains("result"));
    ASSERT_FALSE(response["result"].is_null());
    EXPECT_EQ(response["result"]["contents"]["kind"], "plaintext");
}

TEST(LanguageServerTests, DocumentSymbolReturnsTopLevelDeclarations) {
    LanguageServer server;
    (void)server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{
            "textDocument":{
                "uri":"file:///symbols.rls",
                "languageId":"rls",
                "version":1,
                "text":"define foo(): true\nextern define ext(a: Int) -> Bool"
            }
        }
    })json");

    const auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "id":24,
        "method":"textDocument/documentSymbol",
        "params":{
            "textDocument":{"uri":"file:///symbols.rls"}
        }
    })json");

    ASSERT_EQ(outbound.size(), 1u);
    const auto response = json::parse(outbound[0]);
    ASSERT_TRUE(response["result"].is_array());
    EXPECT_EQ(response["result"].size(), 2u);
}

TEST(LanguageServerTests, WorkspaceSymbolFiltersByQuery) {
    LanguageServer server;
    (void)server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{
            "textDocument":{
                "uri":"file:///ws.rls",
                "languageId":"rls",
                "version":1,
                "text":"define alpha(): true\ndefine beta(): true"
            }
        }
    })json");

    const auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "id":25,
        "method":"workspace/symbol",
        "params":{
            "query":"alp"
        }
    })json");

    ASSERT_EQ(outbound.size(), 1u);
    const auto response = json::parse(outbound[0]);
    ASSERT_TRUE(response["result"].is_array());
    ASSERT_EQ(response["result"].size(), 1u);
    EXPECT_EQ(response["result"][0]["name"], "alpha");
}

TEST(LanguageServerTests, CompletionReturnsKeywordsAndSymbols) {
    LanguageServer server;
    (void)server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{
            "textDocument":{
                "uri":"file:///completion.rls",
                "languageId":"rls",
                "version":1,
                "text":"define alpha(value: Int): value\ndefine beta(): al"
            }
        }
    })json");

    const auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "id":31,
        "method":"textDocument/completion",
        "params":{
            "textDocument":{"uri":"file:///completion.rls"},
            "position":{"line":1,"character":17}
        }
    })json");

    ASSERT_EQ(outbound.size(), 1u);
    const auto response = json::parse(outbound[0]);
    ASSERT_TRUE(response.contains("result"));
    ASSERT_TRUE(response["result"].contains("items"));
    ASSERT_TRUE(response["result"]["items"].is_array());

    bool foundAlpha = false;
    for (const auto& item : response["result"]["items"]) {
        if (item.value("label", "") == "alpha") {
            foundAlpha = true;
            break;
        }
    }
    EXPECT_TRUE(foundAlpha);
}

TEST(LanguageServerTests, InvalidRequestReturnsError) {
    LanguageServer server;

    const auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "id":42
    })json");

    ASSERT_EQ(outbound.size(), 1u);
    const auto response = json::parse(outbound[0]);
    ASSERT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"]["code"], -32600);
}

TEST(LanguageServerTests, ParseErrorReturnsJsonRpcError) {
    LanguageServer server;

    const auto outbound = server.handlePayload("{ this is not valid json");

    ASSERT_EQ(outbound.size(), 1u);
    const auto response = json::parse(outbound[0]);
    ASSERT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"]["code"], -32700);
}

TEST(LanguageServerTests, DidOpenNormalizesFileUriSlashes) {
    LanguageServer server;

    const auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{
            "textDocument":{
                "uri":"file:///C:\\tmp\\norm.rls",
                "languageId":"rls",
                "version":1,
                "text":"define foo() true"
            }
        }
    })json");

    ASSERT_EQ(outbound.size(), 1u);
    const auto notification = json::parse(outbound[0]);
    EXPECT_EQ(notification["params"]["uri"], "file:///C:/tmp/norm.rls");
}

TEST(LanguageServerTests, ParseDiagnosticRangeConvertsToZeroBased) {
    LanguageServer server;

    const auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{
            "textDocument":{
                "uri":"file:///range.rls",
                "languageId":"rls",
                "version":1,
                "text":"define foo() true"
            }
        }
    })json");

    ASSERT_EQ(outbound.size(), 1u);
    const auto notification = json::parse(outbound[0]);
    ASSERT_TRUE(notification["params"].contains("diagnostics"));
    ASSERT_FALSE(notification["params"]["diagnostics"].empty());

    const auto& first = notification["params"]["diagnostics"][0];
    EXPECT_EQ(first["range"]["start"]["line"], 0);
    EXPECT_EQ(first["range"]["start"]["character"], 13);
}

} // namespace
