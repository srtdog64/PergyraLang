# Compiler Clone transport: bounded reconciliation

Read-only source/wire inspection on the String seed-bridge generation,
2026-11-18. This is not an executed copy-transport closure claim.

## One measurement

Frozen complete DRV-2 MIR:
`.tmp/selfhost-memory-20261117/driver.string-boundary.mir.json` in WSL
`/root/mg-main`, SHA-256
`e7767d3059972797baf41654ececd912bfb91a0755547afea6336e6af8bbdf19`.
The input contains 953 declarations and 9,623 routines; its Main source is
`src/self_hosted/compiler/driver/driver_rung2_main.pgy`.

A streaming inspection found 301 selected rows in `call_handoff.clones`,
one action with `compiler_clone=true`, 143 explicit Clone call nodes and
zero expression-node `compiler_clone` fields. Of the selected ledger rows,
106 name a callee and 195 have an empty callee (root/builtin-store cases).
These are different metrics. None counts runtime independent copies or
proves that the selected copy is subsequently released.

## Reached producer/consumer chain

- `ast_expression_graph_compiler_clone_owner` attaches the selected overlay
  to the admitted semantic expression graph.
- `ast_body_collection_ownership_stage_owner` publishes that graph.
- `mir/artifact_lower_owner` carries it to program facts;
  `mir/call_handoff_row_owner` serializes a separate clone ledger.
- `mir/instruction_json_artifact_writer_owner` does not serialize the
  graph's clone overlay on expression nodes.
- The AST emitter consumes the overlay; an AST-text round trip may analyze
  source again. Neither operation demonstrates direct-MIR copy transport.
- `driver_compiler_clone_route_owner` counts a narrower class of declared
  routine/constructor argument landings. Its zero is not the number of
  selected overlay rows. Source route checks and raw MIR entrypoints are
  also distinct boundaries.

The older DM0 "zero uncarried" and lifetime/cases "174 selected" observations
therefore do not establish a contradiction merely by their numbers: they
use different inputs, revisions and predicates. Conversely, the existence
of a ledger does not establish executable transport. That obligation is
still OPEN before S5c or mixed-origin normalization relies on a copy.

## Next falsifier

On one admitted program with a selected copy, establish the producer row,
exact consumer, independent result and post-copy source read. Remove only
the execution transport in a scratch artifact and require a value mismatch
or `-O0` ASan fault, then restore it. A source probe refused before producing
MIR cannot serve as an admitted-use-after-free witness.

The String seed bridge uses only explicit builtin result-origin contracts;
it neither fixes nor depends on this still-open Clone obligation.
