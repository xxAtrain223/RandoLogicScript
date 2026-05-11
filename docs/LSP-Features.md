# RandoLogicScript LSP Feature Status

This document summarizes what the current RandoLogicScript language server already supports and what is still missing.

The current implementation is centered on the server in `lsp/`, the advertised capabilities in `lsp/include/language_server.h`, and the registered endpoint handlers in `lsp/src/endpoints/`.

## Completed Features

### Core protocol and transport

- JSON-RPC framing and deframing over `Content-Length` transport headers.
- Request/notification dispatch through the endpoint registry.
- `initialize`, `shutdown`, and `exit` endpoint handling.
- Error responses for parse errors, invalid requests, unknown methods, and internal handler failures.

### Document lifecycle

- `textDocument/didOpen`
- `textDocument/didChange`
- `textDocument/didClose`
- In-memory document tracking by URI, version, language id, and full text.
- Full-document synchronization (`TextDocumentSyncKind::Full`).

### Diagnostics

- Parse diagnostics published on open and change.
- Semantic diagnostics published after successful parse.
- Empty diagnostics published on close to clear stale editor state.
- Zero-based LSP range conversion from the compiler span model.

### Language features

- `textDocument/definition`
- `textDocument/references`
- `textDocument/hover`
- `textDocument/completion`
- `textDocument/documentSymbol`
- `workspace/symbol`

### Current behavior of implemented language features

- Definition lookup resolves against indexed top-level declarations from open documents.
- Reference lookup searches all open documents for exact identifier text matches.
- Hover shows a lightweight plaintext label based on the indexed declaration kind and name.
- Completion merges:
  - language keywords
  - indexed top-level symbols from open documents
  - named parameter suggestions for the active call site
- Document symbols return a flat list of top-level declarations.
- Workspace symbol search filters indexed symbols with case-insensitive substring matching.

### Test coverage already present

- Transport framing tests.
- Document store tests.
- Language server endpoint tests.
- Protocol integration tests.
- Symbol support helper tests.

## Implemented But Limited

These features exist today, but their scope is intentionally narrow.

### Open-document scope

- Symbol indexing is based on currently open documents.
- Workspace symbol search only sees open documents.
- Definition and reference lookup only search open documents.

### Full-sync only

- `didChange` currently expects full document replacement text.
- Incremental text edits are not supported.

### Lightweight symbol intelligence

- References are found by text scanning with identifier-boundary checks, not by a semantic reference graph.
- Hover does not currently display types, signatures, documentation, or inferred semantic information.
- Completion does not currently provide documentation, resolve requests, snippet insertion, or advanced ranking.
- Document symbols are flat and top-level only.

### Diagnostics scope

- Diagnostics are published for the current open document snapshot.
- Semantic analysis currently runs on the single open file being published, not a fully loaded multi-file workspace project.

## Pending Features

The items below are not currently implemented in the language server.

### Higher-value editor features

- Rename symbol support.
- Signature help.
- Go to implementation / declaration / type definition.
- Find document highlights.
- Folding ranges.
- Semantic tokens.
- Code actions and quick fixes.
- Formatting and range formatting.
- Inlay hints.
- Selection ranges.

### Better project-wide analysis

- Workspace indexing for unopened files.
- Multi-file semantic analysis for diagnostics and navigation.
- Semantically accurate reference tracking instead of text-only matching.
- Richer hover content based on resolved types and declarations.
- Hierarchical document symbols.

### Protocol and UX improvements

- Incremental text synchronization.
- Completion item resolve support.
- Configuration and dynamic capability handling.
- Progress reporting and cancellation handling.
- File watching / workspace refresh behavior.
