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

    const auto outbound = server.handlePayload(R"({
        "jsonrpc":"2.0",
        "id":1,
        "method":"initialize",
        "params":{}
    })");

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

    const auto outbound = server.handlePayload(R"({
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
    })");

    ASSERT_EQ(outbound.size(), 1u);
    const auto notification = json::parse(outbound[0]);
    EXPECT_EQ(notification["method"], "textDocument/publishDiagnostics");
    EXPECT_EQ(notification["params"]["uri"], "file:///bad.rls");
    ASSERT_TRUE(notification["params"].contains("diagnostics"));
    EXPECT_FALSE(notification["params"]["diagnostics"].empty());
}

TEST(LanguageServerTests, DidChangeWithStaleVersionIsIgnored) {
    LanguageServer server;
    (void)server.handlePayload(R"({
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
    })");

    const auto outbound = server.handlePayload(R"({
        "jsonrpc":"2.0",
        "method":"textDocument/didChange",
        "params":{
            "textDocument":{
                "uri":"file:///doc.rls",
                "version":1
            },
            "contentChanges":[{"text":"define foo() true"}]
        }
    })");

    EXPECT_TRUE(outbound.empty());
    const auto* doc = server.documents().find("file:///doc.rls");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->version, 2);
    EXPECT_EQ(doc->text, "define foo(): true");
}

TEST(LanguageServerTests, DidCloseClearsDiagnostics) {
    LanguageServer server;
    (void)server.handlePayload(R"({
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
    })");

    const auto outbound = server.handlePayload(R"({
        "jsonrpc":"2.0",
        "method":"textDocument/didClose",
        "params":{
            "textDocument":{
                "uri":"file:///doc.rls"
            }
        }
    })");

    ASSERT_EQ(outbound.size(), 1u);
    const auto notification = json::parse(outbound[0]);
    EXPECT_EQ(notification["method"], "textDocument/publishDiagnostics");
    EXPECT_EQ(notification["params"]["uri"], "file:///doc.rls");
    ASSERT_TRUE(notification["params"].contains("diagnostics"));
    EXPECT_TRUE(notification["params"]["diagnostics"].empty());
}

} // namespace
