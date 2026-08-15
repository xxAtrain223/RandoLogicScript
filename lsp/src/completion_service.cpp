#include "rls/lsp/completion_service.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>
#include <optional>
#include <set>
#include <tuple>

#include "rls/lsp/document_uri.h"

namespace rls::lsp {
namespace {

enum class CompletionContext {
    TopLevel,
    Type,
    RegionBody,
    MemberAccess,
    Expression,
    Unsupported,
};

struct CurrentDocument {
    AnalysisScheduler::Snapshot snapshot;
    std::string path;
    const ast::SourceText* source = nullptr;
    const parser::SourceIndex* sourceIndex = nullptr;
};

struct Candidate {
    CompletionItem item;
    size_t contextRank = 0;
    bool prefixMatch = false;
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

std::optional<CurrentDocument> currentDocument(
    const ProjectManager& projects, const AnalysisScheduler& scheduler,
    std::string_view uri) {
    const auto* project = projects.projectForDocument(uri);
    const auto path = FileUriToPath(uri);
    if (!project || !path) return std::nullopt;

    const auto snapshot = scheduler.acceptedSnapshot(project->id);
    if (!snapshot || snapshot->generation() != project->generation) {
        return std::nullopt;
    }
    const std::string documentPath = pathString(*path);
    const auto* source = snapshot->sourceText(documentPath);
    const auto* sourceIndex = snapshot->sourceIndex(documentPath);
    if (!source || !sourceIndex) return std::nullopt;
    return CurrentDocument{snapshot, documentPath, source, sourceIndex};
}

std::optional<PresentationRange> presentationRange(
    const ast::SourceText& source, ast::SourceRange range) {
    const auto startOffset = source.byteOffsetFromUtf8Position(range.start);
    const auto endOffset = source.byteOffsetFromUtf8Position(range.end);
    if (!startOffset || !endOffset) return std::nullopt;
    const auto start = source.utf16PositionAtByteOffset(*startOffset);
    const auto end = source.utf16PositionAtByteOffset(*endOffset);
    if (!start || !end) return std::nullopt;
    return PresentationRange{
        {start->line - 1, start->column - 1},
        {end->line - 1, end->column - 1},
    };
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

bool startsWithCaseInsensitive(std::string_view value, std::string_view prefix) {
    if (prefix.size() > value.size()) return false;
    return asciiLower(value.substr(0, prefix.size())) == asciiLower(prefix);
}

CompletionContext completionContextAt(
    const parser::SourceIndex& index, ast::Position position,
    const std::optional<parser::RegionContext>& region,
    const std::optional<parser::MemberAccessContext>& memberAccess,
    const std::optional<parser::NamedArgumentContext>& namedArgument) {
    if (memberAccess) return CompletionContext::MemberAccess;
    if (namedArgument) return CompletionContext::Expression;
    if (const auto name = index.nameAt(position)) {
        switch (name->kind) {
        case parser::SourceNameKind::Type:
            return CompletionContext::Type;
        case parser::SourceNameKind::RegionDataKey:
            return CompletionContext::RegionBody;
        case parser::SourceNameKind::Identifier:
        case parser::SourceNameKind::CallCallee:
            return CompletionContext::Expression;
        case parser::SourceNameKind::MemberObject:
        case parser::SourceNameKind::Member:
        case parser::SourceNameKind::ArgumentLabel:
            return CompletionContext::Unsupported;
        default:
            break;
        }
    }
    const auto syntax = index.syntaxAt(position);
    if (!syntax) {
        if (!region) return CompletionContext::TopLevel;
        return region->activeSection
            ? CompletionContext::Unsupported
            : CompletionContext::RegionBody;
    }
    switch (syntax->kind) {
    case parser::SyntaxKind::Expression:
    case parser::SyntaxKind::Call:
    case parser::SyntaxKind::Argument:
        return CompletionContext::Expression;
    default:
        break;
    }
    if (region && !region->activeSection) return CompletionContext::RegionBody;
    return CompletionContext::Unsupported;
}

std::string_view sectionName(ast::SectionKind kind) {
    switch (kind) {
    case ast::SectionKind::Events: return "events";
    case ast::SectionKind::Locations: return "locations";
    case ast::SectionKind::Exits: return "exits";
    }
    return {};
}

PresentationType presentationType(
    ast::Type type, const std::optional<std::string>& enumName = std::nullopt) {
    std::string name;
    switch (type) {
    case ast::Type::Bool: name = "Bool"; break;
    case ast::Type::Int: name = "Int"; break;
    case ast::Type::String: name = "String"; break;
    case ast::Type::List: name = "List"; break;
    case ast::Type::Callable: name = "Callable"; break;
    case ast::Type::Condition: name = "Condition"; break;
    case ast::Type::Enum: name = "Enum"; break;
    case ast::Type::Void: name = "Void"; break;
    case ast::Type::Error: name = "<error>"; break;
    }
    return {std::move(name), enumName};
}

PresentationProvenance presentationProvenance(sema::SymbolProvenance provenance) {
    switch (provenance) {
    case sema::SymbolProvenance::Source:
        return PresentationProvenance::Source;
    case sema::SymbolProvenance::Extern:
        return PresentationProvenance::Extern;
    case sema::SymbolProvenance::Pattern:
        return PresentationProvenance::Pattern;
    }
    return PresentationProvenance::Source;
}

PresentationSymbol presentationSymbol(
    const sema::AnalysisSnapshot& snapshot, const sema::SymbolRecord& record) {
    PresentationSymbol result{
        .name = record.displayName,
        .provenance = presentationProvenance(record.provenance),
    };
    if (record.type) result.type = presentationType(*record.type, record.enumName);

    switch (record.category) {
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
                parameter->displayName,
                parameter->type
                    ? presentationType(*parameter->type, parameter->enumName)
                    : PresentationType{"<unknown>"},
            });
        }
        result.callable = std::move(callable);
        break;
    }
    case sema::SymbolCategory::Enum:
        result.kind = PresentationSymbolKind::Enum;
        break;
    case sema::SymbolCategory::EnumMember:
        result.kind = PresentationSymbolKind::EnumMember;
        break;
    case sema::SymbolCategory::Parameter:
        result.kind = PresentationSymbolKind::Parameter;
        break;
    default:
        result.kind = PresentationSymbolKind::Value;
        break;
    }
    return result;
}

