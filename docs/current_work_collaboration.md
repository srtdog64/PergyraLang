# Current Work Collaboration Ledger

Updated: 2026-08-26 (Asia/Seoul)

This file coordinates concurrent Codex work. It is not semantic authority and
does not prove completion. Current source, the SoT registries, executable gates,
and `docs/current_work_handoff.md` remain authoritative in that order.

## DONE lease A — intent mode/priority C-codegen last consumer

- Base revision: `3698ab198fd2d84ca66834db0ff90a22cb2ac9f1`.
- Completed owner: Codex task `019f8921-1147-70c1-8eff-b6fee8e59aec`.
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

### Released edit lease

The following paths were exclusive to lease A while it was active. The lease is
now released; this list is historical overlap evidence:

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
  canonical zero storage value. Final code checkpoint `a9e07841` adds that exact
  readiness invariant and its structural negative. Run `32870231909` completed
  29/29 green in 29m24, including full self-host, codegen bootstrap, all three
  platforms, sanitizers, Rocq, and all 20 backend shards. Lease A is closed.

### CI ratchet lesson

When deleting an Option-heavy AST scan, run
`make self-host-pergyra-likeness-test-smoke` before publication. Do not replace
typed absence with a numeric sentinel or raise the likeness ceiling to hide the
drop. If a view already has an explicit presence bit, its unused numeric slot
is storage only; lookup failure itself remains `Option`/`Result`.

## DONE lease B — nested intent direct-MIR LLVM route

Lease A's publication preconditions are satisfied: correction checkpoint
`a9e07841` completed CI run `32870231909` at 29/29 green. Codex goal
`019f8921-1147-70c1-8eff-b6fee8e59aec` owns this executable rung. Other tasks
remain read-only auditors; do not open parallel implementation tracks on the
same route.

- Objective: make the installed direct-MIR `--mir-json-backend=llvm` path
  execute the nested method/intent priority fixture through one exclusive mixed
  callable route. Do not treat the first `referenced-enum` rejection as the
  entire objective.
- Priority order: canonical declaration and routine identity; routine
  kind/owner/source-syntax identity; exact intent mode/priority carrier and
  expression occurrence; owner-directed call/field graph; fail-closed
  negatives; then installed LLVM execution parity.
- Fact owners: the program declaration index owns exact declaration row/bounds,
  the graph owns its bounded source-syntax projection, and the routine index
  owns normalized callable kind/owner/source syntax ID;
  `MirIntentRoutineCarrierProjection`,
  `MirIntentModeProjection`, and `MirIntentPriorityProjection` own intent policy
  carriage and its exact semantic expression root.
- Last legitimate consumer: an exclusive mixed function/method/intent route in
  `DirectMirMultiRoutineProjection`, before the scalar-only route, handing its
  sealed graph to the direct-MIR LLVM terminal projector.
- Forbidden fallback: priority `0` hardcode, source-text reparse, AST
  reconstruction, widening the function-only signature fact to disguise a
  mixed-callable route, fixture branching, native retry, per-fixture special
  cases, or starting a general query/cache track.
- Observed RED: admitted MIR from
  `tests/self_hosted/parity/fixture/intent_priority_nested_observability.pgy`
  through the installed LLVM backend publishes no artifact and first fails at
  `owner=scalar-program-route stage=referenced-enum`.
- Dispatch cause: the fixture currently falls into the scalar-only route, whose
  first rejection is `referenced-enum`; a local no-enum repair would then expose
  `callable-route-envelope stage=signature`. The executable delta instead
  claims one mixed-callable route after composite intent and before scalar
  admission. The referenced-enum owner is not reached and is outside this
  lease.
- Minimal executable delta: add the exclusive mixed-callable route, sealed
  graph/plan, LLVM emitter, and thin projection; exact-cross-seal declaration,
  routine, intent-policy, ordered intent-binding, and expression-graph identities;
  derive `Main -> Outer`, `Outer -> Inner`, `Inner -> Capture`, and subject/zone
  field identity from admitted facts. Preserve priority as a literal-or-formal-
  parameter operand rather than collapsing it to `Int`; evaluate Inner's
  dynamic parameter `requested` in LLVM routine scope, and never substitute
  priority `0` for absence.
