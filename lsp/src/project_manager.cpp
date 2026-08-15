#include "rls/lsp/project_manager.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <utility>

#include "rls/lsp/document_uri.h"

namespace rls::lsp {
namespace {

std::filesystem::path canonicalPath(const std::filesystem::path& path) {
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    return error ? path.lexically_normal() : canonical;
}

std::string pathKey(const std::filesystem::path& path) {
    const auto generic = canonicalPath(path).generic_u8string();
    std::string key;
    key.reserve(generic.size());
    for (const char8_t byte : generic) {
        key.push_back(static_cast<char>(byte));
    }
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(), [](char character) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    });
#endif
    return key;
}

bool isWithin(const std::filesystem::path& path, const std::filesystem::path& root) {
    const std::string candidate = pathKey(path);
    std::string prefix = pathKey(root);
    if (!prefix.ends_with('/')) prefix.push_back('/');
    return candidate == pathKey(root) || candidate.starts_with(prefix);
}

} // namespace

ProjectManager::ProjectManager(DocumentStore& documents, Resolver resolver)
    : documents_(documents), resolver_(std::move(resolver)) {}

ProjectAssignmentResult ProjectManager::documentOpened(std::string_view uri) {
    const auto key = DocumentUriKey(uri);
    const auto path = FileUriToPath(uri);
    if (!key || !path) {
        return ProjectAssignmentResult::InvalidUri;
    }
    const TextDocument* document = documents_.find(uri);
    if (!document) {
        return ProjectAssignmentResult::NotAssigned;
    }

    project::FileProject resolved = resolver_(*path);
    if (!resolved.error.empty()) {
        recordConfigurationDiagnostics(*path, resolved.diagnostics);
        resolved = {};
        resolved.sourceFiles.push_back(canonicalPath(*path));
        resolved.isStandalone = true;
    } else {
        clearConfigurationDiagnostics(*path);
    }
    if (resolved.sourceFiles.empty()) {
        return ProjectAssignmentResult::ResolutionFailed;
    }

    const std::string id = projectId(resolved);
    auto [projectIt, inserted] = projects_.try_emplace(id);
    ManagedProject& managed = projectIt->second;
    if (inserted) {
        managed.id = id;
        managed.manifestGeneration = ++manifestGeneration_;
    }
    managed.manifestPath = resolved.manifest
        ? std::optional(resolved.manifest->manifestPath) : std::nullopt;
    managed.sourceFiles = std::move(resolved.sourceFiles);
    managed.isStandalone = resolved.isStandalone;
    managed.documentGeneration = ++documentGeneration_;
    managed.generation = ++generation_;

    const auto canonicalDocumentPath = canonicalPath(*path);
    assignments_.insert_or_assign(*key, Assignment{
        document->uri,
        canonicalDocumentPath,
        pathKey(canonicalDocumentPath),
        id,
    });
    return ProjectAssignmentResult::Assigned;
}

ProjectAssignmentResult ProjectManager::documentChanged(std::string_view uri) {
    const auto key = DocumentUriKey(uri);
    if (!key) {
        return ProjectAssignmentResult::InvalidUri;
    }
    const auto assignment = assignments_.find(*key);
    if (assignment == assignments_.end()) {
        return ProjectAssignmentResult::NotAssigned;
    }
    ManagedProject& project = projects_.at(assignment->second.projectId);
    project.documentGeneration = ++documentGeneration_;
    project.generation = ++generation_;
    return ProjectAssignmentResult::Assigned;
}

ProjectAssignmentResult ProjectManager::documentClosed(std::string_view uri) {
    return documentChanged(uri);
}

ProjectRefreshResult ProjectManager::refreshOpenDocuments(
    const std::vector<std::filesystem::path>& workspaceRoots,
    bool restrictToWorkspaceRoots) {
    ProjectRefreshResult result;
    std::unordered_set<std::string> previousProjectIds;
    for (auto assignment = assignments_.begin(); assignment != assignments_.end();) {
        if (!documents_.find(assignment->second.uri)) {
            assignment = assignments_.erase(assignment);
            continue;
        }
        previousProjectIds.insert(assignment->second.projectId);
        ++assignment;
    }

    for (auto& [key, assignment] : assignments_) {
        const bool inWorkspace = std::any_of(
            workspaceRoots.begin(), workspaceRoots.end(), [&](const auto& root) {
                return isWithin(assignment.path, root);
            });
        project::FileProject resolved;
        if (restrictToWorkspaceRoots && !inWorkspace) {
            resolved.sourceFiles.push_back(canonicalPath(assignment.path));
            resolved.isStandalone = true;
        } else {
            resolved = resolver_(assignment.path);
        }
        if (!resolved.error.empty() || resolved.sourceFiles.empty()) {
            result.errors.push_back(resolved.error.empty()
                ? "project resolves to no source files" : std::move(resolved.error));
            recordConfigurationDiagnostics(assignment.path, resolved.diagnostics);
            resolved = {};
            resolved.sourceFiles.push_back(canonicalPath(assignment.path));
            resolved.isStandalone = true;
        } else {
            clearConfigurationDiagnostics(assignment.path);
        }

        if (resolved.sourceFiles.empty()) {
            continue;
        }

        const std::string id = projectId(resolved);
        ManagedProject& managed = projects_[id];
        managed.id = id;
        managed.manifestPath = resolved.manifest
            ? std::optional(resolved.manifest->manifestPath) : std::nullopt;
        managed.sourceFiles = std::move(resolved.sourceFiles);
        managed.isStandalone = resolved.isStandalone;
        assignment.projectId = id;
    }

    std::unordered_set<std::string> currentProjectIds;
    for (const auto& [key, assignment] : assignments_) {
        currentProjectIds.insert(assignment.projectId);
    }
    for (const auto& id : currentProjectIds) {
        ManagedProject& project = projects_.at(id);
        project.documentGeneration = ++documentGeneration_;
        project.manifestGeneration = ++manifestGeneration_;
        project.generation = ++generation_;
        result.changedProjectIds.push_back(id);
    }
    for (const auto& id : previousProjectIds) {
        if (!currentProjectIds.contains(id)) {
            result.removedProjectIds.push_back(id);
        }
    }
    for (auto project = projects_.begin(); project != projects_.end();) {
        if (!currentProjectIds.contains(project->first)) {
            project = projects_.erase(project);
        } else {
            ++project;
        }
    }

    std::sort(result.changedProjectIds.begin(), result.changedProjectIds.end());
    std::sort(result.removedProjectIds.begin(), result.removedProjectIds.end());
    result.configurationDiagnostics = configurationDiagnostics();
    return result;
}

