# ArrayString LLVM Join target projection — 2026-08-29

Status: `DONE — PUBLISHED GREEN`

Exact base: `18a0b6666ef3b6119a1c0482693a12bffe1f3798` on `origin/main`.

This directive coordinates one executable prerequisite. It does not close or
reclassify `abi.mir_array_string_layout_projection`, and it is not semantic
authority or progress evidence by itself.

## Shared objective card

- Objective: make the production scalar-program LLVM `StringJoin` body consume
  the target-qualified `DirectMirArrayStringAbiProjection` already derived by
  the LLVM emission root, deleting its local data/length field-index literals.
- Priority: preserve admitted ArrayString layout identity and runtime-call ABI,
  reuse the one target projection, fail closed when it is absent or drifted,
  execute Join in LLVM/C parity, ratchet the retired index literals, then
  minimize patch size.
- Fact owner: `DirectMirArrayStringCapturedAbiReady` owns admitted ArrayString
  layout identity; `DirectMirScalarProgramArrayStringAbiFact` carries the
  program receipt; `DirectMirArrayStringAbiProjection` owns selected-target
  aggregate spelling and field indices.
- Last legitimate consumer: for this rung only,
  `DirectMirScalarProgramLlvmStringJoinBlock`, reached through
  `DirectMirScalarProgramLlvmStringCollectionMaterialization` from the
  production scalar LLVM emission entrypoint.
- Forbidden fallback: literal `extractvalue` indices 0/1 in the Join body,
  type-name lookup, a Join-local projection, runtime symbol or argument-shape
  inference, accepting a missing/drifted program projection, or changing Join
  ownership/allocation semantics.
- Verification gate: `one_mir_string_collection_builtin_projection.sh` must
  execute Split/Join and ArrayString access in C and LLVM, retain its no-
  artifact negative matrix, and the component contract must reject a Join body
  that stops consuming `data_index`/`length_index` from the target projection.

## Edit scope and overlap boundary

- Allowed implementation scope: the LLVM Join materialization owner, its one
  collection-materialization call site, the focused gate's source ratchet,
  the component contract, the ArrayString registry consumer list/rationale,
  and coordination/audit notes for this exact prerequisite.
- The primary task is the sole integration owner. There are no parallel
  implementation tracks on this rung.
- StringSplit, process/directory adapters, parameter binding, value-result
  transfer, owned return, mutation, cleanup, and other expression consumers
  remain outside scope and keep the top-level ABI row `BRIDGE`.

## Commands and budgets

- Static owner and structural checks target 60 seconds.
- The focused String collection C/LLVM gate targets five minutes; its observed
  base runtime is 24 seconds.
- Reuse the current-source installed driver for the first focused gate. Rebuild
  once only if source-fingerprint evidence requires it. Full CI remains the
  exact-head publication boundary; no timeout, cache, or shard change is
  admitted.

## Local implementation evidence

- The LLVM collection materializer passes its already-admitted target
  projection to `DirectMirScalarProgramLlvmStringJoinBlock`. The Join body
  consumes `data_index` and `length_index`; its literal 0/1 reads are deleted
  and structurally forbidden.
- A fresh Pergyra-built current-source DRV-2 was installed. With that exact
  driver, `one_mir_string_collection_builtin_projection.sh` passed C/LLVM
  Split/Join, ArrayString access, semantic-change execution, and every existing
  no-artifact negative in 25 seconds.
- The full component inventory is GREEN. The Join owner is 18/20 and the LLVM
  collection orchestration owner is 115/115; no cap was raised.
- Native-oracle driver C emission completed with zero errors and the three
  pre-existing intent-clause warnings. SoT edge reports 88 authorities, 182
  carriers, and `55/32/1`; live adequacy mutations, single-owner, hard
  contract, likeness `4493/4493`, and documentation quality are GREEN. Local
  Coq/Rocq execution is a declared skip, not a claimed proof run.
- Implementation `fef8fb1fd10b1d32e3f49962c857089da662fd5e` is on
  `origin/main`. Exact-head run `33238680231` completed GREEN 30/30, including
  full self-host, Linux/Windows/macOS, sanitizers, Rocq, codegen bootstrap, and
  backend parity 20/20. The implementation lease is released.

## Output classification

This bounded prerequisite is published executable evidence. It does not own or
imply whole-row closure. The census remains `55/32/1`.
