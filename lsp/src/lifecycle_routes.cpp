#include "rls/lsp/route_modules.h"

#include <nlohmann/json.hpp>

#include "rls/lsp/json_rpc_router.h"
#include "rls/lsp/lifecycle_service.h"
#include "rls/lsp/workspace_service.h"

namespace rls::lsp {
namespace {

using Json = nlohmann::json;

const Json& requireObject(const Json& params) {
    if (!params.is_object()) {
        throw InvalidParams("expected object parameters");
    }
    return params;
}

void requireNull(const Json& params) {
    if (!params.is_null()) {
        throw InvalidParams("method does not accept parameters");
    }
}

std::vector<std::string> workspaceFolders(const Json& params) {
    std::vector<std::string> uris;
    if (params.contains("workspaceFolders") && params["workspaceFolders"].is_array()) {
        for (const auto& folder : params["workspaceFolders"]) {
            if (!folder.is_object()) {
                throw InvalidParams("workspace folder must be an object");
            }
            uris.push_back(folder.at("uri").get<std::string>());
        }
    } else if (params.contains("rootUri") && params["rootUri"].is_string()) {
        uris.push_back(params["rootUri"].get<std::string>());
    }
    return uris;
}

bool definitionLinkSupport(const Json& params) {
    if (!params.contains("capabilities")) {
        return false;
    }
    const auto& capabilities = requireObject(params.at("capabilities"));
    if (!capabilities.contains("textDocument")) {
        return false;
    }
    const auto& textDocument = requireObject(capabilities.at("textDocument"));
    if (!textDocument.contains("definition")) {
        return false;
    }
    const auto& definition = requireObject(textDocument.at("definition"));
    return definition.value("linkSupport", false);
}

bool documentSymbolHierarchySupport(const Json& params) {
    if (!params.contains("capabilities")) {
        return false;
    }
    const auto& capabilities = requireObject(params.at("capabilities"));
    if (!capabilities.contains("textDocument")) {
        return false;
    }
    const auto& textDocument = requireObject(capabilities.at("textDocument"));
    if (!textDocument.contains("documentSymbol")) {
        return false;
    }
    const auto& documentSymbol = requireObject(textDocument.at("documentSymbol"));
    return documentSymbol.value("hierarchicalDocumentSymbolSupport", false);
}

} // namespace

void RegisterLifecycleRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, WorkspaceService& workspace) {
    router.registerRequest("initialize", [&lifecycle, &workspace](const Json& params) {
        requireObject(params);
        const bool hasWorkspaceRoot =
            (params.contains("workspaceFolders") && !params["workspaceFolders"].is_null())
            || (params.contains("rootUri") && params["rootUri"].is_string());
        if (!workspace.initialize(workspaceFolders(params), hasWorkspaceRoot)) {
            throw InvalidParams("invalid workspace folder URI");
        }
        lifecycle.initialize(
            definitionLinkSupport(params), documentSymbolHierarchySupport(params));
        return Json{
            {"capabilities", {
                {"textDocumentSync", {
                    {"openClose", true},
                    {"change", 1},
                }},
                {"definitionProvider", true},
                {"referencesProvider", true},
                {"documentHighlightProvider", true},
                {"documentSymbolProvider", true},
                {"workspaceSymbolProvider", true},
                {"workspace", {
                    {"workspaceFolders", {
                        {"supported", true},
                        {"changeNotifications", true},
                    }},
                }},
            }},
            {"serverInfo", {
                {"name", "RandoLogicScript"},
                {"version", "0.1.0"},
            }},
        };
    });
    router.registerNotification("initialized", [&lifecycle](const Json& params) {
        requireObject(params);
        lifecycle.initialized();
    });
    router.registerRequest("shutdown", [&lifecycle](const Json& params) {
        requireNull(params);
        lifecycle.shutdown();
        return Json(nullptr);
    });
    router.registerNotification("exit", [&lifecycle](const Json& params) {
        requireNull(params);
        lifecycle.exit();
    });
}

} // namespace rls::lsp