CompletionItemKind completionKind(sema::SymbolCategory category) {
    switch (category) {
    case sema::SymbolCategory::Define:
    case sema::SymbolCategory::ExternDefine:
        return CompletionItemKind::Function;
    case sema::SymbolCategory::Enum:
        return CompletionItemKind::Enum;
    case sema::SymbolCategory::EnumMember:
        return CompletionItemKind::EnumMember;
    case sema::SymbolCategory::Parameter:
        return CompletionItemKind::Variable;
    default:
        return CompletionItemKind::Value;
    }
}

bool matchesExpectedType(
    const sema::SymbolRecord& symbol, const std::optional<sema::ExpectedTypeRecord>& expected) {
    if (!expected) return true;
    if (!symbol.type || *symbol.type != expected->type) return false;
    return expected->type != ast::Type::Enum || symbol.enumName == expected->enumName;
}

void addCandidate(
    std::vector<Candidate>& candidates, std::set<std::string>& labels,
    CompletionItem item, size_t rank, std::string_view prefix) {
    if (!labels.insert(item.label).second) return;
    const bool prefixMatch = !prefix.empty()
        && startsWithCaseInsensitive(item.label, prefix);
    candidates.push_back({std::move(item), rank, prefixMatch});
}

} // namespace

CompletionService::CompletionService(
    const ProjectManager& projects, const AnalysisScheduler& scheduler)
    : projects_(projects), scheduler_(scheduler) {}

