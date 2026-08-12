#include "rls/lsp/project_analysis.h"

#include <string>
#include <utility>
#include <vector>

#include "rls/lsp/analysis_scheduler.h"
#include "rls/lsp/project_manager.h"

namespace rls::lsp {

bool ScheduleProjectAnalysis(
    ProjectManager& projects, AnalysisScheduler& scheduler, std::string_view projectId) {
    ProjectSourceSet sourceSet = projects.sourceSetForProject(projectId);
    if (!sourceSet.error.empty()) {
        return false;
    }

    std::vector<sema::SourceInput> sources;
    sources.reserve(sourceSet.sources.size());
    for (auto& source : sourceSet.sources) {
        const auto genericPath = source.path.generic_u8string();
        std::string path;
        path.reserve(genericPath.size());
        for (const char8_t byte : genericPath) {
            path.push_back(static_cast<char>(byte));
        }
        sources.push_back({std::move(path), std::move(source.content)});
    }

    return scheduler.schedule({
        std::string(projectId),
        sourceSet.generation,
        std::move(sources),
    });
}

} // namespace rls::lsp