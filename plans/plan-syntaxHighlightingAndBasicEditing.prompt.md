## Detailed Plan: Syntax Highlighting and Basic Editing

### Goal

Make `.rls` pleasant to read and edit without starting the compiler or language server. Ship portable grammar artifacts with thin editor adapters.

### Ownership

This plan owns lexical syntax classification, editor language registration, bracket/comment/indent metadata, and grammar conformance fixtures. It does not own semantic token classification, diagnostics, parser replacement, or language-server behavior.

### Deliverables

1. **Canonical syntax corpus**
   - Extract representative valid examples from `examples/rls` and focused syntax cases from parser tests.
   - Cover declarations, regions/extensions, section/data keys, defines/extern defines, enums/extern enums, expressions, calls/named arguments, lists, match/member expressions, comments, strings, and malformed/incomplete input.
   - [x] Maintain expected lexical categories independently of parser implementation details.

2. **TextMate grammar**
   - [x] Add a JSON grammar with standard scopes for comments, strings, numeric/boolean literals, declaration keywords, control/operator keywords, type names, declaration names, parameter names, punctuation, and operators.
   - [x] Use `source.rls` as the root scope and standard scopes targeted by existing themes.
   - [x] Highlight names based on syntax context only. An identifier is not an enum value, parameter, or function reference merely because its spelling has a prefix.
   - [x] Cover `#` comments, string escapes, braces/parens/brackets, qualified names, and error-tolerant open constructs.
   - Add scope snapshots for the shared corpus.

3. **Language metadata and VS Code adapter**
   - [x] Register `.rls`, line comments, bracket pairs, auto-closing pairs, surrounding pairs, and word pattern in a minimal VS Code language extension.
   - [x] Keep the extension declarative at this stage: it contains the grammar and language configuration, not compiler behavior.
   - [x] Audit the historical extension before reuse because current grammar includes newer enum/member/callable syntax. No historical extension was present in this repository.
   - Test scope inspection and bracket/comment behavior in VS Code.

4. **Tree-sitter grammar**
   - Create `tree-sitter-rls` with a grammar that represents current RLS syntax and maintains useful error nodes while users type.
   - Add `highlights.scm`, `folds.scm`, and `indents.scm` queries using the same lexical intent as TextMate.
   - Run parser/highlight query tests against the shared corpus.
   - Document Tree-sitter as an editor artifact. PEGTL remains compiler-authoritative and is not replaced.

5. **Drift prevention**
   - [x] Add a grammar-change checklist: a PEGTL keyword, declaration, expression, comment, or delimiter change requires corpus and grammar updates.
   - Add CI jobs for TextMate scope tests and Tree-sitter tests.
   - [x] Add examples for malformed source so grammar regressions do not make editing unusable during incomplete changes.

### File Boundaries

- `parser/src/grammar.h` remains the compiler syntax authority.
- New `tooling/syntax-fixtures/` owns shared examples and expected lexical annotations.
- New `tooling/textmate/` owns the TextMate grammar and scope tests.
- New `tooling/tree-sitter-rls/` owns the Tree-sitter grammar and query tests.
- New `editors/vscode/` owns declarative VS Code packaging.

### Definition of Done

- `.rls` is recognized in VS Code and colorized accurately without the language server.
- TextMate and Tree-sitter cover the shared valid and incomplete corpus.
- Standard themes render meaningful distinctions without a custom theme.
- Grammar tests make new RLS syntax visibly fail until both editor grammars are updated.
