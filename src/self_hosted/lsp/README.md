# LSP Substitution Track

LSP-0 and LSP-1 are active as payload-only substitution rungs. They do not own
the JSON-RPC transport loop, stdin framing, hover, completion, or C LSP session
replacement.

- `diagnostics_owner.pgy` projects self-host semantic diagnostic blocks into a
  `textDocument/publishDiagnostics`-shaped JSON artifact.
- `squiggle_owner.pgy` owns the RED/AMBER/BLUE/VIOLET classification policy
  from diagnostic status/severity/stage/code/facts.
- `main.pgy` is the runnable boundary for parity fixtures.
- `fixture/` and `expected/` are the committed clean/error payload and
  squiggle-policy contracts. Error payloads carry both the self-host lower-case
  code and the C-oracle root code in `data.oracleCode`.

The `ReadStdin(n)` substrate for transport framing is present, and
`transport_owner.pgy` consumes it for one JSON-RPC Content-Length frame. That
is LSP-2a only. Full LSP-2 still needs repeated frame buffering and dispatch.
`O-LSP` has live diagnostic-dump plumbing, but full vocabulary/session parity
remains a later LSP-3 concern.
