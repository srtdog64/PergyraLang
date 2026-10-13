# Remove text-scan tests over docs and source

Status: `IMPLEMENTATION COMPLETE` (2026-10-14)
Base revision: `da227fcf8` (origin/main)
Integration owner: Claude lane

## Decision

The repository owner asked to remove tests that only check whether some text
exists or does not exist in the repository's own docs or source. Agents had
written them as "ratchets": a deleted name must not come back, a doc must
contain a phrase, a function must be named in a file, a file list must stay
fixed. Before this change there were roughly 430 such scripts, holding about
25,000 assertion lines. The largest single file,
`tests/self_hosted_component_contract_smoke.sh`, was 29,500 lines.

They do not prove behavior. They pin wording and file layout, so every
refactor fails on text rather than on a wrong result. Text the agents had
deleted on request came back as test strings that kept the deleted name alive.

Result: 100 files deleted and 396 edited, about 88,800 lines removed. The
largest file kept only its line caps, going from 29,500 lines to about 2,960.

From now on a gate compiles or runs something and checks its result or the
artifacts it emitted. AGENTS.md states the rule.

## What stays

- **Executable behavior.** Compile or run a fixture or binary and check the
  exit status, output, diagnostics or runtime result.
- **Checks on emitted artifacts.** The C, LLVM IR, MIR JSON and logs a test
  produced. These are golden-style behavior checks, including absence checks
  on the output.
- **Generator drift.** `render_*.py --check`, or regenerate and diff against
  the checked-in projection.
- **Line-size caps.** Left alone in this pass, including
  `tests/self_hosted_owner_size_policy.awk` and the per-owner caps.
- **Tree-wide hazard lints with a stated hazard.** Unsafe libc string calls,
  silent-fallback idioms over a whole directory, layer import rules in
  `scripts/sot_registry_gate.py`, UTF-8 and CRLF.
- **Link and path integrity.** A doc's relative links, and the paths a
  registry names, must resolve.

## What was removed

- Required or forbidden doc phrases.
- A symbol, string, schema tag or file that must exist in `src/`, `scripts/`,
  `Makefile` or test sources.
- Tombstones: a removed or retired name, file or pattern that must not
  reappear.
- Occurrence counts, caller counts, file lists, CI-step and test-suite freeze
  lists, sentinel or likeness budgets, and hash ledgers over source text.
- Self-tests whose only job was to exercise those checkers.
- In `scripts/sot_registry_gate.py`:
  - the requirement that a CLOSED row's gate file contain each
    `forbidden_fallbacks` name;
  - the scan of consumer sources for those names;
  - the `path#required-text` check of `enforcement_gate`. Only the path is
    checked now, and the text after `#` is a label for readers.

## Edit scopes

- **Nine batch agents (A1–A9)** each edited a disjoint list of test files and
  only deleted companions that their own files used. The rubric they followed
  is restated in the two sections above.
- **The integration owner** did the rest:
  - the wiring: `Makefile` targets, `scripts/ci_*` steps, cross-calls between
    tests, and meta-gates;
  - `AGENTS.md`, `docs/180`, `docs/131` and `docs/self_hosted/00_agent_entry.md`;
  - the registry gate and the registry field descriptions.

## Security-related text checks

The first pass left the security-related checks unchanged so the owner could
decide on them. On 2026-10-14 the owner decided:

- **Kept:** `tests/capability/run_{budget,cap_env_manifest,channel}_twin_parity.sh`.
  They hold the C-inline and LLVM-export runtime twins to the same budget
  charges, capability intersection and channel ops. A behavior fixture cannot
  easily reach that.
- **Removed:** `tests/security_portability_contract_smoke.sh` and
  `tests/memory_adversarial_catalog_smoke.sh`. The first checked crypto API
  names in runtime sources, the second the manifest counts and CI wiring.
- **Stripped:** `tests/memory_string_safety_smoke.sh` keeps only its four
  tree-wide hazard lints: unsafe libc string calls, `strtok`, truncated
  `snprintf` stack copies, and `+= snprintf` offset accumulation.

## Integration gate

- `bash -n` on every edited script, and `py_compile` on every edited Python
  file.
- No `Makefile`, CI-step or test reference to a deleted script.
- A WSL build of `bin/pgy`, then every edited gate that does not need the
  self-host driver.
