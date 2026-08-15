#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "rls/lsp/analysis_scheduler.h"
#include "rls/lsp/presentation.h"
#include "rls/lsp/project_manager.h"

namespace rls::lsp {

enum class CompletionItemKind {
    Function,
    Enum,
    EnumMember,
    Type,
    Variable,
    Value,
    Keyword,
};

struct CompletionItem {
    std::string label;
    CompletionItemKind kind = CompletionItemKind::Value;
    std::string detail;
    std::string documentation;
    std::string insertText;
    PresentationRange replacementRange;
    std::string sortText;
};

class CompletionService {
public:
    CompletionService(const ProjectManager& projects, const AnalysisScheduler& scheduler);

    std::vector<CompletionItem> complete(
        std::string_view uri, PresentationPosition position) const;

private:
    const ProjectManager& projects_;
    const AnalysisScheduler& scheduler_;
};

} // namespace rls::lsp