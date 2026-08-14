#include <algorithm>
#include <condition_variable>
#include <cstdint>
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

} // namespace