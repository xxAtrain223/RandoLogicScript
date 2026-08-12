#include <sstream>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rls/lsp/client_connection.h"
#include "rls/lsp/message_framer.h"
#include "rls/lsp/server_composition_root.h"

namespace {

using Json = nlohmann::json;
using rls::lsp::ClientConnection;
using rls::lsp::MessageFramer;
using rls::lsp::ServerCompositionRoot;

TEST(ClientConnectionTests, ExchangesOnlyFramedProtocolMessagesOnOutput) {
    const std::string inputBytes = MessageFramer::frame(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})")
        + MessageFramer::frame(R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})")
        + MessageFramer::frame(R"({"jsonrpc":"2.0","method":"exit"})");
    std::istringstream input(inputBytes);
    std::ostringstream output;
    std::ostringstream log;
    ServerCompositionRoot server;
    ClientConnection connection(input, output, log);

    EXPECT_EQ(connection.run(server), 0);
    EXPECT_TRUE(log.str().empty());

    MessageFramer responses;
    responses.append(output.str());
    ASSERT_TRUE(responses.popMessage().has_value());
    const auto shutdown = responses.popMessage();
    ASSERT_TRUE(shutdown.has_value());
    EXPECT_EQ(Json::parse(*shutdown)["id"], 2);
    EXPECT_FALSE(responses.popMessage().has_value());
}

TEST(ClientConnectionTests, ReportsTransportErrorsOnlyToLogStream) {
    std::istringstream input("Bad: header\r\n\r\n");
    std::ostringstream output;
    std::ostringstream log;
    ServerCompositionRoot server;
    ClientConnection connection(input, output, log);

    EXPECT_EQ(connection.run(server), 1);
    EXPECT_TRUE(output.str().empty());
    EXPECT_FALSE(log.str().empty());
}

} // namespace