# Match scrutinee single-evaluation lease — 2026-09-03

Status: `FULL-BOOTSTRAP BORROW REPAIR LOCAL GREEN — EXACT CI PENDING`

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
- Implementation commit `56ecb3359d00509541bef3840091d450fc3614ed`
  reached exact run `33759932471`. Its backend-compare toolchain job failed
  while the seed compiler generated the new readiness owner because two scoped
  bindings reused `statement_row` with incompatible `Option<Int>` and `Int`
  types. The run was cancelled after this first decisive failure.
- The bounded repair gives those bindings distinct, type-stable names:
  `statement_type_row` and `scan_row`. The same local CI stage,
  `self-host-codegen-bootstrap-seed-test-smoke`, exits 0 and reports gen2
  codegen/parser seed artifacts ready. A fresh exact-head run owns the final
  Linux seed and full-matrix verdict.
- Repair `cadae062a775deca1be40008da1136c126d98dbe` reached exact run
  `33762482359`. The backend toolchain and compiler-pair publication are green
  in 11m15s, as are sanitizer, TSan, Rocq 9, Windows, macOS, and self-host
  codegen fixed-point. Linux push passed 22 of 23 steps and failed only at the
  final preparation contract because the generated language-word
  implementation inventory did not yet count the new fixture. The run was
  cancelled after this decisive result.
- Canonical `render_language_keyword_registry.py --write` changes only the
  eight fixture-file counts for language words present in the new fixture.
  Generator `--check` and `language_keyword_registry_smoke.sh` are green. The
  next exact head owns the complete-matrix verdict.
- Inventory closure `12dd5a2eb6c204e2959d32ae52c8478efc4cff0d` reached
  docs-only run `33766129864`. Documentation, the language registry, and the
  post-self-host manifest passed; the SoT edge then correctly rejected the new
  `*_fact_owner.pgy` as unclassified. It is a projection of
  `selfhost.expression_surface`, not an authority: the original graph retains
  meaning while this carrier records only the exactly-once materialization
  decision and stable synthetic reference. Derived carriers therefore rise
  from 184 to 185, with 88 authorities and `55/32/1` unchanged.
- Registry closure `2eb5b3be55d4100b59ef9434cc779704705b173b` passed docs-only
  run `33766760335`. Full exact-head run `33766884880` then failed only when
  Linux full bootstrap saw `SelfMirLowerMatchFromArtifact(ref input, ...)`
  store the nested borrowed expression graph in a newly constructed view. The
  26 later TextBuilder diagnostics were cascades, not independent owners.
- `SelfMirMatchSyntheticGraphView` now receives expression-surface facts by
  value, validates the synthetic root, and constructs the view without
  retaining `ref input` provenance. The focused gate rejects any return of the
  direct nested graph store. The observable fixture remains exact-once green.
- The exact `full_mir_seed` reproduction exits 0 after all 7,476 routines and
  writes a 278,796,939-byte MIR artifact. Local full driver bootstrap also
  exits 0 and proves gen2/gen3 fixed point in 1,699,815ms. Its 2.451GiB peak
  working set and 2.744GiB peak private bytes cross the 80% attention threshold
  without crossing the hard limit; exact-head CI owns the final matrix verdict.
