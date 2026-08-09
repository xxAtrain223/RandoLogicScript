## Detailed Plan: Formatting and Structural Editing

### Goal

Provide a canonical, comment-preserving RLS formatter plus structural folding and selection support. The formatter is usable from the CLI and exposed to editors through LSP later.

### Dependencies and Boundary

This plan depends on syntax knowledge from [plan-syntaxHighlightingAndBasicEditing.prompt.md](plan-syntaxHighlightingAndBasicEditing.prompt.md) and uses [plan-explicitFeatureOrientedLsp.prompt.md](plan-explicitFeatureOrientedLsp.prompt.md) only to expose formatting/folding endpoints. It does not use the semantic AST as a printing source and does not duplicate language-server routing.

### 1. Lossless Representation

1. Introduce a token/trivia or concrete-syntax representation that retains comments, whitespace, delimiters, and error regions.
2. Associate lossless nodes with parser syntax spans where possible without requiring semantic resolution.
3. Preserve all comments and string literal contents exactly unless a documented formatting rule permits a safe normalization.
4. Define behavior for malformed source: either format only safe regions or decline with no edits; never silently drop content.

### 2. Formatting Specification

1. Specify indentation, spaces, blank lines, brace layout, list/call wrapping, named arguments, match arms, and section/region formatting.
2. Define line-width and continuation behavior with deterministic tie-breaking.
3. Keep rules independent of user-specific editor settings initially; later configuration needs a separate compatibility/versioning decision.
4. Make formatting idempotent and preserve parse meaning.

### 3. Formatter Product

1. Implement a reusable formatting library over lossless input.
2. Add `rls format` CLI behavior for files/project source sets, check mode, and safe write behavior.
3. Add golden test fixtures for real examples, edge cases, comments, strings, empty blocks, nested expressions, and malformed input.
4. Verify `format(format(source)) == format(source)` and reparse formatted valid files.

### 4. Structural Editor Features

1. Derive folding ranges from parser/lossless structure for declarations, regions, sections, blocks, and multiline expressions where meaningful.
2. Derive nested selection ranges from syntax containment, not text delimiters alone.
3. Expose `textDocument/formatting`, range formatting if safe, folding range, and selection range through feature modules after the server architecture is ready.
4. Keep basic editor indentation metadata as a serverless fallback.

### Definition of Done

The formatter preserves comments and meaning, reaches an idempotent layout, and structural ranges come from parser structure rather than ad hoc text scanning.
