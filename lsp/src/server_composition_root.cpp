#include "rls/lsp/server_composition_root.h"

#include <cstdint>
#include <string>

namespace rls::lsp {
namespace {

using Json = nlohmann::json;

const Json& requireObject(const Json& value) {
    if (!value.is_object()) {
        throw InvalidParams("expected object parameters");
    }
    return value;
}

} // namespace

ServerCompositionRoot::ServerCompositionRoot() {
    registerLifecycleRoutes();
    registerDocumentRoutes();
    router_.requireRoutes({
        "initialize",
        "initialized",
        "shutdown",
        "exit",
        "textDocument/didOpen",
        "textDocument/didChange",
        "textDocument/didClose",
    });
}

std::vector<std::string> ServerCompositionRoot::handlePayload(std::string_view payload) const {
    return router_.handlePayload(payload);
}

bool ServerCompositionRoot::shouldExit() const {
    return exitRequested_;
}

int ServerCompositionRoot::exitCode() const {
    return shutdownRequested_ ? 0 : 1;
}

const DocumentStore& ServerCompositionRoot::documents() const {
    return documents_;
}

const JsonRpcRouter& ServerCompositionRoot::router() const {
    return router_;
}

void ServerCompositionRoot::requireInitialized() const {
    if (!initialized_ || shutdownRequested_) {
        throw InvalidParams("server is not accepting document updates");
    }
}

void ServerCompositionRoot::registerLifecycleRoutes() {
    router_.registerRequest("initialize", [this](const Json& params) {
        if (!params.is_null()) {
            requireObject(params);
        }
        initialized_ = true;
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
    router_.registerNotification("initialized", [](const Json& params) {
        if (!params.is_null()) {
            requireObject(params);
        }
    });
    router_.registerRequest("shutdown", [this](const Json& params) {
        if (!params.is_null()) {
            throw InvalidParams("shutdown does not accept parameters");
        }
        shutdownRequested_ = true;
        return Json(nullptr);
    });
    router_.registerNotification("exit", [this](const Json& params) {
        if (!params.is_null()) {
            throw InvalidParams("exit does not accept parameters");
        }
        exitRequested_ = true;
    });
}

void ServerCompositionRoot::registerDocumentRoutes() {
    router_.registerNotification("textDocument/didOpen", [this](const Json& params) {
        requireInitialized();
        const auto& document = requireObject(requireObject(params).at("textDocument"));
        const auto result = documents_.open(
            document.at("uri").get<std::string>(),
            document.at("languageId").get<std::string>(),
            document.at("version").get<int64_t>(),
            document.at("text").get<std::string>());
        if (result != DocumentUpdateResult::Applied) {
            throw InvalidParams("document could not be opened");
        }
    });
    router_.registerNotification("textDocument/didChange", [this](const Json& params) {
        requireInitialized();
        const auto& object = requireObject(params);
        const auto& document = requireObject(object.at("textDocument"));
        const auto& changes = object.at("contentChanges");
        if (!changes.is_array() || changes.size() != 1) {
            throw InvalidParams("full synchronization requires one content change");
        }
        const auto& change = requireObject(changes.back());
        if (change.contains("range")) {
            throw InvalidParams("ranged changes are not supported");
        }
        const auto result = documents_.applyFullChange(
            document.at("uri").get<std::string>(),
            document.at("version").get<int64_t>(),
            change.at("text").get<std::string>());
        if (result != DocumentUpdateResult::Applied) {
            throw InvalidParams("document change was rejected");
        }
    });
    router_.registerNotification("textDocument/didClose", [this](const Json& params) {
        requireInitialized();
        const auto& document = requireObject(requireObject(params).at("textDocument"));
        documents_.close(document.at("uri").get<std::string>());
    });
}

} // namespace rls::lsp