- Gates that need an installed self-host driver cannot run on main until the
  DRV-2 bootstrap frontier is cleared. They are checked statically only, and
  the report says so.
- Run on 2026-10-13: the 325 edited scripts on the base commit and on this
  change, in a WSL clone with the native compiler only.
  - 0 scripts went from pass to fail.
  - 10 went from fail to pass. Each had failed on a stale text check.
  - 239 failed in both runs, each with the same last line before and after.
    The last lines show:
    - 198 stopped on a missing installed self-host driver;
    - 4 stopped on the missing Rocq toolchain;
    - 8 stopped on probes or test binaries that this run did not build;
    - 29 stopped on other errors that were not classified.

    Those scripts stopped before their behavior ran, so this is no evidence
    about their semantics. Per-script logs were not kept
    (`compiler_closure_priorities_2026-10-14.md` makes the same point).
  - The text checks had been hiding real failures that now show:
    - `ast_collection_exclusive_value_field_read_owner.pgy` is over its line cap
      (378 > 370);
    - `driver_rung2_owner.pgy` is over its 500-line cap;
    - the diagnostic-registry lint caught the ternary cause at a
      `da227fcf8` call site, which the separate fix commit resolves.
- Four scripts were never run by any target. Only a removed text check named
  them, and that mention made the reachability gate count them as reached:
  - `concurrency_examples_smoke.sh`
  - `direct_mir_scalar_runtime_value_lifecycle_owner.sh`
  - `expression_root_consumption_owner.sh`
  - `slice_length_codegen_source_owner.sh`

  They are now listed in `tests/manual_gate_inventory.tsv`.
- Two CLOSED SoT rows, `selfhost.expression_place_kind` and
  `selfhost.statement_result_type`, returned to BRIDGE. Their only enforcement
  gates were text scans. A third row, `semantic.symbol_type_graph`, was already
  BRIDGE and now records `none#...` as its gate.

## Migrating work in flight

A lane with uncommitted edits to a removed or stripped test should drop those
test edits when it rebases, rather than restore the file. The rule is to prove
the change with a fixture the compiler now refuses or answers differently. A
grep for the new function name does not prove it.

## Removed files

Docs and audits still name some of these as evidence. Such a mention describes
a text scan that no longer exists, not a live gate.

