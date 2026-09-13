#include "rls/lsp/semantic_tokens_service.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <tuple>

#include "rls/lsp/document_uri.h"

namespace rls::lsp {
namespace {

enum class TokenType : uint32_t {
    Function,
    Parameter,
    Enum,
    EnumMember,
    Property,
    Variable,
    Operator,
    RlsPropertyDeclaration,
    Keyword,
};

enum class TokenModifier : uint32_t {
    Declaration,
    Definition,
    Readonly,
    DefaultLibrary,
    Deprecated,
};

struct AbsoluteToken {
    uint32_t line = 0;
    uint32_t character = 0;
    uint32_t length = 0;
    TokenType type = TokenType::Variable;
    uint32_t modifiers = 0;
};

uint32_t modifier(TokenModifier value) {
    return uint32_t{1} << static_cast<uint32_t>(value);
}

std::string pathString(const std::filesystem::path& path) {
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    const auto generic = (error ? path.lexically_normal() : canonical).generic_u8string();
    std::string result;
    result.reserve(generic.size());
    for (const char8_t byte : generic) result.push_back(static_cast<char>(byte));
    return result;
}

std::optional<TokenType> tokenType(sema::SymbolCategory category) {
    switch (category) {
    case sema::SymbolCategory::Define:
    case sema::SymbolCategory::ExternDefine:
        return TokenType::Function;
    case sema::SymbolCategory::Parameter:
        return TokenType::Parameter;
    case sema::SymbolCategory::Enum:
        return TokenType::Enum;
    case sema::SymbolCategory::EnumMember:
    case sema::SymbolCategory::ExternEnumPattern:
        return TokenType::EnumMember;
    case sema::SymbolCategory::SectionEntry:
        return TokenType::Property;
    case sema::SymbolCategory::RegionDataEntry:
    case sema::SymbolCategory::Region:
    case sema::SymbolCategory::RegionExtension:
        return std::nullopt;
    }
    return std::nullopt;
}

bool isReadonly(sema::SymbolCategory category) {
    return category == sema::SymbolCategory::EnumMember;
}

bool isDefinition(sema::SymbolCategory category) {
    return category == sema::SymbolCategory::Define
        || category == sema::SymbolCategory::Enum;
}

bool isDefaultLibrary(
    const sema::SemanticIndex& index, const sema::SymbolRecord& symbol) {
    if (symbol.provenance == sema::SymbolProvenance::Extern) return true;
    if (!symbol.container) return false;
    const auto container = index.declaration(*symbol.container);
    return container && container->provenance == sema::SymbolProvenance::Extern;
}

std::optional<AbsoluteToken> makeToken(
    const sema::AnalysisSnapshot& snapshot, const ast::SourceText& source,
    const sema::OccurrenceRecord& occurrence, const sema::SymbolRecord& symbol) {
    auto type = tokenType(symbol.category);
    const bool propertyDeclaration = occurrence.kind == sema::OccurrenceKind::Declaration
        && (symbol.category == sema::SymbolCategory::RegionDataEntry
            || symbol.category == sema::SymbolCategory::SectionEntry);
    if (propertyDeclaration) type = TokenType::RlsPropertyDeclaration;
    const bool concretePatternValue = symbol.category == sema::SymbolCategory::ExternEnumPattern
        && occurrence.kind != sema::OccurrenceKind::Declaration;
    const bool exitTarget = occurrence.kind == sema::OccurrenceKind::ExitTarget;
    if (concretePatternValue) type = TokenType::EnumMember;
    if (symbol.category == sema::SymbolCategory::SectionEntry
        && occurrence.kind != sema::OccurrenceKind::Declaration) {
        type = TokenType::EnumMember;
    }
    const bool concreteRegionValue = symbol.category == sema::SymbolCategory::Region
        && occurrence.kind != sema::OccurrenceKind::Declaration;
    if (concreteRegionValue) type = TokenType::EnumMember;
    if (exitTarget) {
        type = TokenType::Property;
    }
    if (!type || occurrence.span.start.line == 0
        || occurrence.span.start.line != occurrence.span.end.line) {
        return std::nullopt;
    }
    const auto startOffset = source.byteOffsetFromUtf8Position(occurrence.span.start);
    const auto endOffset = source.byteOffsetFromUtf8Position(occurrence.span.end);
    if (!startOffset || !endOffset || *startOffset >= *endOffset) return std::nullopt;
    const auto start = source.utf16PositionAtByteOffset(*startOffset);
    const auto end = source.utf16PositionAtByteOffset(*endOffset);
    if (!start || !end || start->line != end->line || start->column >= end->column) {
        return std::nullopt;
    }

    uint32_t modifiers = 0;
    if (occurrence.kind == sema::OccurrenceKind::Declaration) {
        modifiers |= modifier(isDefinition(symbol.category)
            && symbol.provenance == sema::SymbolProvenance::Source
                ? TokenModifier::Definition
                : TokenModifier::Declaration);
    }
    if (isReadonly(symbol.category)
        || (symbol.category == sema::SymbolCategory::ExternEnumPattern && !exitTarget)
        || (concreteRegionValue && !exitTarget)) {
        modifiers |= modifier(TokenModifier::Readonly);
    }
    if (!exitTarget && isDefaultLibrary(snapshot.semanticIndex(), symbol)) {
        modifiers |= modifier(TokenModifier::DefaultLibrary);
    }
    return AbsoluteToken{
        start->line - 1,
        start->column - 1,
        end->column - start->column,
        *type,
        modifiers,
    };
}

} // namespace

SemanticTokensService::SemanticTokensService(
    const ProjectManager& projects, AnalysisScheduler& scheduler)
    : projects_(projects), scheduler_(scheduler) {}

const std::vector<std::string>& SemanticTokensService::tokenTypes() {
    static const std::vector<std::string> result = {
        "function", "parameter", "enum", "enumMember", "property", "variable", "operator",
        "rlsPropertyDeclaration", "keyword",
    };
    return result;
}

const std::vector<std::string>& SemanticTokensService::tokenModifiers() {
    static const std::vector<std::string> result = {
        "declaration", "definition", "readonly", "defaultLibrary", "deprecated",
    };
    return result;
}

std::vector<uint32_t> SemanticTokensService::full(std::string_view uri) const {
    const auto* project = projects_.projectForDocument(uri);
    const auto identity = projects_.sourceIdentityForDocument(uri);
    if (!project || !identity) return {};
    const std::string projectId = project->id;
    const uint64_t generation = project->generation;
    auto snapshot = scheduler_.acceptedSnapshot(projectId);
    if (!snapshot || snapshot->generation() != generation) {
        snapshot = scheduler_.awaitSnapshot(projectId, generation);
    }
    if (!snapshot || snapshot->generation() != generation) return {};
    const std::string& documentPath = *identity;
    const auto* source = snapshot->sourceText(documentPath);
    if (!source) return {};

    std::vector<AbsoluteToken> tokens;
    if (const auto* sourceIndex = snapshot->sourceIndex(documentPath)) {
        for (const auto& booleanLiteral : sourceIndex->booleanLiterals()) {
            const auto startOffset = source->byteOffsetFromUtf8Position(booleanLiteral.start);
            const auto endOffset = source->byteOffsetFromUtf8Position(booleanLiteral.end);
            if (!startOffset || !endOffset || *startOffset >= *endOffset) continue;
            const auto start = source->utf16PositionAtByteOffset(*startOffset);
            const auto end = source->utf16PositionAtByteOffset(*endOffset);
            if (!start || !end || start->line != end->line || start->column >= end->column) continue;
            tokens.push_back({
                start->line - 1,
                start->column - 1,
                end->column - start->column,
                TokenType::Keyword,
                0,
            });
        }
        for (const auto& logicalOperator : sourceIndex->logicalOperators()) {
            const auto startOffset = source->byteOffsetFromUtf8Position(logicalOperator.span.start);
            const auto endOffset = source->byteOffsetFromUtf8Position(logicalOperator.span.end);
            if (!startOffset || !endOffset || *startOffset >= *endOffset) continue;
            const auto start = source->utf16PositionAtByteOffset(*startOffset);
            const auto end = source->utf16PositionAtByteOffset(*endOffset);
            if (!start || !end || start->line != end->line || start->column >= end->column) continue;
            tokens.push_back({
                start->line - 1,
                start->column - 1,
                end->column - start->column,
                TokenType::Operator,
                0,
            });
        }
    }
    for (const auto& occurrence : snapshot->semanticIndex().occurrences()) {
        if (occurrence.span.file != documentPath) continue;
        if (!occurrence.symbol && occurrence.kind == sema::OccurrenceKind::TypeReference) {
            const auto startOffset = source->byteOffsetFromUtf8Position(occurrence.span.start);
            const auto endOffset = source->byteOffsetFromUtf8Position(occurrence.span.end);
            if (!startOffset || !endOffset || *startOffset >= *endOffset) continue;
            const auto start = source->utf16PositionAtByteOffset(*startOffset);
            const auto end = source->utf16PositionAtByteOffset(*endOffset);
            if (!start || !end || start->line != end->line || start->column >= end->column) continue;
            tokens.push_back({
                start->line - 1,
                start->column - 1,
                end->column - start->column,
                TokenType::Enum,
                modifier(TokenModifier::DefaultLibrary),
            });
            continue;
        }
        if (!occurrence.symbol) continue;
        const auto symbol = snapshot->declaration(*occurrence.symbol);
        if (!symbol) continue;
        if (const auto token = makeToken(*snapshot, *source, occurrence, *symbol)) {
            tokens.push_back(*token);
        }
    }
    std::sort(tokens.begin(), tokens.end(), [](const AbsoluteToken& left, const AbsoluteToken& right) {
        return std::tie(left.line, left.character, left.length,
                   left.type, left.modifiers)
            < std::tie(right.line, right.character, right.length,
                   right.type, right.modifiers);
    });

    std::vector<AbsoluteToken> validated;
    for (const auto& token : tokens) {
        if (!validated.empty() && token.line == validated.back().line) {
            const uint32_t previousEnd = validated.back().character + validated.back().length;
            if (token.character == validated.back().character
                && token.length == validated.back().length
                && token.type == validated.back().type) {
                validated.back().modifiers |= token.modifiers;
                continue;
            }
            if (token.character < previousEnd) continue;
        }
        validated.push_back(token);
    }

    std::vector<uint32_t> data;
    data.reserve(validated.size() * 5);
    uint32_t previousLine = 0;
    uint32_t previousCharacter = 0;
    for (const auto& token : validated) {
        const uint32_t deltaLine = token.line - previousLine;
        const uint32_t deltaStart = deltaLine == 0
            ? token.character - previousCharacter
            : token.character;
        data.insert(data.end(), {
            deltaLine,
            deltaStart,
            token.length,
            static_cast<uint32_t>(token.type),
            token.modifiers,
        });
        previousLine = token.line;
        previousCharacter = token.character;
    }
    return data;
}

} // namespace rls::lsp
