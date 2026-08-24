#include "rls/lsp/semantic_token_legend.h"

#include <nlohmann/json.hpp>

namespace rls::lsp {
namespace {

using Json = nlohmann::json;

void applyMappings(
    const Json& mappings, std::vector<std::string>& names) {
    if (!mappings.is_object()) return;
    for (auto& name : names) {
        const auto replacement = mappings.find(name);
        if (replacement != mappings.end() && replacement->is_string()
            && !replacement->get_ref<const std::string&>().empty()) {
            name = replacement->get<std::string>();
        }
    }
}

} // namespace

SemanticTokenLegend MapSemanticTokenLegend(
    const Json& initializeParams,
    const std::vector<std::string>& tokenTypes,
    const std::vector<std::string>& tokenModifiers) {
    SemanticTokenLegend result{tokenTypes, tokenModifiers};
    if (!initializeParams.is_object()
        || !initializeParams.contains("initializationOptions")) {
        return result;
    }
    const auto& options = initializeParams.at("initializationOptions");
    if (!options.is_object() || !options.contains("semanticTokens")) return result;
    const auto& semanticTokens = options.at("semanticTokens");
    if (!semanticTokens.is_object() || !semanticTokens.contains("legend")) return result;
    const auto& legend = semanticTokens.at("legend");
    if (!legend.is_object()) return result;
    if (legend.contains("tokenTypes")) {
        applyMappings(legend.at("tokenTypes"), result.tokenTypes);
    }
    if (legend.contains("tokenModifiers")) {
        applyMappings(legend.at("tokenModifiers"), result.tokenModifiers);
    }
    return result;
}

} // namespace rls::lsp