```text
benchmarks/selfhost_source_scan_owner_evidence.json
scripts/protocol_registry_gate.py
tests/abi_ownership_shape_smoke.sh
tests/abstraction_loss_contract_smoke.sh
tests/agent_boundary_sentinel_smoke.sh
tests/air_drift_smoke.sh
tests/artifact_atomic_transaction_contract_smoke.sh
tests/ast_dispatch_partition_smoke.sh
tests/ast_read_surface_smoke.sh
tests/ast_to_mir_loss_contract_smoke.sh
tests/async_direction_adequacy_smoke.sh
tests/async_model_adequacy_smoke.sh
tests/async_model_positioning_smoke.sh
tests/axis_keyword_adequacy_smoke.sh
tests/backend_compare_llvm_coverage_smoke.sh
tests/backend_wasm_pointer_closure_smoke.sh
tests/beta_checklist_shards.sh
tests/beta_test_suite_freeze_smoke.sh
tests/boundary_migration_contract_smoke.sh
tests/compiler_owner_cluster_smoke.sh
tests/evidence_lifecycle_adequacy_smoke.sh
tests/evidence_lifetime_smoke.sh
tests/fixtures/legacy_production_helper_paths.txt
tests/fixtures/ownership_clean_retired_owners.txt
tests/gate_sot_single_owner_smoke.sh
tests/generic_axis_matrix_smoke.sh
tests/grammar_cheatsheet_contract_smoke.sh
tests/intent_compression_contract_smoke.sh
tests/judgment_diagnostic_adequacy_smoke.sh
tests/language_contract_golden_smoke.sh
tests/language_keyword_axis_contract.py
tests/language_surface_hygiene_smoke.sh
tests/llvm_large_aggregate_return_stack_smoke.sh
tests/llvm_mir_region_scope_owner_smoke.sh
tests/loss_contract_adequacy_smoke.sh
tests/mir_lexical_binding_owner_smoke.sh
tests/mir_lowering_api_smoke.sh
tests/mir_only_signature_smoke.sh
tests/module_taxonomy_smoke.sh
tests/object_action_boundary_contract_smoke.sh
tests/ownership_clean_direction_selftest.sh
tests/ownership_clean_direction_smoke.sh
tests/ownership_relocation_cleanup_contract_smoke.sh
tests/parallel_core_contract_smoke.sh
tests/parallel_model_adequacy_smoke.sh
tests/projection_diagnostic_contract_smoke.sh
tests/protocol_registry_smoke.sh
tests/runtime_abi_lifetime_smoke.sh
tests/runtime_authority_contract_smoke.sh
tests/runtime_bc_contract_smoke.sh
tests/runtime_cext_contract_smoke.sh
tests/runtime_intent_observability_contract_smoke.sh
tests/runtime_panic_contract_smoke.sh
tests/self_host_compiler_topology_smoke.sh
tests/self_host_compiler_world_perf_probe.sh
tests/self_host_pergyra_likeness_smoke.sh
tests/self_host_program_graph_unification_smoke.sh
tests/self_host_readiness_scorecard.sh
tests/self_host_source_scan_owner_smoke.sh
tests/self_host_substitution_velocity_smoke.sh
tests/self_host_substrate_contract_smoke.sh
tests/self_host_text_builder_emission_smoke.sh
tests/self_hosted/parity/array_string_layout_consumer_ratchet.py
tests/self_hosted/parity/collection_argument_retirement_order.py
tests/self_hosted/parity/compiler_readonly_query_signatures.py
tests/self_hosted/parity/compiler_root_intent_takeover_gate.sh
tests/self_hosted/parity/direct_mir_mutation_ownership_signature_contract.sh
tests/self_hosted/parity/driver_rung2_collection_mutation_graph_use_owner.sh
tests/self_hosted/parity/driver_rung2_destructure_graph_use_owner.sh
tests/self_hosted/parity/driver_rung2_execution_action_gate.sh
tests/self_hosted/parity/driver_rung2_if_graph_use_owner.sh
tests/self_hosted/parity/driver_rung2_iteration_graph_use_owner.sh
tests/self_hosted/parity/driver_rung2_let_graph_use_owner.sh
tests/self_hosted/parity/driver_rung2_match_graph_use_owner.sh
tests/self_hosted/parity/driver_rung2_return_graph_use_owner.sh
tests/self_hosted/parity/driver_rung2_simple_statement_graph_use_owner.sh
tests/self_hosted/parity/driver_rung2_structured_expression_order_owner.sh
tests/self_hosted/parity/driver_rung2_while_graph_use_owner.sh
tests/self_hosted/parity/driver_source_mir_execution_action_gate.sh
tests/self_hosted/parity/driver_source_mir_install_transaction_gate.sh
tests/self_hosted/parity/intent_execution_protocol_static_owner.py
tests/self_hosted/parity/mir_expression_graph_projection_owner_smoke.sh
tests/self_hosted/parity/mir_json_artifact_writer_lifetime_owner.sh
tests/self_hosted/parity/mir_resource_graph_owner_smoke.sh
tests/self_hosted/parity/semantic_ability_bound_lifetime_owner_smoke.sh
tests/self_hosted/parity/semantic_enum_fact_lifetime_owner_smoke.sh
tests/self_hosted/parity/semantic_expression_environment_owned_lifetime_smoke.sh
tests/self_hosted/parity/semantic_expression_normalization_owner_smoke.sh
tests/self_hosted/parity/semantic_expression_validation_lifetime_owner_smoke.sh
tests/self_hosted/parity/semantic_function_table_owner_smoke.sh
tests/self_hosted/parity/semantic_initializer_environment_row_owner_smoke.sh
tests/self_hosted/parity/semantic_initializer_pressure_owner_smoke.sh
tests/self_hosted/parity/semantic_role_constraint_lifetime_owner_smoke.sh
tests/self_hosted/parity/semantic_try_operand_lifetime_owner_smoke.sh
tests/self_hosted/parity/semantic_tuple_shape_lifetime_owner_smoke.sh
tests/selfhost_driver_lsp_wiring_smoke.sh
tests/slot_analyzer_host_index_smoke.sh
tests/slot_calculus_adequacy_smoke.sh
tests/source_ast_inventory.sh
tests/type_resolution_resolver_inventory_smoke.sh
```
