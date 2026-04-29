#pragma once

#include "document_store.h"

namespace rls::lsp {

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

    const ServerCapabilities& capabilities() const;

    DocumentStore& documents();
    const DocumentStore& documents() const;

private:
    ServerCapabilities capabilities_;
    DocumentStore documents_;
};

} // namespace rls::lsp
