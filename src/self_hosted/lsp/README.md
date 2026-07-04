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
  squiggle-policy contracts.

Transport parity stays blocked on the `G-STDIN` and `O-LSP` gaps documented in
`docs/150_selfhost_driver_lsp_wiring.md`.
