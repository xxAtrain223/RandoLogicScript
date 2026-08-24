# Visual Studio Manual Testing

Run this checklist before a Visual Studio Marketplace release. Record the Visual
Studio version, edition, Windows version, extension version, and result for each pass.

## Version Matrix

- [ ] Oldest supported Visual Studio 2022 x64 release installs and loads the VSIX.
- [ ] Latest supported Visual Studio 2022 x64 release installs and loads the VSIX.
- [ ] Current Visual Studio 2026 x64 release installs and loads the same VSIX.
- [ ] Updating from the previous extension version preserves activation and settings.
- [ ] Uninstall removes the extension and bundled server without leaving a running process.

## Workspace Modes

- [ ] **Open Folder** on an RLS project discovers `rls.json` and all configured sources.
- [ ] Opening a CMake folder containing RLS files activates the language client.
- [ ] Opening one standalone `.rls` file provides diagnostics and authoring features.
- [ ] Paths containing spaces and non-ASCII characters open and navigate correctly.
- [ ] Editing `rls.json` refreshes project membership and clears stale diagnostics.

## Authoring And Navigation

- [ ] Completion appears at top level, expression positions, types, enum members, and arguments.
- [ ] Hover renders readable Markdown and declaration links.
- [ ] Signature help opens at `(` and advances at `,`.
- [ ] Go To Definition navigates to the exact declaration selection.
- [ ] Find All References includes or excludes declarations as requested and opens each result.
- [ ] Document Outline shows expected symbols without duplicate or malformed entries.
- [ ] Workspace symbol search finds public declarations in the active workspace only.
- [ ] Rename preview shows all expected versioned edits and applies them atomically.
- [ ] Invalid rename targets and collisions show a useful refusal instead of partial edits.

## Diagnostics And Presentation

- [ ] Error List shows RLS errors, warnings, and information at correct UTF-16 ranges.
- [ ] Selecting an Error List item navigates to the correct file and span.
- [ ] Diagnostics clear after fixing a source or manifest error.
- [ ] TextMate syntax scopes render coherently in light, dark, and high-contrast themes.
- [ ] Semantic function, parameter, enum, member, property, variable, and operator tokens render.
- [ ] In `examples/soh/src/overworld/root.rls`, `setting`/`is_child` render as
	methods, `TimePasses` as an enum, `No`/`SCENE_ID_MAX`/`RA_LINKS_POCKET` as enum
	members, and event/location labels as properties rather than generic text.
- [ ] Semantic colors remain legible when they overlap TextMate syntax classifications.
- [ ] Comments, bracket matching, auto-closing, surrounding pairs, and word selection behave as configured.

## Accessibility And Lifecycle

- [ ] All extension-driven UI can be reached and dismissed with the keyboard.
- [ ] Quick Info, completion, signature help, Error List, references, and rename expose useful screen-reader text.
- [ ] Focus returns predictably after navigation, rename, and dismissed IntelliSense UI.
- [ ] Killing the server results in one replacement process and a useful Activity Log entry.
- [ ] Closing Visual Studio terminates the bundled server without an orphan process.
- [ ] A missing or invalid development override produces an InfoBar and actionable Activity Log message.