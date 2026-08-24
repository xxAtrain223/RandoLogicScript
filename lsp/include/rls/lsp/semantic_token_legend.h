#pragma once

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace rls::lsp {

struct SemanticTokenLegend {
    std::vector<std::string> tokenTypes;
    std::vector<std::string> tokenModifiers;
};

SemanticTokenLegend MapSemanticTokenLegend(
    const nlohmann::json& initializeParams,
    const std::vector<std::string>& tokenTypes,
    const std::vector<std::string>& tokenModifiers);

} // namespace rls::lsp