- Falsifiers: installed direct-MIR LLVM emits, links, and runs with byte-equal
  expected output and no scalar-route receipt; Outer observes literal priority
  `1`, Inner observes runtime `requested`; graph drift, missing priority, and
  syntax/name crosswire publish no artifact. Referenced-enum name/source-ID/
  payload drift remains a separate owner negative because this fixture has no
  enums. Retain composite-intent no-artifact negatives and finish with the
  component, hard, graph, documentation, and installed parity gates.

### Current observed evidence

- The exclusive route now claims after composite intent and before scalar
  admission. It seals `Main -> OuterPriority -> InnerPriority -> Capture`, the
  subject/zone fields, literal Outer priority `1`, and Inner priority from the
  unique `value/Int/requested` intent binding. It does not touch the unrelated
  referenced-enum owner.
- Actual MIR contradicted the initial routine-parameter assumption: intent
  header `params` are empty and `world/probe/requested` live in ordered intent
  binding carriers. Method `self` is an implicit receiver with null type/ABI,
  so it is validated at the receiver boundary and excluded from the explicit
  typed parameter set. Ownerless routine identity comes only from the admitted
  routine index rather than a second raw-JSON owner read.
- An isolated current-source driver emitted LLVM, linked through the public
  self-host path, and matched native LLVM against the exact nine-line golden.
  The focused gate completes in about seven seconds with five no-artifact
  negatives: missing Inner priority, priority graph drift, duplicate source
  identity, method-owner crosswire, and semantic action-name/target-row
  crosswire.
- `make self-host-direct-mir-nested-intent-program-llvm-test-smoke` rebuilt and
  installed Pergyra-built DRV-2, then passed the same LLVM parity and five
  negatives using the installed driver. The complete component contract also
  passed, including its source-MIR execution ratchet.
- The new Make target is in the existing full fixed-point invocation in both
  push CI and weekly self-host parity; it adds no job and no second self-host
  compiler build. The dispatcher remains at its existing 110-line ratchet.
- Hard contract, progress metric, UTF/documentation, and diff gates pass.
  Likeness passes at sentinel `24/24` and Result/Option `4287/4287`; the improved
  typed-error count is tightened into the baseline rather than left as a CI
  warning.
- Checkpoint `2d43bd66` is published. Run `32884881665` completed 28/29: all
  executable, proof, platform, sanitizer, and backend jobs passed, but Linux
  preparation found that the two new fact owners lacked derived-registry rows.
  The bounded repair classifies both under existing `mir.execution_graph` as
  `projection`; the exact edge is locally green at 86 authorities / 180 derived
  carriers without changing `CLOSED=49 BRIDGE=36 ACTIVE=1`.
- Repair checkpoint `6be30daa` completed run `32888031601` at 29/29 green in
  29m19. `build-linux` passed in 15m18, full self-host passed in 29m15, and all
  20 backend shards, sanitizers, platforms, codegen, and Rocq remained green.
  This lease is closed. Overall stays 78%, strict beta stays 83%, and hard SoT
  stays `CLOSED=49 BRIDGE=36 ACTIVE=1`.

## Peer Codex assignment — read-only audit only

If another Codex is operating on this checkout, perform this bounded task and
do not implement changes in lease B:

1. Read the active objective and diff without modifying files.
2. Look only for dual authority, a hidden AST/default fallback, an inexact
   canonical intent join, a missing negative gate, or a mismatched call
   signature/lifetime.
3. Report findings with exact file and line evidence to the user or append them
   under `Peer review notes` below. If there are no findings, say which claims
   were checked; do not report a generic approval.
4. Do not commit or push shared `main` while lease B is `ACTIVE` or
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
- Lease B read-only header audit found two impossible assumptions before the
  executable path was sealed: ownerless routines store `owner:null` while the
  admitted index owns normalized `""`, and method `self` has null type/ABI so
  it cannot enter the explicit typed-formal owner. Both were corrected without
  weakening the shared parameter fact.
- Lease B read-only gate audit kept LLVM behavior out of the 174/180-line
  C-only priority gate, selected a separate 160-line sibling gate, preserved
  the dispatcher 110-line cap, and connected both CI workflows through the
  existing single Make invocation.
