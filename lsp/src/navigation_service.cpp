#include "rls/lsp/navigation_service.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <limits>
#include <tuple>

#include "rls/lsp/document_uri.h"

namespace rls::lsp {
namespace {

struct NavigationQuery {
    AnalysisScheduler::Snapshot snapshot;
    std::string documentPath;
    sema::SymbolId symbol;
    sema::OccurrenceRecord occurrence;
};

struct CurrentDocument {
    AnalysisScheduler::Snapshot snapshot;
    std::string path;
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

std::optional<CurrentDocument> currentDocument(
    const ProjectManager& projects, const AnalysisScheduler& scheduler,
    std::string_view uri) {
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
    if (!snapshot->sourceText(documentPath) || !snapshot->sourceIndex(documentPath)) {
        return std::nullopt;
    }
    return CurrentDocument{snapshot, documentPath};
}

bool sameSpan(const ast::Span& left, const ast::Span& right) {
    return left.file == right.file
        && left.start.line == right.start.line
        && left.start.column == right.start.column
        && left.end.line == right.end.line
        && left.end.column == right.end.column;
}

std::optional<NavigationQuery> queryAt(
    const ProjectManager& projects, const AnalysisScheduler& scheduler,
    std::string_view uri, NavigationPosition position) {
    if (position.line == std::numeric_limits<uint32_t>::max()
        || position.character == std::numeric_limits<uint32_t>::max()) {
        return std::nullopt;
    }
    const auto document = currentDocument(projects, scheduler, uri);
    if (!document) {
        return std::nullopt;
    }
    const ast::SourceText* source = document->snapshot->sourceText(document->path);
    const auto offset = source->byteOffsetFromUtf16Position({
        position.line + 1, position.character + 1});
    if (!offset) {
        return std::nullopt;
    }
    const auto sourcePosition = source->utf8PositionAtByteOffset(*offset);
    if (!sourcePosition) {
        return std::nullopt;
    }

    const auto symbol = document->snapshot->symbolAt(document->path, *sourcePosition);
    const auto occurrence = document->snapshot->occurrenceAt(document->path, *sourcePosition);
    const auto sourceName = document->snapshot->sourceIndex(document->path)
        ->nameAt(*sourcePosition);
    if (!symbol || !occurrence || occurrence->symbol != symbol
        || !sourceName || !sameSpan(sourceName->span, occurrence->span)) {
        return std::nullopt;
    }
    return NavigationQuery{document->snapshot, document->path, *symbol, *occurrence};
}

bool isTopLevel(sema::SymbolCategory category) {
    return category == sema::SymbolCategory::Region
        || category == sema::SymbolCategory::RegionExtension
        || category == sema::SymbolCategory::Define
        || category == sema::SymbolCategory::ExternDefine
        || category == sema::SymbolCategory::Enum;
}

std::optional<NavigationSymbolKind> symbolKind(sema::SymbolCategory category) {
    switch (category) {
    case sema::SymbolCategory::Region:
    case sema::SymbolCategory::RegionExtension:
        return NavigationSymbolKind::Namespace;
    case sema::SymbolCategory::Define:
    case sema::SymbolCategory::ExternDefine:
        return NavigationSymbolKind::Function;
    case sema::SymbolCategory::Enum:
        return NavigationSymbolKind::Enum;
    case sema::SymbolCategory::EnumMember:
    case sema::SymbolCategory::ExternEnumPattern:
        return NavigationSymbolKind::EnumMember;
    case sema::SymbolCategory::Parameter:
        return NavigationSymbolKind::Variable;
    case sema::SymbolCategory::RegionDataEntry:
        return NavigationSymbolKind::Property;
    case sema::SymbolCategory::SectionEntry:
        return NavigationSymbolKind::Field;
    }
    return std::nullopt;
}

bool sourceOrder(const sema::SymbolRecord* left, const sema::SymbolRecord* right) {
    return std::tie(left->selection.start.line, left->selection.start.column,
        left->selection.end.line, left->selection.end.column)
        < std::tie(right->selection.start.line, right->selection.start.column,
            right->selection.end.line, right->selection.end.column);
}

std::optional<size_t> workspaceCategoryOrder(sema::SymbolCategory category) {
    switch (category) {
    case sema::SymbolCategory::Region: return 0;
    case sema::SymbolCategory::RegionExtension: return 1;
    case sema::SymbolCategory::Define: return 2;
    case sema::SymbolCategory::ExternDefine: return 3;
    case sema::SymbolCategory::Enum: return 4;
    case sema::SymbolCategory::EnumMember: return 5;
    case sema::SymbolCategory::ExternEnumPattern: return 6;
    case sema::SymbolCategory::Parameter:
    case sema::SymbolCategory::RegionDataEntry:
    case sema::SymbolCategory::SectionEntry:
        return std::nullopt;
    }
    return std::nullopt;
}

std::string asciiLower(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))));
    }
    return result;
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

