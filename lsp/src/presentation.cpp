#include "rls/lsp/presentation.h"

#include <string_view>

namespace rls::lsp {
namespace {

std::string_view provenancePrefix(PresentationProvenance provenance) {
    switch (provenance) {
    case PresentationProvenance::Source:
        return {};
    case PresentationProvenance::Extern:
        return "extern ";
    case PresentationProvenance::BuiltIn:
        return "built-in ";
    case PresentationProvenance::Pattern:
        return "extern pattern ";
    }
    return {};
}

std::string_view provenanceNote(const PresentationSymbol& symbol) {
    switch (symbol.provenance) {
    case PresentationProvenance::Source:
        return {};
    case PresentationProvenance::Extern:
        return "*External declaration.*";
    case PresentationProvenance::BuiltIn:
        return "*Built-in symbol.*";
    case PresentationProvenance::Pattern:
        return symbol.declaration
            ? "*External wildcard pattern declaration.*"
            : "*External pattern; no source declaration.*";
    }
    return {};
}

std::string_view symbolKeyword(PresentationSymbolKind kind) {
    switch (kind) {
    case PresentationSymbolKind::Region:
        return "region ";
    case PresentationSymbolKind::Enum:
        return "enum ";
    default:
        return {};
    }
}

void appendMarkdownBlock(std::string& output, std::string_view block) {
    if (block.empty()) return;
    if (!output.empty()) output += "\n\n";
    output += block;
}

} // namespace

std::string PresentationRenderer::renderType(const PresentationType& type) {
    return type.enumIdentity.value_or(type.name);
}

std::string PresentationRenderer::renderParameter(
    const PresentationParameter& parameter) {
    std::string result = parameter.name + ": " + renderType(parameter.type);
    if (parameter.defaultValue) {
        result += " = ";
        result += *parameter.defaultValue;
    } else if (parameter.optional) {
        result += " (optional)";
    }
    return result;
}

std::string PresentationRenderer::renderCallable(const PresentationCallable& callable) {
    std::string result = callable.name + "(";
    for (size_t index = 0; index < callable.parameters.size(); ++index) {
        if (index != 0) result += ", ";
        result += renderParameter(callable.parameters[index]);
    }
    result += ')';
    if (callable.returnType) {
        result += " -> ";
        result += renderType(*callable.returnType);
    }
    return result;
}

RenderedPresentation PresentationRenderer::render(const PresentationSymbol& symbol) const {
    RenderedPresentation result;
    result.detail = provenancePrefix(symbol.provenance);
    if (symbol.callable) {
        result.detail += renderCallable(*symbol.callable);
    } else {
        result.detail += symbolKeyword(symbol.kind);
        result.detail += symbol.name;
        if (symbol.type
            && symbol.kind != PresentationSymbolKind::Enum
            && symbol.kind != PresentationSymbolKind::Region) {
            result.detail += ": ";
            result.detail += renderType(*symbol.type);
        }
    }

    for (const auto& block : symbol.documentation) {
        std::string renderedBlock;
        if (block.heading) {
            renderedBlock = "**" + *block.heading + "**";
            if (!block.markdown.empty()) renderedBlock += "\n\n";
        }
        renderedBlock += block.markdown;
        appendMarkdownBlock(result.documentation, renderedBlock);
    }
    appendMarkdownBlock(result.documentation, provenanceNote(symbol));
    return result;
}

} // namespace rls::lsp