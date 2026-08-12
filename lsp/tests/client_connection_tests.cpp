#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <sstream>
#include <streambuf>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rls/lsp/client_connection.h"
#include "rls/lsp/message_framer.h"
#include "rls/lsp/server_composition_root.h"

namespace {

namespace fs = std::filesystem;

using Json = nlohmann::json;
using rls::lsp::ClientConnection;
using rls::lsp::MessageFramer;
using rls::lsp::ServerCompositionRoot;

class BlockingInputBuffer : public std::streambuf {
public:
    void append(std::string bytes) {
        {
            std::lock_guard lock(mutex_);
            for (char byte : bytes) bytes_.push_back(byte);
        }
        ready_.notify_all();
    }

    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        ready_.notify_all();
    }

protected:
    int_type underflow() override {
        std::unique_lock lock(mutex_);
        ready_.wait(lock, [this] { return closed_ || !bytes_.empty(); });
        if (bytes_.empty()) return traits_type::eof();
        current_ = bytes_.front();
        bytes_.pop_front();
        setg(&current_, &current_, &current_ + 1);
        return traits_type::to_int_type(current_);
    }

private:
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<char> bytes_;
    char current_ = 0;
    bool closed_ = false;
};

class CapturingOutputBuffer : public std::streambuf {
public:
    bool waitForOccurrences(std::string_view value, size_t count) {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(3), [&] {
            size_t occurrences = 0;
            size_t position = 0;
            while ((position = bytes_.find(value, position)) != std::string::npos) {
                ++occurrences;
                position += value.size();
            }
            return occurrences >= count;
        });
    }

    std::string bytes() const {
        std::lock_guard lock(mutex_);
        return bytes_;
    }

protected:
    std::streamsize xsputn(const char* bytes, std::streamsize count) override {
        {
            std::lock_guard lock(mutex_);
            bytes_.append(bytes, static_cast<size_t>(count));
        }
        changed_.notify_all();
        return count;
    }

    int_type overflow(int_type value) override {
        if (traits_type::eq_int_type(value, traits_type::eof())) return traits_type::not_eof(value);
        const char byte = traits_type::to_char_type(value);
        return xsputn(&byte, 1) == 1 ? value : traits_type::eof();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::string bytes_;
};

class InputCloseGuard {
public:
    explicit InputCloseGuard(BlockingInputBuffer& input) : input_(input) {}
    ~InputCloseGuard() { input_.close(); }

private:
    BlockingInputBuffer& input_;
};

std::vector<Json> decodeFrames(std::string_view bytes) {
    MessageFramer framer;
    framer.append(bytes);
    std::vector<Json> messages;
    while (const auto payload = framer.popMessage()) messages.push_back(Json::parse(*payload));
    return messages;
}

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

TEST(ClientConnectionTests, PublishesLiveDiagnosticsWhileWaitingForInput) {
    const fs::path directory = fs::temp_directory_path() /
        ("rls-lsp-live-diagnostics-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(directory);
    const fs::path sourcePath = directory / "main.rls";
    std::ofstream(sourcePath) << "define disk(): true\n";
    const std::string uri = static_cast<std::string>((fs::path(sourcePath)).generic_string());
#ifdef _WIN32
    const std::string fileUri = "file:///" + uri;
#else
    const std::string fileUri = "file://" + uri;
#endif

    BlockingInputBuffer inputBuffer;
    std::istream input(&inputBuffer);
    CapturingOutputBuffer outputBuffer;
    std::ostream output(&outputBuffer);
    std::ostringstream log;
    ServerCompositionRoot server;
    ClientConnection connection(input, output, log);
    auto running = std::async(std::launch::async, [&] { return connection.run(server); });
    InputCloseGuard closeInput(inputBuffer);

    inputBuffer.append(MessageFramer::frame(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})"));
    inputBuffer.append(MessageFramer::frame(
        R"({"jsonrpc":"2.0","method":"initialized","params":{}})"));
    inputBuffer.append(MessageFramer::frame(
        Json{{"jsonrpc", "2.0"}, {"method", "textDocument/didOpen"},
            {"params", {{"textDocument", {
                {"uri", fileUri}, {"languageId", "rls"}, {"version", 1},
                {"text", "region RR_TEST { events { EVENT_TEST: \"invalid\" } }\n"},
            }}}}}.dump()));
    ASSERT_TRUE(outputBuffer.waitForOccurrences("textDocument/publishDiagnostics", 1));

    inputBuffer.append(MessageFramer::frame(
        Json{{"jsonrpc", "2.0"}, {"method", "textDocument/didChange"},
            {"params", {
                {"textDocument", {{"uri", fileUri}, {"version", 2}}},
                {"contentChanges", Json::array({{{"text",
                    "region RR_TEST { events { EVENT_TEST: true } }\n"}}})},
            }}}.dump()));
    ASSERT_TRUE(outputBuffer.waitForOccurrences("textDocument/publishDiagnostics", 2));

    inputBuffer.append(MessageFramer::frame(
        Json{{"jsonrpc", "2.0"}, {"method", "textDocument/didClose"},
            {"params", {{"textDocument", {{"uri", fileUri}}}}}}.dump()));
    ASSERT_TRUE(outputBuffer.waitForOccurrences("textDocument/publishDiagnostics", 3));
    inputBuffer.append(MessageFramer::frame(
        R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})"));
    inputBuffer.append(MessageFramer::frame(
        R"({"jsonrpc":"2.0","method":"exit"})"));
    inputBuffer.close();

    EXPECT_EQ(running.get(), 0);
    EXPECT_TRUE(log.str().empty());
    const auto messages = decodeFrames(outputBuffer.bytes());
    std::vector<Json> diagnostics;
    for (const auto& message : messages) {
        if (message.value("method", "") == "textDocument/publishDiagnostics") {
            diagnostics.push_back(message);
        }
    }
    ASSERT_GE(diagnostics.size(), 3);
    EXPECT_FALSE(diagnostics[0]["params"]["diagnostics"].empty());
    EXPECT_TRUE(diagnostics[1]["params"]["diagnostics"].empty());
    EXPECT_TRUE(diagnostics[2]["params"]["diagnostics"].empty());

    std::error_code error;
    fs::remove_all(directory, error);
}

} // namespace