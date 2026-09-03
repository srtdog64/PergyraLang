# Match scrutinee single-evaluation lease — 2026-09-03

Status: `LOCAL CANDIDATE GREEN — PUBLICATION AND EXACT CI PENDING`

Exact base: `513b668957cdb47627bd2bc490a45916337b2629` on
`origin/main`.

This is a bounded executable lease, not a new semantic authority or a general
match rewrite.

## Objective card

- Objective: make a side-effecting `match` scrutinee evaluate exactly once in
  the installed Pergyra-owned source/MIR path before case dispatch and payload
  extraction.
- Priority: preserve source evaluation order; retain the parser/semantic graph
  identity; materialize one stable value; make every case and payload consumer
  reference that value; fail closed on a missing or mismatched fact; then keep
  the representation minimal.
- Fact owner: `SemanticAstExpressionSurfaceFacts` retains original scrutinee
  meaning. `SemanticAstMatchMaterializationFacts` owns the target-neutral
  decision, synthetic local identity/type, and isolated binding graph root.
  `SemanticAstBodyTypeBundle` carries it once into `SelfMirRoutineInput`.
- Last legitimate consumer: `routine_match_owner.pgy` emits one MIR
  `AST_LET_DECL` from the original graph, then every Option/tagged case and
  payload consumer receives only the synthetic LocalRef graph. Direct AST C
  emission keeps the same one-temporary rule in `stmt_emit.pgy`.
- Forbidden fallback: one scrutinee expression per branch, one additional
  expression for payload extraction, backend-local purity guesses, native/self
  parity as the only oracle, source-text recovery, or a hidden runtime cache.
- Verification gate: an observable-call fixture must print one probe event per
  match in installed Pergyra-owned C execution. A missing stable temporary or
  second emitted probe call is the falsifying case. Pergyra-owned LLVM does not
  yet admit this Option-match structural shape and is not widened into this
  rung.

## Implementation edit scope

- `tests/self_hosted/fixtures/match_scrutinee_single_evaluation.pgy`
- `tests/self_hosted/parity/match_scrutinee_single_evaluation_owner.sh`
- `tests/self_hosted/parity/driver_rung2_match_materialization_delta_owner.sh`
- `tests/self_hosted/parity/driver_rung2_mir_producer_parity_owner.sh`
- `tests/self_hosted/parity/driver_rung2_wrapper_match_loop_phi_parity_owner.sh`
- `src/self_hosted/semantic/ast_match_materialization_fact_owner.pgy`
- `src/self_hosted/semantic/ast_body_type_bundle_owner.pgy`
- `src/self_hosted/semantic/ast_body_type_bundle_readiness_owner.pgy`
- `src/self_hosted/semantic/ast_expression_identity_resolution_owner.pgy`
- `src/self_hosted/mir/routine_input_owner.pgy`
- `src/self_hosted/mir/routine_local_inventory_owner.pgy`
- `src/self_hosted/mir/routine_match_owner.pgy`
- `src/self_hosted/mir/local_ref_identity_owner.pgy`
- `src/self_hosted/mir/artifact_lower_owner.pgy`
- `src/self_hosted/mir/body_type_bundle_storage_lifetime_owner.pgy`
- `src/self_hosted/codegen/emission/stmt_emit.pgy`
- `Makefile`, `src/self_hosted/OWNERS.md`, this directive, collaboration
  ledger, and handoff

Do not edit native codegen, Option/tagged shape owners, the SoT registry, or
generated inventories. Native direct C/LLVM match emission already evaluates
the AST subject once. The legacy native MIR oracle still reconstructs one
subject expression per case; the hard parity owner admits only the exact
Pergyra one-def-plus-synthetic-use delta until that oracle learns the same
shape, and automatically restores byte parity when it does.

## Current observations

- Pergyra-built DRV-2 C and explicit native-pipeline C each emit three calls
  for the existing pure `match c.Bump()` fixture; native-pipeline LLVM emits
  four call sites.
- Native MIR carries `c.Bump()` in two `AST_MATCH_CASE` branch `expr0` rows.
  Existing native/self and C/LLVM parity cannot falsify this because `Bump` is
  observationally pure for the current fixture.
- `match_scrutinee_single_evaluation.pgy` makes evaluation visible through one
  `Probe(Bool)` event per match. The installed C path prints each probe twice:
  condition evaluation plus payload/next-case evaluation. The exact expected
  output contains one `probe-some` and one `probe-none`; the focused gate fails
  on the duplicate events.
- The same fixture reaches semantic/MIR/C but Pergyra-owned LLVM currently
  rejects its Option binding as outside the admitted direct-MIR structural
  subset. Adding that structural shape is a different executable rung.
- The changed-source self-host driver compiles and links. Its execution prints
  the four expected lines exactly once each. Static match-graph ownership, the
  complete component contract, and focused hard parity for
  `class_bump_option_match`, `class_result_chain_loop`, and
  `class_method_result_loop` are green.
- Focused hard parity preserves all case/payload/phi negatives and consumes the
  self-canonical MIR through C byte and runtime comparison. It waives raw
  oracle MIR byte equality only while the oracle lacks the named synthetic
  match def; no general mismatch or fallback is accepted.
