#include "rls/lsp/route_modules.h"

#include <nlohmann/json.hpp>

#include "rls/lsp/json_rpc_router.h"
#include "rls/lsp/lifecycle_service.h"
#include "rls/lsp/semantic_tokens_service.h"
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

bool completionSnippetSupport(const Json& params) {
    if (!params.contains("capabilities")) return false;
    const auto& capabilities = requireObject(params.at("capabilities"));
    if (!capabilities.contains("textDocument")) return false;
    const auto& textDocument = requireObject(capabilities.at("textDocument"));
    if (!textDocument.contains("completion")) return false;
    const auto& completion = requireObject(textDocument.at("completion"));
    if (!completion.contains("completionItem")) return false;
    const auto& completionItem = requireObject(completion.at("completionItem"));
    return completionItem.value("snippetSupport", false);
}

bool workspaceDocumentChangesSupport(const Json& params) {
    if (!params.contains("capabilities")) return false;
    const auto& capabilities = requireObject(params.at("capabilities"));
    if (!capabilities.contains("workspace")) return false;
    const auto& workspace = requireObject(capabilities.at("workspace"));
    if (!workspace.contains("workspaceEdit")) return false;
    const auto& workspaceEdit = requireObject(workspace.at("workspaceEdit"));
    return workspaceEdit.value("documentChanges", false);
}

SectionSnippetIndentation sectionSnippetIndentation(const Json& params) {
    if (!params.contains("initializationOptions")
        || !params.at("initializationOptions").is_object()) {
        return SectionSnippetIndentation::Server;
    }
    const auto& options = params.at("initializationOptions");
    if (!options.contains("completion") || !options.at("completion").is_object()) {
        return SectionSnippetIndentation::Server;
    }
    const auto& completion = options.at("completion");
    if (!completion.contains("sectionSnippetIndentation")
        || !completion.at("sectionSnippetIndentation").is_string()) {
        return SectionSnippetIndentation::Server;
    }
    return completion.at("sectionSnippetIndentation").get<std::string>() == "client"
        ? SectionSnippetIndentation::Client
        : SectionSnippetIndentation::Server;
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
            definitionLinkSupport(params), documentSymbolHierarchySupport(params),
            completionSnippetSupport(params), sectionSnippetIndentation(params),
            workspaceDocumentChangesSupport(params));
        return Json{
            {"capabilities", {
                {"textDocumentSync", {
                    {"openClose", true},
                    {"change", 1},
                }},
                {"definitionProvider", true},
                {"referencesProvider", true},
                {"renameProvider", {{"prepareProvider", true}}},
                {"documentHighlightProvider", true},
                {"documentSymbolProvider", true},
                {"completionProvider", {
                    {"resolveProvider", false},
                }},
                {"signatureHelpProvider", {
                    {"triggerCharacters", {"(", ","}},
                    {"retriggerCharacters", {","}},
                }},
                {"hoverProvider", true},
                {"semanticTokensProvider", {
                    {"legend", {
                        {"tokenTypes", SemanticTokensService::tokenTypes()},
                        {"tokenModifiers", SemanticTokensService::tokenModifiers()},
                    }},
                    {"range", false},
                    {"full", true},
                }},
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