#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rls::project {

struct ConfigurationDiagnostic {
    std::filesystem::path path;
    std::string code;
    std::string message;
    size_t startByte = 0;
    size_t endByte = 0;
};

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
    std::vector<ConfigurationDiagnostic> diagnostics;
};

struct FileProject {
    std::optional<ManifestConfig> manifest;
    std::vector<std::filesystem::path> sourceFiles;
    bool isStandalone = false;
    std::string error;
    std::vector<ConfigurationDiagnostic> diagnostics;
};

/// Collect explicit file and directory inputs using canonical, stable paths.
SourceCollection CollectExplicitSources(const std::vector<std::filesystem::path>& inputs);

/// Find the nearest rls.json at or above a file or directory.
std::optional<std::filesystem::path> FindManifest(const std::filesystem::path& start);

/// Read and validate a version-1 rls.json without loading source contents.
ManifestLoadResult LoadManifest(const std::filesystem::path& manifestPath);

/// Expand manifest source entries into canonical RLS source paths without parsing contents.
SourceCollection CollectManifestSources(const ManifestConfig& config);

/// Resolve a file to nearest-manifest membership or a standalone one-file configuration.
FileProject ResolveFileProject(const std::filesystem::path& file);

} // namespace rls::project