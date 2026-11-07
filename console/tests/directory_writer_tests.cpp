#include <chrono>
#include <iterator>
#include <sstream>

#include <gtest/gtest.h>

#include "../directory_writer.h"

namespace {

namespace fs = std::filesystem;

class ConsoleDirectoryWriter : public testing::Test {
protected:
    fs::path directory = fs::temp_directory_path() / ("rls-writer-tests-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    void SetUp() override { fs::create_directories(directory); }
    void TearDown() override {
        std::error_code error;
        fs::remove_all(directory, error);
    }
};

class FailingBuffer : public std::streambuf {
public:
    int sync() override { return -1; }
    std::streamsize xsputn(const char*, std::streamsize) override { return 0; }
};

TEST_F(ConsoleDirectoryWriter, FinishesEveryOutputBeforeReportingSuccess) {
    DirectoryWriter writer(directory);
    writer.open("first.txt") << "first output";
    writer.open("second.txt") << "second output";
    ASSERT_NO_THROW(writer.finish());
    for (const auto& name : {"first", "second"}) {
        std::ifstream input(directory / (std::string(name) + ".txt"));
        EXPECT_EQ(std::string(std::istreambuf_iterator<char>{input}, {}),
            std::string(name) + " output");
    }
}

TEST_F(ConsoleDirectoryWriter, ReportsOpenFailures) {
    fs::create_directory(directory / "blocked.txt");
    DirectoryWriter writer(directory);
    EXPECT_THROW(writer.open("blocked.txt"), std::ios_base::failure);
}

TEST_F(ConsoleDirectoryWriter, ReportsWriteFailures) {
    DirectoryWriter writer(directory);
    auto& output = writer.open("failed.txt");
    FailingBuffer buffer;
    auto* original = output.rdbuf(&buffer);
    EXPECT_THROW(output << "output", std::ios_base::failure);
    output.exceptions(std::ios::goodbit);
    output.rdbuf(original);
}

TEST_F(ConsoleDirectoryWriter, ReportsFlushFailuresIncludingLaterOutputs) {
    DirectoryWriter writer(directory);
    writer.open("first.txt") << "first output";
    auto& output = writer.open("failed.txt");
    FailingBuffer buffer;
    auto* original = output.rdbuf(&buffer);
    try {
        writer.finish();
        ADD_FAILURE() << "flush failure was not reported";
    } catch (const std::ios_base::failure& error) {
        EXPECT_NE(std::string(error.what()).find("failed.txt"), std::string::npos);
    }
    output.exceptions(std::ios::goodbit);
    output.rdbuf(original);
}

TEST_F(ConsoleDirectoryWriter, ReportsCloseFailures) {
    DirectoryWriter writer(directory);
    auto& output = writer.open("closed.txt");
    dynamic_cast<std::ofstream&>(output).close();
    EXPECT_THROW(writer.finish(), std::ios_base::failure);
}

} // namespace
