#include "rls/lsp/route_modules.h"

#include <nlohmann/json.hpp>

#include "rls/lsp/json_rpc_router.h"
#include "rls/lsp/lifecycle_service.h"

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

} // namespace

void RegisterLifecycleRoutes(JsonRpcRouter& router, LifecycleService& lifecycle) {
    router.registerRequest("initialize", [&lifecycle](const Json& params) {
        requireObject(params);
        lifecycle.initialize();
        return Json{
            {"capabilities", {
                {"textDocumentSync", {
                    {"openClose", true},
                    {"change", 1},
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