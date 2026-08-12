#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "rls/lsp/analysis_scheduler.h"
#include "rls/lsp/document_store.h"
#include "rls/lsp/lifecycle_service.h"
#include "rls/lsp/project_manager.h"

namespace rls::lsp {

enum class DocumentSynchronizationResult {
    Applied,
    NotReady,
    InvalidUri,
    NotOpen,
    StaleVersion,
    ProjectResolutionFailed,
};

class DocumentSynchronizationService {
public:
    DocumentSynchronizationService(
        LifecycleService& lifecycle, DocumentStore& documents, ProjectManager& projects,
        AnalysisScheduler& scheduler);

    DocumentSynchronizationResult open(
        std::string uri, std::string languageId, int64_t version, std::string text);
    DocumentSynchronizationResult change(
        std::string_view uri, int64_t version, std::string text);
    DocumentSynchronizationResult close(std::string_view uri);

private:
    bool schedule(std::string_view uri);

    LifecycleService& lifecycle_;
    DocumentStore& documents_;
    ProjectManager& projects_;
    AnalysisScheduler& scheduler_;
};

} // namespace rls::lsp