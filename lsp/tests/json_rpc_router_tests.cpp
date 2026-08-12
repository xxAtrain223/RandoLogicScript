#include <stdexcept>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rls/lsp/json_rpc_router.h"

namespace {

using Json = nlohmann::json;
using rls::lsp::JsonRpcRouter;

TEST(JsonRpcRouterTests, DispatchesExplicitRequestAndPreservesId) {
    JsonRpcRouter router;
    router.registerRequest("test/echo", [](const Json& params) { return params.at("value"); });

    const auto responses = router.handlePayload(
        R"({"jsonrpc":"2.0","id":"request-1","method":"test/echo","params":{"value":42}})");

    ASSERT_EQ(responses.size(), 1);
    const auto response = Json::parse(responses.front());
    EXPECT_EQ(response["id"], "request-1");
    EXPECT_EQ(response["result"], 42);
}

TEST(JsonRpcRouterTests, NotificationsDoNotProduceResponses) {
    JsonRpcRouter router;
    bool called = false;
    router.registerNotification("test/notify", [&called](const Json&) { called = true; });

    EXPECT_TRUE(router.handlePayload(
        R"({"jsonrpc":"2.0","method":"test/notify","params":{}})").empty());
    EXPECT_TRUE(called);
}

TEST(JsonRpcRouterTests, RejectsDuplicateRoutesAcrossKinds) {
    JsonRpcRouter router;
    router.registerRequest("test/duplicate", [](const Json&) { return Json(nullptr); });

    EXPECT_THROW(router.registerNotification("test/duplicate", [](const Json&) {}), std::logic_error);
    EXPECT_NO_THROW(router.requireRoutes({"test/duplicate"}));
    EXPECT_THROW(router.requireRoutes({"test/missing"}), std::logic_error);
}

TEST(JsonRpcRouterTests, ReturnsStandardProtocolErrors) {
    JsonRpcRouter router;

    auto responses = router.handlePayload("{");
    ASSERT_EQ(responses.size(), 1);
    EXPECT_EQ(Json::parse(responses.front())["error"]["code"], -32700);

    responses = router.handlePayload(R"({"jsonrpc":"2.0","id":1,"method":"missing"})");
    ASSERT_EQ(responses.size(), 1);
    EXPECT_EQ(Json::parse(responses.front())["error"]["code"], -32601);
}

TEST(JsonRpcRouterTests, MapsBindingFailuresToInvalidParams) {
    JsonRpcRouter router;
    router.registerRequest("test/required", [](const Json& params) {
        return params.at("required");
    });

    const auto responses = router.handlePayload(
        R"({"jsonrpc":"2.0","id":2,"method":"test/required","params":{}})");

    ASSERT_EQ(responses.size(), 1);
    EXPECT_EQ(Json::parse(responses.front())["error"]["code"], -32602);
}

TEST(JsonRpcRouterTests, BatchesResponsesAndSuppressesNotifications) {
    JsonRpcRouter router;
    router.registerRequest("test/request", [](const Json&) { return 7; });
    router.registerNotification("test/notification", [](const Json&) {});

    const auto responses = router.handlePayload(R"([
        {"jsonrpc":"2.0","id":1,"method":"test/request"},
        {"jsonrpc":"2.0","method":"test/notification"}
    ])");

    ASSERT_EQ(responses.size(), 1);
    const auto batch = Json::parse(responses.front());
    ASSERT_EQ(batch.size(), 1);
    EXPECT_EQ(batch[0]["result"], 7);
}

} // namespace