#include "project.h"

#include <algorithm>
#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace rls::project {
namespace {

fs::path canonicalPath(const fs::path& path) {
    std::error_code error;
    const auto resolved = fs::weakly_canonical(path, error);
    return error ? fs::absolute(path).lexically_normal() : resolved;
}

bool resolvesWithinRoot(const fs::path& root, const fs::path& path) {
    const auto relative = path.lexically_relative(root);
    if (relative.empty())
        return false;
    return std::ranges::none_of(relative, [](const fs::path& component) {
        return component == "..";
    });
}

std::optional<fs::path> resolveManifestPath(
    const fs::path& root,
    const std::string& value,
    std::string& error)
{
    const fs::path path(value);
    if (path.is_absolute()) {
        error = "manifest paths must be relative: " + value;
        return std::nullopt;
    }

    const auto resolved = canonicalPath(root / path);
    if (!resolvesWithinRoot(root, resolved)) {
        error = "manifest path escapes the project root: " + value;
        return std::nullopt;
    }
    return resolved;
}

} // namespace

SourceCollection CollectExplicitSources(const std::vector<fs::path>& inputs) {
    SourceCollection result;
    std::set<fs::path> paths;

    for (const auto& input : inputs) {
        if (!fs::exists(input)) {
            result.error = "path does not exist: " + input.string();
            return result;
        }

        if (fs::is_directory(input)) {
            const auto before = paths.size();
            for (const auto& entry : fs::recursive_directory_iterator(input)) {
                if (entry.is_regular_file() && entry.path().extension() == ".rls")
                    paths.insert(canonicalPath(entry.path()));
            }
            if (paths.size() == before)
                result.warnings.push_back("no .rls files found in " + input.string());
        } else {
            paths.insert(canonicalPath(input));
        }
    }

    result.sourceFiles.assign(paths.begin(), paths.end());
    return result;
}

std::optional<fs::path> FindManifest(const fs::path& start) {
    fs::path directory = canonicalPath(start);
    if (!fs::is_directory(directory))
        directory = directory.parent_path();

    while (!directory.empty()) {
        const auto manifest = directory / "rls.json";
        if (fs::is_regular_file(manifest))
            return manifest;

        const auto parent = directory.parent_path();
        if (parent == directory)
            break;
        directory = parent;
    }
    return std::nullopt;
}

ManifestLoadResult LoadManifest(const fs::path& manifestPath) {
    ManifestLoadResult result;
    const auto canonicalManifest = canonicalPath(manifestPath);
    std::ifstream input(canonicalManifest);
    if (!input) {
        result.error = "could not open manifest: " + manifestPath.string();
        return result;
    }

    nlohmann::json json;
    try {
        input >> json;
    } catch (const nlohmann::json::exception& exception) {
        result.error = "invalid JSON: " + std::string(exception.what());
        return result;
    }

    if (!json.is_object()) {
        result.error = "manifest must be a JSON object";
        return result;
    }
    for (const auto& [key, value] : json.items()) {
        if (key != "version" && key != "sources" && key != "exclude" && key != "transpilers") {
            result.error = "unknown manifest field: " + key;
            return result;
        }
    }
    if (json.value("version", 0) != 1) {
        result.error = "unsupported manifest version";
        return result;
    }
    if (!json.contains("sources") || !json["sources"].is_array() || json["sources"].empty()) {
        result.error = "manifest requires a non-empty sources array";
        return result;
    }

    ManifestConfig config;
    config.manifestPath = canonicalManifest;
    config.root = canonicalManifest.parent_path();
    for (const auto& source : json["sources"]) {
        if (!source.is_string()) {
            result.error = "sources entries must be strings";
            return result;
        }
        auto resolved = resolveManifestPath(config.root, source.get<std::string>(), result.error);
        if (!resolved)
            return result;
        config.sources.push_back(std::move(*resolved));
    }
    if (json.contains("exclude")) {
        if (!json["exclude"].is_array()) {
            result.error = "exclude must be an array";
            return result;
        }
        for (const auto& exclude : json["exclude"]) {
            if (!exclude.is_string()) {
                result.error = "exclude entries must be strings";
                return result;
            }
            auto resolved = resolveManifestPath(config.root, exclude.get<std::string>(), result.error);
            if (!resolved)
                return result;
            config.excludes.push_back(std::move(*resolved));
        }
    }
    if (json.contains("transpilers")) {
        if (!json["transpilers"].is_object()) {
            result.error = "transpilers must be an object";
            return result;
        }
        for (const auto& [name, settings] : json["transpilers"].items()) {
            if (!settings.is_object() ||
                !settings.contains("output") || !settings["output"].is_string()) {
                result.error = "invalid transpiler configuration: " + name;
                return result;
            }
            auto output = resolveManifestPath(config.root, settings["output"].get<std::string>(), result.error);
            if (!output)
                return result;
            config.transpilerOutputs.emplace_back(name, std::move(*output));
        }
    }

    result.config = std::move(config);
    return result;
}

} // namespace rls::project