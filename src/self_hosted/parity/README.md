# Self-Host Parity Harness

Status: thirteen rung-2 peripheral harnesses, five rung-1 AIR graph consumer
harnesses, one Pergyra-origin fuzz generator harness, plus lexer/parser rung-1,
semantic rung-2, and codegen rung-0..17 compiler-internal harnesses active.

This folder holds oracle comparisons for self-hosted tools and compiler-stage
substitutes. The C compiler and existing shell/C smokes remain the source of
truth until a Pergyra-written component can run and produce deterministic output
that agrees with the C oracle. JSON is used when the owned format is JSON;
semantic verdicts use diagnostic blocks, and codegen rungs use run-stdout.

Hard substitution rungs are parity gates promoted to pass conditions: failure
means the Pergyra substitute, the C oracle surface, or the LLVM oracle leg has a
real source-of-truth problem to close. Bridge scripts may compare artifacts, but
the Pergyra code must not recover hidden semantic facts by parsing older source
payloads.

The parity set currently covers:

- `diagnostic_catalog_checker`
- `stable_subset_section_checker`
- `air_graph_json_validator`
- `air_graph_id_uniqueness`
- `air_graph_node_count_integrity`
- `air_graph_ref_live`
- `air_graph_ref_integrity`
- `air_graph_reachability`
- `ast_read_surface_checker`
- `backend_output_comparator`
- `backend_output_tri_compare` (C/LLVM outputs checked by the Pergyra
  comparator; use `make self-host-backend-tri-compare-extended-test-smoke` for
  the opt-in 29-case C/LLVM closure gate)
- `module_manifest_resolver`
- `stdlib_dispatch_inventory_checker`
- `doc_link_checker`
- `production_header_size_checker`
- `production_c_size_checker`
- `examples_inventory_checker`
- `runtime_boundary_checker`
- `fuzz_backend_parity_generator` (Pergyra-origin deterministic source corpus
  generator; `make self-host-fuzz-backend-generator-parity-test-smoke` checks
  generator C/LLVM byte-identical corpus output, while
  `make fuzz-backend-parity-test-smoke` additionally runs the generated corpus
  through C/LLVM; `make fuzz-backend-parity-matrix-test-smoke` repeats that
  oracle over a bounded seed matrix)
- `lexer` (rung-1 compiler-internal lexer substitution)
- `parser` (rung-1 compiler-internal parser substitution)
- `semantic` (rung-2 compiler-internal semantic verdict substitution)
- `codegen` (rung-0..17 compiler-internal C-emitter substitution)

`make self-host-preparation-test-smoke` runs the full set. Individual parity
targets may still be used for focused work, but a tool is not considered
current unless its `src/self_hosted/parity/<tool>_parity.sh` rung passes.

Minimum parity contract for each tool:

- run the existing C or shell oracle against the same input;
- run the Pergyra tool against the same input;
- compile and run the Pergyra tool through both C and LLVM when the current
  compiler build includes LLVM; C-only builds must keep the C leg mandatory and
  emit an explicit LLVM-leg skip;
- compare exit class first;
- compare the owned stable output shape incrementally while the Pergyra tool
  remains a partial implementation.
- compile/run copied Pergyra tool sources from an ignored build scratch
  directory such as `.tmp/self_hosted/...`; parity harnesses must not leave
  `.exe`, `.o`, `.d`, or probe artifacts beside `src/self_hosted/tools/*/main.pgy`.
- mirror shared Pergyra helper libraries such as `src/self_hosted/lib/*.pgy` into
  the scratch layout expected by relative `import "../../lib/..."` paths before
  running a copied tool.

Compiler-core self-host migration from this folder is allowed only as a verified
rung with its own intent contract and C/LLVM/Pergyra parity gate.

Fuzz harnesses are intentionally separate from long-running fuzz campaigns.
The parity contract here is deterministic: fixed seed, fixed count, stable
manifest/source output, no executable-output extension ambiguity, and optional
generated-case C/LLVM execution equality. Crash minimization, corpus reduction,
and property-pack mapping remain outside the beta parity set until those
policies are frozen.
