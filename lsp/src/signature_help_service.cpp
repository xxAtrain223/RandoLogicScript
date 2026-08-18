#include "rls/lsp/signature_help_service.h"

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
    const auto identity = projects.sourceIdentityForDocument(uri);
    if (!project || !identity) return std::nullopt;
    const std::string projectId = project->id;
    const uint64_t generation = project->generation;
    auto snapshot = scheduler.acceptedSnapshot(projectId);
    if (!snapshot || snapshot->generation() != generation) {
        snapshot = scheduler.awaitSnapshot(projectId, generation);
    }
    if (!snapshot || snapshot->generation() != generation) return std::nullopt;
    const std::string& documentPath = *identity;
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

bool isBeforeOrEqual(ast::Position left, ast::Position right) {
    return left.line < right.line
        || (left.line == right.line && left.column <= right.column);
}

std::optional<size_t> activeArgumentAt(
    const parser::CallContext& call, ast::Position cursor) {
    if (call.activeArgument) return call.activeArgument;
    if (!isBeforeOrEqual(call.callee.end, cursor)) return std::nullopt;
    if (call.argumentRanges.empty()) return 0;
    for (size_t index = 0; index < call.argumentRanges.size(); ++index) {
        if (isBeforeOrEqual(cursor, call.argumentRanges[index].end)) return index;
    }
    return call.argumentRanges.size() - 1;
}

} // namespace

SignatureHelpService::SignatureHelpService(
    const ProjectManager& projects, AnalysisScheduler& scheduler)
    : projects_(projects), scheduler_(scheduler) {}

std::optional<SignatureHelpResult> SignatureHelpService::signatureHelp(
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

    const auto sourceCall = document->sourceIndex->enclosingCall(*cursor);
    const auto call = document->snapshot->callAt(document->path, *cursor);
    if (!sourceCall || !call || !call->target) return std::nullopt;
    const auto callable = document->snapshot->declaration(*call->target);
    if (!callable || (callable->category != sema::SymbolCategory::Define
        && callable->category != sema::SymbolCategory::ExternDefine)) {
        return std::nullopt;
    }

    std::vector<const sema::SymbolRecord*> parameters;
    for (const auto& symbol : document->snapshot->semanticIndex().symbols()) {
        if (symbol.category == sema::SymbolCategory::Parameter
            && symbol.container == callable->id) {
            parameters.push_back(&symbol);
        }
    }
    std::sort(parameters.begin(), parameters.end(), [](const auto* left, const auto* right) {
        return std::tie(left->selection.start.line, left->selection.start.column)
            < std::tie(right->selection.start.line, right->selection.start.column);
    });

    PresentationCallable presentation{.name = callable->displayName};
    for (const auto* parameter : parameters) {
        presentation.parameters.push_back({
            .name = parameter->displayName,
            .type = parameter->type
                ? presentationType(*parameter->type, parameter->enumName)
                : PresentationType{"<unknown>"},
            .defaultValue = parameter->defaultValue,
            .optional = parameter->optional,
        });
    }
    if (callable->type) {
        presentation.returnType = presentationType(
            *callable->type, callable->enumName);
    }

    PresentationSymbol symbol{
        .kind = PresentationSymbolKind::Function,
        .name = callable->displayName,
        .provenance = presentationProvenance(callable->provenance),
        .callable = presentation,
    };
    const auto rendered = PresentationRenderer{}.render(symbol);
    SignatureHelpResult result{
        .label = rendered.detail,
        .documentation = rendered.documentation,
    };
    for (const auto& parameter : presentation.parameters) {
        result.parameterLabels.push_back(
            PresentationRenderer::renderParameter(parameter));
    }

    const auto activeArgument = activeArgumentAt(*sourceCall, *cursor);
    if (activeArgument && *activeArgument < call->normalizedBindings.size()) {
        const auto binding = call->normalizedBindings[*activeArgument];
        if (binding && *binding < parameters.size()) result.activeParameter = *binding;
    } else if (activeArgument && call->normalizedBindings.empty()
        && !parameters.empty()) {
        result.activeParameter = 0;
    }
    return result;
}

} // namespace rls::lsp