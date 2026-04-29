#include <gtest/gtest.h>

#include <stdexcept>

#include "transport.h"

namespace {

using rls::lsp::JsonRpcMessageFramer;

TEST(TransportTests, EncodeAddsContentLengthHeader) {
    const auto framed = JsonRpcMessageFramer::encode("{}");
    EXPECT_EQ(framed, "Content-Length: 2\r\n\r\n{}");
}

TEST(TransportTests, PopMessageReturnsNulloptWhenIncomplete) {
    JsonRpcMessageFramer framer;

    framer.append("Content-Length: 20\r\n\r\n{\"jsonrpc\":\"2.0\"");

    EXPECT_FALSE(framer.popMessage().has_value());
}

TEST(TransportTests, PopMessageReadsExactPayload) {
    JsonRpcMessageFramer framer;
    framer.append("Content-Length: 17\r\n\r\n{\"jsonrpc\":\"2.0\"}");

    auto payload = framer.popMessage();
    ASSERT_TRUE(payload.has_value());
    EXPECT_EQ(*payload, "{\"jsonrpc\":\"2.0\"}");
}

TEST(TransportTests, PopMessageParsesTwoFrames) {
    JsonRpcMessageFramer framer;
    framer.append("Content-Length: 2\r\n\r\n{}Content-Length: 2\r\n\r\n[]");

    auto first = framer.popMessage();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, "{}");

    auto second = framer.popMessage();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, "[]");
}

TEST(TransportTests, MissingContentLengthThrows) {
    JsonRpcMessageFramer framer;
    framer.append("X-Test: 1\r\n\r\n{}");

    EXPECT_THROW((void)framer.popMessage(), std::runtime_error);
}

} // namespace
