#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "rls/lsp/analysis_scheduler.h"
#include "rls/lsp/diagnostic_publisher.h"
#include "rls/lsp/project_manager.h"

namespace rls::lsp {

class WorkspaceService {
public:
    WorkspaceService(
        ProjectManager& projects, AnalysisScheduler& scheduler,
        DiagnosticPublisher& diagnostics);

    bool initialize(
        std::vector<std::string> folderUris, bool restrictToWorkspaceFolders = true);
    bool changeFolders(
        std::vector<std::string> addedUris, std::vector<std::string> removedUris);
    bool watchedFilesChanged(const std::vector<std::string>& uris);

    size_t folderCount() const;
    std::vector<std::string> projectIds() const;

private:
    bool refreshProjects();

    ProjectManager& projects_;
    AnalysisScheduler& scheduler_;
    DiagnosticPublisher& diagnostics_;
    std::unordered_set<std::string> folders_;
    std::vector<std::filesystem::path> folderPaths_;
    bool restrictToWorkspaceFolders_ = false;
};

} // namespace rls::lsp