#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rls::lsp {

struct PresentationPosition {
    uint32_t line = 0;
    uint32_t character = 0;
};

struct PresentationRange {
    PresentationPosition start;
    PresentationPosition end;
};

struct PresentationLocation {
    std::string uri;
    PresentationRange range;
};

struct PresentationType {
    std::string name;
    std::optional<std::string> enumIdentity;
};

struct PresentationParameter {
    std::string name;
    PresentationType type;
    std::optional<std::string> defaultValue;
    bool optional = false;
};

struct PresentationCallable {
    std::string name;
    std::vector<PresentationParameter> parameters;
    std::optional<PresentationType> returnType;
};

enum class PresentationProvenance {
    Source,
    Extern,
    BuiltIn,
    Pattern,
};

enum class PresentationSymbolKind {
    Region,
    Function,
    Enum,
    EnumMember,
    Parameter,
    Property,
    Value,
};

struct DocumentationBlock {
    std::optional<std::string> heading;
    std::string markdown;
};

struct PresentationSymbol {
    PresentationSymbolKind kind = PresentationSymbolKind::Value;
    std::string name;
    PresentationProvenance provenance = PresentationProvenance::Source;
    std::optional<PresentationType> type;
    std::optional<PresentationCallable> callable;
    std::vector<DocumentationBlock> documentation;
    std::optional<PresentationLocation> declaration;
};

struct RenderedPresentation {
    std::string detail;
    std::string documentation;
};

class PresentationRenderer {
public:
    RenderedPresentation render(const PresentationSymbol& symbol) const;

private:
    static std::string renderType(const PresentationType& type);
    static std::string renderCallable(const PresentationCallable& callable);
};

} // namespace rls::lsp