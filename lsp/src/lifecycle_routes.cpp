#include "rls/lsp/route_modules.h"

#include <nlohmann/json.hpp>

#include "rls/lsp/json_rpc_router.h"
#include "rls/lsp/lifecycle_service.h"
#include "rls/lsp/workspace_service.h"

namespace rls::lsp {
namespace {

using Json = nlohmann::json;

void requireObject(const Json& params) {
    if (!params.is_object()) {
        throw InvalidParams("expected object parameters");
    }
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
        lifecycle.initialize();
        return Json{
            {"capabilities", {
                {"textDocumentSync", {
                    {"openClose", true},
                    {"change", 1},
                }},
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