## Detailed Plan: Safe Rename and Diagnostic Quick Fixes

### Goal

Apply safe, semantic multi-file edits for supported symbol renames and provide deterministic code actions for diagnostics with known, behavior-preserving repairs.

### Dependencies and Boundary

Consume stable symbols/references/diagnostic metadata from [plan-compilerQueryModelAndDiagnosticLsp.prompt.md](plan-compilerQueryModelAndDiagnosticLsp.prompt.md), navigation semantics from [plan-symbolNavigationAndDiscovery.prompt.md](plan-symbolNavigationAndDiscovery.prompt.md), authoring data from [plan-authoringAssistanceAndDocumentation.prompt.md](plan-authoringAssistanceAndDocumentation.prompt.md), and workspace-edit routing from [plan-explicitFeatureOrientedLsp.prompt.md](plan-explicitFeatureOrientedLsp.prompt.md). This plan does not add textual search-and-replace fallback.

### 1. Rename Eligibility

1. Implement `prepareRename` from `symbolAt` and symbol-category policy.
2. Support only symbols with concrete declarations and complete occurrence coverage: user regions, defines, enum types/members, parameters, and other categories once modeled.
3. Reject extern/pattern-derived symbols, unresolved names, ambiguous occurrences, generated-only entities, and unsupported entry categories.
4. Validate the proposed name against RLS lexical/reserved-word rules and scope/project collision rules before returning edits.
5. Return the exact declaration/reference name range from occurrence records.

### 2. Rename Execution

1. Implement `textDocument/rename` from the complete reference index.
2. Generate versioned workspace edits grouped by canonical document URI.
3. Require a current snapshot consistent with the request document/version. Refuse rather than risk edits from a stale snapshot.
4. Handle names with local scopes separately from global declarations; same-spelled parameters in separate defines must never co-rename.
5. Preserve qualified enum/member syntax and avoid editing comments, strings, unresolved text, or pattern declarations.
6. Respect client workspace-edit capabilities and fail clearly if required multi-document edits are unsupported.

### 3. Diagnostic Metadata and Code Actions

1. Add stable diagnostic codes and structured payloads in sema/validation.
2. Implement `textDocument/codeAction` by code and payload, never by matching human-readable messages.
3. Start with only deterministic actions:
   - Qualify a uniquely resolvable ambiguous enum member.
   - Insert a missing required argument when a default-safe template exists.
   - Replace an unknown named argument with the unique close parameter name.
   - Add a missing declaration stub only when project conventions identify a safe target location.
4. Construct edits from source/parser ranges and sema-provided facts. Do not attempt broad automated rewrites.
5. Return no action for diagnostics lacking a proven safe transformation.

### Tests

- Cross-file global rename and same-name local parameter isolation.
- Rename collision/reserved-name/extern/pattern rejection.
- Dirty/open buffer versions and stale snapshots.
- Workspace-edit capability variants.
- Each code action's edit range, resulting parse/sema validity, and no edits to comments/strings.
- No action for ambiguous or under-specified repairs.

### Definition of Done

Rename and fixes are available only where compiler facts prove safety; they never depend on spelling-based workspace search or diagnostic-message parsing.
