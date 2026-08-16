#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "rls/lsp/analysis_scheduler.h"
#include "rls/lsp/project_manager.h"

namespace rls::lsp {

class SemanticTokensService {
public:
    SemanticTokensService(const ProjectManager& projects, AnalysisScheduler& scheduler);

    std::vector<uint32_t> full(std::string_view uri) const;

    static const std::vector<std::string>& tokenTypes();
    static const std::vector<std::string>& tokenModifiers();

private:
    const ProjectManager& projects_;
    AnalysisScheduler& scheduler_;
};

} // namespace rls::lsp
