#include "rls/lsp/project_manager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <utility>

#include "rls/lsp/document_uri.h"

namespace rls::lsp {
namespace {

std::filesystem::path canonicalPath(const std::filesystem::path& path) {
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    return error ? path.lexically_normal() : canonical;
}

std::string pathKey(const std::filesystem::path& path) {
    const auto generic = canonicalPath(path).generic_u8string();
    std::string key;
    key.reserve(generic.size());
    for (const char8_t byte : generic) {
        key.push_back(static_cast<char>(byte));
    }
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(), [](char character) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    });
#endif
    return key;
}

} // namespace

ProjectManager::ProjectManager(DocumentStore& documents, Resolver resolver)
    : documents_(documents), resolver_(std::move(resolver)) {}

ProjectAssignmentResult ProjectManager::documentOpened(std::string_view uri) {
    const auto key = DocumentUriKey(uri);
    const auto path = FileUriToPath(uri);
    if (!key || !path) {
        return ProjectAssignmentResult::InvalidUri;
    }
    const TextDocument* document = documents_.find(uri);
    if (!document) {
        return ProjectAssignmentResult::NotAssigned;
    }

    project::FileProject resolved = resolver_(*path);
    if (!resolved.error.empty()) {
        return ProjectAssignmentResult::ResolutionFailed;
    }
    if (resolved.sourceFiles.empty()) {
        return ProjectAssignmentResult::ResolutionFailed;
    }

    const std::string id = projectId(resolved);
    auto [projectIt, inserted] = projects_.try_emplace(id);
    ManagedProject& managed = projectIt->second;
    if (inserted) {
        managed.id = id;
    }
    managed.manifestPath = resolved.manifest
        ? std::optional(resolved.manifest->manifestPath) : std::nullopt;
    managed.sourceFiles = std::move(resolved.sourceFiles);
    managed.isStandalone = resolved.isStandalone;
    ++managed.generation;

    const auto canonicalDocumentPath = canonicalPath(*path);
    assignments_.insert_or_assign(*key, Assignment{
        document->uri,
        canonicalDocumentPath,
        pathKey(canonicalDocumentPath),
        id,
    });
    return ProjectAssignmentResult::Assigned;
}

ProjectAssignmentResult ProjectManager::documentChanged(std::string_view uri) {
    const auto key = DocumentUriKey(uri);
    if (!key) {
        return ProjectAssignmentResult::InvalidUri;
    }
    const auto assignment = assignments_.find(*key);
    if (assignment == assignments_.end()) {
        return ProjectAssignmentResult::NotAssigned;
    }
    ++projects_.at(assignment->second.projectId).generation;
    return ProjectAssignmentResult::Assigned;
}

ProjectAssignmentResult ProjectManager::documentClosed(std::string_view uri) {
    return documentChanged(uri);
}

const ManagedProject* ProjectManager::projectForDocument(std::string_view uri) const {
    const auto key = DocumentUriKey(uri);
    if (!key) {
        return nullptr;
    }
    const auto assignment = assignments_.find(*key);
    if (assignment == assignments_.end()) {
        return nullptr;
    }
    const auto project = projects_.find(assignment->second.projectId);
    return project == projects_.end() ? nullptr : &project->second;
}

ProjectSourceSet ProjectManager::sourceSetForDocument(std::string_view uri) const {
    ProjectSourceSet result;
    const auto project = projectForDocument(uri);
    if (!project) {
        result.error = "document is not assigned to a project";
        return result;
    }
    result.generation = project->generation;

    for (const auto& sourcePath : project->sourceFiles) {
        const TextDocument* overlay = nullptr;
        const std::string sourcePathKey = pathKey(sourcePath);
        for (const auto& [key, assignment] : assignments_) {
            if (assignment.projectId == project->id && assignment.pathKey == sourcePathKey) {
                overlay = documents_.find(assignment.uri);
                break;
            }
        }

        if (overlay) {
            result.sources.push_back({sourcePath, overlay->text});
            continue;
        }

        std::ifstream input(sourcePath, std::ios::binary);
        if (!input) {
            result.error = "failed to read source file: " + sourcePath.string();
            result.sources.clear();
            return result;
        }
        result.sources.push_back({sourcePath,
            std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>())});
    }
    return result;
}

std::string ProjectManager::projectId(const project::FileProject& project) {
    if (project.manifest) {
        return pathKey(project.manifest->manifestPath);
    }
    return pathKey(project.sourceFiles.front());
}

} // namespace rls::lsp