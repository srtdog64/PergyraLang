# Pergyra Semantic Squiggle (VS Code)

Meaning-axis squiggles for Pergyra (design: [`docs/140`](../../docs/140_semantic_squiggle.md)).

This is a **thin client**. The C analyzer (`pgy-lsp`) is the source of truth; the
extension only runs it and recolours advisory diagnostics by their
`data.squiggleClass`:

| colour | meaning | how it is drawn |
| --- | --- | --- |
| red | blocking syntax/type error | VS Code's native error squiggle |
| amber | axis mismatch (Subject/Role/Intent/…) | wavy underline `#d7a000` |
| violet | authority/world boundary | wavy underline `#9b59b6` |
| blue | runtime-erased meaning (reserved) | wavy underline `#3498db` |

The amber/violet/blue colours are applied as editor *decorations* because LSP
`DiagnosticSeverity` cannot express four distinct colours (and violet in
particular is not an LSP severity). Advisory diagnostics never block compilation.

## Build & run

```sh
cd editors/vscode
npm install
npm run compile        # type-checks + emits out/extension.js
```

Then, from the repo root, build the language server:

```sh
make lsp               # produces bin/pgy-lsp
```

Open this folder in VS Code and press **F5** (Run Extension). Set
`pergyra.serverPath` to the absolute path of `bin/pgy-lsp` if it is not on
`PATH`. Open a `.pgy` file; shadow a `subject` binding inside a nested block and
an amber wavy underline appears on the inner `let` — while the file still
compiles.

## Status

`compile` (tsc) is the verification boundary reachable in CI. Running inside the
VS Code Extension Host requires a desktop VS Code instance and is verified
manually. The server side — the `data.squiggleClass` the client reads — is
covered end-to-end by an LSP session smoke (see `docs/140` slice 4a).
