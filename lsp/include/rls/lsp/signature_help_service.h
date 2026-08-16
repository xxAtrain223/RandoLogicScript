#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "rls/lsp/analysis_scheduler.h"
#include "rls/lsp/presentation.h"
#include "rls/lsp/project_manager.h"

namespace rls::lsp {

struct SignatureHelpResult {
    std::string label;
    std::string documentation;
    std::vector<std::string> parameterLabels;
    std::optional<size_t> activeParameter;
};

class SignatureHelpService {
public:
    SignatureHelpService(const ProjectManager& projects, AnalysisScheduler& scheduler);

    std::optional<SignatureHelpResult> signatureHelp(
        std::string_view uri, PresentationPosition position) const;

private:
    const ProjectManager& projects_;
    AnalysisScheduler& scheduler_;
};

} // namespace rls::lsp