#include "rls/lsp/workspace_service.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "rls/lsp/document_uri.h"
#include "rls/lsp/project_analysis.h"

namespace rls::lsp {
namespace {

std::string pathKey(const std::filesystem::path& path) {
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    const auto generic = (error ? path.lexically_normal() : canonical).generic_u8string();
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

WorkspaceService::WorkspaceService(
    ProjectManager& projects, AnalysisScheduler& scheduler,
    DiagnosticPublisher& diagnostics)
    : projects_(projects), scheduler_(scheduler), diagnostics_(diagnostics) {}

bool WorkspaceService::initialize(
    std::vector<std::string> folderUris, bool restrictToWorkspaceFolders) {
    std::unordered_set<std::string> folders;
    std::vector<std::filesystem::path> folderPaths;
    for (const auto& uri : folderUris) {
        const auto key = DocumentUriKey(uri);
        const auto path = FileUriToPath(uri);
        if (!key || !path || !std::filesystem::is_directory(*path)) {
            return false;
        }
        folders.insert(*key);
        folderPaths.push_back(*path);
    }
    folders_ = std::move(folders);
    folderPaths_ = std::move(folderPaths);
    restrictToWorkspaceFolders_ = restrictToWorkspaceFolders;
    return true;
}

bool WorkspaceService::changeFolders(
    std::vector<std::string> addedUris, std::vector<std::string> removedUris) {
    std::vector<std::pair<std::string, std::filesystem::path>> added;
    std::vector<std::string> removed;
    for (const auto& uri : removedUris) {
        const auto key = DocumentUriKey(uri);
        if (!key) {
            return false;
        }
        removed.push_back(*key);
    }
    for (const auto& uri : addedUris) {
        const auto key = DocumentUriKey(uri);
        const auto path = FileUriToPath(uri);
        if (!key || !path || !std::filesystem::is_directory(*path)) {
            return false;
        }
        added.emplace_back(*key, *path);
    }
    for (const auto& key : removed) {
        folders_.erase(key);
    }
    for (const auto& [key, path] : added) {
        folders_.insert(key);
    }
    folderPaths_.clear();
    for (const auto& key : folders_) {
        const auto path = FileUriToPath(key);
        if (path) folderPaths_.push_back(*path);
    }
    restrictToWorkspaceFolders_ = true;
    return refreshProjects();
}

bool WorkspaceService::watchedFilesChanged(const std::vector<std::string>& uris) {
    for (const auto& uri : uris) {
        if (!FileUriToPath(uri)) {
            return false;
        }
    }
    return refreshProjects();
}

size_t WorkspaceService::folderCount() const {
    return folders_.size();
}

std::vector<std::string> WorkspaceService::projectIds() const {
    const auto projectIds = projects_.projectIds();
    if (!restrictToWorkspaceFolders_) {
        return projectIds;
    }

    std::vector<std::string> result;
    for (const auto& projectId : projectIds) {
        const auto* project = projects_.project(projectId);
        if (!project) {
            continue;
        }
        const bool manifestInWorkspace = project->manifestPath
            && std::any_of(folderPaths_.begin(), folderPaths_.end(), [&](const auto& root) {
                return isWithin(*project->manifestPath, root);
            });
        const bool sourceInWorkspace = std::any_of(
            project->sourceFiles.begin(), project->sourceFiles.end(), [&](const auto& source) {
                return std::any_of(folderPaths_.begin(), folderPaths_.end(), [&](const auto& root) {
                    return isWithin(source, root);
                });
            });
        if (manifestInWorkspace || sourceInWorkspace) {
            result.push_back(projectId);
        }
    }
    return result;
}

bool WorkspaceService::refreshProjects() {
    ProjectRefreshResult refresh = projects_.refreshOpenDocuments(
        folderPaths_, restrictToWorkspaceFolders_);
    diagnostics_.publishConfigurationDiagnostics(refresh.configurationDiagnostics);
    for (const auto& projectId : refresh.removedProjectIds) {
        scheduler_.removeProject(projectId);
        diagnostics_.clearProject(projectId);
    }
    bool succeeded = refresh.errors.empty();
    for (const auto& projectId : refresh.changedProjectIds) {
        succeeded = ScheduleProjectAnalysis(projects_, scheduler_, projectId) && succeeded;
    }
    return succeeded;
}

} // namespace rls::lsp