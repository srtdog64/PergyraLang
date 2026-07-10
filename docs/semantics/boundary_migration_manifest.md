# Boundary Migration Manifest

Status: `beta-proof-obligation`

This manifest makes compiler ownership movement executable. A migration is not
complete because a new owner exists; it is complete only when consumers read
the new owner, missing facts fail closed, the old producer is gone, and a gate
rejects the retired path.

Canonical protocol: `docs/180_compiler_logical_spine_handles_gates.md`, section
"Boundary Migration Protocol".

## Row Contract

Columns:

```text
migration_id | fact_kind | stable_handle | old_owner | new_owner |
allowed_bridge | forbidden_old_reads | consumer_inventory | parity_fixture |
negative_fixture | retirement_gate | status
```

- paths are repository-relative;
- comma-separated path fields must be non-empty and every path must exist;
- evidence fields use `path#required-text` so the gate proves that the named
  artifact contains the load-bearing assertion;
- `forbidden_old_reads` is a comma-separated token list. Every token must be
  named by the retirement gate;
- `shadow`, `repointing`, and `mandatory` require the old owner to exist;
- `retired` requires the old owner to be absent;
- aliases and `new ? old` fallback authority are never valid migration states.

<!-- BEGIN boundary-migration-manifest -->
```text
selfhost.local-binding.semantic-owner.v1 | local_binding_identity_and_initializer | artifact_node_id | src/self_hosted/codegen/input/ast_text_local_binding_owner.pgy | src/self_hosted/semantic/ast_local_binding_fact_owner.pgy | none | ast_text_local_binding_owner.pgy,CodegenAstArenaLetNameOrDie,CodegenAstArenaLetTypeNameOrDie,ast_local_initializer_codegen_view_owner.pgy,CodegenAstArenaLetInitializerOrDie | src/self_hosted/codegen/emission/stmt_emit.pgy,src/self_hosted/codegen/emission/function_emit.pgy,src/self_hosted/codegen/emission/program_emit.pgy | src/self_hosted/semantic/ast_local_binding_fact_owner.pgy#func SemanticAstLocalBindingFactsContractReady | src/self_hosted/semantic/ast_local_binding_fact_owner.pgy#invalid_facts.diagnostic_code == "local_binding_invalid" | tests/self_hosted_component_contract_smoke.sh#ast_local_initializer_codegen_view_owner.pgy | retired
selfhost.initializer-type.semantic-owner.v1 | initializer_type_verdict | artifact_node_id | src/self_hosted/semantic/program_check_owner.pgy | src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy | none | CheckProgram,CheckBody,LoadSemanticSource | src/self_hosted/compiler/driver_rung2_owner.pgy | tests/self_hosted/parity/driver_rung2_initializer_parity.sh#initializer verdict parity ok | src/self_hosted/semantic/expected/bad_call_assign.diag#Code: let_type_mismatch | tests/self_hosted_component_contract_smoke.sh#reject_text "src/self_hosted/compiler/driver_rung2_owner.pgy" "CheckProgram(" | repointing
```
<!-- END boundary-migration-manifest -->

## Promotion Rule

1. `shadow`: new owner and parity evidence exist; no consumer claim.
2. `repointing`: consumer inventory is explicit and migration is in progress.
3. `mandatory`: consumers fail closed on missing new facts; old owner may exist
   only as a non-authoritative producer awaiting deletion.
4. `retired`: old owner is absent and the retirement gate names every forbidden
   old read.

No row may skip the evidence fields, reuse another migration ID, or point to a
gate that merely documents the old path.
