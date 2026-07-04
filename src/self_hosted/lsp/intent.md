# Intent

LSP-0 projects the self-host semantic diagnostic block into a
`textDocument/publishDiagnostics` JSON payload. It is a payload owner, not an
LSP transport loop.

## Compiler World Binding

- **stage_resource**: `SemanticVerdictZone`
- **projection_owner**: `src/self_hosted/lsp/diagnostics_owner.pgy`
- **stage_intent**: `ProjectSemanticDiagnostics`
- **payload_contract**: `LspDiagnosticPayloadContractReady`

## Input Contract

The entrypoint reads one source path from `Args()[0]`, loads it through the
semantic source-bundle owner, and consumes `CheckProgram` plus the semantic
diagnostic renderer. Missing input reports a diagnostic payload and exits
non-zero.

## Output Contract

The tool prints one JSON object with schema
`pgy.selfhost.lsp-diagnostics.v1`, method
`textDocument/publishDiagnostics`, URI, and diagnostics. A clean source emits
an empty diagnostics array. An error source emits one diagnostic with code,
reason-as-message, fix, span, facts, severity, and `data.squiggleClass`.

## Oracle

The LSP-0 parity gate compares committed clean/error JSON fixtures and compiles
the tool through C and LLVM. C-side LSP transport parity remains blocked on the
O-LSP dump flag documented in `docs/150_selfhost_driver_lsp_wiring.md`.
