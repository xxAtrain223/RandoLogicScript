#include "rls/lsp/hover_service.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <tuple>

#include "rls/lsp/document_uri.h"

namespace rls::lsp {
namespace {

struct CurrentDocument {
    AnalysisScheduler::Snapshot snapshot;
    std::string path;
    const ast::SourceText* source = nullptr;
    const parser::SourceIndex* sourceIndex = nullptr;
};

std::string pathString(const std::filesystem::path& path) {
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    const auto generic = (error ? path.lexically_normal() : canonical).generic_u8string();
    std::string result;
    result.reserve(generic.size());
    for (const char8_t byte : generic) result.push_back(static_cast<char>(byte));
    return result;
}

std::optional<CurrentDocument> currentDocument(
    const ProjectManager& projects, AnalysisScheduler& scheduler,
    std::string_view uri) {
    const auto* project = projects.projectForDocument(uri);
    const auto path = FileUriToPath(uri);
    if (!project || !path) return std::nullopt;
    const std::string projectId = project->id;
    const uint64_t generation = project->generation;
    auto snapshot = scheduler.acceptedSnapshot(projectId);
    if (!snapshot || snapshot->generation() != generation) {
        snapshot = scheduler.awaitSnapshot(projectId, generation);
    }
    if (!snapshot || snapshot->generation() != generation) return std::nullopt;
    const std::string documentPath = pathString(*path);
    const auto* source = snapshot->sourceText(documentPath);
    const auto* sourceIndex = snapshot->sourceIndex(documentPath);
    if (!source || !sourceIndex) return std::nullopt;
    return CurrentDocument{snapshot, documentPath, source, sourceIndex};
}

PresentationType presentationType(
    ast::Type type, const std::optional<std::string>& enumName) {
    std::string name;
    switch (type) {
    case ast::Type::Bool: name = "Bool"; break;
    case ast::Type::Int: name = "Int"; break;
    case ast::Type::String: name = "String"; break;
    case ast::Type::List: name = "List"; break;
    case ast::Type::Callable: name = "Callable"; break;
    case ast::Type::Condition: name = "Condition"; break;
    case ast::Type::Enum: name = "Enum"; break;
    case ast::Type::Region: name = "Region"; break;
    case ast::Type::Event: name = "Event"; break;
    case ast::Type::Location: name = "Location"; break;
    case ast::Type::Void: name = "Void"; break;
    case ast::Type::Error: name = "<error>"; break;
    }
    return {std::move(name), enumName};
}

PresentationProvenance presentationProvenance(sema::SymbolProvenance provenance) {
    switch (provenance) {
    case sema::SymbolProvenance::Source: return PresentationProvenance::Source;
    case sema::SymbolProvenance::Extern: return PresentationProvenance::Extern;
    case sema::SymbolProvenance::Pattern: return PresentationProvenance::Pattern;
    }
    return PresentationProvenance::Source;
}

std::optional<PresentationRange> presentationRange(
    const ast::SourceText& source, const ast::Span& span) {
    const auto startOffset = source.byteOffsetFromUtf8Position(span.start);
    const auto endOffset = source.byteOffsetFromUtf8Position(span.end);
    if (!startOffset || !endOffset) return std::nullopt;
    const auto start = source.utf16PositionAtByteOffset(*startOffset);
    const auto end = source.utf16PositionAtByteOffset(*endOffset);
    if (!start || !end) return std::nullopt;
    return PresentationRange{
        {start->line - 1, start->column - 1},
        {end->line - 1, end->column - 1},
    };
}

bool containsInclusive(const ast::Span& span, ast::Position position) {
    const auto beforeOrEqual = [](ast::Position left, ast::Position right) {
        return left.line < right.line
            || (left.line == right.line && left.column <= right.column);
    };
    return span.start.line != 0 && beforeOrEqual(span.start, position)
        && beforeOrEqual(position, span.end);
}

std::string categoryDescription(
    const sema::AnalysisSnapshot& snapshot, const sema::SymbolRecord& record) {
    switch (record.category) {
    case sema::SymbolCategory::Region: return "Region value.";
    case sema::SymbolCategory::RegionExtension: return "Region extension.";
    case sema::SymbolCategory::Define: return "Function declaration.";
    case sema::SymbolCategory::ExternDefine: return "External function declaration.";
    case sema::SymbolCategory::Enum: return "Enumeration type.";
    case sema::SymbolCategory::EnumMember:
        return record.enumName
            ? "Member of enum `" + *record.enumName + "`."
            : "Enumeration member.";
    case sema::SymbolCategory::ExternEnumPattern:
        return record.enumName
            ? "Concrete value matched by extern enum pattern `"
                + record.displayName + "` in `" + *record.enumName + "`."
            : "Concrete value matched by an external enum wildcard pattern.";
    case sema::SymbolCategory::Parameter:
        if (record.container) {
            const auto container = snapshot.declaration(*record.container);
            if (container) return "Parameter of `" + container->displayName + "`.";
        }
        return "Function parameter.";
    case sema::SymbolCategory::RegionDataEntry: return "Region data property.";
    case sema::SymbolCategory::SectionEntry:
        if (record.type == ast::Type::Event) return "Declared event value.";
        if (record.type == ast::Type::Location) return "Declared location value.";
        return "Region section entry.";
    }
    return {};
}

PresentationSymbol presentationSymbol(
    const sema::AnalysisSnapshot& snapshot, const sema::SymbolRecord& record) {
    PresentationSymbol result{
        .name = record.displayName,
        .provenance = presentationProvenance(record.provenance),
    };
    if (record.type) result.type = presentationType(*record.type, record.enumName);
    switch (record.category) {
    case sema::SymbolCategory::Region:
    case sema::SymbolCategory::RegionExtension:
        result.kind = PresentationSymbolKind::Region;
        break;
    case sema::SymbolCategory::Define:
    case sema::SymbolCategory::ExternDefine: {
        result.kind = PresentationSymbolKind::Function;
        PresentationCallable callable{.name = record.displayName};
        std::vector<const sema::SymbolRecord*> parameters;
        for (const auto& candidate : snapshot.semanticIndex().symbols()) {
            if (candidate.category == sema::SymbolCategory::Parameter
                && candidate.container == record.id) {
                parameters.push_back(&candidate);
            }
        }
        std::sort(parameters.begin(), parameters.end(), [](const auto* left, const auto* right) {
            return std::tie(left->selection.start.line, left->selection.start.column)
                < std::tie(right->selection.start.line, right->selection.start.column);
        });
        for (const auto* parameter : parameters) {
            callable.parameters.push_back({
                .name = parameter->displayName,
                .type = parameter->type
                    ? presentationType(*parameter->type, parameter->enumName)
                    : PresentationType{"<unknown>"},
                .defaultValue = parameter->defaultValue,
                .optional = parameter->optional,
            });
        }
        if (record.type) callable.returnType = presentationType(*record.type, record.enumName);
        result.callable = std::move(callable);
        break;
    }
    case sema::SymbolCategory::Enum:
        result.kind = PresentationSymbolKind::Enum;
        break;
    case sema::SymbolCategory::EnumMember:
    case sema::SymbolCategory::ExternEnumPattern:
        result.kind = PresentationSymbolKind::EnumMember;
        break;
    case sema::SymbolCategory::Parameter:
        result.kind = PresentationSymbolKind::Parameter;
        result.documentation.push_back({
            .heading = std::nullopt,
            .markdown = categoryDescription(snapshot, record),
        });
        break;
    case sema::SymbolCategory::RegionDataEntry:
        result.kind = PresentationSymbolKind::Property;
        break;
    case sema::SymbolCategory::SectionEntry:
        result.kind = PresentationSymbolKind::Value;
        break;
    }
    if (record.category != sema::SymbolCategory::Parameter) {
        result.documentation.push_back({
            .heading = std::nullopt,
            .markdown = categoryDescription(snapshot, record),
        });
    }

    const auto uri = PathToFileUri(record.declaration.file);
    const auto* source = snapshot.sourceText(record.declaration.file);
    const auto range = source ? presentationRange(*source, record.selection) : std::nullopt;
    if (uri && range) {
        result.declaration = PresentationLocation{*uri, *range};
        const uint32_t line = range->start.line + 1;
        const uint32_t character = range->start.character + 1;
        result.documentation.push_back({
            .heading = "Declaration",
            .markdown = "[Open declaration](" + *uri + "#L"
                + std::to_string(line) + "," + std::to_string(character) + ")",
        });
    }
    return result;
}

std::string hoverMarkdown(const RenderedPresentation& rendered) {
    std::string result = "```rls\n" + rendered.detail + "\n```";
    if (!rendered.documentation.empty()) result += "\n\n" + rendered.documentation;
    return result;
}

} // namespace

HoverService::HoverService(
    const ProjectManager& projects, AnalysisScheduler& scheduler)
    : projects_(projects), scheduler_(scheduler) {}

std::optional<HoverResult> HoverService::hover(
    std::string_view uri, PresentationPosition position) const {
    if (position.line == std::numeric_limits<uint32_t>::max()
        || position.character == std::numeric_limits<uint32_t>::max()) {
        return std::nullopt;
    }
    const auto document = currentDocument(projects_, scheduler_, uri);
    if (!document) return std::nullopt;
    const auto cursorOffset = document->source->byteOffsetFromUtf16Position({
        position.line + 1, position.character + 1});
    if (!cursorOffset) return std::nullopt;
    const auto cursor = document->source->utf8PositionAtByteOffset(*cursorOffset);
    if (!cursor) return std::nullopt;

    if (const auto occurrence = document->snapshot->occurrenceAt(document->path, *cursor)) {
        if (!occurrence->symbol) return std::nullopt;
        const auto declaration = document->snapshot->declaration(*occurrence->symbol);
        const auto range = presentationRange(*document->source, occurrence->span);
        if (!declaration || !range) return std::nullopt;
        auto presentation = presentationSymbol(*document->snapshot, *declaration);
        if (declaration->category == sema::SymbolCategory::ExternEnumPattern) {
            const auto sourceName = document->sourceIndex->nameAt(*cursor);
            if (sourceName && sourceName->span.file == occurrence->span.file
                && sourceName->span.start.line == occurrence->span.start.line
                && sourceName->span.start.column == occurrence->span.start.column
                && sourceName->span.end.line == occurrence->span.end.line
                && sourceName->span.end.column == occurrence->span.end.column) {
                presentation.name = sourceName->text;
            }
        }
        const auto rendered = PresentationRenderer{}.render(presentation);
        return HoverResult{hoverMarkdown(rendered), *range};
    }

    if (const auto sourceCall = document->sourceIndex->enclosingCall(*cursor);
        sourceCall && containsInclusive(sourceCall->callee, *cursor)) {
        const auto call = document->snapshot->callAt(document->path, *cursor);
        if (call && call->target) {
            const auto declaration = document->snapshot->declaration(*call->target);
            const auto range = presentationRange(*document->source, sourceCall->callee);
            if (declaration && range) {
                const auto rendered = PresentationRenderer{}.render(
                    presentationSymbol(*document->snapshot, *declaration));
                return HoverResult{hoverMarkdown(rendered), *range};
            }
        }
    }

    const auto type = document->snapshot->typeAt(document->path, *cursor);
    if (!type || type->type == ast::Type::Error) return std::nullopt;
    const auto range = presentationRange(*document->source, type->span);
    if (!range) return std::nullopt;
    PresentationSymbol expression{
        .kind = PresentationSymbolKind::Value,
        .name = "expression",
        .type = presentationType(type->type, type->enumName),
        .documentation = {{
            .heading = std::nullopt,
            .markdown = "Inferred expression type.",
        }},
    };
    return HoverResult{
        hoverMarkdown(PresentationRenderer{}.render(expression)), *range};
}

} // namespace rls::lsp
