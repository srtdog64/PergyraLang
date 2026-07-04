# Intent

LSP-0 projects the self-host semantic diagnostic block into a
`textDocument/publishDiagnostics` JSON payload. LSP-1 owns the four-color
squiggle classification policy consumed by that payload. This directory is a
payload owner, not an LSP transport loop.

## Compiler World Binding

- **stage_resource**: `SemanticVerdictZone`
- **projection_owner**: `src/self_hosted/lsp/diagnostics_owner.pgy`
- **classification_owner**: `src/self_hosted/lsp/squiggle_owner.pgy`
- **stage_intent**: `ProjectSemanticDiagnostics`
- **payload_contract**: `LspDiagnosticPayloadContractReady`
- **policy_contract**: `LspSquigglePolicyContractReady`

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
reason-as-message, fix, span, facts, severity, `data.squiggleClass`, and
`data.oracleCode`.
The `--squiggle-policy` mode prints the executable LSP-1 classification snapshot
used by the parity gate.

## Oracle

The LSP-0 parity gate compares committed clean/error JSON fixtures, compiles the
tool through C and LLVM, and compares each fixture against the live C LSP
`--dump-diagnostics` oracle through canonical diagnostic events. C-side LSP
transport parity remains blocked on the O-LSP session gap documented in
`docs/150_selfhost_driver_lsp_wiring.md`.
