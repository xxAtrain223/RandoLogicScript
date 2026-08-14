## Cross-Editor RLS Plan Index

This index splits [plan-crossEditorRlsDeveloperExperience.prompt.md](plan-crossEditorRlsDeveloperExperience.prompt.md) into focused refinement documents. A concept has one owning plan. Other plans may state a dependency but must not restate its design.

| Main item                             | Owner plan                                                                                               | Depends on                                             |
| ------------------------------------- | -------------------------------------------------------------------------------------------------------- | ------------------------------------------------------ |
| Syntax highlighting and basic editing | [plan-syntaxHighlightingAndBasicEditing.prompt.md](plan-syntaxHighlightingAndBasicEditing.prompt.md)     | None                                                   |
| RLS project files and loading         | [plan-rlsProjectFilesAndLoading.prompt.md](plan-rlsProjectFilesAndLoading.prompt.md)                     | None                                                   |
| Compiler query model                  | [plan-compilerQueryModelAndDiagnosticLsp.prompt.md](plan-compilerQueryModelAndDiagnosticLsp.prompt.md)   | Project configuration supplies source membership       |
| LSP architecture and live diagnostics | [plan-explicitFeatureOrientedLsp.prompt.md](plan-explicitFeatureOrientedLsp.prompt.md)                   | Project loading; compiler query model                  |
| VS Code language client integration    | [plan-vscodeLanguageClientIntegration.prompt.md](plan-vscodeLanguageClientIntegration.prompt.md)       | Syntax adapter; LSP architecture                       |
| Navigation and discovery              | [plan-symbolNavigationAndDiscovery.prompt.md](plan-symbolNavigationAndDiscovery.prompt.md)               | Compiler query model; LSP architecture                 |
| Completion, hover, signatures, docs   | [plan-authoringAssistanceAndDocumentation.prompt.md](plan-authoringAssistanceAndDocumentation.prompt.md) | Compiler query model; LSP architecture                 |
| Semantic highlighting                 | [plan-semanticHighlighting.prompt.md](plan-semanticHighlighting.prompt.md)                               | Compiler query model; LSP architecture                 |
| Rename and quick fixes                | [plan-renameAndQuickFixes.prompt.md](plan-renameAndQuickFixes.prompt.md)                                 | Navigation; authoring; diagnostics                     |
| Formatting and structural editing     | [plan-formattingAndStructuralEditing.prompt.md](plan-formattingAndStructuralEditing.prompt.md)           | Syntax support; LSP architecture for protocol exposure |
| Performance and advanced navigation   | [plan-performanceAndAdvancedNavigation.prompt.md](plan-performanceAndAdvancedNavigation.prompt.md)       | Measured behavior from shipped features                |

### Ownership Rules

- [plan-rlsProjectFilesAndLoading.prompt.md](plan-rlsProjectFilesAndLoading.prompt.md) owns manifest schema, discovery, source membership, excludes, and transpiler/output configuration.
- [plan-compilerQueryModelAndDiagnosticLsp.prompt.md](plan-compilerQueryModelAndDiagnosticLsp.prompt.md) owns source positions, parser indexes, semantic identity, analysis snapshots, and compiler query APIs.
- [plan-explicitFeatureOrientedLsp.prompt.md](plan-explicitFeatureOrientedLsp.prompt.md) owns JSON-RPC, document synchronization, scheduling, explicit route composition, and diagnostic publication.
- [plan-vscodeLanguageClientIntegration.prompt.md](plan-vscodeLanguageClientIntegration.prompt.md) owns VS Code activation, server discovery/launch, settings, native binary packaging, and extension-host tests.
- Feature plans own their endpoint behavior only. They consume the query/snapshot and LSP service APIs rather than reaching into parser, sema, document-store, or transport internals.
- [plan-syntaxHighlightingAndBasicEditing.prompt.md](plan-syntaxHighlightingAndBasicEditing.prompt.md) and [plan-formattingAndStructuralEditing.prompt.md](plan-formattingAndStructuralEditing.prompt.md) own their own syntax representations. Tree-sitter is an editor parser and does not replace PEGTL; a formatter needs lossless trivia and does not serialize the semantic AST.
