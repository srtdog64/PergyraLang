# `src/self_hosted/mir_lower/`

`mir_lower` is the Pergyra-origin MIR JSON fact consumer for the hard
self-hosting path. It reconstructs the compact AST tree consumed by the
Pergyra codegen slice, but its source of truth is MIR JSON, not source text and
not C-side AST accessors.

`main.pgy` is only the CLI/orchestration boundary. Semantic decisions are owned
by the sibling modules:

- `json_fact_read.pgy` owns bounded JSON fact access.
- `decl_lower.pgy` owns declaration fact reconstruction.
- `routine_lower.pgy` owns routine and CFG traversal.
- `stmt_render.pgy` owns statement/expression rendering from MIR facts.
- `error_owner.pgy` owns the fail-closed diagnostic boundary.

Unsupported declaration or instruction facts must fail closed with
`MIR-LOWER ERROR`; they must not be reconstructed by re-reading source AST text.
The executable contract is `make self-host-mir-json-parity-test-smoke`.
