#include "rls/lsp/project_manager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
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
        return ProjectAssignmentResult::ResolutionFailed;
    }
    if (resolved.sourceFiles.empty()) {
        return ProjectAssignmentResult::ResolutionFailed;
    }

    const std::string id = projectId(resolved);
    auto [projectIt, inserted] = projects_.try_emplace(id);
    ManagedProject& managed = projectIt->second;
    if (inserted) {
        managed.id = id;
    }
    managed.manifestPath = resolved.manifest
        ? std::optional(resolved.manifest->manifestPath) : std::nullopt;
    managed.sourceFiles = std::move(resolved.sourceFiles);
    managed.isStandalone = resolved.isStandalone;
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
    projects_.at(assignment->second.projectId).generation = ++generation_;
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
        projects_.at(id).generation = ++generation_;
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

        std::ifstream input(sourcePath, std::ios::binary);
        if (!input) {
            result.error = "failed to read source file: " + sourcePath.string();
            result.sources.clear();
            return result;
        }
        result.sources.push_back({sourcePath,
            std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>())});
    }
    return result;
}

std::string ProjectManager::projectId(const project::FileProject& project) {
    if (project.manifest) {
        return pathKey(project.manifest->manifestPath);
    }
    return pathKey(project.sourceFiles.front());
}

} // namespace rls::lsp