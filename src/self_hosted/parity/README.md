# Soft Self-Host Parity Harness

Status: ten rung-2 parity harnesses plus one compiler-internal rung-1 harness active.

This folder holds oracle comparisons for soft self-host tools. The C compiler
and existing shell/C smokes remain the source of truth until a tool can run in
Pergyra and produce deterministic JSON output that agrees with the C oracle.

The parity set currently covers:

- `diagnostic_catalog_checker`
- `stable_subset_section_checker`
- `air_graph_json_validator`
- `backend_output_comparator`
- `module_manifest_resolver`
- `stdlib_dispatch_inventory_checker`
- `doc_link_checker`
- `production_header_size_checker`
- `production_c_size_checker`
- `examples_inventory_checker`
- `lex_minimal` (rung-1 compiler-internal substitution)

`make self-host-preparation-test-smoke` runs the full set. Individual parity
targets may still be used for focused work, but a tool is not considered
current unless its `src/self_hosted/parity/<tool>_parity.sh` rung passes.

Minimum parity contract for each tool:

- run the existing C or shell oracle against the same input;
- run the Pergyra tool against the same input;
- compare exit class first;
- compare JSON schema and stable counters incrementally while the Pergyra tool
  remains a partial implementation.
- compile/run copied Pergyra tool sources from an ignored build scratch
  directory such as `.tmp/self_hosted/...`; parity harnesses must not leave
  `.exe`, `.o`, `.d`, or probe artifacts beside `src/self_hosted/tools/*/main.pgy`.
- mirror shared Pergyra helper libraries such as `src/self_hosted/lib/*.pgy` into
  the scratch layout expected by relative `import "../../lib/..."` paths before
  running a copied tool.

No compiler-core self-host migration is allowed from this folder.
