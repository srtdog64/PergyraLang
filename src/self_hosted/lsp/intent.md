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
- **request_dispatch_owner**: `src/self_hosted/lsp/request_owner.pgy`
- **response_emission_owner**: `src/self_hosted/lsp/response_owner.pgy`
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

The `--request-dispatch-probe <max-bytes>` mode is the LSP-2c input boundary:
it reads one stdin buffer, consumes complete transport frames through
`transport_owner.pgy`, and classifies request bodies through the shared JSON
fact table. It is not a response emitter or document-store boundary.

The `--response-probe <max-bytes>` mode is the LSP-2d input boundary: it reads
one stdin buffer, consumes complete transport frames, and projects response
required request bodies into response body/frame plans. It is not a live
read-exact loop or document-store boundary.

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
The `--request-dispatch-probe` mode prints a
`pgy.selfhost.lsp-request-dispatch-stream.v1` artifact with request dispatch
plans.
The `--response-probe` mode prints a
`pgy.selfhost.lsp-response-emission-stream.v1` artifact with response body/frame
plans.

## Oracle

The LSP-0 parity gate compares committed clean/error JSON fixtures, compiles the
tool through C and LLVM, and compares each fixture against the live C LSP
`--dump-diagnostics` oracle through canonical diagnostic events. C-side LSP
transport parity remains blocked on the full `G-LSP-STREAM` session gap
documented in `docs/150_selfhost_driver_lsp_wiring.md`. LSP-2a single-frame
transport parity is checked by
`tests/self_hosted/parity/lsp_transport_frame_parity.sh`; LSP-2b buffered
stream parity is checked by
`tests/self_hosted/parity/lsp_transport_stream_parity.sh`; LSP-2c request
dispatch parity is checked by
`tests/self_hosted/parity/lsp_request_dispatch_parity.sh`; LSP-2d response
emission parity is checked by
`tests/self_hosted/parity/lsp_response_emission_parity.sh`.
