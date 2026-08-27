# Intent

LSP-0 projects the self-host semantic diagnostic block into a
`textDocument/publishDiagnostics` JSON payload. LSP-1 owns the four-color
squiggle classification policy consumed by that payload. This directory is a
payload owner, not an LSP transport loop.

## Compiler World Binding

- **stage_resource**: `SemanticVerdictZone`
- **projection_owner**: `src/self_hosted/lsp/diagnostics_owner.pgy`
- **classification_owner**: `src/self_hosted/lsp/squiggle_owner.pgy`
- **document_store_owner**: `src/self_hosted/lsp/document_store_owner.pgy`
- **feature_response_owner**: `src/self_hosted/lsp/feature_owner.pgy`
- **transport_owner**: `src/self_hosted/lsp/transport_owner.pgy`
- **request_dispatch_owner**: `src/self_hosted/lsp/request_owner.pgy`
- **response_emission_owner**: `src/self_hosted/lsp/response_owner.pgy`
- **session_replay_owner**: `src/self_hosted/lsp/session_owner.pgy`
- **session_state_owner**: `src/self_hosted/lsp/session_state_owner.pgy`
- **hover_content_owner**: `src/self_hosted/lsp/hover_content_owner.pgy`
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

The `--response-probe <max-bytes>` mode is the LSP-2d/LSP-2g input boundary: it reads
one stdin buffer, consumes complete transport frames, and projects response
required request bodies into response body/frame plans. LSP-2g adds valid
no-index response shapes for advertised `textDocument/*` features through
`feature_owner.pgy`. It is not a live read-exact loop or document-store
boundary.

The `--session-replay-probe <max-bytes>` mode is the LSP-2e input boundary: it
reads one stdin buffer, consumes complete transport frames, and emits the
response frame wire string for requests whose response is already owned by
`response_owner.pgy`. It is still not a live read-exact loop or document-store
boundary.

The `--document-store-probe <max-bytes>` mode is the LSP-2f input boundary: it
reads one stdin buffer, consumes complete transport frames, and projects
`textDocument/didOpen` plus `textDocument/didChange` into a deterministic
multi-document state artifact. It is still not a live read-exact loop or
feature handler.

The `--session-state-probe <max-bytes>` mode is the LSP-2h input boundary: it
reads one stdin buffer and projects both the response replay artifact and the
multi-document store artifact into one session-state artifact. It is still not
a live read-exact loop or semantic feature content.

The `--hover-content-probe <max-bytes>` mode is the LSP-2i input boundary: it
reads one stdin buffer, consumes a buffered document snapshot plus
`textDocument/hover` requests, and projects bounded markdown hover content. It
is still not a live read-exact loop or indexed symbol database.

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
plans, including no-index feature response shapes.
The `--session-replay-probe` mode prints a
`pgy.selfhost.lsp-session-replay.v1` artifact with the emitted response frame
wire string and per-frame list.
The `--document-store-probe` mode prints a
`pgy.selfhost.lsp-document-store.v1` artifact with mutation count, final URI,
final version, final text, deterministic typed revision rows, queued
publication candidates, current-generation publication admissions, and event
rows. URI/version/text and HostTask generation remain one revision record;
lower/equal conflicting versions do not partially mutate the store.
The `--session-state-probe` mode prints a
`pgy.selfhost.lsp-session-state.v1` artifact with raw `session` and
`documentStore` fact objects.
The `--hover-content-probe` mode prints a
`pgy.selfhost.lsp-hover-content.v1` artifact with hover request events and
markdown/null hover results over the buffered document snapshot.

## Oracle

The LSP-0 parity gate compares committed clean/error JSON fixtures, compiles the
tool through C and LLVM, and requires the default public C launcher
`--dump-diagnostics` output to be byte-equal to the installed Pergyra tool.
The retained C implementation is reached only through
`--native-pipeline --dump-diagnostics` and is compared through canonical
diagnostic events. A missing installed tool must fail with no payload. C-side LSP
transport parity remains blocked on the full `G-LSP-STREAM` session gap
documented in `docs/150_selfhost_driver_lsp_wiring.md`. LSP-2a single-frame
transport parity is checked by
`tests/self_hosted/parity/lsp_transport_frame_parity.sh`; LSP-2b buffered
stream parity is checked by
`tests/self_hosted/parity/lsp_transport_stream_parity.sh`; LSP-2c request
dispatch parity is checked by
`tests/self_hosted/parity/lsp_request_dispatch_parity.sh`; LSP-2d response
emission parity is checked by
`tests/self_hosted/parity/lsp_response_emission_parity.sh`; LSP-2e session
replay parity is checked by
`tests/self_hosted/parity/lsp_session_replay_parity.sh`; LSP-2f document-store
parity is checked by `tests/self_hosted/parity/lsp_document_store_parity.sh`.
Its Insere-derived latest-only negative is checked by
`tests/self_hosted/parity/lsp_document_latest_publication_parity.sh`: the real
`Main --document-store-probe` route rejects stale version, same-version payload
conflict, and a superseded diagnostics candidate while admitting the latest
candidate for each URI on C and LLVM.
LSP-2g feature-shape parity is checked by the response-emission and
session-replay parity gates because those are the artifact owners that consume
`feature_owner.pgy`.
LSP-2h session-state parity is checked by
`tests/self_hosted/parity/lsp_session_state_parity.sh`.
LSP-2i hover-content parity is checked by
`tests/self_hosted/parity/lsp_hover_content_parity.sh`.
