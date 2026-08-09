## Plan: Cross-Editor RLS Developer Experience

Build editor support around portable standards: TextMate and Tree-sitter for syntax awareness, `rls.json` for project discovery/configuration, and LSP 3.17 for diagnostics and language intelligence. Parser and sema outputs will drive editor features; endpoints will not rediscover symbols through raw-text scanning.

1. **P0: Syntax highlighting and basic editing**

   Provide colorization, comments, bracket matching, indentation, folding, and auto-closing without running the compiler. TextMate supports VS Code, Sublime Text, and compatible hosts. Tree-sitter extends coverage to Neovim, Helix, Zed, and Emacs.

   Derive both grammars from [parser/src/grammar.h](../parser/src/grammar.h), parser tests, and [examples/rls](../examples/rls). Add shared fixtures and conformance checks so compiler grammar changes require corresponding editor grammar updates. Avoid semantic guesses based on identifier prefixes.

2. **P0: RLS project files and shared project loading**

   Introduce `rls.json` as the common project definition for the CLI and language server. It identifies the project root, source paths, exclusions, transpilers, and output paths. This supports multiple RLS projects inside one editor workspace.

   Define a versioned schema with `version`, `sources`, optional `exclude`, and `transpilers`. Resolve relative paths against the manifest directory. Discover the nearest manifest by walking upward from an edited file; nested manifests create separate projects.

   Move source collection and transpiler configuration out of [console/main.cpp](../console/main.cpp) into shared services. Add `--project <path>` while preserving explicit input arguments. Files without a manifest receive standalone parsing and local diagnostics, but no cross-file semantic results.

3. **P0: Compiler query model and diagnostic LSP**

   Run the real parser and sema passes as users edit, publishing parser, type, project, and configuration diagnostics to any LSP-compatible editor.

   Add an immutable `AnalysisSnapshot` containing the project, diagnostics, and compiler-produced query indexes. The parser should index token/name spans and syntax nodes. Sema should attach stable symbol identities, scopes, types, enum identities, resolved calls, declarations, and references.

   Expose queries such as:

   - Syntax or symbol at a position
   - Declaration and references for a symbol
   - Visible symbols at a position
   - Enclosing call and active argument
   - Expected type and enum identity at a position

   Populate these indexes in [parser/src/builder.cpp](../parser/src/builder.cpp), [sema/src/collect_declarations.cpp](../sema/src/collect_declarations.cpp), and [sema/src/resolve_types.cpp](../sema/src/resolve_types.cpp). Endpoints must not scan identifiers or search matching text across files.

   Centralize unavoidable text mechanics in a tested `SourceText` abstraction: line starts, edit application, byte offsets, UTF-8/UTF-16 conversion, and incomplete-token replacement ranges. Any invalid-source fallback stays in the compiler tooling layer and never claims semantic identity.

4. **P0: Explicit, feature-oriented LSP architecture**

   Salvage JSON-RPC transport, document versioning, URI handling, and useful tests from the historical branch. Replace its static endpoint registration and endpoint-local lookup logic.

   Use one explicit composition root and router. Group typed handlers into lifecycle, synchronization/diagnostics, navigation, authoring, highlighting, refactoring, and formatting modules. Handlers decode protocol data, invoke an injected service, and encode the response.

   Inject the document store, project manager, analysis scheduler, client connection, and logger explicitly. Exclude static registrars, linker force-loading, globals, and hidden singleton state.

5. **P1: Navigation and symbol discovery**

   Implement definition, references, document highlights, document symbols, and workspace symbols from `AnalysisSnapshot` queries.

   Cover regions, extensions, defines, extern defines, enums, enum members, parameters, entries, call targets, and qualified members. Definition of an `extend region` target goes to the base declaration; references include extensions and usages. Pattern-derived external enum values retain provenance but do not receive fabricated source definitions.

6. **P1: Completion, signature help, hover, and documentation**

   Build one signature/type renderer shared by completion, signature help, and hover. Use parser context and sema scopes to suggest only relevant declarations, section keys, parameters, functions, regions, entries, enum types, and enum members.

   Derive active arguments from parsed call spans, including named/default arguments. Use the isolated incomplete-token fallback only when recovery cannot produce syntax context.

   Initially synthesize documentation from signatures, types, defaults, provenance, and source locations. Later, introduce `##` documentation comments as an explicit language feature; ordinary `#` comments remain regular comments.

7. **P1: Semantic highlighting**

   Overlay distinctions that lexical grammars cannot establish, such as parameter versus enum member, declaration versus reference, user versus extern define, and unresolved names.

   Generate semantic tokens directly from compiler occurrence records and `Name` spans. The endpoint must not tokenize the document again. Start with full-document tokens; add range or delta support only after profiling.

8. **P2: Safe rename and quick fixes**

   Implement prepare-rename and rename from stable symbol/reference indexes, checking collisions, scopes, extern symbols, dirty document versions, and client workspace-edit capabilities.

   Give diagnostics stable codes and structured data so code actions never parse message strings. Begin with deterministic fixes such as enum qualification, missing arguments, uniquely matched named-argument corrections, and declaration stubs.

9. **P2: Formatting and structural editing**

   Add a lossless token/trivia or concrete-syntax representation because the semantic AST discards comments and exact whitespace. Build an idempotent standalone formatter, then expose it through LSP.

   Derive folding and selection ranges from parser-owned spans. Verify formatting preserves comments, parses equivalently, and is stable across repeated runs.

10. **P3: Performance and advanced navigation**

   Measure project discovery, parsing, analysis, indexing, and memory use before introducing per-file caching, dependency-aware invalidation, background analysis, or semantic-token deltas.

   Add call hierarchy from resolved call edges if workflows justify it. Omit type hierarchy because RLS has no subtype model.

**Key Decisions**

- `rls.json` is required for the first diagnostics-capable LSP.
- Compiler parser/sema passes own syntax and semantic queries.
- Raw-text calculations are centralized and narrowly limited.
- Endpoints use explicit typed routing and injected feature services.
- The historical branch is selectively salvaged, not merged wholesale.
- Semantic tokens supplement syntax highlighting; they do not provide completion, errors, or navigation.
- Whole-project analysis is acceptable initially if measurements remain interactive.
