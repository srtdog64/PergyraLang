# ArrayString C DirWalk target projection — 2026-08-29

Status: `PUBLISHED — REMOTE GREEN`

Exact base: `568c7b07292ee4ddee4e573b4f1ec1db2d2e9f27` on `origin/main`.

This directive coordinates one executable prerequisite. It does not close or
reclassify `abi.mir_array_string_layout_projection`, and it is not semantic
authority or progress evidence by itself.

## Shared objective card

- Objective: make the production scalar C `DirWalk` adapter consume the
  target-qualified ArrayString projection already derived by the C emission
  root, delete its positional four-field reconstruction, and keep the
  program-fact/projection cross-seal behind one named readiness owner.
- Priority: exact admitted layout identity, one shared fact/projection
  readiness decision, C target binding, field-name projection, C/LLVM direct-
  call parity, missing/drifted-layout failure with no artifact, old-path
  ratchet, then patch size.
- Fact owner: `DirectMirArrayStringCapturedAbiReady` owns admitted layout
  identity; `DirectMirScalarProgramArrayStringAbiFact` carries it;
  `DirectMirScalarProgramArrayStringAbiProjectionReadyForFact` owns the one
  program-fact/target-projection cross-seal.
- Last legitimate consumers: the reached last consumer is
  `DirectMirScalarProgramCDirWalkBlock`. Existing scalar C storage and LLVM
  collection materializers also consume the shared readiness verdict and must
  delete their duplicate local cross-seal functions in this ownership move.
- Forbidden fallback: positional `{source.data, source.length, ...}` layout,
  hard-coded adapter field names, adapter-local projection, duplicated C/LLVM
  fact/projection readiness, type-name lookup, accepting missing/drifted ABI
  facts, or changing the public `PgyArray_String`/private `pgy_as` type boundary.
- Verification gate: `direct_mir_scalar_dir_walk_direct_call_owner.sh` must
  execute nested-path C/LLVM parity and reject call identity plus ArrayString
  layout mutations without artifacts. The component contract must require the
  shared readiness owner and projected C adapter fields and reject the old
  positional initializer and duplicate readiness owners.

## Edit scope and overlap boundary

- Allowed implementation scope: the scalar program ArrayString projection
  owner, existing C/LLVM projection-readiness consumers, C DirWalk adapter and
  its collection-materialization call site, focused mutation/gate, component
  ratchet, ArrayString registry consumer list/rationale, and exact coordination
  and audit notes.
- The primary task is the sole integration owner. There are no parallel
  implementation tracks on this rung.
- LLVM DirWalk, Args, parameter binding, value-result transfer, owned return,
  mutation, cleanup, and other expression materializers remain outside scope
  and keep the top-level ABI row `BRIDGE`.

## Commands and budgets

- Static owner and structural checks target 60 seconds.
- The focused DirWalk C/LLVM gate targets five minutes; observed base runtime
  is 10 seconds.
- Rebuild the current-source installed DRV-2 once after source changes, then
  reuse it. Full CI remains the exact-head publication boundary; no timeout,
  cache, or shard change is admitted.

## Output classification

Source and test edits are implementation candidates until focused parity and
negative gates, structural and registry ratchets, current-source driver
evidence, commit, push, and exact-head CI are observed. The census remains
`55/32/1`.

## Local implementation evidence

- A fresh current-source Pergyra-built DRV-2 is installed. Native-oracle
  emission of `driver_bootstrap_main.pgy` completed with zero errors and the
  three already-declared redundant-`who` warnings.
- The focused DirWalk gate executes C and LLVM successfully and rejects the
  three call-identity mutations plus an ArrayString allocator-offset mutation
  without publishing the requested artifact.
- Generated C uses target-projected designated fields:
  `.data`, `.length`, `.capacity`, and `.allocator`; the old positional
  initializer is absent.
- The full self-host component contract, SoT edge and live adequacy checks,
  single-owner gate, hard contract, likeness `4493/4493`, and documentation
  quality gate are green. Local Coq/Rocq remains an explicit declared skip.
- Owner sizes are 39/50 for scalar-program projection, 58/90 for C storage,
  99/115 for LLVM collection materialization, 24/25 for C DirWalk, and 80/110
  for C collection orchestration. No cap was raised.

Publication result: implementation
`067871d5e1697d5b8d3655057f625b203f938a46` is on `origin/main`; exact-head
run `33241239809` completed GREEN 30/30. `build-linux` passed in 24m36s and
full self-host passed in 21m21s. Backend parity passed 20/20. The lease is
released. `abi.mir_array_string_layout_projection` stays `BRIDGE`.
