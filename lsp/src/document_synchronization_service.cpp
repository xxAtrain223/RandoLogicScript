#include "rls/lsp/document_synchronization_service.h"

#include <utility>

#include "rls/lsp/project_analysis.h"

namespace rls::lsp {
namespace {

DocumentSynchronizationResult translate(DocumentUpdateResult result) {
    switch (result) {
    case DocumentUpdateResult::Applied:
        return DocumentSynchronizationResult::Applied;
    case DocumentUpdateResult::InvalidUri:
        return DocumentSynchronizationResult::InvalidUri;
    case DocumentUpdateResult::NotOpen:
        return DocumentSynchronizationResult::NotOpen;
    case DocumentUpdateResult::StaleVersion:
        return DocumentSynchronizationResult::StaleVersion;
    }
    return DocumentSynchronizationResult::ProjectResolutionFailed;
}

} // namespace

DocumentSynchronizationService::DocumentSynchronizationService(
    LifecycleService& lifecycle, DocumentStore& documents, ProjectManager& projects,
        AnalysisScheduler& scheduler, DiagnosticPublisher& diagnostics)
        : lifecycle_(lifecycle), documents_(documents), projects_(projects), scheduler_(scheduler),
            diagnostics_(diagnostics) {}

DocumentSynchronizationResult DocumentSynchronizationService::open(
    std::string uri, std::string languageId, int64_t version, std::string text) {
    if (!lifecycle_.acceptsDocumentUpdates()) {
        return DocumentSynchronizationResult::NotReady;
    }

    const auto update = documents_.open(uri, std::move(languageId), version, std::move(text));
    if (update != DocumentUpdateResult::Applied) {
        return translate(update);
    }

    const auto assignment = projects_.documentOpened(uri);
    if (assignment != ProjectAssignmentResult::Assigned) {
        documents_.close(uri);
        return assignment == ProjectAssignmentResult::InvalidUri
            ? DocumentSynchronizationResult::InvalidUri
            : DocumentSynchronizationResult::ProjectResolutionFailed;
    }
    diagnostics_.documentOpened(uri);
    return schedule(uri) ? DocumentSynchronizationResult::Applied
        : DocumentSynchronizationResult::ProjectResolutionFailed;
}

DocumentSynchronizationResult DocumentSynchronizationService::change(
    std::string_view uri, int64_t version, std::string text) {
    if (!lifecycle_.acceptsDocumentUpdates()) {
        return DocumentSynchronizationResult::NotReady;
    }
    if (!projects_.projectForDocument(uri)) {
        return DocumentSynchronizationResult::NotOpen;
    }

    const auto update = documents_.applyFullChange(uri, version, std::move(text));
    if (update != DocumentUpdateResult::Applied) {
        return translate(update);
    }
    if (projects_.documentChanged(uri) != ProjectAssignmentResult::Assigned) {
        return DocumentSynchronizationResult::ProjectResolutionFailed;
    }
    return schedule(uri) ? DocumentSynchronizationResult::Applied
        : DocumentSynchronizationResult::ProjectResolutionFailed;
}

DocumentSynchronizationResult DocumentSynchronizationService::close(std::string_view uri) {
    if (!lifecycle_.acceptsDocumentUpdates()) {
        return DocumentSynchronizationResult::NotReady;
    }
    const ManagedProject* project = projects_.projectForDocument(uri);
    if (!project || !documents_.close(uri)) {
        return DocumentSynchronizationResult::NotOpen;
    }
    const bool standalone = project->isStandalone;
    if (projects_.documentClosed(uri) != ProjectAssignmentResult::Assigned) {
        return DocumentSynchronizationResult::ProjectResolutionFailed;
    }
    diagnostics_.documentClosed(uri, standalone);
    return schedule(uri) ? DocumentSynchronizationResult::Applied
        : DocumentSynchronizationResult::ProjectResolutionFailed;
}

bool DocumentSynchronizationService::schedule(std::string_view uri) {
    const ManagedProject* project = projects_.projectForDocument(uri);
    if (!project) {
        return false;
    }
    return ScheduleProjectAnalysis(projects_, scheduler_, project->id);
}

} // namespace rls::lsp