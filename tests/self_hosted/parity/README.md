# Self-Host Parity Harness

Status: fifteen rung-2 peripheral harnesses, five rung-1 AIR graph consumer
harnesses, one Pergyra-origin fuzz generator harness, plus lexer/parser rung-1,
semantic rung-2, and codegen rung-0..20 compiler-internal harnesses active.

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
- `abi_layout_row_manifest`
- `backend_output_comparator`
- `backend_output_tri_compare` (C/LLVM outputs checked by the Pergyra
  comparator; use `make self-host-backend-tri-compare-extended-test-smoke` for
  the opt-in 29-case C/LLVM closure gate)
- `completeness_impact_planner` (changed-path JSON plus `run_group_plan`
  projection for runner-consumable proof-gate groups; the paired run-group
  runner validates all groups and executes a bounded prefix)
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
  through C/LLVM and treats generated nonzero exits as invariant failures;
  `make fuzz-backend-parity-matrix-test-smoke` repeats that oracle over a
  bounded seed matrix. The generated corpus includes predicate-driven cursor
  updates so branch-style and predicate-value-style state transitions stay
  equivalent across backends)
- `lexer` (rung-1 compiler-internal lexer substitution)
- `parser` (rung-1 compiler-internal parser substitution)
- `semantic` (rung-2 compiler-internal semantic verdict substitution)
- `codegen` (rung-0..20 compiler-internal C-emitter substitution)

`make self-host-preparation-test-smoke` runs the full set. Individual parity
targets may still be used for focused work, but a tool is not considered
current unless its `tests/self_hosted/parity/<tool>_parity.sh` rung passes.

Minimum parity contract for each tool:

- run the existing C or shell oracle against the same input;
- run the Pergyra tool against the same input;
- compile and run the Pergyra tool through both C and LLVM when the current
  compiler build includes LLVM; C-only builds must keep the C leg mandatory and
  emit an explicit LLVM-leg skip;
- compare exit class first;
- compare the owned stable output shape incrementally while the Pergyra tool
  remains a partial implementation.
- compile/run the TestHarness-manifest-projected Pergyra source owner directly;
  scratch directories such as `.tmp/self_hosted/...` are for binaries, logs, and
  comparable artifacts, not copied source trees.
- keep full-corpus probes and campaigns opt-in or bounded by default, so routine
  self-host verification does not accumulate a campaign's worth of scratch
  output.
- use `make clean-scratch` to reset accumulated `.tmp/self_hosted` and
  backend-compare scratch artifacts without touching build outputs. The target
  resets the whole ignored `.tmp` scratch zone, and bootstrap compiler logs must
  stay bounded evidence artifacts rather than unbounded compiler stderr dumps.
- use `make build-resource-report` before broad local CI runs when the machine
  feels stalled. `make clean-local-artifacts` is the explicit heavier reset for
  ignored root-level `build-*` / `bin-*` variants plus `.tmp`; it is not part of
  normal narrow-gate work.
- never leave `.exe`, `.o`, `.d`, or probe artifacts beside
  `src/self_hosted/tools/*/main.pgy`.

Compiler-core self-host migration from this folder is allowed only as a verified
rung with its own intent contract and C/LLVM/Pergyra parity gate.

Fuzz harnesses are intentionally separate from long-running fuzz campaigns.
The parity contract here is deterministic: fixed seed, fixed count, stable
manifest/source output, no executable-output extension ambiguity, and optional
generated-case C/LLVM execution equality. Crash minimization, corpus reduction,
and property-pack mapping remain outside the beta parity set until those
policies are frozen.
