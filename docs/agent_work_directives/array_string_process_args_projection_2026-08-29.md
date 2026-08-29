# ArrayString process Args target projection — 2026-08-29

Status: `ACTIVE — IMPLEMENTATION`

Exact base: `3beff662f2f536ba2bb45f1ab056584f2d5158b1` on `origin/main`.

This directive coordinates one executable prerequisite. It does not close or
reclassify `abi.mir_array_string_layout_projection`, and it is not semantic
authority or progress evidence by itself.

## Shared objective card

- Objective: make the production scalar C and LLVM process-Args adapters
  consume the target-qualified ArrayString projection already derived by their
  emission roots, and delete the LLVM adapter's repeated storage-alignment
  literals for ArrayString alloca, initialization, and result load.
- Priority: exact admitted layout identity, one fact/projection/target
  cross-seal, C/LLVM target binding, projected LLVM storage alignment,
  copied-string ownership, direct-call parity, missing/drifted-layout failure
  with no artifact, old-path ratchet, then patch size.
- Fact owner: `DirectMirArrayStringCapturedAbiReady` owns admitted layout
  identity; `DirectMirScalarProgramArrayStringAbiFact` carries it;
  `DirectMirScalarProgramArrayStringAbiProjectionReadyForFact` owns the one
  program-fact/target-projection cross-seal.
- Last legitimate consumers: `DirectMirScalarProgramCProcessArgsBlock` and
  `DirectMirScalarProgramLlvmProcessArgsBlock` are the reached adapters. Their
  C/LLVM collection materializers carry the already-derived projection.
- Forbidden fallback: calling either Args adapter without a target projection,
  literal ArrayString storage alignment in the LLVM Args alloca/store/load,
  adapter-local projection, type-name lookup, accepting missing/drifted ABI
  facts, changing argc/argv capture, or changing copied-string ownership.
- Verification gate: `direct_mir_scalar_process_args_direct_call_owner.sh`
  must execute nested-call C/LLVM parity and reject call identity plus
  ArrayString layout-align drift without artifacts. The component contract
  must require projection carriage and readiness, projected LLVM alignment,
  and reject the old three literal-alignment rows.

## Edit scope and overlap boundary

- Allowed implementation scope: C/LLVM process-Args adapters and their two
  collection-materialization call sites, focused mutation/gate, component
  ratchet, ArrayString registry consumer list/rationale, `OWNERS.md`, and exact
  coordination/audit/handoff notes.
- The primary task is the sole integration owner. There are no parallel
  implementation tracks on this rung.
- DirWalk, parameter binding, value-result transfer, owned return, mutation,
  cleanup, and other expression materializers remain outside scope and keep
  the top-level ABI row `BRIDGE`.

## Commands and budgets

- Static owner and structural checks target 60 seconds.
- The focused process-Args C/LLVM gate targets five minutes; observed base
  runtime is six seconds.
- Rebuild the current-source installed DRV-2 once after source changes, then
  reuse it. Full CI remains the exact-head publication boundary; no timeout,
  cache, or shard change is admitted.

## Output classification

Source and test edits are implementation candidates until focused parity and
negative gates, structural and registry ratchets, current-source driver
evidence, commit, push, and exact-head CI are observed. The census remains
`55/32/1`.

## Local implementation evidence

- A current-source Pergyra-built DRV-2 is installed. Native-oracle emission of
  `driver_bootstrap_main.pgy` completed with zero errors and the three already-
  declared redundant-`who` warnings.
- The focused Args gate executes C and LLVM successfully and rejects three
  call-identity mutations plus an ArrayString parameter-layout alignment
  mutation without publishing the requested artifact.
- Generated LLVM retains the admitted alignment value `8`, while the source
  materializer derives all three ArrayString storage uses from
  `projection.storage.align`; the old literal rows are structurally forbidden.
- The full self-host component contract, SoT edge and live adequacy checks,
  single-owner gate, hard contract, likeness `4493/4493`, and documentation
  quality gate are green. Local Coq/Rocq remains an explicit declared skip.
- Owner sizes are 35 lines for C Args, 29/30 for LLVM Args, 84/110 for C
  collection orchestration, and 102/115 for LLVM collection orchestration. No
  cap was raised.

This remains an implementation candidate until commit, push, and exact-head
CI are observed. `abi.mir_array_string_layout_projection` stays `BRIDGE`.
