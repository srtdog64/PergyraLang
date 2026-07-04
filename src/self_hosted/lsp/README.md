# LSP Substitution Track

LSP-0 is active as a payload-only substitution rung. It does not own the
JSON-RPC transport loop, stdin framing, hover, completion, or C LSP session
replacement.

- `diagnostics_owner.pgy` projects self-host semantic diagnostic blocks into a
  `textDocument/publishDiagnostics`-shaped JSON artifact.
- `main.pgy` is the runnable boundary for parity fixtures.
- `fixture/` and `expected/` are the committed clean/error payload contract.

Transport parity stays blocked on the `G-STDIN` and `O-LSP` gaps documented in
`docs/150_selfhost_driver_lsp_wiring.md`.
