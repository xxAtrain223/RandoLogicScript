#include <stdexcept>

#include <gtest/gtest.h>

#include "rls/lsp/message_framer.h"

namespace {

using rls::lsp::MessageFramer;

TEST(MessageFramerTests, FramesPayloadUsingByteLength) {
    EXPECT_EQ(MessageFramer::frame("{}"), "Content-Length: 2\r\n\r\n{}");
}

TEST(MessageFramerTests, WaitsForACompleteSplitFrame) {
    MessageFramer framer;
    framer.append("Content-Length: 2\r\n");
    EXPECT_FALSE(framer.popMessage().has_value());

    framer.append("\r\n{}");
    ASSERT_TRUE(framer.popMessage().has_value());
}

TEST(MessageFramerTests, PreservesFollowingFrames) {
    MessageFramer framer;
    framer.append("Content-Length: 2\r\n\r\n{}Content-Length: 2\r\n\r\n[]");

    EXPECT_EQ(framer.popMessage(), "{}");
    EXPECT_EQ(framer.popMessage(), "[]");
    EXPECT_FALSE(framer.popMessage().has_value());
}

TEST(MessageFramerTests, AcceptsCaseInsensitiveHeaderName) {
    MessageFramer framer;
    framer.append("content-length:\t2\r\n\r\n{}");

    EXPECT_EQ(framer.popMessage(), "{}");
}

TEST(MessageFramerTests, RejectsInvalidOrDuplicateLengths) {
    MessageFramer invalid;
    invalid.append("Content-Length: 2x\r\n\r\n{}");
    EXPECT_THROW((void)invalid.popMessage(), std::runtime_error);

    MessageFramer duplicate;
    duplicate.append("Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}");
    EXPECT_THROW((void)duplicate.popMessage(), std::runtime_error);
}

TEST(MessageFramerTests, EnforcesConfiguredLimits) {
    MessageFramer payloadLimited(1);
    payloadLimited.append("Content-Length: 2\r\n\r\n{}");
    EXPECT_THROW((void)payloadLimited.popMessage(), std::runtime_error);

    MessageFramer headerLimited(16, 24);
    headerLimited.append("X-Long: 12345678901234567890");
    EXPECT_THROW((void)headerLimited.popMessage(), std::runtime_error);
}

} // namespace