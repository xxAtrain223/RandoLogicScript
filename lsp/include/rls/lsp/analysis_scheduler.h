#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "analysis_snapshot.h"

namespace rls::lsp {

struct AnalysisRequest {
    std::string projectId;
    uint64_t generation = 0;
    std::vector<sema::SourceInput> sources;
};

class AnalysisScheduler {
public:
    using Snapshot = std::shared_ptr<const sema::AnalysisSnapshot>;
    using Builder = std::function<std::optional<Snapshot>(
        std::vector<sema::SourceInput>, uint64_t, std::stop_token)>;
    using AcceptedHandler = std::function<void(std::string, Snapshot)>;

    struct Options {
        std::chrono::milliseconds debounce{75};
        size_t maximumConcurrency = 2;
    };

    AnalysisScheduler();
    explicit AnalysisScheduler(Options options, Builder builder = {});
    ~AnalysisScheduler();

    AnalysisScheduler(const AnalysisScheduler&) = delete;
    AnalysisScheduler& operator=(const AnalysisScheduler&) = delete;

    bool schedule(AnalysisRequest request);
    void setAcceptedHandler(AcceptedHandler handler);
    Snapshot acceptedSnapshot(std::string_view projectId) const;
    void waitForIdle();

private:
    struct PendingRequest {
        AnalysisRequest request;
        std::chrono::steady_clock::time_point readyAt;
    };

    struct ProjectState {
        uint64_t latestGeneration = 0;
        std::optional<PendingRequest> pending;
        std::shared_ptr<std::stop_source> activeCancellation;
        Snapshot accepted;
    };

    void worker(std::stop_token shutdown);
    bool isIdle() const;

    Options options_;
    Builder builder_;
    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::condition_variable idle_;
    std::unordered_map<std::string, ProjectState> projects_;
    AcceptedHandler acceptedHandler_;
    std::vector<std::jthread> workers_;
    size_t activeBuilds_ = 0;
};

} // namespace rls::lsp