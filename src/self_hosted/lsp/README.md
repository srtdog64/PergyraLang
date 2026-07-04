# LSP Substitution Track

LSP-0 and LSP-1 are active as payload-only substitution rungs. They do not own
the JSON-RPC transport loop, stdin framing, indexed hover/completion content,
or C LSP session replacement.

- `diagnostics_owner.pgy` projects self-host semantic diagnostic blocks into a
  `textDocument/publishDiagnostics`-shaped JSON artifact.
- `document_store_owner.pgy` projects buffered `didOpen`/`didChange` request
  bodies into deterministic multi-document state.
- `feature_owner.pgy` owns no-index response shapes for advertised
  `textDocument/*` features.
- `squiggle_owner.pgy` owns the RED/AMBER/BLUE/VIOLET classification policy
  from diagnostic status/severity/stage/code/facts.
- `request_owner.pgy` classifies buffered JSON-RPC request bodies through the
  shared JSON fact-table owner.
- `response_owner.pgy` projects response-required request bodies into response
  body/frame plans.
- `session_owner.pgy` replays one buffered request stream into response frames
  for the subset already owned by `response_owner.pgy`.
- `main.pgy` is the runnable boundary for parity fixtures.
- `fixture/` and `expected/` are the committed clean/error payload and
  squiggle-policy contracts. Error payloads carry both the self-host lower-case
  code and the C-oracle root code in `data.oracleCode`.

The `ReadStdin(n)` substrate for transport framing is present, and
`transport_owner.pgy` consumes it for one JSON-RPC Content-Length frame
(LSP-2a) and ordered multi-frame consumption from one stdin buffer (LSP-2b).
`request_owner.pgy` consumes those buffered bodies for request dispatch planning
(LSP-2c), `response_owner.pgy` emits basic response plans (LSP-2d),
`session_owner.pgy` replays emitted response frames from one buffered request
stream (LSP-2e), and `document_store_owner.pgy` projects buffered
`didOpen`/`didChange` into deterministic multi-document state (LSP-2f). `feature_owner.pgy`
provides valid no-index hover/completion/document-symbol/definition/references/
rename response shapes consumed by response/session replay (LSP-2g). Full LSP-2
still needs a live read-exact loop, session consumption of document-state
mutation, and semantic feature content.
`O-LSP` has live diagnostic-dump plumbing, but full vocabulary/session parity
remains a later LSP-3 concern.