const ManagedProject* ProjectManager::projectForDocument(std::string_view uri) const {
    const auto key = DocumentUriKey(uri);
    if (!key) {
        return nullptr;
    }
    const auto assignment = assignments_.find(*key);
    if (assignment == assignments_.end()) {
        return nullptr;
    }
    const auto project = projects_.find(assignment->second.projectId);
    return project == projects_.end() ? nullptr : &project->second;
}

const ManagedProject* ProjectManager::project(std::string_view projectId) const {
    const auto project = projects_.find(std::string(projectId));
    return project == projects_.end() ? nullptr : &project->second;
}

std::vector<std::string> ProjectManager::projectIds() const {
    std::vector<std::string> result;
    result.reserve(projects_.size());
    for (const auto& [id, project] : projects_) {
        result.push_back(id);
    }
    std::sort(result.begin(), result.end());
    return result;
}

ProjectSourceSet ProjectManager::sourceSetForDocument(std::string_view uri) const {
    const auto project = projectForDocument(uri);
    if (!project) {
        ProjectSourceSet result;
        result.error = "document is not assigned to a project";
        return result;
    }
    return sourceSetForProject(project->id);
}

ProjectSourceSet ProjectManager::sourceSetForProject(std::string_view projectId) const {
    ProjectSourceSet result;
    const auto projectIt = projects_.find(std::string(projectId));
    if (projectIt == projects_.end()) {
        result.error = "project is not managed";
        return result;
    }
    const ManagedProject* project = &projectIt->second;
    result.generation = project->generation;
    result.documentGeneration = project->documentGeneration;
    result.manifestGeneration = project->manifestGeneration;

    for (const auto& sourcePath : project->sourceFiles) {
        const TextDocument* overlay = nullptr;
        const std::string sourcePathKey = pathKey(sourcePath);
        for (const auto& [key, assignment] : assignments_) {
            if (assignment.projectId == project->id && assignment.pathKey == sourcePathKey) {
                overlay = documents_.find(assignment.uri);
                break;
            }
        }

        if (overlay) {
            result.sources.push_back({sourcePath, overlay->text});
            continue;
        }

        result.sources.push_back({sourcePath, std::nullopt});
    }
    return result;
}

std::vector<project::ConfigurationDiagnostic> ProjectManager::configurationDiagnostics() const {
    std::vector<project::ConfigurationDiagnostic> diagnostics;
    diagnostics.reserve(configurationDiagnostics_.size());
    for (const auto& [key, diagnostic] : configurationDiagnostics_) {
        diagnostics.push_back(diagnostic);
    }
    std::sort(diagnostics.begin(), diagnostics.end(), [](const auto& left, const auto& right) {
        return pathKey(left.path) < pathKey(right.path);
    });
    return diagnostics;
}

void ProjectManager::recordConfigurationDiagnostics(
    const std::filesystem::path& documentPath,
    const std::vector<project::ConfigurationDiagnostic>& diagnostics) {
    clearConfigurationDiagnostics(documentPath);
    for (const auto& diagnostic : diagnostics) {
        configurationDiagnostics_.insert_or_assign(pathKey(diagnostic.path), diagnostic);
    }
}

void ProjectManager::clearConfigurationDiagnostics(const std::filesystem::path& documentPath) {
    for (auto diagnostic = configurationDiagnostics_.begin();
         diagnostic != configurationDiagnostics_.end();) {
        if (isWithin(documentPath, diagnostic->second.path.parent_path())) {
            diagnostic = configurationDiagnostics_.erase(diagnostic);
        } else {
            ++diagnostic;
        }
    }
}

std::string ProjectManager::projectId(const project::FileProject& project) {
    if (project.manifest) {
        return pathKey(project.manifest->manifestPath);
    }
    return pathKey(project.sourceFiles.front());
}

} // namespace rls::lsp