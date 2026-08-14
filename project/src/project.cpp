#include "project.h"
#include "project_diagnostics.h"

#include <algorithm>
#include <fstream>
#include <iterator>
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
    const fs::path& manifestPath,
    const std::string& value,
    std::optional<ConfigurationDiagnostic>& diagnostic)
{
    const fs::path path(value);
    if (path.is_absolute()) {
        diagnostic = diagnostics::ManifestPathMustBeRelative(manifestPath, value);
        return std::nullopt;
    }

    const auto resolved = canonicalPath(root / path);
    if (!resolvesWithinRoot(root, resolved)) {
        diagnostic = diagnostics::ManifestPathEscapesRoot(manifestPath, value);
        return std::nullopt;
    }
    return resolved;
}

bool isWithin(const fs::path& parent, const fs::path& path) {
    const auto relative = path.lexically_relative(parent);
    return !relative.empty() && std::ranges::none_of(relative, [](const fs::path& component) {
        return component == "..";
    });
}

bool isDefaultExcluded(const fs::path& relativePath) {
    static const std::set<fs::path> excludedNames = {
        ".git", ".hg", ".svn", ".cache", "build", "node_modules",
    };
    return std::ranges::any_of(relativePath, [](const fs::path& component) {
        return excludedNames.contains(component);
    });
}

bool isManifestExcluded(const ManifestConfig& config, const fs::path& path) {
    return std::ranges::any_of(config.excludes, [&path](const fs::path& exclude) {
        const auto pattern = exclude.generic_string();
        const auto suffix = std::string("/**");
        const auto prefix = pattern.ends_with(suffix)
            ? fs::path(pattern.substr(0, pattern.size() - suffix.size()))
            : exclude;
        return isWithin(prefix, path) || prefix == path;
    });
}

bool isOutputExcluded(const ManifestConfig& config, const fs::path& path) {
    return std::ranges::any_of(config.transpilerOutputs, [&path](const auto& output) {
        return isWithin(output.second, path) || output.second == path;
    });
}

bool isExcluded(
    const ManifestConfig& config,
    const fs::path& path,
    bool overridesDefaultExclusions,
    bool includesOutput)
{
    if (isManifestExcluded(config, path))
        return true;
    if (!includesOutput && isOutputExcluded(config, path))
        return true;
    return !overridesDefaultExclusions && isDefaultExcluded(path.lexically_relative(config.root));
}

void setManifestError(
    ManifestLoadResult& result, ConfigurationDiagnostic diagnostic) {
    result.error = diagnostic.message;
    result.diagnostics.push_back(std::move(diagnostic));
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
        setManifestError(result, diagnostics::ManifestUnavailable(
            canonicalManifest, manifestPath.string()));
        return result;
    }

    const std::string manifestContent{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(manifestContent);
    } catch (const nlohmann::json::parse_error& exception) {
        const size_t offset = exception.byte == 0
            ? 0 : std::min(exception.byte - 1, manifestContent.size());
        setManifestError(result, diagnostics::InvalidJson(
            canonicalManifest, exception.what(), offset, offset));
        return result;
    }

    if (!json.is_object()) {
        setManifestError(result, diagnostics::ManifestMustBeObject(canonicalManifest));
        return result;
    }
    for (const auto& [key, value] : json.items()) {
        if (key != "version" && key != "sources" && key != "exclude" && key != "transpilers") {
            setManifestError(result, diagnostics::UnknownManifestField(
                canonicalManifest, key));
            return result;
        }
    }
    if (json.value("version", 0) != 1) {
        setManifestError(result, diagnostics::UnsupportedManifestVersion(canonicalManifest));
        return result;
    }
    if (!json.contains("sources") || !json["sources"].is_array() || json["sources"].empty()) {
        setManifestError(result, diagnostics::SourcesRequired(canonicalManifest));
        return result;
    }

    ManifestConfig config;
    config.manifestPath = canonicalManifest;
    config.root = canonicalManifest.parent_path();
    for (const auto& source : json["sources"]) {
        if (!source.is_string()) {
            setManifestError(result, diagnostics::SourceEntryMustBeString(canonicalManifest));
            return result;
        }
        std::optional<ConfigurationDiagnostic> diagnostic;
        auto resolved = resolveManifestPath(
            config.root, canonicalManifest, source.get<std::string>(), diagnostic);
        if (!resolved) {
            setManifestError(result, std::move(*diagnostic));
            return result;
        }
        config.sources.push_back(std::move(*resolved));
    }
    if (json.contains("exclude")) {
        if (!json["exclude"].is_array()) {
            setManifestError(result, diagnostics::ExcludeMustBeArray(canonicalManifest));
            return result;
        }
        for (const auto& exclude : json["exclude"]) {
            if (!exclude.is_string()) {
                setManifestError(result, diagnostics::ExcludeEntryMustBeString(canonicalManifest));
                return result;
            }
            std::optional<ConfigurationDiagnostic> diagnostic;
            auto resolved = resolveManifestPath(
                config.root, canonicalManifest, exclude.get<std::string>(), diagnostic);
            if (!resolved) {
                setManifestError(result, std::move(*diagnostic));
                return result;
            }
            config.excludes.push_back(std::move(*resolved));
        }
    }
    if (json.contains("transpilers")) {
        if (!json["transpilers"].is_object()) {
            setManifestError(result, diagnostics::TranspilersMustBeObject(canonicalManifest));
            return result;
        }
        for (const auto& [name, settings] : json["transpilers"].items()) {
            if (!settings.is_object() ||
                !settings.contains("output") || !settings["output"].is_string()) {
                setManifestError(result, diagnostics::InvalidTranspilerConfiguration(
                    canonicalManifest, name));
                return result;
            }
            std::optional<ConfigurationDiagnostic> diagnostic;
            auto output = resolveManifestPath(
                config.root, canonicalManifest,
                settings["output"].get<std::string>(), diagnostic);
            if (!output) {
                setManifestError(result, std::move(*diagnostic));
                return result;
            }
            config.transpilerOutputs.emplace_back(name, std::move(*output));
        }
    }

    result.config = std::move(config);
    return result;
}

