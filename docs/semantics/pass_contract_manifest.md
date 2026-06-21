# Compiler Pass Contract Manifest

Status: `beta-proof-obligation`

This manifest is the pass-level companion to
[`09_abstraction_loss_contracts.md`](09_abstraction_loss_contracts.md). The loss
contract says what a boundary may lose. This file pins the major beta-closure
passes to the facts they must consume, preserve, invalidate, and diagnose.

It is not a claim that every pass is fully closed. It is a machine-gated index:
each row names a live stage artifact and a live smoke gate. A pass that is not
listed here is not allowed to be cited as beta-stable proof evidence for the
five closure targets.

## Contract Fields

Each pass row has these fields:

| Field | Meaning |
|---|---|
| `pass_id` | Stable identifier used by docs and smoke gates. |
| `stage_artifact` | Primary owner file for the pass contract. |
| `enforcement_gate` | Smoke gate that proves the current contract surface. |
| `required_facts` | Facts the pass must consume from an owner artifact. |
| `preserved_facts` | Facts the pass must carry forward or expose as evidence. |
| `invalidated_facts` | Facts the pass intentionally stops later layers from using. |
| `stable_diagnostics` | Diagnostics or hard-fail vocabulary that must remain stable. |
| `forbidden_reads` | Reads that would reopen the source-of-truth seam. |

Forbidden-read vocabulary is intentionally shared across rows:

- `unowned_ast_rescan`: a later pass rereads AST/source when a typed fact owner
  already exists.
- `backend_ast_semantic_read`: C or LLVM recovers a semantic fact from AST
  instead of MIR/DIR/RIR/DAG metadata.
- `backend_local_layout_guess`: a backend chooses ABI/layout facts without a
  MIR ABI or runtime ABI owner.
- `compat_success_without_fact`: compatibility code succeeds when the
  beta-stable fact owner is missing.
- `backend_source_axis_physicalization`: a backend turns a source-level
  world/zone/intent/slot axis into a physical carrier, padding, barrier, or
  runtime check without an AIR/MIR/ABI fact.

## Manifest

Columns:

`pass_id | stage_artifact | enforcement_gate | status | required_facts | preserved_facts | invalidated_facts | stable_diagnostics | forbidden_reads`

<!-- BEGIN pass-contract-manifest -->
```text
mir_cfg_body_safety | src/compiler/mir_fact_surface_validate.c | tests/cfg_body_dataflow_smoke.sh | gate-backed | source_statement_inventory, source_local_type_names, routine_signature, resource_ops, cleanup_edges | cfg_edges, branch_join, loop_facts, cleanup_roots, pin_regions, channel_receive_facts, cancellation_facts | AST_body_order, source_payload_shape_as_semantics | missing MIR value expression fact; STMT fallback is outside allowed residual statement policy | unowned_ast_rescan, backend_ast_semantic_read, compat_success_without_fact
air_boundary_evidence | src/compiler/air_validate_global_evidence.c | tests/air_drift_smoke.sh | gate-backed | MIR_evidence_nodes, RIR_boundary_evidence, DAG_metadata_counts, runtime_evidence_nodes | provider_subject_identity, authority_evidence, effect_evidence, coordination_evidence, fallback_count_zero | summary_counter_as_proof, anonymous_boundary_evidence | missing boundary evidence; AIR global evidence fallback count must be zero | unowned_ast_rescan, compat_success_without_fact
air_abstraction_compression | src/compiler/air_boundary.c | tests/air_json_schema_smoke.sh | gate-backed | AIRIntentNode, AIRBoundaryNode, authority_contract, sync_class, failure_class, evidence_provenance | compression_budget, compression_reason, proof_gated_erasure_vocabulary | backend_source_axis_physicalization, optimizer_guess_erasure | unknown compression budget is invalid; AIR JSON must expose compression_budget and compression_reason | backend_source_axis_physicalization, backend_local_layout_guess, compat_success_without_fact
dag_type_resolution | src/semantic/type_checker_resolution_metadata.c | tests/type_resolution_resolver_inventory_smoke.sh | gate-backed | type_resolution_metadata, stable_constructed_shells, generic_default_rows, ability_bound_rows | resolved_type_identity, metadata_hits, metadata_dead_ends, stable_diagnostic_categories | recursive_resolver_fallback, materializer_fallback, nullable_annotation_read | recursive resolver fallback is retired; metadata dead-end trace | unowned_ast_rescan, compat_success_without_fact
mir_decl_bootstrap_parity | src/codegen/llvm_decl.c | tests/mir_declaration_inventory_smoke.sh | gate-backed | MIR_routine_signature, MIR_decl_headers, ordered_intent_bindings, hosted_field_views, ability_ref_metadata | C_LLVM_signature_parity, declaration_inventory_parity, generic_default_actuals | AST_signature_reconstruction, AST_binding_order_reconstruction | silent i32 fallback is not allowed; MIR-only path missing metadata | backend_ast_semantic_read, compat_success_without_fact
abi_slot_pin_layout | src/compiler/mir_abi_layout.c | tests/abi_ownership_shape_smoke.sh | gate-backed | runtime_ABI_spec, MIR_type_layout, slot_handle_shape, pin_region_facts, resource_owner_metadata | ownership_shape, lifetime_shape, panic_failure_ABI, explicit_tag_option_layout | backend_local_option_niche, packed_field_addressability, slot_handle_packing | missing owner slot ABI metadata; explicit tag MIR fact required | backend_local_layout_guess, compat_success_without_fact
```
<!-- END pass-contract-manifest -->

## Acceptance Rule

A new beta-stable lowering, verifier, optimizer, layout pass, or backend pass
must either:

1. add a row here with a real owner artifact and gate, or
2. explicitly declare itself out of beta proof scope.

It is not enough for a pass to compile or produce matching output. The row must
state which facts the pass reads, which facts it preserves, which facts become
invalid, which diagnostics prove a missing fact, and which older sources it is
forbidden to reread.

## Remaining Work

This manifest is still coarse-grained. It should be split further when the
implementation gains separately owned optimization passes, a direct wasm
backend, source-level layout controls, backend consumption of AIR compression
facts, or broader self-hosted compiler slices.
Until then, these rows are the beta-closure choke points for the five active
compiler source-of-truth targets.
