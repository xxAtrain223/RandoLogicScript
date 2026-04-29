#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "language_server.h"

namespace {

using json = nlohmann::json;
using rls::lsp::LanguageServer;

TEST(ProtocolIntegrationTests, InitializeOpenDefinitionFlow) {
    LanguageServer server;

    auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "id":1,
        "method":"initialize",
        "params":{}
    })json");
    ASSERT_EQ(outbound.size(), 1u);
    auto initResponse = json::parse(outbound[0]);
    ASSERT_TRUE(initResponse.contains("result"));

    outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{
            "textDocument":{
                "uri":"file:///flow.rls",
                "languageId":"rls",
                "version":1,
                "text":"define foo(): true\ndefine bar(): foo()"
            }
        }
    })json");
    ASSERT_EQ(outbound.size(), 1u);
    auto diagNotification = json::parse(outbound[0]);
    EXPECT_EQ(diagNotification["method"], "textDocument/publishDiagnostics");

    outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "id":2,
        "method":"textDocument/definition",
        "params":{
            "textDocument":{"uri":"file:///flow.rls"},
            "position":{"line":1,"character":15}
        }
    })json");
    ASSERT_EQ(outbound.size(), 1u);
    auto defResponse = json::parse(outbound[0]);
    ASSERT_TRUE(defResponse["result"].is_array());
    ASSERT_FALSE(defResponse["result"].empty());
    EXPECT_EQ(defResponse["result"][0]["uri"], "file:///flow.rls");
}

TEST(ProtocolIntegrationTests, OpenChangeCloseLifecyclePublishesDiagnostics) {
    LanguageServer server;

    auto outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didOpen",
        "params":{
            "textDocument":{
                "uri":"file:///lifecycle.rls",
                "languageId":"rls",
                "version":1,
                "text":"define bad() true"
            }
        }
    })json");
    ASSERT_EQ(outbound.size(), 1u);
    auto openDiag = json::parse(outbound[0]);
    ASSERT_FALSE(openDiag["params"]["diagnostics"].empty());

    outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didChange",
        "params":{
            "textDocument":{
                "uri":"file:///lifecycle.rls",
                "version":2
            },
            "contentChanges":[{"text":"define good(): true"}]
        }
    })json");
    ASSERT_EQ(outbound.size(), 1u);
    auto changeDiag = json::parse(outbound[0]);
    ASSERT_TRUE(changeDiag["params"]["diagnostics"].is_array());
    for (const auto& diagnostic : changeDiag["params"]["diagnostics"]) {
        EXPECT_NE(diagnostic.value("severity", 3), 1);
    }

    outbound = server.handlePayload(R"json({
        "jsonrpc":"2.0",
        "method":"textDocument/didClose",
        "params":{
            "textDocument":{
                "uri":"file:///lifecycle.rls"
            }
        }
    })json");
    ASSERT_EQ(outbound.size(), 1u);
    auto closeDiag = json::parse(outbound[0]);
    EXPECT_TRUE(closeDiag["params"]["diagnostics"].empty());
}

} // namespace