SourceCollection CollectManifestSources(const ManifestConfig& config) {
    SourceCollection result;
    std::set<fs::path> paths;

    for (const auto& source : config.sources) {
        if (!fs::exists(source)) {
            result.error = "manifest source does not exist: " + source.string();
            return result;
        }

        if (fs::is_regular_file(source)) {
            if (source.extension() == ".rls" && !isExcluded(config, source, true, true))
                paths.insert(source);
            continue;
        }

        const auto before = paths.size();
        const bool overridesDefaultExclusions = source != config.root &&
            isDefaultExcluded(source.lexically_relative(config.root));
        const bool includesOutput = std::ranges::any_of(
            config.transpilerOutputs,
            [&source](const auto& output) {
                return source == output.second || isWithin(output.second, source);
            });
        for (auto entry = fs::recursive_directory_iterator(source);
             entry != fs::recursive_directory_iterator(); ++entry) {
            const auto path = canonicalPath(entry->path());
            if (entry->is_directory() && isExcluded(
                config, path, overridesDefaultExclusions, includesOutput)) {
                entry.disable_recursion_pending();
                continue;
            }
            if (entry->is_regular_file() && path.extension() == ".rls" &&
                !isExcluded(config, path, overridesDefaultExclusions, includesOutput)) {
                paths.insert(path);
            }
        }
        if (paths.size() == before)
            result.warnings.push_back("no .rls files found in manifest source: " + source.string());
    }

    result.sourceFiles.assign(paths.begin(), paths.end());
    if (result.sourceFiles.empty())
        result.error = "manifest does not resolve to any .rls source files";
    return result;
}

FileProject ResolveFileProject(const fs::path& file) {
    FileProject result;
    if (!fs::exists(file)) {
        result.error = "path does not exist: " + file.string();
        return result;
    }

    const auto canonicalFile = canonicalPath(file);
    const auto manifestPath = FindManifest(canonicalFile);
    if (!manifestPath) {
        result.sourceFiles.push_back(canonicalFile);
        result.isStandalone = true;
        return result;
    }

    auto manifest = LoadManifest(*manifestPath);
    if (!manifest.config) {
        result.error = std::move(manifest.error);
        result.diagnostics = std::move(manifest.diagnostics);
        return result;
    }

    auto sources = CollectManifestSources(*manifest.config);
    if (!sources.error.empty()) {
        result.error = std::move(sources.error);
        result.diagnostics.push_back(diagnostics::SourceCollectionFailed(
            *manifestPath, result.error));
        return result;
    }

    result.manifest = std::move(manifest.config);
    result.sourceFiles = std::move(sources.sourceFiles);
    return result;
}

} // namespace rls::project