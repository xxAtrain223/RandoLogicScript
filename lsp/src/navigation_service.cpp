#include "rls/lsp/navigation_service.h"

#include <filesystem>
#include <limits>

#include "rls/lsp/document_uri.h"

namespace rls::lsp {
namespace {

struct NavigationQuery {
    AnalysisScheduler::Snapshot snapshot;
    std::string documentPath;
    sema::SymbolId symbol;
    sema::OccurrenceRecord occurrence;
};

std::string pathString(const std::filesystem::path& path) {
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    const auto generic = (error ? path.lexically_normal() : canonical).generic_u8string();
    std::string value;
    value.reserve(generic.size());
    for (const char8_t byte : generic) {
        value.push_back(static_cast<char>(byte));
    }
    return value;
}

std::optional<NavigationRange> rangeFor(
    const sema::AnalysisSnapshot& snapshot, const ast::Span& span) {
    const ast::SourceText* source = snapshot.sourceText(span.file);
    if (!source) {
        return std::nullopt;
    }
    const auto startOffset = source->byteOffsetFromUtf8Position(span.start);
    const auto endOffset = source->byteOffsetFromUtf8Position(span.end);
    if (!startOffset || !endOffset) {
        return std::nullopt;
    }
    const auto start = source->utf16PositionAtByteOffset(*startOffset);
    const auto end = source->utf16PositionAtByteOffset(*endOffset);
    if (!start || !end) {
        return std::nullopt;
    }
    return NavigationRange{
        {start->line - 1, start->column - 1},
        {end->line - 1, end->column - 1},
    };
}

std::optional<NavigationQuery> queryAt(
    const ProjectManager& projects, const AnalysisScheduler& scheduler,
    std::string_view uri, NavigationPosition position) {
    if (position.line == std::numeric_limits<uint32_t>::max()
        || position.character == std::numeric_limits<uint32_t>::max()) {
        return std::nullopt;
    }
    const auto* project = projects.projectForDocument(uri);
    const auto path = FileUriToPath(uri);
    if (!project || !path) {
        return std::nullopt;
    }
    const auto snapshot = scheduler.acceptedSnapshot(project->id);
    if (!snapshot || snapshot->generation() != project->generation) {
        return std::nullopt;
    }

    const std::string documentPath = pathString(*path);
    const ast::SourceText* source = snapshot->sourceText(documentPath);
    if (!source) {
        return std::nullopt;
    }
    const auto offset = source->byteOffsetFromUtf16Position({
        position.line + 1, position.character + 1});
    if (!offset) {
        return std::nullopt;
    }
    const auto sourcePosition = source->utf8PositionAtByteOffset(*offset);
    if (!sourcePosition) {
        return std::nullopt;
    }

    const auto symbol = snapshot->symbolAt(documentPath, *sourcePosition);
    const auto occurrence = snapshot->occurrenceAt(documentPath, *sourcePosition);
    if (!symbol || !occurrence || occurrence->symbol != symbol) {
        return std::nullopt;
    }
    return NavigationQuery{snapshot, documentPath, *symbol, *occurrence};
}

} // namespace

NavigationService::NavigationService(
    const ProjectManager& projects, const AnalysisScheduler& scheduler)
    : projects_(projects), scheduler_(scheduler) {}

std::optional<DefinitionResult> NavigationService::definition(
    std::string_view uri, NavigationPosition position) const {
    const auto query = queryAt(projects_, scheduler_, uri, position);
    if (!query) {
        return std::nullopt;
    }
    const auto declaration = query->snapshot->declaration(query->symbol);
    if (!declaration || declaration->provenance == sema::SymbolProvenance::Pattern) {
        return std::nullopt;
    }

    const auto originRange = rangeFor(*query->snapshot, query->occurrence.span);
    const auto targetRange = rangeFor(*query->snapshot, declaration->declaration);
    const auto targetSelectionRange = rangeFor(*query->snapshot, declaration->selection);
    const auto targetUri = PathToFileUri(declaration->declaration.file);
    if (!originRange || !targetRange || !targetSelectionRange || !targetUri) {
        return std::nullopt;
    }
    return DefinitionResult{
        *originRange,
        *targetUri,
        *targetRange,
        *targetSelectionRange,
    };
}

std::vector<NavigationLocation> NavigationService::references(
    std::string_view uri, NavigationPosition position, bool includeDeclaration) const {
    const auto query = queryAt(projects_, scheduler_, uri, position);
    if (!query) {
        return {};
    }

    std::vector<NavigationLocation> result;
    for (const auto& occurrence : query->snapshot->references(query->symbol)) {
        if (!includeDeclaration && occurrence.kind == sema::OccurrenceKind::Declaration) {
            continue;
        }
        const auto occurrenceUri = PathToFileUri(occurrence.span.file);
        const auto occurrenceRange = rangeFor(*query->snapshot, occurrence.span);
        if (occurrenceUri && occurrenceRange) {
            result.push_back({*occurrenceUri, *occurrenceRange});
        }
    }
    return result;
}

std::vector<NavigationRange> NavigationService::documentHighlights(
    std::string_view uri, NavigationPosition position) const {
    const auto query = queryAt(projects_, scheduler_, uri, position);
    if (!query) {
        return {};
    }

    std::vector<NavigationRange> result;
    for (const auto& occurrence : query->snapshot->references(query->symbol)) {
        if (occurrence.span.file != query->documentPath) {
            continue;
        }
        if (const auto occurrenceRange = rangeFor(*query->snapshot, occurrence.span)) {
            result.push_back(*occurrenceRange);
        }
    }
    return result;
}

} // namespace rls::lsp