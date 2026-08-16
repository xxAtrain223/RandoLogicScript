#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "rls/lsp/analysis_scheduler.h"
#include "rls/lsp/presentation.h"
#include "rls/lsp/project_manager.h"

namespace rls::lsp {

struct HoverResult {
    std::string markdown;
    PresentationRange range;
};

class HoverService {
public:
    HoverService(const ProjectManager& projects, AnalysisScheduler& scheduler);

    std::optional<HoverResult> hover(
        std::string_view uri, PresentationPosition position) const;

private:
    const ProjectManager& projects_;
    AnalysisScheduler& scheduler_;
};

} // namespace rls::lsp
