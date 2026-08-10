#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rls::project {

struct SourceCollection {
    std::vector<std::filesystem::path> sourceFiles;
    std::vector<std::string> warnings;
    std::string error;
};

struct ManifestConfig {
    std::filesystem::path manifestPath;
    std::filesystem::path root;
    std::vector<std::filesystem::path> sources;
    std::vector<std::filesystem::path> excludes;
    std::vector<std::pair<std::string, std::filesystem::path>> transpilerOutputs;
};

struct ManifestLoadResult {
    std::optional<ManifestConfig> config;
    std::string error;
};

/// Collect explicit file and directory inputs using canonical, stable paths.
SourceCollection CollectExplicitSources(const std::vector<std::filesystem::path>& inputs);

/// Find the nearest rls.json at or above a file or directory.
std::optional<std::filesystem::path> FindManifest(const std::filesystem::path& start);

/// Read and validate a version-1 rls.json without loading source contents.
ManifestLoadResult LoadManifest(const std::filesystem::path& manifestPath);

/// Expand manifest source entries into canonical RLS source paths without parsing contents.
SourceCollection CollectManifestSources(const ManifestConfig& config);

} // namespace rls::project