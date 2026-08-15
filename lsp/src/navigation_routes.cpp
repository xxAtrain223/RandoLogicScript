#include "rls/lsp/route_modules.h"

#include <cstdint>
#include <limits>

#include <nlohmann/json.hpp>

#include "rls/lsp/json_rpc_router.h"
#include "rls/lsp/lifecycle_service.h"
#include "rls/lsp/navigation_service.h"

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

Json position(const NavigationPosition& value) {
    return {{"line", value.line}, {"character", value.character}};
}

Json range(const NavigationRange& value) {
    return {{"start", position(value.start)}, {"end", position(value.end)}};
}

int symbolKind(NavigationSymbolKind kind) {
    switch (kind) {
    case NavigationSymbolKind::Namespace: return 3;
    case NavigationSymbolKind::Function: return 12;
    case NavigationSymbolKind::Enum: return 10;
    case NavigationSymbolKind::EnumMember: return 22;
    case NavigationSymbolKind::Variable: return 13;
    case NavigationSymbolKind::Property: return 7;
    case NavigationSymbolKind::Field: return 8;
    }
    return 13;
}

Json documentSymbol(const NavigationDocumentSymbol& symbol) {
    Json children = Json::array();
    for (const auto& child : symbol.children) {
        children.push_back(documentSymbol(child));
    }
    return {
        {"name", symbol.name},
        {"kind", symbolKind(symbol.kind)},
        {"range", range(symbol.range)},
        {"selectionRange", range(symbol.selectionRange)},
        {"children", std::move(children)},
    };
}

void appendSymbolInformation(
    Json& result, const NavigationDocumentSymbol& symbol,
    std::string_view uri, std::optional<std::string_view> containerName) {
    Json information = {
        {"name", symbol.name},
        {"kind", symbolKind(symbol.kind)},
        {"location", {
            {"uri", uri},
            {"range", range(symbol.selectionRange)},
        }},
    };
    if (containerName) {
        information["containerName"] = *containerName;
    }
    result.push_back(std::move(information));
    for (const auto& child : symbol.children) {
        appendSymbolInformation(result, child, uri, symbol.name);
    }
}

NavigationPosition requestPosition(const Json& object) {
    const auto& value = requireObject(object.at("position"));
    return {
        requirePositionComponent(value.at("line")),
        requirePositionComponent(value.at("character")),
    };
}

} // namespace

void RegisterNavigationRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, NavigationService& navigation) {
    router.registerRequest("textDocument/definition", [&lifecycle, &navigation](const Json& params) {
        const auto& object = requireObject(params);
        const auto& document = requireObject(object.at("textDocument"));
        const auto definition = navigation.definition(
            document.at("uri").get<std::string>(),
            requestPosition(object));
        if (!definition) {
            return Json(nullptr);
        }
        if (!lifecycle.supportsDefinitionLinks()) {
            return Json::array({{
                {"uri", definition->targetUri},
                {"range", range(definition->targetSelectionRange)},
            }});
        }
        return Json::array({{
            {"originSelectionRange", range(definition->originSelectionRange)},
            {"targetUri", definition->targetUri},
            {"targetRange", range(definition->targetRange)},
            {"targetSelectionRange", range(definition->targetSelectionRange)},
        }});
    });
    router.registerRequest("textDocument/references", [&navigation](const Json& params) {
        const auto& object = requireObject(params);
        const auto& document = requireObject(object.at("textDocument"));
        const auto& context = requireObject(object.at("context"));
        if (!context.at("includeDeclaration").is_boolean()) {
            throw InvalidParams("includeDeclaration must be a boolean");
        }
        Json result = Json::array();
        for (const auto& reference : navigation.references(
            document.at("uri").get<std::string>(), requestPosition(object),
            context.at("includeDeclaration").get<bool>())) {
            result.push_back({
                {"uri", reference.uri},
                {"range", range(reference.range)},
            });
        }
        return result;
    });
    router.registerRequest("textDocument/documentHighlight", [&navigation](const Json& params) {
        const auto& object = requireObject(params);
        const auto& document = requireObject(object.at("textDocument"));
        Json result = Json::array();
        for (const auto& highlight : navigation.documentHighlights(
            document.at("uri").get<std::string>(), requestPosition(object))) {
            result.push_back({
                {"range", range(highlight)},
                {"kind", 1},
            });
        }
        return result;
    });
    router.registerRequest("textDocument/documentSymbol", [&lifecycle, &navigation](const Json& params) {
        const auto& document = requireObject(requireObject(params).at("textDocument"));
        const std::string uri = document.at("uri").get<std::string>();
        Json result = Json::array();
        for (const auto& symbol : navigation.documentSymbols(uri)) {
            if (lifecycle.supportsDocumentSymbolHierarchy()) {
                result.push_back(documentSymbol(symbol));
            } else {
                appendSymbolInformation(result, symbol, uri, std::nullopt);
            }
        }
        return result;
    });
}

} // namespace rls::lsp