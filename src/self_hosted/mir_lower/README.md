# `src/self_hosted/mir_lower/`

`mir_lower` is the Pergyra-origin MIR JSON fact consumer for the hard
self-hosting path. It reconstructs the compact AST tree consumed by the
Pergyra codegen slice, but its source of truth is MIR JSON, not source text and
not C-side AST accessors.

`main.pgy` is only the CLI/orchestration boundary. Semantic decisions are owned
by the sibling modules:

- `run_owner.pgy` owns CLI mode selection and output orchestration.
- `mir_json_input_owner.pgy` owns argv path selection, file reads, and schema
  gating for MIR JSON input.
- `fixture_manifest_owner.pgy` owns the curated MIR parity source fixture
  manifest consumed by the shell runner.
- `json_fact_read.pgy` owns bounded JSON fact access, including MIR
  declaration row/object/array bounds.
- `decl_lower.pgy` owns declaration fact reconstruction and consumes MIR
  declaration rows from `json_fact_read.pgy`.
- `program_lower.pgy` owns document-order Program assembly and supported
  routine selection.
- `routine_inventory_owner.pgy` owns routine discovery and bounded routine
  header facts.
- `routine_lower.pgy` owns CFG/body reconstruction for a selected routine.
- `stmt_render.pgy` owns statement/expression rendering from MIR facts.
- `error_owner.pgy` owns the fail-closed diagnostic boundary.

Unsupported declaration or instruction facts must fail closed with
`MIR-LOWER ERROR`; they must not be reconstructed by re-reading source AST text.
The executable contract is `make self-host-mir-json-parity-test-smoke`.
