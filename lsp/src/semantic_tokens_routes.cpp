#include "rls/lsp/route_modules.h"

#include <nlohmann/json.hpp>

#include "rls/lsp/json_rpc_router.h"
#include "rls/lsp/semantic_tokens_service.h"

namespace rls::lsp {
namespace {

using Json = nlohmann::json;

const Json& requireObject(const Json& value) {
    if (!value.is_object()) throw InvalidParams("expected object parameters");
    return value;
}

} // namespace

void RegisterSemanticTokenRoutes(
    JsonRpcRouter& router, SemanticTokensService& semanticTokens) {
    router.registerRequest(
        "textDocument/semanticTokens/full",
        [&semanticTokens](const Json& params) {
            const auto& object = requireObject(params);
            const auto& document = requireObject(object.at("textDocument"));
            return Json{{"data", semanticTokens.full(
                document.at("uri").get<std::string>())}};
        });
}

} // namespace rls::lsp