std::vector<CompletionItem> CompletionService::complete(
    std::string_view uri, PresentationPosition position) const {
    if (position.line == std::numeric_limits<uint32_t>::max()
        || position.character == std::numeric_limits<uint32_t>::max()) {
        return {};
    }
    const auto document = currentDocument(projects_, scheduler_, uri);
    if (!document) return {};

    const auto cursorOffset = document->source->byteOffsetFromUtf16Position({
        position.line + 1, position.character + 1});
    if (!cursorOffset) return {};
    const auto cursorPosition = document->source->utf8PositionAtByteOffset(*cursorOffset);
    if (!cursorPosition) return {};

    const auto token = document->source->incompleteTokenRangeAt(*cursorPosition);
    ast::SourceRange replacement{*cursorPosition, *cursorPosition};
    ast::Position contextPosition = *cursorPosition;
    std::string prefix;
    if (token) {
        replacement = *token;
        const auto tokenStart = document->source->byteOffsetFromUtf8Position(token->start);
        if (!tokenStart || *tokenStart > *cursorOffset) return {};
        prefix = document->source->content().substr(*tokenStart, *cursorOffset - *tokenStart);
        if (*cursorOffset > *tokenStart) {
            const auto previous = document->source->utf8PositionAtByteOffset(*cursorOffset - 1);
            if (previous) contextPosition = *previous;
        }
    }
    const auto editRange = presentationRange(*document->source, replacement);
    if (!editRange) return {};

    const auto region = document->sourceIndex->regionContextAt(contextPosition);
    const auto memberAccess = document->sourceIndex->memberAccessAt(*cursorPosition);
    auto namedArgument = document->sourceIndex->namedArgumentAt(*cursorPosition);
    if (namedArgument && document->sourceIndex->syntaxAt(contextPosition)
        && !document->sourceIndex->enclosingCall(contextPosition)) {
        namedArgument.reset();
    }
    const auto context = completionContextAt(
        *document->sourceIndex, contextPosition, region, memberAccess, namedArgument);
    const auto expected = document->snapshot->expectedTypeAt(document->path, contextPosition);
    std::vector<Candidate> candidates;
    std::set<std::string> labels;
    const auto makeItem = [&](std::string label, CompletionItemKind kind,
                              std::string detail = {}, std::string documentation = {}) {
        return CompletionItem{
            .label = label,
            .kind = kind,
            .detail = std::move(detail),
            .documentation = std::move(documentation),
            .insertText = std::move(label),
            .replacementRange = *editRange,
        };
    };

    if (context == CompletionContext::TopLevel) {
        static constexpr std::string_view keywords[] = {
            "define", "enum", "extend region", "extern define", "extern enum", "region",
        };
        for (const auto keyword : keywords) {
            addCandidate(candidates, labels,
                makeItem(std::string(keyword), CompletionItemKind::Keyword, "declaration keyword"),
                0, prefix);
        }
    } else if (context == CompletionContext::Type) {
        static constexpr std::string_view builtInTypes[] = {
            "Bool", "Callable", "Condition", "Int", "List", "String",
        };
        for (const auto type : builtInTypes) {
            addCandidate(candidates, labels,
                makeItem(std::string(type), CompletionItemKind::Type, "built-in type"),
                10, prefix);
        }
        for (const auto& symbol : document->snapshot->semanticIndex().symbols()) {
            if (symbol.category != sema::SymbolCategory::Enum) continue;
            const auto rendered = PresentationRenderer{}.render(
                presentationSymbol(*document->snapshot, symbol));
            addCandidate(candidates, labels,
                makeItem(symbol.displayName, CompletionItemKind::Enum,
                    rendered.detail, rendered.documentation),
                0, prefix);
        }
    } else if (context == CompletionContext::RegionBody && region) {
        if (!region->extension) {
            static constexpr std::string_view dataKeys[] = {
                "areas", "name", "scene", "timePasses",
            };
            for (const auto key : dataKeys) {
                if (std::find(region->dataKeys.begin(), region->dataKeys.end(), key)
                    != region->dataKeys.end()) {
                    continue;
                }
                addCandidate(candidates, labels,
                    makeItem(std::string(key), CompletionItemKind::Property,
                        "region data key"),
                    0, prefix);
            }
        }
        for (const auto kind : {
                 ast::SectionKind::Events,
                 ast::SectionKind::Locations,
                 ast::SectionKind::Exits,
             }) {
            if (std::find(region->sectionKinds.begin(), region->sectionKinds.end(), kind)
                != region->sectionKinds.end()) {
                continue;
            }
            addCandidate(candidates, labels,
                makeItem(std::string(sectionName(kind)), CompletionItemKind::Keyword,
                    "region section"),
                10, prefix);
        }
    } else if (context == CompletionContext::MemberAccess && memberAccess) {
        const sema::SymbolRecord* enumSymbol = nullptr;
        for (const auto& symbol : document->snapshot->semanticIndex().symbols()) {
            if (symbol.category == sema::SymbolCategory::Enum
                && symbol.displayName == memberAccess->object) {
                enumSymbol = &symbol;
                break;
            }
        }
        if (enumSymbol) {
            for (const auto& symbol : document->snapshot->semanticIndex().symbols()) {
                if (symbol.category != sema::SymbolCategory::EnumMember
                    || symbol.container != enumSymbol->id) {
                    continue;
                }
                const auto rendered = PresentationRenderer{}.render(
                    presentationSymbol(*document->snapshot, symbol));
                addCandidate(candidates, labels,
                    makeItem(symbol.displayName, CompletionItemKind::EnumMember,
                        rendered.detail, rendered.documentation),
                    0, prefix);
            }
        }
    } else if (context == CompletionContext::Expression) {
        if (namedArgument) {
            const sema::SymbolRecord* callable = nullptr;
            for (const auto& symbol : document->snapshot->semanticIndex().symbols()) {
                const bool isCallable = symbol.category == sema::SymbolCategory::Define
                    || symbol.category == sema::SymbolCategory::ExternDefine;
                if (isCallable && symbol.displayName == namedArgument->callee) {
                    callable = &symbol;
                    break;
                }
            }
            if (callable) {
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

                std::vector<bool> bound(parameters.size(), false);
                size_t nextPositional = 0;
                for (size_t argumentIndex = 0;
                     argumentIndex < namedArgument->argumentLabels.size(); ++argumentIndex) {
                    if (argumentIndex == namedArgument->activeArgument) continue;
                    const auto& label = namedArgument->argumentLabels[argumentIndex];
                    if (label) {
                        const auto parameter = std::find_if(
                            parameters.begin(), parameters.end(), [&](const auto* candidate) {
                                return candidate->displayName == *label;
                            });
                        if (parameter != parameters.end()) {
                            bound[static_cast<size_t>(parameter - parameters.begin())] = true;
                        }
                        continue;
                    }
                    if (argumentIndex > namedArgument->activeArgument) continue;
                    while (nextPositional < bound.size() && bound[nextPositional]) {
                        ++nextPositional;
                    }
                    if (nextPositional < bound.size()) bound[nextPositional++] = true;
                }

                for (size_t parameterIndex = 0;
                     parameterIndex < parameters.size(); ++parameterIndex) {
                    if (bound[parameterIndex]) continue;
                    const auto* parameter = parameters[parameterIndex];
                    const auto rendered = PresentationRenderer{}.render(
                        presentationSymbol(*document->snapshot, *parameter));
                    auto item = makeItem(parameter->displayName, CompletionItemKind::Property,
                        rendered.detail, rendered.documentation);
                    item.insertText += ": ";
                    addCandidate(candidates, labels, std::move(item), 0, prefix);
                }
            }
        }
        for (const auto symbolId : document->snapshot->visibleSymbolsAt(
                 document->path, contextPosition)) {
            const auto symbol = document->snapshot->declaration(symbolId);
            if (!symbol) continue;
            const bool callable = symbol->category == sema::SymbolCategory::Define
                || symbol->category == sema::SymbolCategory::ExternDefine;
            const bool parameter = symbol->category == sema::SymbolCategory::Parameter;
            if ((!callable && !parameter)
                || (parameter && !matchesExpectedType(*symbol, expected))) {
                continue;
            }
            const auto rendered = PresentationRenderer{}.render(
                presentationSymbol(*document->snapshot, *symbol));
            const size_t rank = parameter ? 10 : 20;
            addCandidate(candidates, labels,
                makeItem(symbol->displayName, completionKind(symbol->category),
                    rendered.detail, rendered.documentation),
                rank, prefix);
        }

        if (expected && expected->type == ast::Type::Enum && expected->enumName) {
            for (const auto& symbol : document->snapshot->semanticIndex().symbols()) {
                if (symbol.category != sema::SymbolCategory::EnumMember
                    || symbol.enumName != expected->enumName) {
                    continue;
                }
                const auto rendered = PresentationRenderer{}.render(
                    presentationSymbol(*document->snapshot, symbol));
                addCandidate(candidates, labels,
                    makeItem(symbol.displayName, CompletionItemKind::EnumMember,
                        rendered.detail, rendered.documentation),
                    0, prefix);
            }
        }

        if (!expected || expected->type == ast::Type::Bool) {
            for (const std::string_view literal : {"always", "false", "never", "true"}) {
                PresentationSymbol symbol{
                    .name = std::string(literal),
                    .provenance = PresentationProvenance::BuiltIn,
                    .type = PresentationType{.name = "Bool"},
                };
                const auto rendered = PresentationRenderer{}.render(symbol);
                addCandidate(candidates, labels,
                    makeItem(std::string(literal), CompletionItemKind::Value,
                        rendered.detail, rendered.documentation),
                    30, prefix);
            }
        }
        for (const std::string_view keyword : {"match", "not"}) {
            addCandidate(candidates, labels,
                makeItem(std::string(keyword), CompletionItemKind::Keyword, "expression keyword"),
                40, prefix);
        }
        if (region) {
            PresentationSymbol symbol{
                .name = "here",
                .provenance = PresentationProvenance::BuiltIn,
                .type = PresentationType{.name = "Enum", .enumIdentity = "Region"},
            };
            const auto rendered = PresentationRenderer{}.render(symbol);
            addCandidate(candidates, labels,
                makeItem("here", CompletionItemKind::Keyword,
                    rendered.detail, rendered.documentation),
                5, prefix);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
        return std::tuple(!left.prefixMatch, left.contextRank, asciiLower(left.item.label), left.item.label)
            < std::tuple(!right.prefixMatch, right.contextRank, asciiLower(right.item.label), right.item.label);
    });
    std::vector<CompletionItem> result;
    result.reserve(candidates.size());
    for (size_t index = 0; index < candidates.size(); ++index) {
        const std::string ordinal = std::to_string(index);
        const size_t padding = ordinal.size() < 8 ? 8 - ordinal.size() : 0;
        candidates[index].item.sortText = std::string(padding, '0') + ordinal;
        result.push_back(std::move(candidates[index].item));
    }
    return result;
}

} // namespace rls::lsp