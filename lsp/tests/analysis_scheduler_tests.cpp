#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "analysis_snapshot.h"
#include "rls/lsp/analysis_scheduler.h"

namespace {

using rls::lsp::AnalysisRequest;
using rls::lsp::AnalysisScheduler;

std::optional<AnalysisScheduler::Snapshot> snapshotFor(
    std::vector<rls::sema::SourceInput> sources, uint64_t generation) {
    return rls::sema::AnalysisSnapshot::Create(std::move(sources), generation);
}

AnalysisRequest request(std::string projectId, uint64_t generation) {
    return {
        std::move(projectId),
        generation,
        {{"main.rls", "define value(): true\n"}},
    };
}

TEST(AnalysisSchedulerTests, DebouncesPendingWorkPerProject) {
    std::mutex mutex;
    std::vector<uint64_t> builtGenerations;
    AnalysisScheduler scheduler(
        {.debounce = std::chrono::milliseconds(40), .maximumConcurrency = 1},
        [&](std::vector<rls::sema::SourceInput> sources, uint64_t generation,
            std::stop_token) {
            {
                std::lock_guard lock(mutex);
                builtGenerations.push_back(generation);
            }
            return snapshotFor(std::move(sources), generation);
        });

    EXPECT_TRUE(scheduler.schedule(request("project", 1)));
    EXPECT_TRUE(scheduler.schedule(request("project", 2)));
    EXPECT_FALSE(scheduler.schedule(request("project", 2)));
    scheduler.waitForIdle();

    std::lock_guard lock(mutex);
    ASSERT_EQ(builtGenerations.size(), 1);
    EXPECT_EQ(builtGenerations.front(), 2);
    ASSERT_NE(scheduler.acceptedSnapshot("project"), nullptr);
    EXPECT_EQ(scheduler.acceptedSnapshot("project")->generation(), 2);
}

TEST(AnalysisSchedulerTests, AwaitSnapshotExpeditesPendingGeneration) {
    AnalysisScheduler scheduler(
        {.debounce = std::chrono::seconds(5), .maximumConcurrency = 1});

    ASSERT_TRUE(scheduler.schedule(request("project", 1)));
    const auto snapshot = scheduler.awaitSnapshot(
        "project", 1, std::chrono::seconds(1));

    ASSERT_NE(snapshot, nullptr);
    EXPECT_EQ(snapshot->generation(), 1u);
}

TEST(AnalysisSchedulerTests, AwaitSnapshotFailsClosedWithoutScheduledGeneration) {
    AnalysisScheduler scheduler(
        {.debounce = std::chrono::seconds(5), .maximumConcurrency = 1});
    ASSERT_TRUE(scheduler.schedule(request("project", 1)));

    EXPECT_EQ(scheduler.awaitSnapshot(
        "project", 2, std::chrono::milliseconds(10)), nullptr);
    const auto snapshot = scheduler.awaitSnapshot(
        "project", 1, std::chrono::seconds(1));
    ASSERT_NE(snapshot, nullptr);
}

TEST(AnalysisSchedulerTests, CancelsRunningWorkAndSuppressesItsResult) {
    std::mutex mutex;
    std::condition_variable started;
    std::condition_variable cancelled;
    bool firstStarted = false;
    bool firstCancelled = false;

    AnalysisScheduler scheduler(
        {.debounce = std::chrono::milliseconds(0), .maximumConcurrency = 1},
        [&](std::vector<rls::sema::SourceInput> sources, uint64_t generation,
            std::stop_token cancellation) -> std::optional<AnalysisScheduler::Snapshot> {
            if (generation == 1) {
                std::unique_lock lock(mutex);
                firstStarted = true;
                started.notify_all();
                std::stop_callback wakeOnCancellation(cancellation, [&] { cancelled.notify_all(); });
                cancelled.wait(lock, [&] { return cancellation.stop_requested(); });
                firstCancelled = true;
            }
            return snapshotFor(std::move(sources), generation);
        });

    ASSERT_TRUE(scheduler.schedule(request("project", 1)));
    {
        std::unique_lock lock(mutex);
        started.wait(lock, [&] { return firstStarted; });
    }
    ASSERT_TRUE(scheduler.schedule(request("project", 2)));
    scheduler.waitForIdle();

    EXPECT_TRUE(firstCancelled);
    const auto accepted = scheduler.acceptedSnapshot("project");
    ASSERT_NE(accepted, nullptr);
    EXPECT_EQ(accepted->generation(), 2);
}

TEST(AnalysisSchedulerTests, BoundsConcurrentBuildsAcrossProjects) {
    std::mutex mutex;
    std::condition_variable started;
    std::condition_variable release;
    size_t active = 0;
    size_t maximumActive = 0;
    size_t startedCount = 0;
    bool mayComplete = false;

    AnalysisScheduler scheduler(
        {.debounce = std::chrono::milliseconds(0), .maximumConcurrency = 2},
        [&](std::vector<rls::sema::SourceInput> sources, uint64_t generation,
            std::stop_token) {
            {
                std::unique_lock lock(mutex);
                ++active;
                ++startedCount;
                maximumActive = std::max(maximumActive, active);
                started.notify_all();
                release.wait(lock, [&] { return mayComplete; });
                --active;
            }
            return snapshotFor(std::move(sources), generation);
        });

    ASSERT_TRUE(scheduler.schedule(request("one", 1)));
    ASSERT_TRUE(scheduler.schedule(request("two", 1)));
    ASSERT_TRUE(scheduler.schedule(request("three", 1)));
    {
        std::unique_lock lock(mutex);
        started.wait(lock, [&] { return startedCount == 2; });
        EXPECT_EQ(maximumActive, 2);
        mayComplete = true;
    }
    release.notify_all();
    scheduler.waitForIdle();

    EXPECT_EQ(startedCount, 3);
    EXPECT_EQ(maximumActive, 2);
}

TEST(AnalysisSchedulerTests, DefaultBuilderCreatesWholeProjectSnapshot) {
    AnalysisScheduler scheduler(
        {.debounce = std::chrono::milliseconds(0), .maximumConcurrency = 1});

    ASSERT_TRUE(scheduler.schedule({
        "project",
        7,
        {
            {"first.rls", "define first(): true\n"},
            {"second.rls", "define second(): first()\n"},
        },
    }));
    scheduler.waitForIdle();

    const auto accepted = scheduler.acceptedSnapshot("project");
    ASSERT_NE(accepted, nullptr);
    EXPECT_EQ(accepted->generation(), 7);
    EXPECT_EQ(accepted->documentCount(), 2);
}

TEST(AnalysisSchedulerTests, BuilderFailureDoesNotStrandScheduler) {
    AnalysisScheduler scheduler(
        {.debounce = std::chrono::milliseconds(0), .maximumConcurrency = 1},
        [](std::vector<rls::sema::SourceInput> sources, uint64_t generation,
            std::stop_token) -> std::optional<AnalysisScheduler::Snapshot> {
            if (generation == 1) {
                throw std::runtime_error("build failed");
            }
            return snapshotFor(std::move(sources), generation);
        });

    ASSERT_TRUE(scheduler.schedule(request("project", 1)));
    scheduler.waitForIdle();
    EXPECT_EQ(scheduler.acceptedSnapshot("project"), nullptr);

    ASSERT_TRUE(scheduler.schedule(request("project", 2)));
    scheduler.waitForIdle();
    ASSERT_NE(scheduler.acceptedSnapshot("project"), nullptr);
    EXPECT_EQ(scheduler.acceptedSnapshot("project")->generation(), 2);
}

TEST(AnalysisSchedulerTests, RejectsRegressedDocumentOrManifestGenerations) {
    AnalysisScheduler scheduler(
        {.debounce = std::chrono::milliseconds(40), .maximumConcurrency = 1});
    AnalysisRequest current = request("project", 10);
    current.documentGeneration = 4;
    current.manifestGeneration = 6;
    ASSERT_TRUE(scheduler.schedule(std::move(current)));

    AnalysisRequest staleDocument = request("project", 11);
    staleDocument.documentGeneration = 3;
    staleDocument.manifestGeneration = 7;
    EXPECT_FALSE(scheduler.schedule(std::move(staleDocument)));

    AnalysisRequest staleManifest = request("project", 12);
    staleManifest.documentGeneration = 5;
    staleManifest.manifestGeneration = 5;
    EXPECT_FALSE(scheduler.schedule(std::move(staleManifest)));
    scheduler.waitForIdle();
    ASSERT_NE(scheduler.acceptedSnapshot("project"), nullptr);
    EXPECT_EQ(scheduler.acceptedSnapshot("project")->generation(), 10);
}

TEST(AnalysisSchedulerTests, CancelsSupersededDiskReadBeforeSnapshotBuild) {
    std::mutex mutex;
    std::condition_variable started;
    std::condition_variable cancelled;
    bool readStarted = false;
    bool readCancelled = false;
    std::vector<uint64_t> builtGenerations;

    AnalysisScheduler scheduler(
        {.debounce = std::chrono::milliseconds(0), .maximumConcurrency = 1},
        [&](std::vector<rls::sema::SourceInput> sources, uint64_t generation,
            std::stop_token) {
            builtGenerations.push_back(generation);
            return snapshotFor(std::move(sources), generation);
        },
        [&](const std::filesystem::path&, std::stop_token cancellationToken)
            -> std::optional<std::string> {
            std::unique_lock lock(mutex);
            readStarted = true;
            started.notify_all();
            std::stop_callback wakeOnCancellation(
                cancellationToken, [&] { cancelled.notify_all(); });
            cancelled.wait(lock, [&] { return cancellationToken.stop_requested(); });
            readCancelled = true;
            return std::nullopt;
        });

    ASSERT_TRUE(scheduler.schedule({
        "project", 1, {{"slow.rls", std::nullopt, "slow.rls"}}, 1, 1,
    }));
    {
        std::unique_lock lock(mutex);
        started.wait(lock, [&] { return readStarted; });
    }
    ASSERT_TRUE(scheduler.schedule({
        "project", 2, {{"fresh.rls", "define fresh(): true\n"}}, 2, 1,
    }));
    scheduler.waitForIdle();

    EXPECT_TRUE(readCancelled);
    ASSERT_EQ(builtGenerations.size(), 1);
    EXPECT_EQ(builtGenerations.front(), 2);
    ASSERT_NE(scheduler.acceptedSnapshot("project"), nullptr);
    EXPECT_EQ(scheduler.acceptedSnapshot("project")->generation(), 2);
}

TEST(AnalysisSchedulerTests, DefaultReaderAnalyzesEmptyDiskFile) {
    const auto path = std::filesystem::temp_directory_path() /
        ("rls-empty-source-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".rls");
    std::ofstream(path, std::ios::binary);
    AnalysisScheduler scheduler(
        {.debounce = std::chrono::milliseconds(0), .maximumConcurrency = 1});

    ASSERT_TRUE(scheduler.schedule({
        "project", 1, {{path.generic_string(), std::nullopt, path}}, 1, 1,
    }));
    scheduler.waitForIdle();

    const auto snapshot = scheduler.acceptedSnapshot("project");
    ASSERT_NE(snapshot, nullptr);
    ASSERT_EQ(snapshot->documentCount(), 1);
    const std::string canonicalPath = std::filesystem::weakly_canonical(path).generic_string();
    ASSERT_NE(snapshot->sourceText(canonicalPath), nullptr);
    EXPECT_TRUE(snapshot->sourceText(canonicalPath)->content().empty());
    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST(AnalysisSchedulerTests, DefaultReaderDecodesBomEncodedSources) {
    const auto directory = std::filesystem::temp_directory_path();
    const auto littleEndianPath = directory / "rls-utf16-le-source.rls";
    const auto bigEndianPath = directory / "rls-utf16-be-source.rls";
    const auto utf8BomPath = directory / "rls-utf8-bom-source.rls";
    {
        std::ofstream output(littleEndianPath, std::ios::binary);
        const char bytes[] = "\xff\xfe" "d\0e\0f\0i\0n\0e\0 \0l\0e\0(\0)\0:\0 \0t\0r\0u\0e\0\n\0";
        output.write(bytes, sizeof(bytes) - 1);
    }
    {
        std::ofstream output(bigEndianPath, std::ios::binary);
        const char bytes[] = "\xfe\xff\0d\0e\0f\0i\0n\0e\0 \0b\0e\0(\0)\0:\0 \0t\0r\0u\0e\0\n";
        output.write(bytes, sizeof(bytes) - 1);
    }
    {
        std::ofstream output(utf8BomPath, std::ios::binary);
        output << "\xef\xbb\xbf" "define utf8(): true\n";
    }

    AnalysisScheduler scheduler(
        {.debounce = std::chrono::milliseconds(0), .maximumConcurrency = 1});
    ASSERT_TRUE(scheduler.schedule({
        "project", 1,
        {
            {littleEndianPath.generic_string(), std::nullopt, littleEndianPath},
            {bigEndianPath.generic_string(), std::nullopt, bigEndianPath},
            {utf8BomPath.generic_string(), std::nullopt, utf8BomPath},
        },
        1, 1,
    }));
    scheduler.waitForIdle();

    const auto snapshot = scheduler.acceptedSnapshot("project");
    ASSERT_NE(snapshot, nullptr);
    EXPECT_EQ(snapshot->documentCount(), 3);
    EXPECT_EQ(snapshot->sourceText(std::filesystem::weakly_canonical(littleEndianPath).generic_string())
                  ->content(),
        "define le(): true\n");
    EXPECT_EQ(snapshot->sourceText(std::filesystem::weakly_canonical(bigEndianPath).generic_string())
                  ->content(),
        "define be(): true\n");
    EXPECT_EQ(snapshot->sourceText(std::filesystem::weakly_canonical(utf8BomPath).generic_string())
                  ->content(),
        "define utf8(): true\n");

    std::error_code error;
    std::filesystem::remove(littleEndianPath, error);
    std::filesystem::remove(bigEndianPath, error);
    std::filesystem::remove(utf8BomPath, error);
}

TEST(AnalysisSchedulerTests, DefaultReaderRejectsMalformedUtf16) {
    const auto path = std::filesystem::temp_directory_path() /
        "rls-malformed-utf16-source.rls";
    {
        std::ofstream output(path, std::ios::binary);
        const char bytes[] = "\xff\xfe\0\xd8";
        output.write(bytes, sizeof(bytes) - 1);
    }

    AnalysisScheduler scheduler(
        {.debounce = std::chrono::milliseconds(0), .maximumConcurrency = 1});
    ASSERT_TRUE(scheduler.schedule({
        "project", 1, {{path.generic_string(), std::nullopt, path}}, 1, 1,
    }));
    scheduler.waitForIdle();

    EXPECT_EQ(scheduler.acceptedSnapshot("project"), nullptr);
    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST(AnalysisSchedulerTests, CanonicalizesFilesystemOverlayIdentity) {
    const auto path = std::filesystem::temp_directory_path() /
        "rls-overlay-parent" / ".." / "rls-overlay-source.rls";
    AnalysisScheduler scheduler(
        {.debounce = std::chrono::milliseconds(0), .maximumConcurrency = 1});

    ASSERT_TRUE(scheduler.schedule({
        "project", 1, {{path.generic_string(), "define ready(): true\n"}}, 1, 1,
    }));
    scheduler.waitForIdle();

    const auto snapshot = scheduler.acceptedSnapshot("project");
    ASSERT_NE(snapshot, nullptr);
    const std::string canonicalPath = std::filesystem::weakly_canonical(path).generic_string();
    ASSERT_NE(snapshot->sourceText(canonicalPath), nullptr);
}

TEST(AnalysisSchedulerTests, DiskReadFailureDoesNotInvokeSnapshotBuilder) {
    size_t buildCount = 0;
    AnalysisScheduler scheduler(
        {.debounce = std::chrono::milliseconds(0), .maximumConcurrency = 1},
        [&](std::vector<rls::sema::SourceInput> sources, uint64_t generation,
            std::stop_token) {
            ++buildCount;
            return snapshotFor(std::move(sources), generation);
        },
        [](const std::filesystem::path&, std::stop_token)
            -> std::optional<std::string> { return std::nullopt; });

    ASSERT_TRUE(scheduler.schedule({
        "project", 1, {{"missing.rls", std::nullopt, "missing.rls"}}, 1, 1,
    }));
    scheduler.waitForIdle();

    EXPECT_EQ(buildCount, 0);
    EXPECT_EQ(scheduler.acceptedSnapshot("project"), nullptr);
}

} // namespace