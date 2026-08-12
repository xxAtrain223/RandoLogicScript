#include "rls/lsp/route_modules.h"

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include "rls/lsp/document_synchronization_service.h"
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

void requireApplied(DocumentSynchronizationResult result) {
    if (result != DocumentSynchronizationResult::Applied) {
        throw InvalidParams("document synchronization was rejected");
    }
}

} // namespace

void RegisterDocumentSynchronizationRoutes(
    JsonRpcRouter& router, DocumentSynchronizationService& synchronization) {
    router.registerNotification("textDocument/didOpen", [&synchronization](const Json& params) {
        const auto& document = requireObject(requireObject(params).at("textDocument"));
        requireApplied(synchronization.open(
            document.at("uri").get<std::string>(),
            document.at("languageId").get<std::string>(),
            document.at("version").get<int64_t>(),
            document.at("text").get<std::string>()));
    });
    router.registerNotification("textDocument/didChange", [&synchronization](const Json& params) {
        const auto& object = requireObject(params);
        const auto& document = requireObject(object.at("textDocument"));
        const auto& changes = object.at("contentChanges");
        if (!changes.is_array() || changes.size() != 1) {
            throw InvalidParams("full synchronization requires one content change");
        }
        const auto& change = requireObject(changes.front());
        if (change.contains("range")) {
            throw InvalidParams("ranged changes are not supported");
        }
        requireApplied(synchronization.change(
            document.at("uri").get<std::string>(),
            document.at("version").get<int64_t>(),
            change.at("text").get<std::string>()));
    });
    router.registerNotification("textDocument/didClose", [&synchronization](const Json& params) {
        const auto& document = requireObject(requireObject(params).at("textDocument"));
        requireApplied(synchronization.close(document.at("uri").get<std::string>()));
    });
}

} // namespace rls::lsp