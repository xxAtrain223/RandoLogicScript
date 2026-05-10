#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "document_store.h"

namespace rls::lsp {

namespace detail {
struct EndpointAccess;
}

enum class TextDocumentSyncKind {
    None = 0,
    Full = 1,
    Incremental = 2,
};

struct ServerCapabilities {
    TextDocumentSyncKind textDocumentSync = TextDocumentSyncKind::Full;
    bool definitionProvider = true;
    bool referencesProvider = true;
    bool hoverProvider = true;
    bool completionProvider = true;
    bool documentSymbolProvider = true;
    bool workspaceSymbolProvider = true;
};

class LanguageServer {
public:
    LanguageServer();

    // Handles one JSON-RPC payload and returns zero or more outbound JSON-RPC
    // payloads (without Content-Length framing).
    std::vector<std::string> handlePayload(std::string_view payload);

    const ServerCapabilities& capabilities() const;

    DocumentStore& documents();
    const DocumentStore& documents() const;

    bool shouldExit() const;

private:
    friend struct detail::EndpointAccess;

    std::vector<std::string> publishDiagnostics(const std::string& uri) const;

    ServerCapabilities capabilities_;
    DocumentStore documents_;
    bool isShutdown_ = false;
    bool shouldExit_ = false;
};

} // namespace rls::lsp
