#include <chrono>
#include <filesystem>

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
    EXPECT_FALSE(server.router().contains("textDocument/publishDiagnostics"));
}

TEST(ServerCompositionRootTests, AdvertisesSynchronizationAndDefinition) {
    ServerCompositionRoot server;
    const auto responses = server.handlePayload(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");

    ASSERT_EQ(responses.size(), 1);
    const auto result = Json::parse(responses.front())["result"];
    EXPECT_EQ(result["capabilities"]["textDocumentSync"]["change"], 1);
    EXPECT_TRUE(result["capabilities"]["workspace"]["workspaceFolders"]["supported"]);
    EXPECT_EQ(result["capabilities"]["definitionProvider"], true);
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