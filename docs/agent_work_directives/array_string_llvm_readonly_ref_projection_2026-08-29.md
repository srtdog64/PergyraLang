# ArrayString LLVM readonly-ref target projection — 2026-08-29

Status: `LOCAL GREEN — PUBLICATION PENDING`

Exact base: `4a97f19a17a64e36a66b29747098a80811f11285` on `origin/main`.

This directive coordinates one executable prerequisite. It does not close or
reclassify `abi.mir_array_string_layout_projection`, and it is not semantic
authority or progress evidence by itself.

## Shared objective card

- Objective: make the production scalar LLVM read-only `Array<String>`
  parameter load consume the target-qualified projection already derived by
  the LLVM emission root, and delete the load's repeated storage-alignment
  literal.
- Priority: exact admitted layout identity, one fact/projection/target
  cross-seal, root-to-expression projection carriage, projected LLVM load
  alignment, read-only pointer semantics, C/LLVM execution parity,
  missing/drifted-layout failure with no artifact, old-path ratchet, then patch
  size.
- Fact owner: `DirectMirArrayStringCapturedAbiReady` owns admitted layout
  identity; `DirectMirScalarProgramArrayStringAbiFact` carries it;
  `DirectMirScalarProgramArrayStringAbiProjectionReadyForFact` owns the one
  program-fact/target-projection cross-seal.
- Last legitimate consumer:
  `DirectMirScalarProgramLlvmArrayStringReadonlyRefParameterRead` is the
  reached LLVM load consumer. `DirectMirScalarCfgEmitProgramLlvm` remains the
  one target-projection derivation root.
- Forbidden fallback: calling the read-only load without the carried
  projection, literal `align 8` in that load, deriving another projection in
  the expression or read-only owner, type-name lookup, accepting a missing or
  drifted ArrayString ABI fact, or changing pointer forwarding and read-only
  carriage semantics.
- Verification gate:
  `direct_mir_scalar_array_string_readonly_ref_owner.sh` must execute C/LLVM
  pointer parity and reject carriage, pass-shape, resource, type,
  ABI-required, and ABI-layout drift without artifacts. The component contract
  must require root-to-consumer projection carriage/readiness and reject the
  old LLVM load-alignment literal.

## Edit scope and overlap boundary

- Allowed implementation scope: the LLVM emission root/routine, LLVM
  operation and nested expression call-through owners needed to carry the
  already-derived projection, the LLVM expression/read-only consumer, focused
  mutation/gate, component ratchet, ArrayString registry consumer/rationale,
  `src/self_hosted/OWNERS.md`, and exact coordination/audit/handoff notes.
- The primary task is the sole integration owner. There are no parallel
  implementation tracks on this rung.
- Owned-parameter move semantics, value-result transfer, owned return,
  mutation, cleanup, C behavior, and other expression materializers remain
  outside scope and keep the top-level ABI row `BRIDGE`.

## Commands and budgets

- Static owner and structural checks target 60 seconds.
- The focused read-only C/LLVM gate targets five minutes; observed base runtime
  remains within that budget.
- Rebuild the current-source installed DRV-2 once after source changes, then
  reuse it. Full CI remains the exact-head publication boundary; no timeout,
  cache, or shard change is admitted.

## Output classification

Source and test edits are implementation candidates until focused parity and
negative gates, structural and registry ratchets, current-source driver
evidence, commit, push, and exact-head CI are observed. The census remains
`55/32/1`.

## Local implementation evidence

- The final source reproduces the fingerprinted Pergyra-built DRV-2, and that
  driver emitted the 11,348,955-byte production bootstrap C artifact.
- The focused read-only gate executes C/LLVM parity and rejects six parameter
  policy/layout mutations without artifacts.
- The full component contract, SoT edge/live adequacy, single-owner,
  hard-contract, tightened likeness `4501/4501`, documentation, and diff gates
  are green. Coq/Rocq is an explicit declared skip on this runner.
- All touched source owners remain within existing caps; no cap, timeout,
  cache, or shard was raised.
- The owned-parameter candidate remains outside this lease because its missing
  caller-side move retirement is a distinct executable correctness rung.
