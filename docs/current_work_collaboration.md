# Current Work Collaboration Ledger

Updated: 2026-08-25 (Asia/Seoul)

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
