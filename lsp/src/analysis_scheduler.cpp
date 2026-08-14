#include "rls/lsp/analysis_scheduler.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rls::lsp {
namespace {

std::optional<AnalysisScheduler::Snapshot> buildSnapshot(
    std::vector<sema::SourceInput> sources, uint64_t generation,
    std::stop_token cancellation) {
    return sema::AnalysisSnapshot::Create(
        std::move(sources), generation, cancellation);
}

} // namespace

AnalysisScheduler::AnalysisScheduler()
    : AnalysisScheduler(Options{}) {}

AnalysisScheduler::AnalysisScheduler(Options options, Builder builder)
    : options_(options), builder_(builder ? std::move(builder) : Builder(buildSnapshot)) {
    if (options_.maximumConcurrency == 0) {
        throw std::invalid_argument("analysis concurrency must be at least one");
    }
    workers_.reserve(options_.maximumConcurrency);
    for (size_t index = 0; index < options_.maximumConcurrency; ++index) {
        workers_.emplace_back([this](std::stop_token shutdown) { worker(shutdown); });
    }
}

AnalysisScheduler::~AnalysisScheduler() {
    for (auto& workerThread : workers_) {
        workerThread.request_stop();
    }
    {
        std::lock_guard lock(mutex_);
        for (auto& [projectId, state] : projects_) {
            if (state.activeCancellation) {
                state.activeCancellation->request_stop();
            }
        }
    }
    wake_.notify_all();
    workers_.clear();
}

bool AnalysisScheduler::schedule(AnalysisRequest request) {
    if (request.projectId.empty() || request.sources.empty()) {
        return false;
    }

    std::lock_guard lock(mutex_);
    ProjectState& state = projects_[request.projectId];
    if (request.generation <= state.latestGeneration
        || request.documentGeneration < state.latestDocumentGeneration
        || request.manifestGeneration < state.latestManifestGeneration) {
        return false;
    }

    state.latestGeneration = request.generation;
    state.latestDocumentGeneration = request.documentGeneration;
    state.latestManifestGeneration = request.manifestGeneration;
    state.removed = false;
    if (state.activeCancellation) {
        state.activeCancellation->request_stop();
    }
    state.pending = PendingRequest{
        std::move(request),
        std::chrono::steady_clock::now() + options_.debounce,
    };
    wake_.notify_all();
    return true;
}

void AnalysisScheduler::removeProject(std::string_view projectId) {
    std::lock_guard lock(mutex_);
    const auto project = projects_.find(std::string(projectId));
    if (project == projects_.end()) {
        return;
    }
    ProjectState& state = project->second;
    state.removed = true;
    state.pending.reset();
    state.accepted.reset();
    if (state.activeCancellation) {
        state.activeCancellation->request_stop();
    }
    if (isIdle()) {
        idle_.notify_all();
    }
    wake_.notify_all();
}

void AnalysisScheduler::setAcceptedHandler(AcceptedHandler handler) {
    std::lock_guard lock(mutex_);
    acceptedHandler_ = std::move(handler);
}

AnalysisScheduler::Snapshot AnalysisScheduler::acceptedSnapshot(std::string_view projectId) const {
    std::lock_guard lock(mutex_);
    const auto project = projects_.find(std::string(projectId));
    return project == projects_.end() ? nullptr : project->second.accepted;
}

void AnalysisScheduler::waitForIdle() {
    std::unique_lock lock(mutex_);
    idle_.wait(lock, [this] { return isIdle(); });
}

bool AnalysisScheduler::isIdle() const {
    if (activeBuilds_ != 0) {
        return false;
    }
    return std::none_of(projects_.begin(), projects_.end(), [](const auto& entry) {
        return entry.second.pending.has_value();
    });
}

void AnalysisScheduler::worker(std::stop_token shutdown) {
    while (!shutdown.stop_requested()) {
        AnalysisRequest request;
        std::shared_ptr<std::stop_source> cancellation;

        {
            std::unique_lock lock(mutex_);
            while (!shutdown.stop_requested()) {
                const auto now = std::chrono::steady_clock::now();
                auto selected = projects_.end();
                auto nextReady = std::chrono::steady_clock::time_point::max();

                for (auto project = projects_.begin(); project != projects_.end(); ++project) {
                    ProjectState& state = project->second;
                    if (state.removed || !state.pending || state.activeCancellation) {
                        continue;
                    }
                    if (state.pending->readyAt <= now) {
                        selected = project;
                        break;
                    }
                    nextReady = std::min(nextReady, state.pending->readyAt);
                }

                if (selected != projects_.end()) {
                    ProjectState& state = selected->second;
                    request = std::move(state.pending->request);
                    state.pending.reset();
                    cancellation = std::make_shared<std::stop_source>();
                    state.activeCancellation = cancellation;
                    ++activeBuilds_;
                    break;
                }

                if (nextReady == std::chrono::steady_clock::time_point::max()) {
                    wake_.wait(lock);
                } else {
                    wake_.wait_until(lock, nextReady);
                }
            }
        }

        if (shutdown.stop_requested()) {
            break;
        }

        std::optional<Snapshot> snapshot;
        try {
            snapshot = builder_(
                std::move(request.sources), request.generation, cancellation->get_token());
        } catch (...) {
            snapshot = std::nullopt;
        }

        AcceptedHandler acceptedHandler;
        Snapshot acceptedSnapshot;
        {
            std::lock_guard lock(mutex_);
            ProjectState& state = projects_.at(request.projectId);
            if (state.activeCancellation == cancellation) {
                state.activeCancellation.reset();
                if (snapshot && !cancellation->stop_requested()
                    && !state.removed && state.latestGeneration == request.generation
                    && state.latestDocumentGeneration == request.documentGeneration
                    && state.latestManifestGeneration == request.manifestGeneration) {
                    state.accepted = std::move(*snapshot);
                    acceptedSnapshot = state.accepted;
                    acceptedHandler = acceptedHandler_;
                }
            }
            --activeBuilds_;
            if (isIdle()) {
                idle_.notify_all();
            }
        }
        if (acceptedHandler && acceptedSnapshot) {
            try {
                acceptedHandler(request.projectId, std::move(acceptedSnapshot));
            } catch (...) {
            }
        }
        wake_.notify_all();
    }
}

} // namespace rls::lsp