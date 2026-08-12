#include "rls/lsp/route_modules.h"

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "rls/lsp/json_rpc_router.h"
#include "rls/lsp/lifecycle_service.h"
#include "rls/lsp/workspace_service.h"

namespace rls::lsp {
namespace {

using Json = nlohmann::json;

const Json& requireObject(const Json& value) {
    if (!value.is_object()) {
        throw InvalidParams("expected object parameters");
    }
    return value;
}

std::vector<std::string> folderUris(const Json& folders) {
    if (!folders.is_array()) {
        throw InvalidParams("workspace folders must be an array");
    }
    std::vector<std::string> uris;
    uris.reserve(folders.size());
    for (const auto& folder : folders) {
        uris.push_back(requireObject(folder).at("uri").get<std::string>());
    }
    return uris;
}

} // namespace

void RegisterWorkspaceRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, WorkspaceService& workspace) {
    router.registerNotification("workspace/didChangeWorkspaceFolders",
        [&lifecycle, &workspace](const Json& params) {
            if (!lifecycle.acceptsDocumentUpdates()) {
                throw InvalidParams("server is not initialized");
            }
            const auto& event = requireObject(requireObject(params).at("event"));
            if (!workspace.changeFolders(
                folderUris(event.at("added")), folderUris(event.at("removed")))) {
                throw InvalidParams("workspace reload failed");
            }
        });
    router.registerNotification("workspace/didChangeWatchedFiles",
        [&lifecycle, &workspace](const Json& params) {
            if (!lifecycle.acceptsDocumentUpdates()) {
                throw InvalidParams("server is not initialized");
            }
            const auto& changes = requireObject(params).at("changes");
            if (!changes.is_array()) {
                throw InvalidParams("file changes must be an array");
            }
            std::vector<std::string> uris;
            uris.reserve(changes.size());
            for (const auto& change : changes) {
                uris.push_back(requireObject(change).at("uri").get<std::string>());
            }
            if (!workspace.watchedFilesChanged(uris)) {
                throw InvalidParams("watched-file reload failed");
            }
        });
}

} // namespace rls::lsp