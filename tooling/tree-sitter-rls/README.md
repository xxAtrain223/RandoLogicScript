# Tree-sitter RLS Grammar

This package is the incremental editor grammar for Rando Logic Script. It provides syntax trees, highlighting, folding, and indentation queries for editor integrations.

`parser/src/grammar.h` remains compiler-authoritative. Changes to that PEGTL grammar which affect lexical syntax, declarations, expressions, comments, or delimiters must be reflected here and in the TextMate grammar.

## Commands

Run these commands from this directory:

```powershell
npm install
npm run generate
npm test
```

`npm test` exercises the valid part of the shared fixture, verifies controlled error recovery for its incomplete tail, and runs the highlight, fold, and indent queries.