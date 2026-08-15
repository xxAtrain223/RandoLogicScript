# Editor Configuration

## Completion Snippet Indentation

Editors can choose who supplies indentation for multiline section completion snippets through the language-server initialization option:

```json
{
  "completion": {
    "sectionSnippetIndentation": "client"
  }
}
```

Use `client` when the editor adjusts indentation for multiline completion text. The language server sends relative indentation and sets LSP `insertTextMode` to `adjustIndentation`.

Use `server` when the editor inserts snippet text as-is. The language server embeds the current line's indentation and sets `insertTextMode` to `asIs`.

The language server defaults to `server`. The VS Code extension exposes this option as `randoLogicScript.completion.sectionSnippetIndentation` and defaults to `client`.

Restart the language server after changing this setting.