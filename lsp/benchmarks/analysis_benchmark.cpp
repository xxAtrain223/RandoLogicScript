#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "rls/lsp/analysis_scheduler.h"

#ifndef RLS_BENCHMARK_BUILD_TYPE
#define RLS_BENCHMARK_BUILD_TYPE "unknown"
#endif

namespace {

using Clock = std::chrono::steady_clock;
using Milliseconds = std::chrono::duration<double, std::milli>;
using rls::lsp::AnalysisRequest;
using rls::lsp::AnalysisScheduler;
using rls::lsp::AnalysisSource;
using rls::lsp::AnalysisTimings;

constexpr std::array<std::string_view, 9> StageNames{
    "source_read",
    "parse",
    "declaration_collection",
    "type_resolution",
    "validation",
    "semantic_index",
    "snapshot_other",
    "snapshot_replacement",
    "scheduler_other",
};

struct Measurement {
    double wallMs = 0.0;
    std::array<double, StageNames.size()> stagesMs{};
};

struct Options {
    std::filesystem::path projectRoot;
    size_t iterations = 10;
    size_t warmupIterations = 2;
};

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("unable to read " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

Options parseOptions(int argc, char** argv) {
    if (argc < 2) {
        throw std::runtime_error(
            "usage: rls_lsp_benchmark <project-root> [--iterations N] [--warmup N]");
    }
    Options options{std::filesystem::absolute(argv[1])};
    for (int index = 2; index < argc; index += 2) {
        if (index + 1 >= argc) {
            throw std::runtime_error("missing value for " + std::string(argv[index]));
        }
        const auto value = static_cast<size_t>(std::stoull(argv[index + 1]));
        const std::string argument = argv[index];
        if (argument == "--iterations") {
            options.iterations = value;
        } else if (argument == "--warmup") {
            options.warmupIterations = value;
        } else {
            throw std::runtime_error("unknown argument " + argument);
        }
    }
    if (options.iterations == 0) {
        throw std::runtime_error("iterations must be at least one");
    }
    return options;
}

std::vector<std::filesystem::path> discoverSources(
    const std::filesystem::path& projectRoot) {
    const auto manifest = nlohmann::json::parse(readFile(projectRoot / "rls.json"));
    std::vector<std::filesystem::path> sources;
    for (const auto& sourceRoot : manifest.at("sources")) {
        const auto root = projectRoot / sourceRoot.get<std::string>();
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (entry.is_regular_file() && entry.path().extension() == ".rls") {
                sources.push_back(std::filesystem::weakly_canonical(entry.path()));
            }
        }
    }
    std::sort(sources.begin(), sources.end());
    if (sources.empty()) {
        throw std::runtime_error("project contains no .rls sources");
    }
    return sources;
}

std::vector<AnalysisSource> makeSources(
    const std::vector<std::filesystem::path>& paths,
    const std::filesystem::path& editedPath,
    const std::optional<std::string>& editedContent) {
    std::vector<AnalysisSource> sources;
    sources.reserve(paths.size());
    for (const auto& path : paths) {
        sources.push_back({
            path.generic_string(),
            path == editedPath ? editedContent : std::nullopt,
            path,
        });
    }
    return sources;
}

double percentile(std::vector<double> samples, double quantile) {
    std::sort(samples.begin(), samples.end());
    const auto index = static_cast<size_t>(quantile * static_cast<double>(samples.size() - 1));
    return samples[index];
}

double milliseconds(std::chrono::nanoseconds duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

Measurement runGeneration(
    AnalysisScheduler& scheduler, AnalysisRequest request) {
    const auto generation = request.generation;
    const auto timings = std::make_shared<AnalysisTimings>();
    request.timings = timings;
    const auto started = Clock::now();
    if (!scheduler.schedule(std::move(request))) {
        throw std::runtime_error("scheduler rejected benchmark generation");
    }
    scheduler.waitForIdle();
    const double elapsed = Milliseconds(Clock::now() - started).count();
    const auto snapshot = scheduler.acceptedSnapshot("benchmark");
    if (!snapshot || snapshot->generation() != generation) {
        throw std::runtime_error("benchmark generation did not produce a snapshot");
    }
    const auto& snapshotTimings = timings->snapshot;
    const std::array<double, 6> measuredStages{
        milliseconds(timings->sourceRead),
        milliseconds(snapshotTimings.parse),
        milliseconds(snapshotTimings.analysis.declarationCollection),
        milliseconds(snapshotTimings.analysis.typeResolution),
        milliseconds(snapshotTimings.analysis.validation),
        milliseconds(snapshotTimings.semanticIndex),
    };
    const double compilerStages = std::accumulate(
        measuredStages.begin() + 1, measuredStages.end(), 0.0);
    const double snapshotOther = std::max(
        0.0, milliseconds(timings->snapshotBuild) - compilerStages);
    const double snapshotReplacement = milliseconds(timings->snapshotReplacement);
    const double schedulerOther = std::max(
        0.0, elapsed - measuredStages.front() - milliseconds(timings->snapshotBuild)
            - snapshotReplacement);
    return {
        elapsed,
        {
            measuredStages[0], measuredStages[1], measuredStages[2],
            measuredStages[3], measuredStages[4], measuredStages[5],
            snapshotOther, snapshotReplacement, schedulerOther,
        },
    };
}

nlohmann::json stageValues(const std::array<double, StageNames.size()>& stages) {
    nlohmann::json result = nlohmann::json::object();
    for (size_t index = 0; index < StageNames.size(); ++index) {
        result[StageNames[index]] = stages[index];
    }
    return result;
}

nlohmann::json aggregateStages(
    const std::vector<Measurement>& samples,
    const std::function<double(std::vector<double>)>& aggregate) {
    nlohmann::json result = nlohmann::json::object();
    for (size_t stage = 0; stage < StageNames.size(); ++stage) {
        std::vector<double> values;
        values.reserve(samples.size());
        for (const auto& sample : samples) values.push_back(sample.stagesMs[stage]);
        result[StageNames[stage]] = aggregate(std::move(values));
    }
    return result;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parseOptions(argc, argv);
        const auto paths = discoverSources(options.projectRoot);
        const auto editedPath = paths.front();
        const auto originalContent = readFile(editedPath);
        AnalysisScheduler scheduler(
            {.debounce = std::chrono::milliseconds(0), .maximumConcurrency = 1});

        uint64_t generation = 1;
        const Measurement initial = runGeneration(scheduler, {
            "benchmark", generation++, makeSources(paths, editedPath, std::nullopt), 1, 1,
        });

        std::vector<Measurement> measurements;
        for (size_t iteration = 0;
             iteration < options.warmupIterations + options.iterations; ++iteration) {
            std::string changedContent = originalContent;
            changedContent += iteration % 2 == 0 ? "\n" : "\n\n";
            const Measurement measurement = runGeneration(scheduler, {
                "benchmark", generation++,
                makeSources(paths, editedPath, std::move(changedContent)),
                generation, 1,
            });
            if (iteration >= options.warmupIterations) {
                measurements.push_back(measurement);
            }
        }

        std::vector<double> samples;
        samples.reserve(measurements.size());
        for (const auto& measurement : measurements) samples.push_back(measurement.wallMs);
        const double total = std::accumulate(samples.begin(), samples.end(), 0.0);
        const nlohmann::json result{
            {"build_type", RLS_BENCHMARK_BUILD_TYPE},
            {"project", options.projectRoot.generic_string()},
            {"source_count", paths.size()},
            {"edited_source", editedPath.generic_string()},
            {"iterations", options.iterations},
            {"warmup_iterations", options.warmupIterations},
            {"initial_analysis_ms", initial.wallMs},
            {"initial_stages_ms", stageValues(initial.stagesMs)},
            {"edit_mean_ms", total / static_cast<double>(samples.size())},
            {"edit_median_ms", percentile(samples, 0.5)},
            {"edit_p95_ms", percentile(samples, 0.95)},
            {"edit_min_ms", *std::min_element(samples.begin(), samples.end())},
            {"edit_max_ms", *std::max_element(samples.begin(), samples.end())},
            {"edit_samples_ms", samples},
            {"edit_stage_mean_ms", aggregateStages(measurements, [](std::vector<double> values) {
                return std::accumulate(values.begin(), values.end(), 0.0)
                    / static_cast<double>(values.size());
            })},
            {"edit_stage_median_ms", aggregateStages(measurements, [](std::vector<double> values) {
                return percentile(std::move(values), 0.5);
            })},
            {"edit_stage_p95_ms", aggregateStages(measurements, [](std::vector<double> values) {
                return percentile(std::move(values), 0.95);
            })},
        };
        std::cout << result.dump(2) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "rls_lsp_benchmark: " << error.what() << '\n';
        return 1;
    }
}