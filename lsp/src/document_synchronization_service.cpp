#include "rls/lsp/document_synchronization_service.h"

#include <utility>

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
    AnalysisScheduler& scheduler)
    : lifecycle_(lifecycle), documents_(documents), projects_(projects), scheduler_(scheduler) {}

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
    if (!projects_.projectForDocument(uri) || !documents_.close(uri)) {
        return DocumentSynchronizationResult::NotOpen;
    }
    if (projects_.documentClosed(uri) != ProjectAssignmentResult::Assigned) {
        return DocumentSynchronizationResult::ProjectResolutionFailed;
    }
    return schedule(uri) ? DocumentSynchronizationResult::Applied
        : DocumentSynchronizationResult::ProjectResolutionFailed;
}

bool DocumentSynchronizationService::schedule(std::string_view uri) {
    const ManagedProject* project = projects_.projectForDocument(uri);
    if (!project) {
        return false;
    }
    const std::string projectId = project->id;
    ProjectSourceSet sourceSet = projects_.sourceSetForDocument(uri);
    if (!sourceSet.error.empty()) {
        return false;
    }

    std::vector<sema::SourceInput> sources;
    sources.reserve(sourceSet.sources.size());
    for (auto& source : sourceSet.sources) {
        const auto genericPath = source.path.generic_u8string();
        std::string path;
        path.reserve(genericPath.size());
        for (const char8_t byte : genericPath) {
            path.push_back(static_cast<char>(byte));
        }
        sources.push_back({std::move(path), std::move(source.content)});
    }

    return scheduler_.schedule({
        projectId,
        sourceSet.generation,
        std::move(sources),
    });
}

} // namespace rls::lsp