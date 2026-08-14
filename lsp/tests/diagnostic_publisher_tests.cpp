#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "analysis_snapshot.h"
#include "rls/lsp/diagnostic_publisher.h"
#include "rls/lsp/document_uri.h"
#include "rls/lsp/outbound_message_queue.h"

namespace fs = std::filesystem;

namespace {

using Json = nlohmann::json;
using rls::lsp::DiagnosticPublisher;
using rls::lsp::OutboundMessageQueue;

class TemporaryDirectory {
public:
    TemporaryDirectory() : path_(fs::temp_directory_path() /
        ("rls-lsp-diagnostics-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()))) {
        fs::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

std::string genericPath(const fs::path& path) {
    const auto generic = fs::weakly_canonical(path).generic_u8string();
    std::string value;
    value.reserve(generic.size());
    for (const char8_t byte : generic) value.push_back(static_cast<char>(byte));
    return value;
}

std::shared_ptr<const rls::sema::AnalysisSnapshot> snapshot(
    const fs::path& path, std::string content, uint64_t generation) {
    const auto result = rls::sema::AnalysisSnapshot::Create({
        {genericPath(path), std::move(content)},
    }, generation);
    EXPECT_TRUE(result.has_value());
    return result ? *result : nullptr;
}

const Json* findDiagnostic(const Json& notification, std::string_view code) {
    for (const auto& diagnostic : notification["params"]["diagnostics"]) {
        if (diagnostic.value("code", "") == code) return &diagnostic;
    }
    return nullptr;
}

TEST(DiagnosticPublisherTests, PublishesUtf16RangesCodesSeverityAndRelatedInformation) {
    TemporaryDirectory directory;
    const fs::path path = directory.path() / "main.rls";
    std::ofstream(path) << "placeholder";
    const std::string content =
        "region RR_TEST { name: \"\xF0\x9F\x98\x80\" name: \"Second\" }\n";
    const auto analyzed = snapshot(path, content, 1);
    ASSERT_NE(analyzed, nullptr);

    OutboundMessageQueue outbound;
    DiagnosticPublisher publisher(outbound);
    publisher.acceptedSnapshot("project", analyzed);

    const auto payload = outbound.tryPop();
    ASSERT_TRUE(payload.has_value());
    const Json notification = Json::parse(*payload);
    EXPECT_EQ(notification["method"], "textDocument/publishDiagnostics");
    EXPECT_EQ(notification["params"]["uri"], *rls::lsp::PathToFileUri(path));

    const Json* diagnostic = findDiagnostic(notification, "RLS-V002");
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_EQ((*diagnostic)["severity"], 1);
    EXPECT_EQ((*diagnostic)["source"], "rls");
    ASSERT_TRUE(diagnostic->contains("relatedInformation"));
    EXPECT_EQ((*diagnostic)["relatedInformation"][0]["message"], "first definition");

    const auto compilerDiagnostic = analyzed->diagnosticsFor(genericPath(path));
    const auto duplicate = std::find_if(
        compilerDiagnostic.begin(), compilerDiagnostic.end(), [](const auto& value) {
            return value.code == "RLS-V002";
        });
    ASSERT_NE(duplicate, compilerDiagnostic.end());
    EXPECT_LT((*diagnostic)["range"]["start"]["character"].get<uint32_t>(),
        duplicate->span.start.column - 1);
}

TEST(DiagnosticPublisherTests, PublishesOnlyChangesAndClearsResolvedDiagnostics) {
    TemporaryDirectory directory;
    const fs::path path = directory.path() / "main.rls";
    std::ofstream(path) << "placeholder";
    OutboundMessageQueue outbound;
    DiagnosticPublisher publisher(outbound);
    const auto broken = snapshot(path,
        "region RR_TEST { events { EVENT_TEST: \"invalid\" } }\n", 1);

    publisher.acceptedSnapshot("project", broken);
    ASSERT_TRUE(outbound.tryPop().has_value());
    publisher.acceptedSnapshot("project", broken);
    EXPECT_FALSE(outbound.tryPop().has_value());

    publisher.acceptedSnapshot("project", snapshot(path,
        "region RR_TEST { events { EVENT_TEST: true } }\n", 2));
    const auto clearPayload = outbound.tryPop();
    ASSERT_TRUE(clearPayload.has_value());
    const Json clear = Json::parse(*clearPayload);
    EXPECT_TRUE(clear["params"]["diagnostics"].empty());
}

TEST(DiagnosticPublisherTests, ClearsAndSuppressesClosedStandaloneDocuments) {
    TemporaryDirectory directory;
    const fs::path path = directory.path() / "main.rls";
    std::ofstream(path) << "placeholder";
    const std::string uri = *rls::lsp::PathToFileUri(path);
    OutboundMessageQueue outbound;
    DiagnosticPublisher publisher(outbound);
    const auto broken = snapshot(path,
        "region RR_TEST { events { EVENT_TEST: \"invalid\" } }\n", 1);

    publisher.acceptedSnapshot("project", broken);
    ASSERT_TRUE(outbound.tryPop().has_value());
    publisher.documentClosed(uri, true);
    const auto clearPayload = outbound.tryPop();
    ASSERT_TRUE(clearPayload.has_value());
    EXPECT_TRUE(Json::parse(*clearPayload)["params"]["diagnostics"].empty());

    publisher.acceptedSnapshot("project", broken);
    EXPECT_FALSE(outbound.tryPop().has_value());
    publisher.documentOpened(uri);
    publisher.acceptedSnapshot("project", broken);
    EXPECT_TRUE(outbound.tryPop().has_value());
}

TEST(DiagnosticPublisherTests, PublishesParserDiagnosticsWithFallbackRanges) {
    TemporaryDirectory directory;
    const fs::path path = directory.path() / "main.rls";
    std::ofstream(path) << "placeholder";
    OutboundMessageQueue outbound;
    DiagnosticPublisher publisher(outbound);

    publisher.acceptedSnapshot("project", snapshot(path, "define broken(\n", 1));
    const auto payload = outbound.tryPop();
    ASSERT_TRUE(payload.has_value());
    const Json diagnostics = Json::parse(*payload)["params"]["diagnostics"];
    ASSERT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics[0]["severity"], 1);
    EXPECT_TRUE(diagnostics[0].contains("range"));
}

TEST(DiagnosticPublisherTests, ClearsDocumentsRemovedFromAcceptedProjectSnapshot) {
    TemporaryDirectory directory;
    const fs::path firstPath = directory.path() / "first.rls";
    const fs::path secondPath = directory.path() / "second.rls";
    std::ofstream(firstPath) << "placeholder";
    std::ofstream(secondPath) << "placeholder";
    const auto firstSnapshot = rls::sema::AnalysisSnapshot::Create({
        {genericPath(firstPath), "define first(): missing\n"},
        {genericPath(secondPath), "define second(): missing\n"},
    }, 1);
    ASSERT_TRUE(firstSnapshot.has_value());

    OutboundMessageQueue outbound;
    DiagnosticPublisher publisher(outbound);
    publisher.acceptedSnapshot("project", *firstSnapshot);
    ASSERT_TRUE(outbound.tryPop().has_value());
    ASSERT_TRUE(outbound.tryPop().has_value());

    publisher.acceptedSnapshot("project", snapshot(firstPath,
        "define first(): missing\n", 2));
    const auto clearPayload = outbound.tryPop();
    ASSERT_TRUE(clearPayload.has_value());
    const Json clear = Json::parse(*clearPayload);
    EXPECT_EQ(clear["params"]["uri"], *rls::lsp::PathToFileUri(secondPath));
    EXPECT_TRUE(clear["params"]["diagnostics"].empty());
}

TEST(DiagnosticPublisherTests, PublishesCrossFileRelatedDeclarationLocations) {
    TemporaryDirectory directory;
    const fs::path firstPath = directory.path() / "first.rls";
    const fs::path secondPath = directory.path() / "second.rls";
    std::ofstream(firstPath) << "placeholder";
    std::ofstream(secondPath) << "placeholder";
    const auto analyzed = rls::sema::AnalysisSnapshot::Create({
        {genericPath(firstPath), "region RR_DUP { name: \"First\" }\n"},
        {genericPath(secondPath), "region RR_DUP { name: \"Second\" }\n"},
    }, 1);
    ASSERT_TRUE(analyzed.has_value());

    OutboundMessageQueue outbound;
    DiagnosticPublisher publisher(outbound);
    publisher.acceptedSnapshot("project", *analyzed);

    const auto firstPayload = outbound.tryPop();
    const auto secondPayload = outbound.tryPop();
    ASSERT_TRUE(firstPayload.has_value());
    ASSERT_TRUE(secondPayload.has_value());
    const Json first = Json::parse(*firstPayload);
    const Json second = Json::parse(*secondPayload);
    const Json* duplicate = findDiagnostic(first, "RLS-S001");
    if (!duplicate) duplicate = findDiagnostic(second, "RLS-S001");
    ASSERT_NE(duplicate, nullptr);
    ASSERT_TRUE(duplicate->contains("relatedInformation"));
    EXPECT_EQ((*duplicate)["relatedInformation"][0]["location"]["uri"],
        *rls::lsp::PathToFileUri(firstPath));
}

TEST(DiagnosticPublisherTests, PublishesAndClearsManifestConfigurationDiagnostics) {
    TemporaryDirectory directory;
    const fs::path manifestPath = directory.path() / "rls.json";
    const std::string content = "{\"name\":\"\xF0\x9F\x98\x80\", invalid}";
    std::ofstream(manifestPath, std::ios::binary) << content;
    const auto loaded = rls::project::LoadManifest(manifestPath);
    ASSERT_EQ(loaded.diagnostics.size(), 1);

    OutboundMessageQueue outbound;
    DiagnosticPublisher publisher(outbound);
    publisher.publishConfigurationDiagnostics(loaded.diagnostics);
    const auto payload = outbound.tryPop();
    ASSERT_TRUE(payload.has_value());
    const Json notification = Json::parse(*payload);
    ASSERT_EQ(notification["params"]["diagnostics"].size(), 1);
    const Json& diagnostic = notification["params"]["diagnostics"][0];
    EXPECT_EQ(notification["params"]["uri"], *rls::lsp::PathToFileUri(manifestPath));
    EXPECT_EQ(diagnostic["code"], "RLS-C002");
    EXPECT_EQ(diagnostic["severity"], 1);
    EXPECT_EQ(diagnostic["source"], "rls");
    EXPECT_LT(diagnostic["range"]["start"]["character"].get<size_t>(),
        loaded.diagnostics[0].startByte);

    publisher.publishConfigurationDiagnostics({});
    const auto clearPayload = outbound.tryPop();
    ASSERT_TRUE(clearPayload.has_value());
    EXPECT_TRUE(Json::parse(*clearPayload)["params"]["diagnostics"].empty());
}

TEST(DiagnosticPublisherTests, KeepsMultipleManifestDiagnosticsIsolated) {
    TemporaryDirectory first;
    TemporaryDirectory second;
    const fs::path firstManifest = first.path() / "rls.json";
    const fs::path secondManifest = second.path() / "rls.json";
    std::ofstream(firstManifest) << "{ invalid";
    std::ofstream(secondManifest) << "{ invalid";
    const auto firstLoad = rls::project::LoadManifest(firstManifest);
    const auto secondLoad = rls::project::LoadManifest(secondManifest);
    std::vector<rls::project::ConfigurationDiagnostic> both = firstLoad.diagnostics;
    both.insert(both.end(), secondLoad.diagnostics.begin(), secondLoad.diagnostics.end());

    OutboundMessageQueue outbound;
    DiagnosticPublisher publisher(outbound);
    publisher.publishConfigurationDiagnostics(both);
    ASSERT_TRUE(outbound.tryPop().has_value());
    ASSERT_TRUE(outbound.tryPop().has_value());

    publisher.publishConfigurationDiagnostics(secondLoad.diagnostics);
    const auto clearPayload = outbound.tryPop();
    ASSERT_TRUE(clearPayload.has_value());
    const Json clear = Json::parse(*clearPayload);
    EXPECT_EQ(clear["params"]["uri"], *rls::lsp::PathToFileUri(firstManifest));
    EXPECT_TRUE(clear["params"]["diagnostics"].empty());
    EXPECT_FALSE(outbound.tryPop().has_value());
}

TEST(DiagnosticPublisherTests, PreservesStructuredCompilerActionData) {
    TemporaryDirectory directory;
    const fs::path path = directory.path() / "main.rls";
    std::ofstream(path) << "placeholder";
    const auto analyzed = snapshot(path, "define broken(): missing\n", 1);
    ASSERT_NE(analyzed, nullptr);

    OutboundMessageQueue outbound;
    DiagnosticPublisher publisher(outbound);
    publisher.acceptedSnapshot("project", analyzed);
    const auto payload = outbound.tryPop();
    ASSERT_TRUE(payload.has_value());
    const Json notification = Json::parse(*payload);
    const Json* diagnostic = findDiagnostic(notification, "RLS-T006");
    ASSERT_NE(diagnostic, nullptr);
    ASSERT_TRUE(diagnostic->contains("data"));
    EXPECT_EQ((*diagnostic)["data"]["version"], 1);
    EXPECT_EQ((*diagnostic)["data"]["actionKind"], "rls.declareSymbol");
    ASSERT_EQ((*diagnostic)["data"]["arguments"].size(), 1);
    EXPECT_EQ((*diagnostic)["data"]["arguments"][0], "missing");
}

TEST(DiagnosticPublisherTests, PreservesStructuredConfigurationActionData) {
    TemporaryDirectory directory;
    const fs::path manifestPath = directory.path() / "rls.json";
    std::ofstream(manifestPath) << "{ invalid";
    const auto loaded = rls::project::LoadManifest(manifestPath);
    ASSERT_EQ(loaded.diagnostics.size(), 1);
    ASSERT_TRUE(loaded.diagnostics[0].data.has_value());

    OutboundMessageQueue outbound;
    DiagnosticPublisher publisher(outbound);
    publisher.publishConfigurationDiagnostics(loaded.diagnostics);
    const auto payload = outbound.tryPop();
    ASSERT_TRUE(payload.has_value());
    const Json diagnostic = Json::parse(*payload)["params"]["diagnostics"][0];
    EXPECT_EQ(diagnostic["data"]["version"], 1);
    EXPECT_EQ(diagnostic["data"]["actionKind"], "rls.fixManifestJson");
    ASSERT_EQ(diagnostic["data"]["arguments"].size(), 1);
    EXPECT_FALSE(diagnostic["data"]["arguments"][0].get<std::string>().empty());
}

} // namespace