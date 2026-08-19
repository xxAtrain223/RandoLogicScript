#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "rls/lsp/navigation_service.h"

namespace rls::lsp {

class AnalysisScheduler;
class DocumentStore;
class ProjectManager;

struct RenameTextEdit {
    NavigationRange range;
    std::string newText;
};

struct RenameDocumentEdit {
    std::string uri;
    std::optional<int64_t> version;
    std::vector<RenameTextEdit> edits;
};

struct RenameWorkspaceEdit {
    std::vector<RenameDocumentEdit> documents;
};

enum class RenameError {
    None,
    NotRenameable,
    InvalidName,
    Collision,
    StaleSnapshot,
    UnsupportedClient,
};

template<typename T>
struct RenameResult {
    std::optional<T> value;
    RenameError error = RenameError::None;
};

class RenameService {
public:
    RenameService(
        const DocumentStore& documents, const ProjectManager& projects,
        const AnalysisScheduler& scheduler);

    RenameResult<NavigationRange> prepare(
        std::string_view uri, NavigationPosition position) const;
    RenameResult<RenameWorkspaceEdit> rename(
        std::string_view uri, NavigationPosition position, std::string_view newName,
        bool supportsDocumentChanges) const;

private:
    const DocumentStore& documents_;
    const ProjectManager& projects_;
    const AnalysisScheduler& scheduler_;
};

std::string_view RenameErrorMessage(RenameError error);

} // namespace rls::lsp