#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "project.h"
#include "rls/lsp/document_store.h"

namespace rls::lsp {

struct ProjectSource {
    std::filesystem::path path;
    std::string content;
};

struct ManagedProject {
    std::string id;
    std::optional<std::filesystem::path> manifestPath;
    std::vector<std::filesystem::path> sourceFiles;
    bool isStandalone = false;
    uint64_t generation = 0;
    uint64_t documentGeneration = 0;
    uint64_t manifestGeneration = 0;
};

struct ProjectSourceSet {
    std::vector<ProjectSource> sources;
    uint64_t generation = 0;
    uint64_t documentGeneration = 0;
    uint64_t manifestGeneration = 0;
    std::string error;
};

struct ProjectRefreshResult {
    std::vector<std::string> changedProjectIds;
    std::vector<std::string> removedProjectIds;
    std::vector<std::string> errors;
    std::vector<project::ConfigurationDiagnostic> configurationDiagnostics;
};

enum class ProjectAssignmentResult {
    Assigned,
    InvalidUri,
    ResolutionFailed,
    NotAssigned,
};

class ProjectManager {
public:
    using Resolver = std::function<project::FileProject(const std::filesystem::path&)>;

    explicit ProjectManager(DocumentStore& documents, Resolver resolver = project::ResolveFileProject);

    ProjectAssignmentResult documentOpened(std::string_view uri);
    ProjectAssignmentResult documentChanged(std::string_view uri);
    ProjectAssignmentResult documentClosed(std::string_view uri);
    ProjectRefreshResult refreshOpenDocuments(
        const std::vector<std::filesystem::path>& workspaceRoots = {},
        bool restrictToWorkspaceRoots = false);

    const ManagedProject* projectForDocument(std::string_view uri) const;
    ProjectSourceSet sourceSetForDocument(std::string_view uri) const;
    ProjectSourceSet sourceSetForProject(std::string_view projectId) const;
    std::vector<project::ConfigurationDiagnostic> configurationDiagnostics() const;

private:
    struct Assignment {
        std::string uri;
        std::filesystem::path path;
        std::string pathKey;
        std::string projectId;
    };

    static std::string projectId(const project::FileProject& project);
    void recordConfigurationDiagnostics(
        const std::filesystem::path& documentPath,
        const std::vector<project::ConfigurationDiagnostic>& diagnostics);
    void clearConfigurationDiagnostics(const std::filesystem::path& documentPath);

    DocumentStore& documents_;
    Resolver resolver_;
    std::unordered_map<std::string, Assignment> assignments_;
    std::unordered_map<std::string, ManagedProject> projects_;
    std::unordered_map<std::string, project::ConfigurationDiagnostic> configurationDiagnostics_;
    uint64_t generation_ = 0;
    uint64_t documentGeneration_ = 0;
    uint64_t manifestGeneration_ = 0;
};

} // namespace rls::lsp