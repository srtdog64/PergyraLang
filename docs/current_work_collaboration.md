# Current Work Collaboration Ledger

Updated: 2026-08-26 (Asia/Seoul)

This file coordinates concurrent Codex work. It is not semantic authority and
does not prove completion. Current source, the SoT registries, executable gates,
and `docs/current_work_handoff.md` remain authoritative in that order.

## PUBLISHING lease A — intent mode/priority C-codegen last consumer

- Base revision: `3698ab198fd2d84ca66834db0ff90a22cb2ac9f1`.
- Active owner: Codex task `019f8921-1147-70c1-8eff-b6fee8e59aec`.
- Objective: delete the reconstructed-AST read of intent mode/priority from C
  emission and feed the last consumer one exact semantic-DIR or admitted-MIR
  policy receipt.
- Fact owner: production C uses admitted MIR `IntentMode` and
  `IntentEval(priority)` carriers plus the canonical expression occurrence;
  the direct semantic codegen entrypoint materializes the same receipt from
  admitted DIR policy facts.
- Last legitimate consumer: `CodegenIntentObservabilityEmitPrologue`.
- Forbidden fallback: `TypedAstArena*` or `AstTreeArtifact` in the mode/priority
  emission owners, missing-receipt defaults, name-only joins, graph
  reconstruction, or a dual MIR/AST read.
- Integration gate: complete component contract; current-source driver build;
  installed nested mode/priority C parity including missing, duplicate,
  missing-graph, and graph-drift negatives; composite-intent LLVM parity; then
  documentation/registry gates and `git diff --check`.

### Edit lease

Until this lease is marked `DONE`, do not edit, stage, commit, or overwrite:

- `src/self_hosted/codegen/input/intent_policy_codegen_view_owner.pgy`
- `src/self_hosted/compiler/intent_policy_c_codegen_bridge_owner.pgy`
- `src/self_hosted/compiler/codegen_callable_receiver_bridge_owner.pgy`
- `src/self_hosted/compiler/driver_rung2_owner.pgy`
- `src/self_hosted/codegen/emission/intent_*emit_owner.pgy`
- `src/self_hosted/codegen/emission/program_{emit,entry,admitted_semantic_owner}.pgy`
- `tests/self_hosted/parity/intent_{mode,priority}_nested_observability_owner.sh`
- `tests/self_hosted_component_contract_smoke.sh`
- `src/self_hosted/OWNERS.md`
- `docs/current_work_handoff.md`

Preserve the unrelated user-owned untracked `pgy-80135c2c/` directory. Do not
inspect it as project evidence, stage it, delete it, or rewrite it.

### Current observed evidence

- `tests/self_hosted_component_contract_smoke.sh`: PASS after the AST-read
  residue ratchet and owner inventory update.
- Current source graph through the Pergyra-built codegen seed: PASS,
  10,609,620-byte C artifact, 114.27 seconds, no `CODEGEN ERROR`.
- Isolated current-source driver C compile: PASS, 17.12 seconds.
- Nested intent priority/mode MIR carriage, C execution parity, missing,
  duplicate, missing-graph, and graph-drift rejection: PASS.
- Composite-intent direct-MIR LLVM success/failure parity and four no-artifact
  negatives: PASS.
- Zero-intent source-to-C through the isolated driver: PASS.
- Official current-source `make self-host-compiler`: PASS in 515.18 seconds;
  `bin/pgy-self-driver.exe` was installed by the Pergyra-built DRV-2 path.
- Installed nested mode/priority MIR carriage and public/native C execution
  parity: PASS, including missing, duplicate, missing-graph, and graph-drift
  rejection.
- Installed composite-intent direct-MIR LLVM success/failure parity and four
  no-artifact negatives: PASS.
- Final complete component contract: PASS after its internal source-MIR action
  ratchet and the syntax-ID/name stale-identity ratchet both passed.
- Next executable falsifier is observed RED rather than inferred: projecting
  the nested value-priority fixture's admitted MIR through
  `--mir-json-backend=llvm` fails closed at `scalar-program-route` stage
  `referenced-enum` and publishes no artifact. This is outside lease A and must
  not be repaired until lease A is published and a new objective card is set.
- First publication checkpoint `b6de9ba7` produced CI run `32862729216` at
  28/29. All full self-host, codegen bootstrap, platform, sanitizer, proof, and
  20 backend shards passed; `build-linux` alone caught likeness drift:
  sentinel `25 > 24` and Result/Option use `4264 < 4267`.
- The pending fix does not loosen either ratchet. Absent priority uses the
  existing explicit `priority_present` bit with a non-semantic zero storage
  value, while routine lookup carries absence as local `Option<Int>`. Local
  likeness is back at sentinel `24/24` and Result/Option `4267/4267`.
- The fixed current-source graph generated and compiled; nested mode/priority C
  parity and composite-intent LLVM parity pass with that isolated driver. The
  exact failed Linux target passed component, hard, and graph gates locally,
  then stopped only because local Coq/Rocq is unavailable; remote Rocq passed.
