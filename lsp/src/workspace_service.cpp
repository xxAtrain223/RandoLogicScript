#include "rls/lsp/workspace_service.h"

#include <utility>

#include "rls/lsp/document_uri.h"
#include "rls/lsp/project_analysis.h"

namespace rls::lsp {

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