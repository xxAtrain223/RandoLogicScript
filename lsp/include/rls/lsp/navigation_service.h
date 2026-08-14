#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "rls/lsp/analysis_scheduler.h"
#include "rls/lsp/project_manager.h"

namespace rls::lsp {

struct NavigationPosition {
    uint32_t line = 0;
    uint32_t character = 0;
};

struct NavigationRange {
    NavigationPosition start;
    NavigationPosition end;
};

struct DefinitionResult {
    NavigationRange originSelectionRange;
    std::string targetUri;
    NavigationRange targetRange;
    NavigationRange targetSelectionRange;
};

class NavigationService {
public:
    NavigationService(const ProjectManager& projects, const AnalysisScheduler& scheduler);

    std::optional<DefinitionResult> definition(
        std::string_view uri, NavigationPosition position) const;

private:
    const ProjectManager& projects_;
    const AnalysisScheduler& scheduler_;
};

} // namespace rls::lsp