- Fix checkpoint `eba0103d` completed run `32866213832` at 29/29 green in
  29m53, including the repaired Linux likeness row, full self-host, all three
  platforms, sanitizers, Rocq, codegen bootstrap, and all 20 backend shards.
- A peer follow-up after that run required absent priority receipts to keep the
  canonical zero storage value. The pending final checkpoint adds that exact
  readiness invariant and its structural negative; local likeness, docs, the
  source-MIR action, and complete component contract pass. A new remote run is
  required because this is a code invariant, not a result-only note.

### CI ratchet lesson

When deleting an Option-heavy AST scan, run
`make self-host-pergyra-likeness-test-smoke` before publication. Do not replace
typed absence with a numeric sentinel or raise the likeness ceiling to hide the
drop. If a view already has an explicit presence bit, its unused numeric slot
is storage only; lookup failure itself remains `Option`/`Result`.

## Proposed lease B — referenced-enum LLVM last consumer

This card is prepared only. It must not be claimed or implemented until lease
A is marked `DONE` and the corrected checkpoint is remotely green.

- Goal: make installed direct-MIR `--mir-json-backend=llvm` consume the
  referenced-enum fact owner for the observed RED fixture instead of failing
  closed at `scalar-program-route` stage `referenced-enum`.
- Priority order: exact declaration/variant/payload identity, one fact-owner
  read, migrate the last LLVM orchestration consumer, delete or narrow the
  rejection-only path for this admitted shape, negative ratchet, then
  execution parity.
- Fact owner:
  `src/self_hosted/compiler/direct_mir_scalar_program_referenced_enum_fact_owner.pgy`
  owns referenced declarations, variant ordinals/names, and payload types.
- Last legitimate consumer: the direct-MIR LLVM program projector's
  referenced-enum admission/projection seam.
- Forbidden fallback: priority `0` hardcode, source-text reparse, AST
  reconstruction, fixture branching, native retry, per-fixture special cases,
  or starting a general query/cache track.
- Falsifying fixture: admit the MIR from
  `tests/self_hosted/parity/fixture/intent_priority_nested_observability.pgy`
  through the installed driver's LLVM backend; it currently publishes no
  artifact and fails at `owner=scalar-program-route stage=referenced-enum`.
- Observed root-cause evidence (2026-08-26): the RED MIR has no enum
  declarations, only `PriorityProbe`, `PriorityProbeZone`, and four routines.
  `DirectMirScalarProgramReferencedEnumFactFromAdmitted` first builds a
  signature fact for every routine. `DirectMirRoutineSignatureFactReady`
  accepts `kind == "function"` only, so the fixture's method/intent routines
  invalidate that scan before any referenced declaration can be selected. The
  route then rejects at `referenced-enum`. The rung must therefore replace this
  scalar-signature-only reference scan with an admitted-declaration-driven
  referenced-enum owner path; it must not widen routine signatures or pretend
  absent enums are the semantic fact.
- Gates: that RED becomes executable parity without artifact publication on
  failure; retain the four composite-intent no-artifact negatives; run
  `tests/self_hosted/parity/direct_mir_composite_intent_program_llvm_owner.sh`;
  then component/hard/graph documentation gates before publication.

## Peer Codex assignment — read-only audit only

If another Codex is operating on this checkout, perform this bounded task and
do not implement changes in lease A:

1. Read the active objective and diff without modifying files.
2. Look only for dual authority, a hidden AST/default fallback, an inexact
   canonical intent join, a missing negative gate, or a mismatched call
   signature/lifetime.
3. Report findings with exact file and line evidence to the user or append them
   under `Peer review notes` below. If there are no findings, say which claims
   were checked; do not report a generic approval.
4. Do not commit or push shared `main` while lease A is `ACTIVE` or
   `PUBLISHING`.

## Peer review notes

- Peer Codex (read-only) reviewed the lease A diff before its final
  integration evidence was appended. Two findings were fixed by the lease
  owner in the current worktree:
  1. `CodegenIntentExecutionPlanDefinitionBlock` still accepted an unused
     `expression_surfaces` parameter after the policy-view migration;
     removed together with the stale call argument in `program_emit.pgy`.
  2. `CodegenIntentPolicyMirRoutineRowOrDie` could skip a same-syntax-id MIR
     intent row with a mismatched name instead of rejecting it. The lease owner
     tightened the join further after re-audit: canonical syntax ID is claimed
     first, any mismatched intent name fails immediately with its own stale-
     identity diagnostic, and duplicate syntax ownership is rejected even if a
     second row has the expected name.
- No dual authority, hidden AST/default fallback, or missing negative gate was
  found in the reviewed slice. The semantic entrypoint's DIR receipt scan is
  documented as an admission-time read, not a C-emission AST fallback.
- Follow-up read-only finding: the later sentinel cleanup changed absent
  priority rows from `-1` to `0`, but readiness initially did not reject an
  absent row with nonzero root. The lease owner restored the exact invariant
  `priority_present[row] || root == 0` and added a component-contract ratchet
  for the absent-row nonzero receipt.
