#include "rls/lsp/navigation_service.h"

#include <filesystem>
#include <limits>

#include "rls/lsp/document_uri.h"

namespace rls::lsp {
namespace {

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

} // namespace

NavigationService::NavigationService(
    const ProjectManager& projects, const AnalysisScheduler& scheduler)
    : projects_(projects), scheduler_(scheduler) {}

std::optional<DefinitionResult> NavigationService::definition(
    std::string_view uri, NavigationPosition position) const {
    if (position.line == std::numeric_limits<uint32_t>::max()
        || position.character == std::numeric_limits<uint32_t>::max()) {
        return std::nullopt;
    }
    const auto* project = projects_.projectForDocument(uri);
    const auto path = FileUriToPath(uri);
    if (!project || !path) {
        return std::nullopt;
    }
    const auto snapshot = scheduler_.acceptedSnapshot(project->id);
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
    const auto declaration = snapshot->declaration(*symbol);
    if (!declaration || declaration->provenance == sema::SymbolProvenance::Pattern) {
        return std::nullopt;
    }

    const auto originRange = rangeFor(*snapshot, occurrence->span);
    const auto targetRange = rangeFor(*snapshot, declaration->declaration);
    const auto targetSelectionRange = rangeFor(*snapshot, declaration->selection);
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

} // namespace rls::lsp