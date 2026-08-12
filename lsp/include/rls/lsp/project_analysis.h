#pragma once

#include <string_view>

namespace rls::lsp {

class AnalysisScheduler;
class ProjectManager;

bool ScheduleProjectAnalysis(
    ProjectManager& projects, AnalysisScheduler& scheduler, std::string_view projectId);

} // namespace rls::lsp