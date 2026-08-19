#include "rls/lsp/rename_service.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <limits>
#include <map>

#include "rls/lsp/analysis_scheduler.h"
#include "rls/lsp/document_store.h"
#include "rls/lsp/document_uri.h"
#include "rls/lsp/project_manager.h"

namespace rls::lsp {
namespace {

struct RenameQuery {
    AnalysisScheduler::Snapshot snapshot;
    sema::SymbolRecord declaration;
    sema::OccurrenceRecord occurrence;
    std::string occurrenceText;
};

bool sameSpan(const ast::Span& left, const ast::Span& right) {
    return left.file == right.file
        && left.start.line == right.start.line
        && left.start.column == right.start.column
        && left.end.line == right.end.line
        && left.end.column == right.end.column;
}

std::optional<NavigationRange> rangeFor(
    const sema::AnalysisSnapshot& snapshot, const ast::Span& span) {
    const auto* source = snapshot.sourceText(span.file);
    if (!source) return std::nullopt;
    const auto startOffset = source->byteOffsetFromUtf8Position(span.start);
    const auto endOffset = source->byteOffsetFromUtf8Position(span.end);
    if (!startOffset || !endOffset) return std::nullopt;
    const auto start = source->utf16PositionAtByteOffset(*startOffset);
    const auto end = source->utf16PositionAtByteOffset(*endOffset);
    if (!start || !end) return std::nullopt;
    return NavigationRange{
        {start->line - 1, start->column - 1},
        {end->line - 1, end->column - 1},
    };
}

bool isRenameable(
    const sema::SymbolRecord& symbol, const sema::OccurrenceRecord& occurrence) {
    if (symbol.category == sema::SymbolCategory::ExternEnumPattern) {
        return occurrence.kind == sema::OccurrenceKind::ExitTarget;
    }
    if (symbol.provenance != sema::SymbolProvenance::Source
        && symbol.category != sema::SymbolCategory::ExternDefine
        && symbol.category != sema::SymbolCategory::Enum
        && symbol.category != sema::SymbolCategory::EnumMember) {
        return false;
    }
    switch (symbol.category) {
    case sema::SymbolCategory::Region:
    case sema::SymbolCategory::Define:
    case sema::SymbolCategory::ExternDefine:
    case sema::SymbolCategory::Enum:
    case sema::SymbolCategory::EnumMember:
    case sema::SymbolCategory::Parameter:
    case sema::SymbolCategory::RegionDataEntry:
    case sema::SymbolCategory::SectionEntry:
        return true;
    case sema::SymbolCategory::RegionExtension:
    case sema::SymbolCategory::ExternEnumPattern:
        return false;
    }
    return false;
}

std::optional<std::string> occurrenceText(
    const sema::AnalysisSnapshot& snapshot, const sema::OccurrenceRecord& occurrence) {
    const auto* source = snapshot.sourceText(occurrence.span.file);
    if (!source) return std::nullopt;
    const auto start = source->byteOffsetFromUtf8Position(occurrence.span.start);
    const auto end = source->byteOffsetFromUtf8Position(occurrence.span.end);
    if (!start || !end || *start > *end) return std::nullopt;
    return source->content().substr(*start, *end - *start);
}

bool validIdentifier(std::string_view value) {
    if (value.empty()) return false;
    const auto alpha = [](char character) {
        return std::isalpha(static_cast<unsigned char>(character)) != 0;
    };
    const auto alnum = [](char character) {
        return std::isalnum(static_cast<unsigned char>(character)) != 0;
    };
    if (!alpha(value.front()) && value.front() != '_') return false;
    if (!std::all_of(value.begin() + 1, value.end(), [&](char character) {
        return alnum(character) || character == '_';
    })) return false;
    static constexpr std::array reserved{
        "region", "extend", "extern", "define", "enum", "events", "locations",
        "exits", "true", "false", "always", "never", "and", "or", "not",
        "is", "here", "match",
    };
    return std::find(reserved.begin(), reserved.end(), value) == reserved.end();
}

bool conflicts(
    const sema::SemanticIndex& index, const sema::SymbolRecord& target,
    std::string_view newName) {
    if (newName == target.displayName) return false;
    for (const auto& candidate : index.symbols()) {
        if (target.category == sema::SymbolCategory::EnumMember
            && candidate.category == sema::SymbolCategory::ExternEnumPattern
            && candidate.container == target.container
            && index.patternMatches(candidate.id, newName)) {
            return true;
        }
        if (candidate.displayName != newName) continue;
        if (target.category == sema::SymbolCategory::Parameter) {
            if (candidate.category == target.category && candidate.container == target.container) {
                return true;
            }
            continue;
        }
        if (target.category == sema::SymbolCategory::EnumMember) {
            if (candidate.category == target.category && candidate.container == target.container) {
                return true;
            }
            continue;
        }
        if (target.category == sema::SymbolCategory::SectionEntry) {
            if (candidate.category == target.category && candidate.type == target.type) return true;
            continue;
        }
        if (target.category == sema::SymbolCategory::RegionDataEntry) {
            if (candidate.category == target.category) return true;
            continue;
        }
        if (candidate.category == target.category) return true;
        if (((target.category == sema::SymbolCategory::Define
                    || target.category == sema::SymbolCategory::ExternDefine)
                && (candidate.category == sema::SymbolCategory::Define
                    || candidate.category == sema::SymbolCategory::ExternDefine))
            || (target.category == sema::SymbolCategory::Enum
                && candidate.category == sema::SymbolCategory::Enum)) {
            return true;
        }
    }
    return false;
}

bool patternConflicts(
    const sema::AnalysisSnapshot& snapshot, const sema::SymbolRecord& target,
    std::string_view currentName, std::string_view newName) {
    if (newName == currentName) return false;
    for (const auto& symbol : snapshot.semanticIndex().symbols()) {
        if (symbol.displayName != newName) continue;
        if (symbol.category == sema::SymbolCategory::Region
            || (symbol.category == sema::SymbolCategory::EnumMember
                && symbol.container == target.container)) {
            return true;
        }
    }
    for (const auto& occurrence : snapshot.references(target.id)) {
        const auto text = occurrenceText(snapshot, occurrence);
        if (text && *text == newName) return true;
    }
    return false;
}

RenameResult<RenameQuery> queryAt(
    const ProjectManager& projects, const AnalysisScheduler& scheduler,
    std::string_view uri, NavigationPosition position) {
    const auto* project = projects.projectForDocument(uri);
    const auto identity = projects.sourceIdentityForDocument(uri);
    if (!project || !identity) return {{}, RenameError::NotRenameable};
    const auto snapshot = scheduler.acceptedSnapshot(project->id);
    if (!snapshot || snapshot->generation() != project->generation) {
        return {{}, RenameError::StaleSnapshot};
    }
    const auto* source = snapshot->sourceText(*identity);
    const auto* sourceIndex = snapshot->sourceIndex(*identity);
    if (!source || !sourceIndex
        || position.line == std::numeric_limits<uint32_t>::max()
        || position.character == std::numeric_limits<uint32_t>::max()) {
        return {{}, RenameError::NotRenameable};
    }
    const auto offset = source->byteOffsetFromUtf16Position({
        position.line + 1, position.character + 1});
    if (!offset) return {{}, RenameError::NotRenameable};
    std::optional<sema::OccurrenceRecord> occurrence;
    std::optional<rls::parser::SourceNameContext> sourceName;
    const auto findAt = [&](size_t byteOffset) {
        const auto sourcePosition = source->utf8PositionAtByteOffset(byteOffset);
        if (!sourcePosition) return false;
        occurrence = snapshot->occurrenceAt(*identity, *sourcePosition);
        sourceName = sourceIndex->nameAt(*sourcePosition);
        return occurrence && occurrence->symbol && sourceName
            && sameSpan(sourceName->span, occurrence->span);
    };
    if (!findAt(*offset)) {
        if (*offset == 0
            || !std::isalnum(static_cast<unsigned char>(source->content()[*offset - 1]))
                && source->content()[*offset - 1] != '_'
            || !findAt(*offset - 1)) {
            return {{}, RenameError::NotRenameable};
        }
    }
    const auto declaration = snapshot->declaration(*occurrence->symbol);
    if (!declaration || !isRenameable(*declaration, *occurrence)) {
        return {{}, RenameError::NotRenameable};
    }
    return {RenameQuery{
        snapshot, *declaration, *occurrence, sourceName->text}, RenameError::None};
}

} // namespace

RenameService::RenameService(
    const DocumentStore& documents, const ProjectManager& projects,
    const AnalysisScheduler& scheduler)
    : documents_(documents), projects_(projects), scheduler_(scheduler) {}

RenameResult<NavigationRange> RenameService::prepare(
    std::string_view uri, NavigationPosition position) const {
    const auto query = queryAt(projects_, scheduler_, uri, position);
    if (!query.value) return {{}, query.error};
    const auto result = rangeFor(*query.value->snapshot, query.value->occurrence.span);
    return result ? RenameResult<NavigationRange>{*result, RenameError::None}
                  : RenameResult<NavigationRange>{{}, RenameError::NotRenameable};
}

RenameResult<RenameWorkspaceEdit> RenameService::rename(
    std::string_view uri, NavigationPosition position, std::string_view newName,
    bool supportsDocumentChanges) const {
    if (!supportsDocumentChanges) return {{}, RenameError::UnsupportedClient};
    if (!validIdentifier(newName)) return {{}, RenameError::InvalidName};
    const auto query = queryAt(projects_, scheduler_, uri, position);
    if (!query.value) return {{}, query.error};
    const bool patternBacked = query.value->declaration.category
        == sema::SymbolCategory::ExternEnumPattern;
    if (patternBacked && !query.value->snapshot->semanticIndex().patternMatches(
            query.value->declaration.id, newName)) {
        return {{}, RenameError::InvalidName};
    }
    if ((patternBacked && patternConflicts(
            *query.value->snapshot, query.value->declaration,
            query.value->occurrenceText, newName))
        || (!patternBacked && conflicts(
            query.value->snapshot->semanticIndex(), query.value->declaration, newName))) {
        return {{}, RenameError::Collision};
    }

    std::map<std::string, RenameDocumentEdit> grouped;
    for (const auto& occurrence : query.value->snapshot->references(query.value->declaration.id)) {
        if (patternBacked) {
            const auto text = occurrenceText(*query.value->snapshot, occurrence);
            if (!text || *text != query.value->occurrenceText) continue;
        }
        const auto occurrenceUri = PathToFileUri(occurrence.span.file);
        const auto occurrenceRange = rangeFor(*query.value->snapshot, occurrence.span);
        if (!occurrenceUri || !occurrenceRange) return {{}, RenameError::StaleSnapshot};
        auto [entry, inserted] = grouped.try_emplace(
            *occurrenceUri, RenameDocumentEdit{*occurrenceUri, std::nullopt, {}});
        if (inserted) {
            if (const auto* document = documents_.find(*occurrenceUri)) {
                entry->second.version = document->version;
            }
        }
        entry->second.edits.push_back({*occurrenceRange, std::string(newName)});
    }
    RenameWorkspaceEdit edit;
    for (auto& [unused, document] : grouped) {
        edit.documents.push_back(std::move(document));
    }
    return {std::move(edit), RenameError::None};
}

std::string_view RenameErrorMessage(RenameError error) {
    switch (error) {
    case RenameError::NotRenameable: return "symbol cannot be renamed";
    case RenameError::InvalidName: return "new name is not a valid RLS identifier";
    case RenameError::Collision: return "new name conflicts with an existing declaration";
    case RenameError::StaleSnapshot: return "rename requires a current analysis snapshot";
    case RenameError::UnsupportedClient:
        return "client does not support versioned workspace document changes";
    case RenameError::None: return "";
    }
    return "rename failed";
}

} // namespace rls::lsp