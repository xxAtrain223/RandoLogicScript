# Editor Grammar Change Checklist

When changing the PEGTL grammar in `parser/src/grammar.h`, review the editor artifacts whenever the change affects a keyword, declaration, expression form, comment form, delimiter, literal, or identifier syntax.

- Add or update an example in `tooling/syntax-fixtures/` and its lexical expectations.
- Update `editors/vscode/syntaxes/rls.tmLanguage.json` and verify the changed source in VS Code's scope inspector.
- Update the Tree-sitter grammar and highlight, fold, and indent queries when that grammar is present.
- Add malformed or incomplete input when the change introduces an open construct.
- Run the grammar-specific tests before merging.