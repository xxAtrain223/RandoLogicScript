#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "document_store.h"

namespace rls::lsp {

namespace detail {
/// Private bridge that grants endpoint implementations controlled access to
/// LanguageServer internals.
struct EndpointAccess;
}

/// LSP text synchronization modes that the server may advertise.
enum class TextDocumentSyncKind {
    /// The server does not maintain synchronized document contents.
    None = 0,
    /// The client sends complete document contents on each change.
    Full = 1,
    /// The client sends incremental document edits.
    Incremental = 2,
};

/// Capability set advertised by the server during initialization.
struct ServerCapabilities {
    /// Declares how text document changes are synchronized.
    TextDocumentSyncKind textDocumentSync = TextDocumentSyncKind::Full;
    /// Indicates support for go-to-definition requests.
    bool definitionProvider = true;
    /// Indicates support for find-references requests.
    bool referencesProvider = true;
    /// Indicates support for hover requests.
    bool hoverProvider = true;
    /// Indicates support for completion requests.
    bool completionProvider = true;
    /// Indicates support for document symbol requests.
    bool documentSymbolProvider = true;
    /// Indicates support for workspace symbol requests.
    bool workspaceSymbolProvider = true;
};

/// Owns document state and dispatches JSON-RPC requests for the language
/// server process.
class LanguageServer {
public:
    /// Constructs a language server with the default capability set.
    LanguageServer();

    /// Handles one JSON-RPC payload and returns zero or more outbound JSON-RPC
    /// payloads without Content-Length framing.
    std::vector<std::string> handlePayload(std::string_view payload);

    /// Returns the server capabilities that should be advertised to clients.
    const ServerCapabilities& capabilities() const;

    /// Returns mutable access to the synchronized document store.
    DocumentStore& documents();
    /// Returns read-only access to the synchronized document store.
    const DocumentStore& documents() const;

    /// Returns true when the server has received an exit-triggering request and
    /// the host process should terminate.
    bool shouldExit() const;

private:
    friend struct detail::EndpointAccess;

    /// Builds textDocument/publishDiagnostics notifications for one open
    /// document.
    std::vector<std::string> publishDiagnostics(const std::string& uri) const;

    /// Capabilities currently reported by the server.
    ServerCapabilities capabilities_;
    /// Open documents tracked for endpoint handlers.
    DocumentStore documents_;
    /// True after a shutdown request has been processed.
    bool isShutdown_ = false;
    /// True after an exit request indicates the host should terminate.
    bool shouldExit_ = false;
};

} // namespace rls::lsp
