#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rls/lsp/server_composition_root.h"

namespace {

using Json = nlohmann::json;
using rls::lsp::ServerCompositionRoot;

TEST(ServerCompositionRootTests, RegistersOnlyImplementedRoutes) {
    ServerCompositionRoot server;

    EXPECT_TRUE(server.router().contains("initialize"));
    EXPECT_TRUE(server.router().contains("textDocument/didOpen"));
    EXPECT_FALSE(server.router().contains("textDocument/definition"));
    EXPECT_FALSE(server.router().contains("textDocument/publishDiagnostics"));
}

TEST(ServerCompositionRootTests, AdvertisesFullSynchronizationOnly) {
    ServerCompositionRoot server;
    const auto responses = server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");

    ASSERT_EQ(responses.size(), 1);
    const auto result = Json::parse(responses.front())["result"];
    EXPECT_EQ(result["capabilities"]["textDocumentSync"]["change"], 1);
    EXPECT_FALSE(result["capabilities"].contains("definitionProvider"));
}

TEST(ServerCompositionRootTests, SynchronizesOpenChangeAndClose) {
    ServerCompositionRoot server;
    server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
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

} // namespace