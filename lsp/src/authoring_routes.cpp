#include "rls/lsp/route_modules.h"

#include <cstdint>
#include <limits>

#include <nlohmann/json.hpp>

#include "rls/lsp/completion_service.h"
#include "rls/lsp/json_rpc_router.h"

namespace rls::lsp {
namespace {

using Json = nlohmann::json;

const Json& requireObject(const Json& value) {
    if (!value.is_object()) {
        throw InvalidParams("expected object parameters");
    }
    return value;
}

uint32_t requirePositionComponent(const Json& value) {
    uint64_t component = 0;
    if (value.is_number_unsigned()) {
        component = value.get<uint64_t>();
    } else if (value.is_number_integer()) {
        const int64_t signedComponent = value.get<int64_t>();
        if (signedComponent < 0) {
            throw InvalidParams("position components must be non-negative");
        }
        component = static_cast<uint64_t>(signedComponent);
    } else {
        throw InvalidParams("position components must be integers");
    }
    if (component > std::numeric_limits<uint32_t>::max()) {
        throw InvalidParams("position component is too large");
    }
    return static_cast<uint32_t>(component);
}

Json position(const PresentationPosition& value) {
    return {{"line", value.line}, {"character", value.character}};
}

Json range(const PresentationRange& value) {
    return {{"start", position(value.start)}, {"end", position(value.end)}};
}

int completionKind(CompletionItemKind kind) {
    switch (kind) {
    case CompletionItemKind::Function: return 3;
    case CompletionItemKind::Type: return 7;
    case CompletionItemKind::Variable: return 6;
    case CompletionItemKind::Value: return 12;
    case CompletionItemKind::Enum: return 13;
    case CompletionItemKind::Keyword: return 14;
    case CompletionItemKind::EnumMember: return 20;
    }
    return 1;
}

} // namespace

void RegisterAuthoringRoutes(JsonRpcRouter& router, CompletionService& completion) {
    router.registerRequest("textDocument/completion", [&completion](const Json& params) {
        const auto& object = requireObject(params);
        const auto& document = requireObject(object.at("textDocument"));
        const auto& requestPosition = requireObject(object.at("position"));
        const PresentationPosition cursor{
            requirePositionComponent(requestPosition.at("line")),
            requirePositionComponent(requestPosition.at("character")),
        };

        Json result = Json::array();
        for (const auto& item : completion.complete(
                 document.at("uri").get<std::string>(), cursor)) {
            Json completionItem = {
                {"label", item.label},
                {"kind", completionKind(item.kind)},
                {"sortText", item.sortText},
                {"textEdit", {
                    {"range", range(item.replacementRange)},
                    {"newText", item.insertText},
                }},
            };
            if (!item.detail.empty()) completionItem["detail"] = item.detail;
            if (!item.documentation.empty()) {
                completionItem["documentation"] = {
                    {"kind", "markdown"},
                    {"value", item.documentation},
                };
            }
            result.push_back(std::move(completionItem));
        }
        return result;
    });
}

} // namespace rls::lsp