#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

struct NavigationLocation {
    std::string uri;
    NavigationRange range;
};

enum class NavigationSymbolKind {
    Namespace,
    Function,
    Enum,
    EnumMember,
    Variable,
    Property,
    Field,
};

struct NavigationDocumentSymbol {
    std::string name;
    NavigationSymbolKind kind;
    NavigationRange range;
    NavigationRange selectionRange;
    std::vector<NavigationDocumentSymbol> children;
};

struct NavigationWorkspaceSymbol {
    std::string name;
    NavigationSymbolKind kind;
    NavigationLocation location;
    std::optional<std::string> containerName;
};

class NavigationService {
public:
    NavigationService(const ProjectManager& projects, const AnalysisScheduler& scheduler);

    std::optional<DefinitionResult> definition(
        std::string_view uri, NavigationPosition position) const;
    std::vector<NavigationLocation> references(
        std::string_view uri, NavigationPosition position, bool includeDeclaration) const;
    std::vector<NavigationRange> documentHighlights(
        std::string_view uri, NavigationPosition position) const;
    std::vector<NavigationDocumentSymbol> documentSymbols(std::string_view uri) const;
    std::vector<NavigationWorkspaceSymbol> workspaceSymbols(
        std::string_view query, const std::vector<std::string>& projectIds) const;

private:
    const ProjectManager& projects_;
    const AnalysisScheduler& scheduler_;
};

} // namespace rls::lsp