std::vector<NavigationDocumentSymbol> NavigationService::documentSymbols(
    std::string_view uri) const {
    const auto document = currentDocument(projects_, scheduler_, uri);
    if (!document) {
        return {};
    }
    const auto* sourceIndex = document->snapshot->sourceIndex(document->path);
    const auto declarations = sourceIndex->declarationsIn(document->path);
    const auto& records = document->snapshot->semanticIndex().symbols();

    std::function<std::optional<NavigationDocumentSymbol>(const sema::SymbolRecord&)> build;
    build = [&](const sema::SymbolRecord& record)
        -> std::optional<NavigationDocumentSymbol> {
        const auto kind = symbolKind(record.category);
        const auto symbolRange = rangeFor(*document->snapshot, record.declaration);
        const auto selectionRange = rangeFor(*document->snapshot, record.selection);
        if (!kind || !symbolRange || !selectionRange) {
            return std::nullopt;
        }

        std::vector<const sema::SymbolRecord*> childRecords;
        for (const auto& candidate : records) {
            if (candidate.declaration.file == document->path
                && candidate.container == record.id
                && !isTopLevel(candidate.category)) {
                childRecords.push_back(&candidate);
            }
        }
        std::sort(childRecords.begin(), childRecords.end(), sourceOrder);

        NavigationDocumentSymbol result{
            record.displayName, *kind, *symbolRange, *selectionRange, {}};
        for (const auto* child : childRecords) {
            if (auto symbol = build(*child)) {
                result.children.push_back(std::move(*symbol));
            }
        }
        return result;
    };

    std::vector<const sema::SymbolRecord*> topLevelRecords;
    for (const auto& record : records) {
        if (record.declaration.file != document->path || !isTopLevel(record.category)) {
            continue;
        }
        const bool parserDeclaration = std::any_of(
            declarations.begin(), declarations.end(), [&](const auto& declaration) {
                return sameSpan(declaration.span, record.declaration);
            });
        if (parserDeclaration) {
            topLevelRecords.push_back(&record);
        }
    }
    std::sort(topLevelRecords.begin(), topLevelRecords.end(), sourceOrder);

    std::vector<NavigationDocumentSymbol> result;
    for (const auto* record : topLevelRecords) {
        if (auto symbol = build(*record)) {
            result.push_back(std::move(*symbol));
        }
    }
    return result;
}

std::vector<NavigationWorkspaceSymbol> NavigationService::workspaceSymbols(
    std::string_view query, const std::vector<std::string>& projectIds) const {
    struct Candidate {
        NavigationWorkspaceSymbol symbol;
        size_t categoryOrder;
        std::string foldedName;
    };

    const std::string foldedQuery = asciiLower(query);
    std::vector<Candidate> candidates;
    for (const auto& projectId : projectIds) {
        const auto* project = projects_.project(projectId);
        const auto snapshot = scheduler_.acceptedSnapshot(projectId);
        if (!project || !snapshot || snapshot->generation() != project->generation) {
            continue;
        }
        for (const auto& record : snapshot->semanticIndex().symbols()) {
            const auto categoryOrder = workspaceCategoryOrder(record.category);
            const auto kind = symbolKind(record.category);
            const std::string foldedName = asciiLower(record.displayName);
            if (!categoryOrder || !kind
                || foldedName.find(foldedQuery) == std::string::npos) {
                continue;
            }
            const auto uri = PathToFileUri(record.selection.file);
            const auto symbolRange = rangeFor(*snapshot, record.selection);
            if (!uri || !symbolRange) {
                continue;
            }
            std::optional<std::string> containerName;
            if (record.container) {
                const auto container = snapshot->declaration(*record.container);
                if (container) containerName = container->displayName;
            }
            candidates.push_back({
                {
                    record.displayName,
                    *kind,
                    {*uri, *symbolRange},
                    std::move(containerName),
                },
                *categoryOrder,
                foldedName,
            });
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        return std::tie(left.categoryOrder, left.foldedName, left.symbol.name,
            left.symbol.location.uri, left.symbol.location.range.start.line,
            left.symbol.location.range.start.character)
            < std::tie(right.categoryOrder, right.foldedName, right.symbol.name,
                right.symbol.location.uri, right.symbol.location.range.start.line,
                right.symbol.location.range.start.character);
    });
    std::vector<NavigationWorkspaceSymbol> result;
    result.reserve(candidates.size());
    for (auto& candidate : candidates) {
        result.push_back(std::move(candidate.symbol));
    }
    return result;
}

} // namespace rls::lsp