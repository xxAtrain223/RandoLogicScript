#include "rls/lsp/route_modules.h"

#include <cstdint>
#include <limits>

#include <nlohmann/json.hpp>

#include "rls/lsp/completion_service.h"
#include "rls/lsp/hover_service.h"
#include "rls/lsp/json_rpc_router.h"
#include "rls/lsp/lifecycle_service.h"
#include "rls/lsp/signature_help_service.h"

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
    case CompletionItemKind::Property: return 10;
    case CompletionItemKind::Variable: return 6;
    case CompletionItemKind::Value: return 12;
    case CompletionItemKind::Enum: return 13;
    case CompletionItemKind::Keyword: return 14;
    case CompletionItemKind::EnumMember: return 20;
    }
    return 1;
}

} // namespace

void RegisterAuthoringRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, CompletionService& completion,
    SignatureHelpService& signatureHelp, HoverService& hover) {
    router.registerRequest("textDocument/completion", [&lifecycle, &completion](const Json& params) {
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
            const bool useSnippet = lifecycle.supportsCompletionSnippets()
                && item.snippetText.has_value();
            const bool serverIndented = useSnippet
                && lifecycle.sectionSnippetIndentation()
                    == SectionSnippetIndentation::Server
                && item.serverIndentedSnippetText.has_value();
            const std::string& insertion = serverIndented
                ? *item.serverIndentedSnippetText
                : useSnippet ? *item.snippetText : item.insertText;
            Json completionItem = {
                {"label", item.label},
                {"kind", completionKind(item.kind)},
                {"sortText", item.sortText},
                {"insertTextFormat", useSnippet ? 2 : 1},
                {"textEdit", {
                    {"range", range(item.replacementRange)},
                    {"newText", insertion},
                }},
            };
            if (useSnippet && item.serverIndentedSnippetText) {
                completionItem["insertTextMode"] = serverIndented ? 1 : 2;
            }
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

    router.registerRequest("textDocument/signatureHelp", [&signatureHelp](const Json& params) {
        const auto& object = requireObject(params);
        const auto& document = requireObject(object.at("textDocument"));
        const auto& requestPosition = requireObject(object.at("position"));
        const auto result = signatureHelp.signatureHelp(
            document.at("uri").get<std::string>(),
            {
                requirePositionComponent(requestPosition.at("line")),
                requirePositionComponent(requestPosition.at("character")),
            });
        if (!result) return Json(nullptr);

        Json parameters = Json::array();
        for (const auto& label : result->parameterLabels) {
            parameters.push_back({{"label", label}});
        }
        Json signature = {
            {"label", result->label},
            {"parameters", std::move(parameters)},
        };
        if (!result->documentation.empty()) {
            signature["documentation"] = {
                {"kind", "markdown"},
                {"value", result->documentation},
            };
        }
        if (result->activeParameter) {
            signature["activeParameter"] = *result->activeParameter;
        }
        Json response = {
            {"signatures", Json::array({std::move(signature)})},
            {"activeSignature", 0},
        };
        if (result->activeParameter) {
            response["activeParameter"] = *result->activeParameter;
        }
        return response;
    });

    router.registerRequest("textDocument/hover", [&hover](const Json& params) {
        const auto& object = requireObject(params);
        const auto& document = requireObject(object.at("textDocument"));
        const auto& requestPosition = requireObject(object.at("position"));
        const auto result = hover.hover(
            document.at("uri").get<std::string>(),
            {
                requirePositionComponent(requestPosition.at("line")),
                requirePositionComponent(requestPosition.at("character")),
            });
        if (!result) return Json(nullptr);
        return Json{
            {"contents", {
                {"kind", "markdown"},
                {"value", result->markdown},
            }},
            {"range", range(result->range)},
        };
    });
}

} // namespace rls::lsp