# Intent

LSP-0 projects the self-host semantic diagnostic block into a
`textDocument/publishDiagnostics` JSON payload. LSP-1 owns the four-color
squiggle classification policy consumed by that payload. This directory is a
payload owner, not an LSP transport loop.

## Compiler World Binding

- **stage_resource**: `SemanticVerdictZone`
- **projection_owner**: `src/self_hosted/lsp/diagnostics_owner.pgy`
- **classification_owner**: `src/self_hosted/lsp/squiggle_owner.pgy`
- **transport_owner**: `src/self_hosted/lsp/transport_owner.pgy`
- **stage_intent**: `ProjectSemanticDiagnostics`
- **payload_contract**: `LspDiagnosticPayloadContractReady`
- **policy_contract**: `LspSquigglePolicyContractReady`

## Input Contract

The default entrypoint reads one source path from `Args()[0]`, loads it through
the semantic source-bundle owner, and consumes `CheckProgram` plus the semantic
diagnostic renderer. Missing input reports a diagnostic payload and exits
non-zero.

The `--transport-frame-probe <max-bytes>` mode is the LSP-2a input boundary:
it reads stdin through `ReadStdin(max_bytes)` and parses one JSON-RPC
Content-Length frame. It is not the full LSP session loop.

The `--transport-stream-probe <max-bytes>` mode is the LSP-2b input boundary:
it reads one stdin buffer through `ReadStdin(max_bytes)`, consumes complete
Content-Length frames in order, and reports the first partial frame reason. It
is not a live read-exact loop or request dispatch boundary.

## Output Contract

The tool prints one JSON object with schema
`pgy.selfhost.lsp-diagnostics.v1`, method
`textDocument/publishDiagnostics`, URI, and diagnostics. A clean source emits
an empty diagnostics array. An error source emits one diagnostic with code,
reason-as-message, fix, span, facts, severity, `data.squiggleClass`, and
`data.oracleCode`.
The `--squiggle-policy` mode prints the executable LSP-1 classification snapshot
used by the parity gate.
The `--transport-frame-probe` mode prints a
`pgy.selfhost.lsp-transport-frame.v1` artifact with `contentLength`,
`bodyLength`, and body/error fields.
The `--transport-stream-probe` mode prints a
`pgy.selfhost.lsp-transport-stream.v1` artifact with frame count, partial
reason, and frame bodies.

## Oracle

The LSP-0 parity gate compares committed clean/error JSON fixtures, compiles the
tool through C and LLVM, and compares each fixture against the live C LSP
`--dump-diagnostics` oracle through canonical diagnostic events. C-side LSP
transport parity remains blocked on the full `G-LSP-STREAM` session gap
documented in `docs/150_selfhost_driver_lsp_wiring.md`. LSP-2a single-frame
transport parity is checked by
`tests/self_hosted/parity/lsp_transport_frame_parity.sh`; LSP-2b buffered
stream parity is checked by
`tests/self_hosted/parity/lsp_transport_stream_parity.sh`.
