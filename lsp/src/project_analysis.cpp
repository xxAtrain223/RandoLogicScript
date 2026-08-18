#include "rls/lsp/project_analysis.h"

#include "rls/lsp/analysis_scheduler.h"
#include "rls/lsp/project_manager.h"

namespace rls::lsp {

bool ScheduleProjectAnalysis(
    ProjectManager& projects, AnalysisScheduler& scheduler, std::string_view projectId) {
    ProjectSourceSet sourceSet = projects.sourceSetForProject(projectId);
    if (!sourceSet.error.empty()) {
        return false;
    }

    std::vector<AnalysisSource> sources;
    sources.reserve(sourceSet.sources.size());
    for (auto& source : sourceSet.sources) {
        sources.push_back({
            std::move(source.identity),
            std::move(source.content),
            std::move(source.diskPath),
        });
    }

    return scheduler.schedule({
        std::string(projectId),
        sourceSet.generation,
        std::move(sources),
        sourceSet.documentGeneration,
        sourceSet.manifestGeneration,
    });
}

} // namespace rls::lsp